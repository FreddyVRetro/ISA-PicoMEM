/*
    Microsoft serial mouse -- device layer on top of the generic 8250uart
    core (see 8250uart.h for the split). ALL mouse-specific behavior lives
    here: power derived from DTR+RTS, the >=3 ms power-cycle -> 'M' ID rule,
    motion accumulation, packet formatting and report-rate pacing.

    Originally adapted from PicoGUS (Copyright (c) 2024 Artem Vasilev -
    wbcbz7, MIT license); reworked for WonderMCA 2026-07-15. The
    Logitech/IntelliMouse/MouseSystems variants of the original were dropped
    -- Microsoft 3-byte is what CTMOUSE/Windows use; see git history to
    resurrect them as formatters on this layer.

    Wire format (Microsoft, 1200 baud 7N1):
      byte0  0x40 | LB<<5 | RB<<4 | Y[7:6]>>4 | X[7:6]>>6   (SYNC, bit6=1)
      byte1  X[5:0]
      byte2  Y[5:0]
    'M' (0x4D) is emitted once on power-up (host raises DTR+RTS).

    Core/context: everything here runs on Core 0 (device task + USB feed +
    uartemu callbacks). Nothing is shared with Core 1.
*/

#pragma once

#include <stdint.h>
#include "8250uart.h"
#include "tusb.h"          // hid_mouse_report_t

#ifdef __cplusplus
extern "C" {
#endif

enum {
    SERMOUSE_REPORTRATE_MIN     = 20,
    SERMOUSE_REPORTRATE_MAX     = 200,
    SERMOUSE_REPORTRATE_DEFAULT = 60,
};

// One-time init. uart = the UART instance this mouse sits behind (the mouse
// registers nothing itself -- the dev glue wires uartemu_ops_t and forwards
// the mcr_change callback to sermouse_mcr_change).
void sermouse_init(uartemu_t *uart, uint8_t report_rate_hz, int16_t sensitivity_8p8);

// uartemu mcr_change hook (Core 0, ordered): power/reset edge logic.
// t_write_us = time_us_32() captured at the guest's OUT (edge durations are
// measured between WRITES, immune to Core-0 stalls).
void sermouse_mcr_change(uint8_t prev_mcr, uint8_t new_mcr, uint32_t t_write_us);

// Core-0 pump: rate-paced packet generation. Call every main-loop pass.
void sermouse_task(void);

// POST/reboot quiesce: flush RX, drop modem lines, powered=0 -- releases a
// held IRQ condition across a warm reboot. Safe no-op before init.
void sermouse_quiesce(void);

// USB HID feed (Core 0): accumulate motion/buttons.
void sermouse_process_report(hid_mouse_report_t const *report);

// True while the host-side detection window is open (just after a reset):
// the dev glue drops USB movement so the scan sees only the 'M'.
bool sermouse_in_detect_window(void);

void sermouse_set_report_rate_hz(uint8_t rate_hz);
void sermouse_set_sensitivity(int16_t sensitivity_8p8);

// debug: packets formatted + last packet bytes (stage-3 gate)
void sermouse_debug_pkt(uint32_t *pkt_count, uint8_t out_bytes[3], uint32_t *resets);

#ifdef __cplusplus
}
#endif
