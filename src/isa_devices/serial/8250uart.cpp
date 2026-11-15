/*
    8250/16450 UART emulation core -- WonderMCA lock-free rewrite.
    See 8250uart.h for the architecture contract and DEVLOG 2026-07-15 for
    the bug history that motivated it.

    Originally adapted from PicoGUS (Copyright (c) 2024 Artem Vasilev - wbcbz7,
    MIT license); reworked for WonderMCA 2026-07-15.

    Ownership map (every field has exactly ONE writer core):

      Core-1-owned (bus handler writes, Core 0 reads):
        ier/lcr/mcr/scratch/dll/dlm  guest write-through register shadows --
                                     an OUT followed by an IN reads back
                                     coherently within the same bus cycle
                                     (CTMOUSE's O/I probe, loopback MSR test)
        rxq_tail                     RX queue consumer index
        rbr_reads/iir_reads/msr_reads  read-side-effect counters (Core 0
                                     diffs them instead of an event queue --
                                     unoverflowable by construction)
        iowq[], iowq_head            guest-write event queue (SPSC producer)
        c1_lowers                    lower-on-empty count

      Core-0-owned (uartemu_task writes, Core 1 reads):
        rxq[], rxq_head, rx_epoch    guest-visible RX byte (depth <= 1)
        iir_shadow                   presented IIR (C1 derive-overrides RX)
        msr_static/msr_delta         modem line image
        iowq_tail                    event queue consumer index

      Core-0-private: everything else.

    RX path: the device queues into pend[] (Core 0); the pacer moves ONE
    byte at a time into the guest-visible rxq slot every rx_interval_us --
    true 16450 single-byte semantics, no silent overwrite, no framing slip.
    Each rxq entry carries rx_epoch in its high byte: uartemu_rx_flush()
    bumps the epoch and any in-flight stale byte is discarded by Core 1's
    peek without ever touching the consumer index cross-core.
*/

#include <string.h>
#include "pico/time.h"
#include "hardware/sync.h"     // __dmb()
#include "pico/platform.h"     // __not_in_flash_func
#include "8250uart.h"

// Core-1 entries execute inside the CHRDY-stretched MCA bus cycle and MUST
// live in SRAM: Core 1 fetching flash-XIP code mid-cycle both stretches the
// cycle unpredictably and contends with Core 0's XIP stream (project rule --
// see mca_irq.c). Without this they end up at 0x1001xxxx via a veneer
// (verified in the 2026-07-15 bench binary).
#define UARTEMU_C1_FUNC(f) __not_in_flash_func(f)

// -----------------------------------------------------------------------
// lifecycle
// -----------------------------------------------------------------------
void uartemu_init(uartemu_t *u, uint16_t iobase, const uartemu_ops_t *ops) {
    memset((void *)u, 0, sizeof(*u));
    u->iobase         = iobase;
    u->iir_shadow     = 0x01;         // no interrupt pending
    u->rx_interval_us = 5555;         // ~1200 baud effective default
    if (ops) u->ops = *ops;
}

// -----------------------------------------------------------------------
// Core-1 side
// -----------------------------------------------------------------------

// Peek the RX queue from Core 1, discarding stale-epoch entries (Core 1 is
// the only tail writer, so advancing past them here is SPSC-legal).
static inline bool c1_rx_peek(uartemu_t *u, uint8_t *out) {
    while (u->rxq_tail != u->rxq_head) {
        __dmb();
        uint16_t e = u->rxq[u->rxq_tail & (UARTEMU_RXQ_SZ - 1)];
        if ((uint8_t)(e >> 8) == u->rx_epoch) {
            if (out) *out = (uint8_t)e;
            return true;
        }
        u->rxq_tail++;                // stale epoch: discard
    }
    return false;
}

static inline void c1_lower(uartemu_t *u) {
    if (u->ops.c1_lower_irq) {
        u->ops.c1_lower_irq(u->ops.ctx);
        u->c1_lowers++;
    }
}

uint8_t UARTEMU_C1_FUNC(uartemu_c1_read)(uartemu_t *u, uint8_t reg) {
    switch (reg & 7) {
        case 0:                                   // RBR / divisor LSB
            if (u->lcr & 0x80) return u->dll;
            {
                uint8_t b;
                if (c1_rx_peek(u, &b)) {
                    u->last_rbr = b;
                    u->rxq_tail++;                // consume
                    u->rbr_reads++;
                }
                // Queue drained (or spurious read): drop the IRQ line NOW.
                // Core 1 may only LOWER -- idempotent-safe; Core 0 re-raises
                // within one task pass if anything is still pending. Keeps a
                // Core-0 stall (SD/USB) from turning one serviced byte into
                // a level-IRQ refire burst.
                if (!c1_rx_peek(u, 0)) c1_lower(u);
                return u->last_rbr;               // stale value on empty read
            }

        case 1:                                   // IER / divisor MSB
            return (u->lcr & 0x80) ? u->dlm : u->ier;

        case 2: {                                 // IIR
            uint8_t v = u->iir_shadow;
            // Derive-override: never claim "RX data available" once the
            // queue is drained -- Core 0's shadow update is loop-pass
            // latent, and a stale 0x04 would send the guest to an empty RBR.
            if ((v & 0x0F) == 0x04 && !c1_rx_peek(u, 0)) v = 0x01;
            // Lower-only side effects (all idempotent; Core 0's invariant
            // re-raises within one pass if a cause is genuinely pending):
            //  0x01: "nothing pending" while asserted would strand an
            //        IIR-only ISR (self-heal).
            //  0x02: reading IIR CLEARS the THRE interrupt on a real 16450;
            //        Core 0 clears tx_int via the iir_reads diff, but the
            //        LINE must drop now or a level-8259 refires for the
            //        whole of a Core-0 stall.
            if (v == 0x01 || (v & 0x0F) == 0x02) c1_lower(u);
            u->iir_reads++;
            u->c1_dbg_iir = v;             // canary: what C1 actually returned
            return v;
        }

        case 3: return u->lcr;                    // LCR
        case 4: return u->mcr;                    // MCR (write-through)

        case 5: {                                 // LSR: DR derived live;
            // Canary: record C1's OWN view of the ring at this instant --
            // compared against Core 0's view in the heartbeat log; a
            // disagreement proves a cross-core visibility fault.
            u->c1_dbg_head  = u->rxq_head;
            u->c1_dbg_tail  = u->rxq_tail;
            u->c1_dbg_entry = u->rxq[u->rxq_tail & (UARTEMU_RXQ_SZ - 1)];
            return 0x60 | (c1_rx_peek(u, 0) ? 0x01 : 0x00); // THRE|TEMT set
        }

        case 6: {                                 // MSR
            uint8_t v;
            uint8_t mcr = u->mcr;
            if (mcr & 0x10) {
                // Loopback: MCR outputs map to MSR inputs. Derived from the
                // write-through MCR shadow, so an OUT MCR / IN MSR pair is
                // exact within one bus cycle (CTMOUSE's UART probe).
                v = ((mcr & 0x02) ? 0x10 : 0)     // RTS  -> CTS
                  | ((mcr & 0x01) ? 0x20 : 0)     // DTR  -> DSR
                  | ((mcr & 0x04) ? 0x40 : 0)     // OUT1 -> RI
                  | ((mcr & 0x08) ? 0x80 : 0);    // OUT2 -> DCD
            } else {
                v = u->msr_static;
            }
            v |= u->msr_delta;
            u->msr_reads++;                       // Core 0 clears the deltas
            // Reading MSR clears the modem-status interrupt (16450). If it
            // was the presented cause, drop the line now (lower-only rule)
            // so a Core-0 stall can't refire it at the guest.
            if ((u->iir_shadow & 0x0F) == 0x00) c1_lower(u);
            return v;
        }

        case 7: return u->scratch;                // scratchpad
    }
    return 0xFF;
}

void UARTEMU_C1_FUNC(uartemu_c1_write)(uartemu_t *u, uint8_t reg, uint8_t val) {
    reg &= 7;
    bool dlab = (u->lcr & 0x80) != 0;

    // Write-through shadows first: guest readback is instantly coherent.
    switch (reg) {
        case 0: if (dlab) { u->dll = val; return; }   break; // THR: queue below
        case 1: if (dlab) { u->dlm = val; return; }
                u->ier = val & 0x0F;                  break;
        case 2: return;                                      // FCR: 16450, ignore
        case 3: u->lcr = val;                         return;
        case 4: u->mcr = val & 0x1F;                  break;
        case 5: return;                                      // LSR: read-only
        case 6:                                       break; // MSR write: C0 clears deltas
        case 7: u->scratch = val;                     return;
    }

    // Publish protocol-relevant writes (THR/IER/MCR/MSR) to Core 0 in order,
    // timestamped AT THE GUEST'S OUT (time_us_32 is one timer-register read,
    // CHRDY-safe) -- edge durations like the mouse's 3 ms power rule must
    // never be measured at drain time, or a Core-0 stall corrupts them.
    uint8_t head = u->iowq_head;
    if ((uint8_t)(head - u->iowq_tail) < UARTEMU_IOWQ_SZ) {
        uint8_t slot = head & (UARTEMU_IOWQ_SZ - 1);
        u->iowq[slot]    = (uint16_t)((reg << 8) | val);
        u->iowq_ts[slot] = time_us_32();
        __dmb();
        u->iowq_head = (uint8_t)(head + 1);
    } else {
        u->iowq_drops++;  // Core 0 stalled >32 guest writes; counted + reconciled
    }
}

// -----------------------------------------------------------------------
// Core-0 side
// -----------------------------------------------------------------------

static inline uint8_t pend_occ(uartemu_t *u) {
    return (uint8_t)(u->pend_head - u->pend_tail);
}

// Effective MSR high nibble for a given MCR in loopback (RTS->CTS, DTR->DSR,
// OUT1->RI, OUT2->DCD) -- also used to generate loopback modem DELTAS, which
// diagnostic tools (CheckIt-style) verify.
static inline uint8_t loopback_msr_hi(uint8_t mcr) {
    return (uint8_t)(((mcr & 0x02) ? 0x10 : 0) | ((mcr & 0x01) ? 0x20 : 0) |
                     ((mcr & 0x04) ? 0x40 : 0) | ((mcr & 0x08) ? 0x80 : 0));
}

// Fold a change of the effective MSR high nibble into the delta bits
// (dCTS/dDSR/dDCD on any edge, TERI on RI falling edge).
static inline void msr_fold_delta(uartemu_t *u, uint8_t old_hi, uint8_t new_hi) {
    uint8_t chg = (uint8_t)(old_hi ^ new_hi);
    if (!chg) return;
    u->msr_delta |= (uint8_t)(((chg & 0x10) ? 0x01 : 0)
                            | ((chg & 0x20) ? 0x02 : 0)
                            | (((chg & 0x40) && !(new_hi & 0x40)) ? 0x04 : 0)
                            | ((chg & 0x80) ? 0x08 : 0));
}

static void c0_handle_iow(uartemu_t *u, uint8_t reg, uint8_t val, uint32_t t_write) {
    switch (reg) {
        case 0:                                    // THR (guest TX)
            if (u->mcr_seen & 0x10) {
                // Loopback echo: the byte comes straight back to the guest
                // and is NOT transmitted to the device (16450 behavior).
                // Bypass inter-byte pacing -- a real loopback echoes within
                // one byte time, and CTMOUSE's UART probe polls LSR for it
                // right after a consuming RBR flush (which would otherwise
                // push the echo a full rx_interval into the future).
                uartemu_queue_bytes(u, &val, 1);
                u->next_byte_t = time_us_64();
                u->pace_rush   = 1;   // consumption in the same pass must not
                                      // re-delay the echo by rx_interval
            } else if (u->ops.tx_byte) {
                u->ops.tx_byte(u->ops.ctx, val);   // guest -> device data
            }
            if (u->ier & 0x02) u->tx_int = 1;      // sent instantly, THRE again
            break;

        case 1:                                    // IER
            // 16450: the THRE interrupt arms when the enable bit RISES while
            // THR is empty (always, here). Edge-triggered -- a guest
            // re-writing IER with bit1 already set must NOT re-fire it
            // (Windows rewrites IER; a level rule would retrigger forever).
            if ((val & 0x02) && !(u->ier_seen & 0x02)) u->tx_int = 1;
            u->ier_seen = val & 0x0F;
            break;

        case 4: {                                  // MCR: device edge logic
            uint8_t prev = u->mcr_seen;
            u->mcr_seen = val;
            if (prev == val) break;
            // Loopback modem deltas: inside loopback the effective MSR high
            // nibble follows the MCR outputs -- transitions generate real
            // delta bits/interrupts on a 16450 (CheckIt-class probes test it).
            uint8_t old_hi = (prev & 0x10) ? loopback_msr_hi(prev) : u->msr_static;
            uint8_t new_hi = (val  & 0x10) ? loopback_msr_hi(val)  : u->msr_static;
            msr_fold_delta(u, old_hi, new_hi);
            if (u->ops.mcr_change) u->ops.mcr_change(u->ops.ctx, prev, val, t_write);
            break;
        }

        case 6:                                    // MSR write clears deltas
            u->msr_delta = 0;
            break;

        default: break;
    }
}

// Detect guest consumption of the live RX byte (consumer index moved past
// the live slot). Called before the pacer AND again before the IRQ decision
// so a read that lands mid-pass can't cause a spurious raise on an empty
// register. head-tail is bounded by the pacer's occupancy check, so the
// int8 window test cannot wrap.
static inline void c0_rx_consumption(uartemu_t *u, uint64_t now) {
    if (u->byte_live &&
        (int8_t)(u->rxq_tail - (uint8_t)(u->live_slot + 1)) >= 0) {
        u->byte_live = 0;
        // Inter-byte pacing -- but never re-delay a rush byte (loopback
        // echo) that set an immediate deadline in this same pass.
        if (!u->pace_rush) u->next_byte_t = now + u->rx_interval_us;
    }
}

void uartemu_task(uartemu_t *u) {
    uint64_t now = time_us_64();

    // 1. Read side effects FIRST, via counter diffs. Ordering matters: these
    //    counters carry no position in the guest's program order, so they
    //    must only ever clear state that existed BEFORE this pass. Running
    //    them after the iowq drain cleared a tx_int/msr_delta that the drain
    //    itself had just set (IIR read -> THR write within one stall window
    //    = TX chain deadlock). Processed first, a stale read can only clear
    //    pre-existing state; a late-processed read costs at most one extra
    //    interrupt, which the derive-override/self-heal absorbs.
    uint32_t r = u->msr_reads;
    if (r != u->msr_reads_seen) { u->msr_reads_seen = r; u->msr_delta = 0; }
    r = u->iir_reads;
    if (r != u->iir_reads_seen) {
        u->iir_reads_seen = r;
        // The guest saw IIR: if TX-empty was the presented interrupt (no RX
        // above it in priority), reading IIR clears it (16450 semantics).
        if (u->tx_int && !(u->byte_live && (u->ier & 0x01))) u->tx_int = 0;
    }

    // 2. Drain the guest-write event queue (ordered protocol side effects),
    //    with the timestamp captured at the guest's OUT.
    while (u->iowq_tail != u->iowq_head) {
        __dmb();
        uint8_t  slot = u->iowq_tail & (UARTEMU_IOWQ_SZ - 1);
        uint16_t e  = u->iowq[slot];
        uint32_t ts = u->iowq_ts[slot];
        u->iowq_tail++;
        c0_handle_iow(u, (uint8_t)(e >> 8), (uint8_t)e, ts);
    }

    // 2b. Overflow reconciliation: if Core 1 dropped writes (stall > 32
    //     OUTs), the stream is no longer complete -- resync the edge
    //     trackers from the live write-through shadows so a missed MCR
    //     power edge or IER enable can't wedge the device forever.
    r = u->iowq_drops;
    if (r != u->iowq_drops_seen) {
        u->iowq_drops_seen = r;
        if (u->mcr_seen != u->mcr)
            c0_handle_iow(u, 4, u->mcr, time_us_32());  // synthetic, drain-time ts
        if (u->ier_seen != u->ier)
            c0_handle_iow(u, 1, u->ier, time_us_32());
    }

    // 3. RX pacing: detect consumption, move ONE pending byte per interval.
    //    The occupancy bound is load-bearing: without it, repeated
    //    flush+push cycles with no guest reads lap the 4-slot ring and a
    //    fresh-epoch byte gets delivered TWICE (and the int8 consumption
    //    test wraps at 129). Core 0 reading the C1-owned tail is SPSC-legal.
    c0_rx_consumption(u, now);
    if (!u->byte_live && pend_occ(u) && (int64_t)(now - u->next_byte_t) >= 0 &&
        (uint8_t)(u->rxq_head - u->rxq_tail) < UARTEMU_RXQ_SZ) {
        uint8_t b = u->pend[u->pend_tail & (UARTEMU_PEND_SZ - 1)];
        u->pend_tail++;
        uint8_t slot = u->rxq_head;
        u->rxq[slot & (UARTEMU_RXQ_SZ - 1)] = (uint16_t)((u->rx_epoch << 8) | b);
        __dmb();
        u->rxq_head  = (uint8_t)(slot + 1);
        u->live_slot = slot;
        u->byte_live = 1;
        u->pace_rush = 0;             // rush (if any) satisfied
    }

    // 4. Interrupt state + the physical line (sole raise owner). Re-check
    //    consumption first: the guest may have drained the byte we just
    //    pushed (or an earlier one) since step 3 -- raising for it now
    //    would fire an empty-register IRQ.
    c0_rx_consumption(u, now);
    // Invariant watchdog: after the consumption re-check, byte_live implies
    // head > tail. live=1 with an empty ring is the wedge observed on
    // hardware 2026-07-15 (pre-occupancy-guard build): Core 0 holds the IRQ
    // for a byte Core 1 can't see -> level-8259 storm. Structurally
    // impossible with the guard; if it EVER fires, heal + count instead of
    // storming, and the counter tells us the impossible happened.
    if (u->byte_live && (uint8_t)(u->rxq_head - u->rxq_tail) == 0) {
        u->byte_live = 0;
        u->heal_live++;
    }
    uint8_t ier = u->ier, mcr = u->mcr;
    bool eff_out2 = (mcr & 0x10) ? false : ((mcr & 0x08) != 0);
    // A pend byte BLOCKED by a stale-full ring also counts as an RX cause:
    // the ring only drains when Core 1 peeks (guest LSR/IIR/RBR reads), and
    // an IRQ-only guest won't read until we interrupt -- without this nudge
    // a burst of unread resets livelocks. The nudge IRQ's own IIR read
    // discards the stale entries (derive-override peek), tail catches up,
    // and the next pass delivers the byte with a real IRQ.
    bool rx_blocked = pend_occ(u) &&
                      (uint8_t)(u->rxq_head - u->rxq_tail) >= UARTEMU_RXQ_SZ;
    bool rx_p = (u->byte_live || rx_blocked) && (ier & 0x01);
    bool tx_p = u->tx_int    && (ier & 0x02);
    bool md_p = u->msr_delta && (ier & 0x08);
    u->iir_shadow = rx_p ? 0x04 : tx_p ? 0x02 : md_p ? 0x00 : 0x01;
    __dmb();                          // IIR visible before the IRQ fires

    bool should = eff_out2 && (rx_p || tx_p || md_p);
    uint32_t cl = u->c1_lowers;
    if (should) {
        // Re-raise if we never raised OR Core 1 lowered behind our back
        // (drain-vs-push race): the invariant heals within one pass.
        if (!u->line_raised || cl != u->c1_lowers_seen) {
            if (u->ops.raise_irq) u->ops.raise_irq(u->ops.ctx);
            u->line_raised = 1;
            u->irq_raises++;
        }
    } else if (u->line_raised) {
        if (u->ops.lower_irq) u->ops.lower_irq(u->ops.ctx);
        u->line_raised = 0;
        u->irq_lowers++;
    }
    u->c1_lowers_seen = cl;
}

// -----------------------------------------------------------------------
// Core-0 device API
// -----------------------------------------------------------------------
bool uartemu_tx_idle(uartemu_t *u) {
    return pend_occ(u) == 0 && !u->byte_live;
}

bool uartemu_queue_bytes(uartemu_t *u, const uint8_t *b, uint8_t n) {
    if ((uint8_t)(UARTEMU_PEND_SZ - pend_occ(u)) < n) return false;
    for (uint8_t i = 0; i < n; i++) {
        u->pend[u->pend_head & (UARTEMU_PEND_SZ - 1)] = b[i];
        u->pend_head++;
    }
    return true;
}

void uartemu_rx_flush(uartemu_t *u) {
    u->rx_epoch++;                    // in-flight byte dies at C1's peek
    // Poison every slot with the just-retired epoch so a u8 epoch WRAP (256
    // flushes) can never resurrect an ancient unconsumed entry. Slot writes
    // are aligned u16 stores (atomic); Core 1 mid-peek sees either the old
    // entry (stale epoch -> discard) or the poison (stale -> discard).
    uint16_t poison = (uint16_t)((uint8_t)(u->rx_epoch - 1) << 8);
    for (int i = 0; i < UARTEMU_RXQ_SZ; i++) u->rxq[i] = poison;
    u->pend_head = u->pend_tail = 0;
    u->byte_live = 0;
    u->next_byte_t = time_us_64();    // next queued byte may go immediately
}

void uartemu_set_modem_lines(uartemu_t *u, int cts, int dsr, int ri, int dcd) {
    uint8_t s = (uint8_t)((cts ? 0x10 : 0) | (dsr ? 0x20 : 0) |
                          (ri  ? 0x40 : 0) | (dcd ? 0x80 : 0));
    uint8_t old = u->msr_static;
    uint8_t chg = (uint8_t)(s ^ old);
    if (chg) {
        u->msr_delta |= (uint8_t)(((chg & 0x10) ? 0x01 : 0)         // dCTS
                                | ((chg & 0x20) ? 0x02 : 0)         // dDSR
                                | (((chg & 0x40) && !(s & 0x40)) ? 0x04 : 0) // TERI
                                | ((chg & 0x80) ? 0x08 : 0));       // dDCD
        u->msr_static = s;
    }
}

void uartemu_set_rx_interval_us(uartemu_t *u, uint32_t us) {
    u->rx_interval_us = us ? us : 1;
}

uint8_t  uartemu_get_mcr(uartemu_t *u)     { return u->mcr_seen; }
uint16_t uartemu_get_divisor(uartemu_t *u) { return (uint16_t)((u->dlm << 8) | u->dll); }
uint8_t  uartemu_get_lcr(uartemu_t *u)     { return u->lcr; }

// -----------------------------------------------------------------------
// debug
// -----------------------------------------------------------------------
void uartemu_debug(uartemu_t *u, struct uartemu_dbg_t *out) {
    if (!out) return;
    out->ier         = u->ier;
    out->lcr         = u->lcr;
    out->mcr         = u->mcr;
    out->iir         = u->iir_shadow;
    out->epoch       = u->rx_epoch;
    out->pend_occ    = pend_occ(u);
    out->byte_live   = u->byte_live;
    out->tx_int      = u->tx_int;
    out->msr_delta   = u->msr_delta;
    out->line_raised = u->line_raised;
    out->iowq_drops  = u->iowq_drops;
    out->rbr_reads   = u->rbr_reads;
    out->iir_reads   = u->iir_reads;
    out->msr_reads   = u->msr_reads;
    out->c1_lowers   = u->c1_lowers;
    out->irq_raises  = u->irq_raises;
    out->irq_lowers  = u->irq_lowers;
    out->heal_live   = u->heal_live;
}

uint32_t uartemu_struct_size(void) { return (uint32_t)sizeof(uartemu_t); }
