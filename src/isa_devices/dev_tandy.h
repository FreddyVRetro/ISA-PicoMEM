#pragma once

#include "../dev_audiomix.h"

#define TDY_DELAY 4  // Time on in s after the last access

typedef struct dev_tdy_t {            
    bool     active;          // True if configured
    volatile uint8_t delay;   // counter for the Nb of second since last I/O
    uint16_t baseport;
} dev_tdy_t;

extern dev_tdy_t dev_tdy;

extern uint8_t dev_tdy_delay;  // counter for the Nb of second since last I/O

__force_inline void dev_tdy_enable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active | AD_TDY;
}

__force_inline void dev_tdy_disable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_TDY;
}

extern uint8_t dev_tdy_install(uint16_t baseport);
extern void dev_tdy_remove();
extern bool dev_tdy_installed();
extern void dev_tdy_update();
extern bool dev_tdy_ior(uint32_t Addr,uint8_t *Data);
extern void dev_tdy_iow(uint32_t Addr,uint8_t Data);