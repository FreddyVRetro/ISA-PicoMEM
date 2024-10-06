/*
 * Copyright (c) 2023 Rumbledethumps
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _DEV_H_
#define _DEV_H_

#include <stdint.h>

#define DEV_DESC_SIZE 80

extern char dev_message[CFG_TUH_DEVICE_MAX][DEV_DESC_SIZE];

extern uint8_t usb_get_devnb();
extern void usb_print_status();
extern void usb_set_status(uint8_t dev_addr, const char *format, ...);

#endif /* _DEV_H_ */
