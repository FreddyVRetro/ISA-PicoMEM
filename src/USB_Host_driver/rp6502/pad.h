/*
 * Copyright (c) 2023 Rumbledethumps
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PAD_H_
#define _PAD_H_

#include "tusb.h"
#include <stdint.h>

/* Kernel events
 */

void pad_init(void);
void pad_stop(void);
void pad_task(void);

// Set the extended register value.
bool pad_xreg(uint16_t word);

// Process HID gamepad report.
void pad_report(uint8_t dev_addr, uint8_t const *report);

#endif /* _PAD_H_ */
