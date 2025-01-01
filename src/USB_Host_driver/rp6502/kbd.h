/*
 * Copyright (c) 2023 Rumbledethumps
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _KBD_H_
#define _KBD_H_

#include "tusb.h"

/* Kernel events
 */

void kbd_init(void);
void kbd_task(void);
void kbd_stop(void);

// Process HID keyboard report.
void kbd_report(uint8_t dev_addr, uint8_t instance, hid_keyboard_report_t const *report);

// Set the extended register value.
bool kbd_xreg(uint16_t word);

// Send LEDs to keyboards in next task.
void kbd_hid_leds_dirty();

#endif /* _KBD_H_ */
