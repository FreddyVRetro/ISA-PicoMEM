/*
 * Copyright (c) 2023 Rumbledethumps
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _MOU_H_
#define _MOU_H_

#include "tusb.h"

/* Kernel events
 */

void mou_init(void);
void mou_stop(void);

// Set the extended register value.
bool mou_xreg(uint16_t word);

// Process HID keyboard report.
void mou_report(hid_mouse_report_t const *report);

#endif /* _MOU_H_ */
