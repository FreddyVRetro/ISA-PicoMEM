/*
    8250/16450 UART emulation core -- WonderMCA lock-free rewrite.

    Originally adapted from PicoGUS (Copyright (c) 2024 Artem Vasilev - wbcbz7,
    MIT license); architecturally reworked for WonderMCA 2026-07-15.

    This is a GENERIC 16450 register model: it knows nothing about the device
    behind the port. Devices (serial mouse today; modem bridge, serial
    console, ... tomorrow) plug in through uartemu_ops_t and the Core-0
    producer API. Multiple instances are supported (one uartemu_t per COM
    port) -- no globals, no heap.

    DESIGN RULE (see WonderMCA/doc/DEVLOG.md 2026-07-15): Core 1 (the MCA bus
    handler) only READS a register image and pushes guest writes into an SPSC
    queue -- no locks, no alarms, no callbacks, no IRQ raising. Core 0
    (device task -> uartemu_task) owns ALL protocol state, interrupt
    bookkeeping, RX byte pacing and the physical IRQ line. Every shared field
    has exactly ONE writer core, so no critical section exists anywhere.

    Why the rework: the PicoGUS original ran its port traps on the same core
    as its device logic, so a shared critical_section was cheap. On WonderMCA
    the traps run on Core 1 *inside a CHRDY-stretched bus cycle*; the
    inherited lock coupled the bus handler to Core-0 alarm callbacks and
    produced a family of cross-core races (unlocked int_queue RMW, shared
    one-shot timer-event aliasing, RX framing slip, split RTS handling).

    The one deliberate cross-core exception: Core 1 may LOWER the IRQ line
    when the guest drains the last RX byte (lower-only, idempotent-safe), so
    a Core-0 stall (SD/USB) can't turn one serviced byte into a level-IRQ
    refire burst. Core 0's per-pass invariant re-raises if anything is still
    pending, healing any lower-vs-push race within one loop pass.

    Not modeled (16450 scope, extend when a device needs it): FIFOs (FCR is
    ignored, IIR never reports FIFO mode), RX error/break LSR bits, overrun
    flag (uartemu_queue_bytes simply reports no-room to the device).
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------
// device hooks -- all Core-0 context unless noted
// -----------------------------------------------------------------------
typedef struct {
    // Physical IRQ line. raise/lower run on Core 0 (uartemu_task).
    // c1_lower runs on CORE 1 (drain-to-empty) and must be a single
    // atomic-safe operation (mca_irq_lower qualifies). Any may be NULL.
    void (*raise_irq)(void *ctx);
    void (*lower_irq)(void *ctx);
    void (*c1_lower_irq)(void *ctx);

    // Guest wrote MCR: delivered IN ORDER from the write stream, with the
    // previous value, so the device can do edge logic (a mouse derives
    // power from DTR+RTS, a modem watches DTR for hangup). t_write_us is
    // the time_us_32() captured ON CORE 1 at the guest's OUT -- durations
    // between MCR edges must use it, never the drain time, or a Core-0
    // stall corrupts edge timing (the 3 ms mouse power rule). May be NULL.
    void (*mcr_change)(void *ctx, uint8_t prev_mcr, uint8_t new_mcr, uint32_t t_write_us);

    // Guest transmitted a byte (THR write, not loopback): guest -> device
    // data path (a modem consumes this; a mouse ignores it). May be NULL.
    void (*tx_byte)(void *ctx, uint8_t byte);

    void *ctx;
} uartemu_ops_t;

// -----------------------------------------------------------------------
// instance -- fields are PRIVATE to 8250uart.cpp; exposed only so devices
// can allocate instances statically. One writer core per field (see .cpp).
// -----------------------------------------------------------------------
enum {
    UARTEMU_RXQ_SZ  = 4,    // guest-visible RX ring (<=1 live byte + stale)
    UARTEMU_PEND_SZ = 32,   // Core-0 pending device->guest bytes
    UARTEMU_IOWQ_SZ = 32,   // guest-write events awaiting Core-0 processing
};

typedef struct uartemu_t {
    uint16_t iobase;

    // ---- Core-1-owned ----------------------------------------------------
    volatile uint8_t  ier, lcr, mcr, scratch, dll, dlm; // write-through shadows
    volatile uint8_t  rxq_tail;
    volatile uint32_t rbr_reads, iir_reads, msr_reads;
    volatile uint32_t c1_lowers;
    volatile uint16_t iowq[UARTEMU_IOWQ_SZ];    // slots + head: C1 producer
    volatile uint32_t iowq_ts[UARTEMU_IOWQ_SZ]; // time_us_32 at guest OUT
    volatile uint8_t  iowq_head;
    volatile uint32_t iowq_drops;
    uint8_t  last_rbr;

    // ---- Core-0-owned ----------------------------------------------------
    volatile uint16_t rxq[UARTEMU_RXQ_SZ];   // (epoch << 8) | byte
    volatile uint8_t  rxq_head;
    volatile uint8_t  rx_epoch;
    volatile uint8_t  iir_shadow;
    volatile uint8_t  msr_static;            // CTS/DSR/RI/DCD image
    volatile uint8_t  msr_delta;             // delta bits (incl. TERI)
    volatile uint8_t  iowq_tail;             // consumer index

    // ---- Core-0-private ----------------------------------------------------
    uint8_t  pend[UARTEMU_PEND_SZ];
    uint8_t  pend_head, pend_tail;
    uint8_t  byte_live, live_slot;
    uint8_t  tx_int;
    uint8_t  pace_rush;                      // deliver next byte unpaced
                                             // (loopback echo); consumption
                                             // must not re-delay it
    uint8_t  mcr_seen;                       // last MCR processed (edges)
    uint8_t  ier_seen;                       // last IER processed (THRE edge)
    uint8_t  line_raised;
    uint64_t next_byte_t;
    uint32_t rx_interval_us;
    uint32_t iir_reads_seen, msr_reads_seen;
    uint32_t iowq_drops_seen;
    uint32_t c1_lowers_seen;
    uint32_t irq_raises, irq_lowers;
    uint32_t heal_live;                      // invariant-watchdog fires (expect 0)

    // ---- Core-1-owned debug snapshots (what Core 1 ACTUALLY sees) --------
    // Written on every LSR/IIR read; printed by Core 0. If these disagree
    // with Core 0's own head/tail/slot view, the two cores are not looking
    // at the same memory/code -- the 2026-07-15 bench contradiction.
    volatile uint8_t  c1_dbg_head, c1_dbg_tail;
    volatile uint16_t c1_dbg_entry;          // raw rxq[tail&3] as C1 read it
    volatile uint8_t  c1_dbg_iir;            // last IIR value C1 returned

    uartemu_ops_t ops;
} uartemu_t;

// -----------------------------------------------------------------------
// lifecycle (Core 0, before Core-1 dispatch is armed)
// -----------------------------------------------------------------------
void uartemu_init(uartemu_t *u, uint16_t iobase, const uartemu_ops_t *ops);

// -----------------------------------------------------------------------
// Core-1 bus entries (called ONLY from the device's ior/iow glue)
// -----------------------------------------------------------------------
uint8_t uartemu_c1_read (uartemu_t *u, uint8_t reg);       // reg = addr & 7
void    uartemu_c1_write(uartemu_t *u, uint8_t reg, uint8_t val);

// -----------------------------------------------------------------------
// Core-0 task: drain guest writes, pace RX bytes, drive the IRQ line
// -----------------------------------------------------------------------
void uartemu_task(uartemu_t *u);

// -----------------------------------------------------------------------
// Core-0 device API
// -----------------------------------------------------------------------
bool    uartemu_tx_idle(uartemu_t *u);       // no device byte awaiting the guest
bool    uartemu_queue_bytes(uartemu_t *u, const uint8_t *b, uint8_t n);
void    uartemu_rx_flush(uartemu_t *u);      // drop pending + in-flight RX bytes
void    uartemu_set_modem_lines(uartemu_t *u, int cts, int dsr, int ri, int dcd);
void    uartemu_set_rx_interval_us(uartemu_t *u, uint32_t us);
uint8_t uartemu_get_mcr(uartemu_t *u);       // last guest MCR (Core-0 view)
uint16_t uartemu_get_divisor(uartemu_t *u);  // programmed baud divisor
uint8_t uartemu_get_lcr(uartemu_t *u);       // word format (7N1 vs 8N1 ...)

// -----------------------------------------------------------------------
// debug snapshot (Core 0, throttled logging only -- lock-free reads)
// -----------------------------------------------------------------------
struct uartemu_dbg_t {
    uint8_t  ier, lcr, mcr, iir, epoch;
    uint8_t  pend_occ, byte_live;
    uint8_t  tx_int, msr_delta, line_raised;
    uint32_t iowq_drops;
    uint32_t rbr_reads, iir_reads, msr_reads;
    uint32_t c1_lowers;
    uint32_t irq_raises, irq_lowers;
    uint32_t heal_live;
};
void uartemu_debug(uartemu_t *u, struct uartemu_dbg_t *out);

// ABI-skew canary: sizeof(uartemu_t) as compiled into 8250uart.o. The device
// glue compares it with ITS OWN sizeof at init -- a mismatch proves the two
// translation units were built against different struct layouts (stale
// object), which would produce exactly a "two cores see different structs"
// bench signature.
uint32_t uartemu_struct_size(void);

#ifdef __cplusplus
}
#endif
