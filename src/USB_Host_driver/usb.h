/*
 * Copyright (c) 2023 Rumbledethumps
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _DEV_H_
#define _DEV_H_

#include <stdint.h>
#include <stdbool.h>

#define DEV_DESC_SIZE 80

#ifdef __cplusplus
extern "C" {
#endif

extern char dev_message[CFG_TUH_DEVICE_MAX][DEV_DESC_SIZE];

extern uint8_t usb_get_devnb();
extern void usb_print_status();
extern void usb_set_status(uint8_t dev_addr, const char *format, ...);

// True when a USB MSC device is enumerated AND its FatFs volume "1:"
// has been mounted by tuh_msc_mount_cb in fatfs_usb.c. Used by the
// floppy/HDD image lister to know whether to expose the [USB] virtual
// root entry alongside [SD].
extern bool usb_msc_volume_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* _DEV_H_ */
