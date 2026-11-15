// dev_sermouse.cpp -- WonderMCA serial-mouse device glue (COM2 0x2F8/IRQ3).
//
// Wires the generic 8250uart core (8250uart.h) + the Microsoft mouse device
// layer (sermouse.h) into the WonderMCA world:
//   - Core-1 bus dispatch entries (dev_sermouse_ior/iow -- called from the
//     bus handler inside the CHRDY-stretched cycle; lock-free by design)
//   - the MCA IRQ line hooks (mca_irq_raise/lower on s_irq)
//   - the Core-0 pump (dev_sermouse_task from the pm_cmd hot loop)
//   - the USB HID feed (dev_sermouse_feed)
//   - bring-up instrumentation (throttled Core-0 logging only)
//
// See dev_sermouse.h and WonderMCA/doc/DEVLOG.md 2026-07-15 (architecture).

#include "dev_sermouse.h"
#include "8250uart.h"
#include "sermouse.h"

#include "pico/time.h"
#include <stdbool.h>

// Bring-up instrumentation (throttled ~2 s heartbeat + init canaries).
// Flip to 1 when hunting serial-mouse issues: the [REG]/[IOW] lines
// fingerprint any guest driver (see DEVLOG 2026-07-16, Win3.11 cursor).
// The cheap Core-1 counters stay compiled either way; only the console
// output goes quiet.
#define SERMOUSE_DEBUG 0
#if SERMOUSE_DEBUG
#include "../../pm_debug.h"       // PM_INFO
#include "../../pm_gvars.h"       // PM_DP_RAM -- BIOS-side debug bytes
#include "../../pm_defines.h"     // BV_Dbg_Win / BV_Dbg_MInj / ... macros
#endif

// WonderMCA IRQ raise/lower (mca_irq.c, C linkage). Only present on the
// WonderMCA build; other boards get no-op IRQ (serial mouse is a WM10 feature).
#if BOARD_WM10
extern "C" void mca_irq_raise(uint8_t irqnb, bool force);
extern "C" void mca_irq_lower(uint8_t irqnb);
#include "wm_10.h"                // PIN_IRQ3 -- pad-level sampler (stage-6 gate)
#include "hardware/gpio.h"
#endif

// ------------------------------------------------------------------
// state
// ------------------------------------------------------------------
static uartemu_t s_uart;             // the COM2 UART instance
static uint8_t   s_irq    = 3;       // COM2 -> IRQ3
static uint16_t  s_iobase = 0x2F8;
static bool      s_inited = false;

// ------------------------------------------------------------------
// Instrumentation (Core-1 writes cheap counters; Core-0 logs throttled).
// NO logging in ior/iow -- that would stall the bus handler.
// ------------------------------------------------------------------
static volatile uint32_t s_rd = 0, s_wr = 0, s_rbr_rd = 0, s_feed = 0, s_feed_drop = 0;
// Per-register read profile -- fingerprints WHICH driver's ISR is consuming:
// an LSR-polling loop (CTMOUSE-style) vs an IIR-loop (mouse.drv-style) vs
// MSR readers. Indexed by reg 0..7.
static volatile uint32_t s_reg_rd[8];
// Full write trace: every guest IOW as (reg<<8)|val, in order, first 48.
// wr totals ~34 in the Win3.11 sessions, so this captures the COMPLETE
// init sequence -- divisor, LCR word length, IER order -- which identifies
// the client (MS mouse.drv = 1200 baud 7N1) vs a generic 8N1 comm open.
static volatile uint16_t s_wr_trace[48];
static volatile uint8_t  s_wr_trace_n = 0;
static volatile uint8_t  s_rbr_hist[12];
static volatile uint8_t  s_rbr_pos = 0;
// Post-RTS-raise detection-scan capture: the first 8 bytes the host reads
// after a reset -- 'M' (4D) expected as byte 0. Armed from Core 1 (instr only).
static volatile uint8_t  s_prev_mcr = 0;
static volatile uint8_t  s_detwin[8];
static volatile uint8_t  s_detwin_pos = 0;
static volatile bool     s_detwin_on = false;
static volatile uint8_t  s_mcr_hist[8];
static volatile uint8_t  s_mcr_pos = 0;

#if BOARD_WM10
// PIN_IRQ3 pad-level sampler (stage-6 physical gate) -- see DEVLOG. Sampled
// every task pass; catches any driver of the pad, C code or not.
static volatile uint32_t s_pin_edges = 0;
static uint8_t           s_pin_prev  = 0;
#endif

// ------------------------------------------------------------------
// IRQ hooks for the UART core. raise/lower: Core 0. c1_lower: Core 1,
// single atomic SIO write (mca_irq_lower), idempotent-safe.
// ------------------------------------------------------------------
static void hook_raise(void *ctx) {
    (void)ctx;
#if BOARD_WM10
    mca_irq_raise(s_irq, false);
#endif
}
static void hook_lower(void *ctx) {
    (void)ctx;
#if BOARD_WM10
    mca_irq_lower(s_irq);
#endif
}
static void hook_mcr_change(void *ctx, uint8_t prev, uint8_t cur, uint32_t t_write_us) {
    (void)ctx;
    sermouse_mcr_change(prev, cur, t_write_us);
}

// ------------------------------------------------------------------
// Device interface
// ------------------------------------------------------------------

// POST/reboot quiesce: the card has no CHRESET input, so state left by the
// previous OS session (armed IER + queued RX byte) would hold IRQ3 asserted
// straight through POST. Flush the mouse/UART state and force the line low.
void dev_sermouse_quiesce(void) {
    if (!s_inited) return;
    sermouse_quiesce();
    hook_lower(0);
}

void dev_sermouse_init(uint16_t iobase, uint8_t irqnb) {
    s_iobase = iobase;
    s_irq    = irqnb;
    uartemu_ops_t ops = {};
    ops.raise_irq    = hook_raise;
    ops.lower_irq    = hook_lower;
    ops.c1_lower_irq = hook_lower;    // same op; safe from either core
    ops.mcr_change   = hook_mcr_change;
    ops.tx_byte      = 0;             // a mouse has no host->device data
    ops.ctx          = 0;
    uartemu_init(&s_uart, iobase, &ops);
    sermouse_init(&s_uart, SERMOUSE_REPORTRATE_DEFAULT, 256);
    s_inited = true;
#if SERMOUSE_DEBUG
    PM_INFO("[SERMOUSE] init iobase=0x%03X irq=%u (Microsoft, %u Hz, lock-free core, bld " __DATE__ " " __TIME__ ")\n",
            (unsigned)iobase, (unsigned)irqnb, (unsigned)SERMOUSE_REPORTRATE_DEFAULT);
    // ABI-skew canary: both TUs must agree on the struct layout. A mismatch
    // = stale object file = "two cores see different structs" bench class.
    PM_INFO("[SERMOUSE] canary: &s_uart=%p sizeof(dev)=%u sizeof(uart.o)=%lu %s\n",
            (void *)&s_uart, (unsigned)sizeof(uartemu_t),
            (unsigned long)uartemu_struct_size(),
            (sizeof(uartemu_t) == uartemu_struct_size()) ? "OK" : "<<< SKEW!");
    // Core-0 path self-test: queue -> pace -> read back through the SAME
    // C1 entry points, all on Core 0. PASS here + failure from the guest
    // isolates the fault to Core-1 execution/visibility specifically.
    {
        uint8_t t = 0xA5;
        uartemu_queue_bytes(&s_uart, &t, 1);
        uartemu_task(&s_uart);
        uint8_t lsr = uartemu_c1_read(&s_uart, 5);
        uint8_t rbr = uartemu_c1_read(&s_uart, 0);
        PM_INFO("[SERMOUSE] C0 self-test: lsr=%02X rbr=%02X (%s)\n",
                lsr, rbr, (lsr == 0x61 && rbr == 0xA5) ? "PASS" : "<<< FAIL");
        uartemu_rx_flush(&s_uart);
        uartemu_task(&s_uart);
    }
#endif
}

// Core-1 bus entries. addr_lo = low address bits of the decoded 8-port
// block; the UART register is its low 3 bits. Hot path: lock-free reads +
// cheap counters only.
void __not_in_flash_func(dev_sermouse_ior)(uint8_t addr_lo, uint8_t *data) {
    uint8_t reg = addr_lo & 7;
    uint8_t v = uartemu_c1_read(&s_uart, reg);
    *data = v;
    s_rd++;
    s_reg_rd[reg]++;
    if (reg == 0) {                  // RBR byte
        s_rbr_rd++;
        s_rbr_hist[s_rbr_pos] = v;   // explicit wrap: u8 ++ then %12 scrambles
        s_rbr_pos = (uint8_t)((s_rbr_pos + 1) % 12);   // ordering after 256 reads
        if (s_detwin_on) {           // post-reset detection-scan capture
            s_detwin[s_detwin_pos++] = v;
            if (s_detwin_pos >= 8) s_detwin_on = false;
        }
    }
}

void __not_in_flash_func(dev_sermouse_iow)(uint8_t addr_lo, uint8_t data) {
    uint8_t reg = addr_lo & 7;
    uartemu_c1_write(&s_uart, reg, data);
    s_wr++;
    if (s_wr_trace_n < 48) s_wr_trace[s_wr_trace_n++] = (uint16_t)((reg << 8) | data);
    if (reg == 4) {                  // MCR trace + detwin arm (instr only --
        s_mcr_hist[s_mcr_pos++ & 7] = data;   // protocol logic is on Core 0)
        if ((data & 0x02) && !(s_prev_mcr & 0x02) && !(data & 0x10)) {
            s_detwin_pos = 0;
            s_detwin_on  = true;
        }
        s_prev_mcr = data;
    }
}

// Core-0 pump + throttled instrumentation.
void dev_sermouse_task(void) {
    if (!s_inited) return;
    uartemu_task(&s_uart);   // drain guest writes, pace RX, drive the IRQ line
    sermouse_task();         // rate-paced packet generation

#if BOARD_WM10
    // Pad-level sample of PIN_IRQ3 (input buffer reads the real pad state
    // even in output mode -- but NOT the bus line behind the open-drain
    // 74LVC06; use SERTEST's 8259 IRR readback for that).
    {
        uint8_t lvl = (uint8_t)gpio_get(PIN_IRQ3);
        if (lvl && !s_pin_prev) s_pin_edges++;
        s_pin_prev = lvl;
    }
#endif

#if SERMOUSE_DEBUG
    static uint64_t s_next_log = 0;
    uint64_t now = time_us_64();
    if ((int64_t)(now - s_next_log) >= 0) {
        struct uartemu_dbg_t d;
        uartemu_debug(&s_uart, &d);
        uint32_t pkts = 0, resets = 0;
        uint8_t  pkb[3] = {0, 0, 0};
        sermouse_debug_pkt(&pkts, pkb, &resets);
        if (s_rd || s_wr) {
            PM_INFO("[SERMOUSE] rd=%lu wr=%lu rbr=%lu | mcr=%02X ier=%02X lcr=%02X iir=%02X ep=%u pend=%u live=%u tx=%u md=%02X | irqR=%lu irqL=%lu c1L=%lu line=%u drops=%lu | bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                    (unsigned long)s_rd, (unsigned long)s_wr, (unsigned long)s_rbr_rd,
                    d.mcr, d.ier, d.lcr, d.iir, d.epoch, d.pend_occ, d.byte_live,
                    d.tx_int, d.msr_delta,
                    (unsigned long)d.irq_raises, (unsigned long)d.irq_lowers,
                    (unsigned long)d.c1_lowers, d.line_raised,
                    (unsigned long)d.iowq_drops,
                    s_rbr_hist[0], s_rbr_hist[1], s_rbr_hist[2], s_rbr_hist[3],
                    s_rbr_hist[4], s_rbr_hist[5], s_rbr_hist[6], s_rbr_hist[7],
                    s_rbr_hist[8], s_rbr_hist[9], s_rbr_hist[10], s_rbr_hist[11]);
            PM_INFO("[SERMOUSE] feed=%lu drop=%lu pkts=%lu resets=%lu fmt: %02X %02X %02X | mcrhist: %02X %02X %02X %02X %02X %02X %02X %02X | detwin: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                    (unsigned long)s_feed, (unsigned long)s_feed_drop,
                    (unsigned long)pkts, (unsigned long)resets,
                    pkb[0], pkb[1], pkb[2],
                    s_mcr_hist[0], s_mcr_hist[1], s_mcr_hist[2], s_mcr_hist[3],
                    s_mcr_hist[4], s_mcr_hist[5], s_mcr_hist[6], s_mcr_hist[7],
                    s_detwin[0], s_detwin[1], s_detwin[2], s_detwin[3],
                    s_detwin[4], s_detwin[5], s_detwin[6], s_detwin[7]);
            // Cross-core view comparison: Core 0's ring state vs what Core 1
            // recorded at its last LSR read. ANY disagreement (h/t/entry) is
            // the smoking gun for the 2026-07-15 "two views" contradiction.
            PM_INFO("[SERMOUSE][RXQ] C0: h=%u t=%u ls=%u live=%u ep=%u s=%04X %04X %04X %04X | C1saw: h=%u t=%u e=%04X iir=%02X\n",
                    (unsigned)s_uart.rxq_head, (unsigned)s_uart.rxq_tail,
                    (unsigned)s_uart.live_slot, (unsigned)s_uart.byte_live,
                    (unsigned)s_uart.rx_epoch,
                    (unsigned)s_uart.rxq[0], (unsigned)s_uart.rxq[1],
                    (unsigned)s_uart.rxq[2], (unsigned)s_uart.rxq[3],
                    (unsigned)s_uart.c1_dbg_head, (unsigned)s_uart.c1_dbg_tail,
                    (unsigned)s_uart.c1_dbg_entry, (unsigned)s_uart.c1_dbg_iir);
            // BIOS-side mouse routing (DPRAM debug bytes, pm_vars.asm 0xF46+):
            // win = WIN_LOADED snapshot (1 => PMBIOS routes mouse to 8042 aux
            // inject, NOT INT 33h), 1c = INT 1C tick count (u8 wrap -- must be
            // MOVING), minj = aux packets injected, 2f = INT 2F hook hits,
            // rdy = PS/2 mailbox pending flag. Answers "did Windows get
            // detected and which path is live" without a guest-side probe.
            PM_INFO("[SERMOUSE][WIN] win=%u 1c=%u minj=%u 2f=%u rdy=%u\n",
                    (unsigned)BV_Dbg_Win, (unsigned)BV_Dbg_Int1C,
                    (unsigned)BV_Dbg_MInj, (unsigned)BV_Dbg_2FCnt,
                    (unsigned)BV_USBMouse_Rdy);
            // ISR fingerprint: which registers does the consumer read?
            // rbr/iir/lsr/msr -- an LSR-poll loop vs an IIR loop vs MSR
            // line-state checks identifies the driver behind the reads.
            PM_INFO("[SERMOUSE][REG] rbr=%lu ier=%lu iir=%lu lcr=%lu mcr=%lu lsr=%lu msr=%lu scr=%lu div=%u\n",
                    (unsigned long)s_reg_rd[0], (unsigned long)s_reg_rd[1],
                    (unsigned long)s_reg_rd[2], (unsigned long)s_reg_rd[3],
                    (unsigned long)s_reg_rd[4], (unsigned long)s_reg_rd[5],
                    (unsigned long)s_reg_rd[6], (unsigned long)s_reg_rd[7],
                    (unsigned)uartemu_get_divisor(&s_uart));
            // Complete guest write history (reg:val) -- the init sequence.
            {
                char wbuf[48 * 5 + 4];
                int  wp = 0, wn = s_wr_trace_n;
                for (int i = 0; i < wn && wp < (int)sizeof(wbuf) - 6; i++)
                    wp += snprintf(wbuf + wp, sizeof(wbuf) - wp, " %X:%02X",
                                   s_wr_trace[i] >> 8, s_wr_trace[i] & 0xFF);
                PM_INFO("[SERMOUSE][IOW]%s\n", wbuf);
            }
#if BOARD_WM10
            PM_INFO("[SERMOUSE][PIN40] in=%u out=%u oe=%u fn=%u edges=%lu heal=%lu bld=" __TIME__ "\n",
                    (unsigned)gpio_get(PIN_IRQ3),
                    (unsigned)gpio_get_out_level(PIN_IRQ3),
                    (unsigned)gpio_is_dir_out(PIN_IRQ3),
                    (unsigned)gpio_get_function(PIN_IRQ3),   // expect 5 (SIO)
                    (unsigned long)s_pin_edges,
                    (unsigned long)d.heal_live);             // expect 0 (watchdog)
#endif
        }
        s_next_log = now + 2000000;    // ~2 s
    }
#endif
}

void dev_sermouse_feed(const void *hid_report) {
    if (!s_inited) return;
    // Drop movement during the post-reset detection window so the host's
    // scan sees only the 'M' (a real mouse is quiet while it boots).
    if (sermouse_in_detect_window()) { s_feed_drop++; return; }
    sermouse_process_report((hid_mouse_report_t const *)hid_report);
    s_feed++;
}
