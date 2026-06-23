#pragma once

#include "../dev_audiomix.h"

typedef struct dev_adl_t {            
    bool     active;          // True if configured
    volatile uint8_t delay;   // counter for the Nb of second since last I/O
} dev_adl_t;

extern dev_adl_t dev_adlib;

__force_inline void dev_adlib_enable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active | AD_ADLIB;
}

__force_inline void dev_adlib_disable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_ADLIB;
}

extern uint8_t dev_adlib_install();
extern void dev_adlib_remove();
extern bool dev_adlib_installed();
extern void dev_adlib_update();
extern bool dev_adlib_ior(uint32_t Addr,uint8_t *Data);
extern void dev_adlib_iow(uint32_t Addr,uint8_t Data);