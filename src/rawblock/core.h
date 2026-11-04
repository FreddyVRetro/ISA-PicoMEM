/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PM_BLOCK_CORE_H
#define PM_BLOCK_CORE_H
#include <stdint.h>
#include "protocol.h"
struct pmb_media {
 uint32_t generation, token, capacity;
 uint16_t sector_size, vid, pid;
 uint8_t present, fault;
};
/* The backend writes only into this private sector buffer. Never host memory. */
typedef int (*pmb_reader)(uint32_t lba, uint8_t *data);
void pmb_execute(struct pmb_media *, const uint8_t *, uint8_t *, pmb_reader);
void pmb_attach(struct pmb_media *, uint32_t, uint16_t, uint16_t, uint16_t);
void pmb_detach(struct pmb_media *);
#endif
