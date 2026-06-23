#pragma once

#include "../dev_audiomix.h"

typedef struct dev_cvx_t {            
    bool     active;          // True if configured
    volatile uint8_t delay;   // counter for the Nb of second since last I/O
    uint16_t baseport;
} dev_cvx_t;

extern dev_cvx_t dev_cvx;

__force_inline void dev_cvx_enable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active | AD_CVX;
}

__force_inline void dev_cvx_disable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_CVX;
}

extern uint8_t dev_cvx_install(uint8_t type, uint16_t baseport);
extern void dev_cvx_remove();
extern bool dev_cvx_installed();
extern void dev_cvx_update();
extern bool dev_cvx_ior(uint32_t Addr,uint8_t *Data);
extern void dev_cvx_iow(uint32_t Addr,uint8_t Data);

void dev_cvx_test();