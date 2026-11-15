/*
    Microsoft serial mouse -- device layer. See sermouse.h.
    All Core 0; no cross-core state.
*/

#include <string.h>
#include "pico/time.h"
#include "sermouse.h"

#define CLAMP(v, lo, hi) (((v) < (lo)) ? (lo) : ((v) > (hi)) ? (hi) : (v))

static struct {
    uartemu_t *uart;

    // motion accumulation (USB feed -> next packet)
    int16_t  acc_x, acc_y;
    uint8_t  buttons, buttons_prev;
    uint8_t  btn_latch;               // presses not yet reported: a click
                                      // shorter than one report interval
                                      // must still appear in >=1 packet

    // config
    int16_t  sensitivity;             // 8.8 fixed point (256 = 1.0)
    uint8_t  rate_hz;
    uint32_t interval_us;

    // power / reset state (from MCR edges, guest-write timestamps)
    uint8_t  powered;                 // DTR+RTS up, not loopback
    uint8_t  pending_off;             // power fell; commit deferred 3 ms --
                                      // a real mouse's caps ride a short dip
                                      // out with NO visible effect at all
    uint32_t t_off_u32;               // time_us_32 of the falling WRITE
    uint64_t suppress_until;          // detection window: drop movement

    // pacing
    uint64_t next_pkt_t;

    // debug
    uint32_t dbg_pkts;
    uint8_t  dbg_last[3];
    uint32_t dbg_resets;
} m;

// Post-reset silence: long enough for the host's detection scan to read the
// 'M' without a movement packet trailing into it, short enough to feel
// instant. A real mouse is similarly quiet while it boots (~14 ms) plus the
// host's own scan time.
#define SERMOUSE_DETECT_WINDOW_US   300000ULL

// A DTR/RTS dip shorter than this is host MCR churn, not a power cycle -- a
// real mouse's capacitors ride it out. Prevents Windows' constant MCR writes
// from spraying 'M' bytes into the movement stream.
#define SERMOUSE_MIN_OFF_US         3000ULL

void sermouse_init(uartemu_t *uart, uint8_t report_rate_hz, int16_t sensitivity_8p8) {
    memset(&m, 0, sizeof(m));
    m.uart        = uart;
    m.sensitivity = sensitivity_8p8 ? sensitivity_8p8 : 256;
    sermouse_set_report_rate_hz(report_rate_hz);
}

void sermouse_set_report_rate_hz(uint8_t rate_hz) {
    // clamp FIRST, then derive -- the pre-rework code divided by the
    // unclamped argument (rate 0 = div-by-zero hard fault).
    m.rate_hz     = (uint8_t)CLAMP(rate_hz, SERMOUSE_REPORTRATE_MIN, SERMOUSE_REPORTRATE_MAX);
    m.interval_us = 1000000u / m.rate_hz;
}

void sermouse_set_sensitivity(int16_t sensitivity_8p8) {
    if (sensitivity_8p8) m.sensitivity = sensitivity_8p8;
}

// -----------------------------------------------------------------------
// power / reset (uartemu mcr_change hook, Core 0, ordered)
//
// The capacitor model, done symmetrically (2026-07-15 review findings): a
// DTR/RTS dip is not committed as a power-off until it has LASTED 3 ms.
//   fall edge  -> only arm pending_off (nothing guest-visible happens)
//   fast rise  -> cancel: no flush, no modem-line glitch, no deltas, no 'M'
//   3 ms pass  -> commit (from sermouse_task): powered=0, flush, lines down
//   later rise -> full reset: flush + 'M' + lines up + detect window
// Durations compare guest-WRITE timestamps (time_us_32 at the OUT), so a
// Core-0 stall between the writes cannot fake or hide a power cycle.
// -----------------------------------------------------------------------
static void sermouse_reset(void) {
    uint64_t now = time_us_64();
    m.acc_x = m.acc_y = 0;
    m.btn_latch = 0;
    m.buttons_prev = m.buttons;
    uartemu_rx_flush(m.uart);
    uartemu_set_modem_lines(m.uart, 1, 1, 0, 1);      // CTS/DSR/DCD up
    static const uint8_t id = 'M';
    uartemu_queue_bytes(m.uart, &id, 1);
    m.suppress_until = now + SERMOUSE_DETECT_WINDOW_US;
    m.next_pkt_t     = now + SERMOUSE_DETECT_WINDOW_US;
    m.dbg_resets++;
}

static void sermouse_commit_off(void) {
    m.pending_off = 0;
    m.powered     = 0;
    uartemu_rx_flush(m.uart);                          // its buffers die
    uartemu_set_modem_lines(m.uart, 0, 0, 0, 0);       // lines fall
}

// POST/reboot quiesce (called from the config-apply CMD, i.e. every boot):
// drop everything left from the previous OS session so a stale RX byte or an
// armed IER cannot hold IRQ3 across a warm reboot (no CHRESET on the card).
// The mouse re-announces normally on the next driver DTR power cycle.
void sermouse_quiesce(void) {
    if (!m.uart) return;                               // not initialized yet
    m.acc_x = m.acc_y = 0;
    m.btn_latch = 0;
    m.buttons_prev = m.buttons;
    sermouse_commit_off();
}

void sermouse_mcr_change(uint8_t prev_mcr, uint8_t new_mcr, uint32_t t_write_us) {
    bool pw_prev = ((prev_mcr & 0x03) == 0x03) && !(prev_mcr & 0x10);
    bool pw_now  = ((new_mcr  & 0x03) == 0x03) && !(new_mcr  & 0x10);

    if (pw_prev && !pw_now) {
        // Power fell: arm the deferred commit only. Guest-visible state is
        // untouched -- if the dip is short, it never happened.
        m.pending_off = 1;
        m.t_off_u32   = t_write_us;
    } else if (!pw_prev && pw_now) {
        if (m.pending_off) {
            m.pending_off = 0;
            if ((uint32_t)(t_write_us - m.t_off_u32) >= SERMOUSE_MIN_OFF_US) {
                // dip was a real power cycle (measured write-to-write)
                m.powered = 1;
                sermouse_reset();
            }
            // else: ridden out -- nothing happened, keep running
        } else if (!m.powered) {
            // rise after a committed off (or first power-on ever)
            m.powered = 1;
            sermouse_reset();
        }
        m.powered = 1;
    }
}

// Called from sermouse_task: commit a pending power-off once the dip has
// genuinely lasted 3 ms with no rise in between.
static void sermouse_poll_power(void) {
    if (m.pending_off &&
        (uint32_t)(time_us_32() - m.t_off_u32) >= SERMOUSE_MIN_OFF_US) {
        sermouse_commit_off();
    }
}

bool sermouse_in_detect_window(void) {
    return (int64_t)(time_us_64() - m.suppress_until) < 0;
}

// -----------------------------------------------------------------------
// USB feed (Core 0)
// -----------------------------------------------------------------------
void sermouse_process_report(hid_mouse_report_t const *report) {
    if (!m.powered) return;
    // Accumulate with saturation so a stall can't wind up a huge jump.
    m.acc_x = (int16_t)CLAMP((int32_t)m.acc_x + report->x, -2048, 2047);
    m.acc_y = (int16_t)CLAMP((int32_t)m.acc_y + report->y, -2048, 2047);
    // Latch presses: a press+release inside one report interval would
    // otherwise collapse to "no change" and the click would vanish. The
    // latch keeps the press visible for at least one packet; the following
    // packet then carries the release (a click = two packets, as on wire).
    m.btn_latch |= (uint8_t)(report->buttons & ~m.buttons);
    m.buttons = report->buttons;
}

// -----------------------------------------------------------------------
// packet generation (Core 0 pump)
// -----------------------------------------------------------------------
void sermouse_task(void) {
    if (!m.uart) return;
    sermouse_poll_power();                                 // deferred off-commit
    if (!m.powered) return;

    uint64_t now = time_us_64();
    if ((int64_t)(now - m.next_pkt_t) < 0) return;         // rate limit
    if (!uartemu_tx_idle(m.uart)) return;                  // prior bytes in flight

    uint8_t rpt_btn = (uint8_t)(m.buttons | m.btn_latch);  // press-latch (>=1 pkt)
    if (m.acc_x == 0 && m.acc_y == 0 && rpt_btn == m.buttons_prev)
        return;                                            // nothing to report

    int16_t x = (int16_t)CLAMP(((int32_t)m.acc_x * m.sensitivity) >> 8, -127, 127);
    int16_t y = (int16_t)CLAMP(((int32_t)m.acc_y * m.sensitivity) >> 8, -127, 127);
    m.acc_x = m.acc_y = 0;

    uint8_t p[3];
    p[0] = (uint8_t)(0x40
         | ((rpt_btn & MOUSE_BUTTON_LEFT)  ? 0x20 : 0)
         | ((rpt_btn & MOUSE_BUTTON_RIGHT) ? 0x10 : 0)
         | ((((uint8_t)y) & 0xC0) >> 4)
         | ((((uint8_t)x) & 0xC0) >> 6));
    p[1] = ((uint8_t)x) & 0x3F;
    p[2] = ((uint8_t)y) & 0x3F;

    if (uartemu_queue_bytes(m.uart, p, 3)) {
        m.buttons_prev = rpt_btn;
        m.btn_latch    = 0;          // reported once; release follows naturally
        m.next_pkt_t   = now + m.interval_us;
        m.dbg_pkts++;
        m.dbg_last[0] = p[0]; m.dbg_last[1] = p[1]; m.dbg_last[2] = p[2];
    }
    // queue full: retry next pass (uartemu_tx_idle above makes this rare)
}

// -----------------------------------------------------------------------
// debug
// -----------------------------------------------------------------------
void sermouse_debug_pkt(uint32_t *pkt_count, uint8_t out_bytes[3], uint32_t *resets) {
    if (pkt_count) *pkt_count = m.dbg_pkts;
    if (out_bytes) { out_bytes[0] = m.dbg_last[0]; out_bytes[1] = m.dbg_last[1]; out_bytes[2] = m.dbg_last[2]; }
    if (resets) *resets = m.dbg_resets;
}
