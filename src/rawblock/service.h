/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PM_BLOCK_SERVICE_H
#define PM_BLOCK_SERVICE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint8_t pm_raw_ior(unsigned port);
void pm_raw_iow(unsigned port, uint8_t data);
void pm_raw_task(void);
#ifdef __cplusplus
}
#endif
#endif
