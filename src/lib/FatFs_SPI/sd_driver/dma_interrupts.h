
#pragma once

#include "pico.h"

#ifdef __cplusplus
extern "C" {
#endif

void dma_irq_add_handler(const uint num, bool exclusive);

#ifdef __cplusplus
}
#endif
