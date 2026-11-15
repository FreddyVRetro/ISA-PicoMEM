#pragma once

#include <stdint.h>
#include "pm_defines.h"

extern bool dev_null_ior(uint32_t CTRL_AL8,uint8_t *Data);
extern void dev_null_iow(uint32_t CTRL_AL8,uint8_t Data);

extern bool (*dev_ior[DEV_TOTAL])(uint32_t Addr, uint8_t *Data);
extern void (*dev_iow[DEV_TOTAL])(uint32_t Addr, uint8_t Data);
