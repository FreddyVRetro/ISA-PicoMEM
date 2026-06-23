#pragma once

#include "dev_audiomix.h"

typedef struct dev_gus_t {            
    bool     active;          // True if configured
    volatile uint8_t delay;   // counter for the Nb of second since last I/O
    uint16_t baseport;
} dev_gus_t;

extern dev_gus_t dev_gus;

__force_inline void dev_gus_enable_mix() {
  //printf("G On");
  BV_Int13hCLI=BV_SPIPSRAM|1;                   // Block interrupt during Int13h
  dev_audiomix.dev_active = dev_audiomix.dev_active | AD_GUS;
}

__force_inline void dev_gus_disable_mix() {
  //printf("G Off");
  BV_Int13hCLI=BV_SPIPSRAM|0;                   // Block interrupt during Int13h
  dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_GUS;
}

extern uint8_t dev_gus_set_irq_dma(uint8_t irq,uint8_t dma);
extern uint8_t dev_gus_install(uint16_t baseport);
extern void dev_gus_remove();
extern bool dev_gus_installed();
extern void dev_gus_update();
extern bool dev_gus_ior(uint32_t Addr,uint8_t *Data);
extern void dev_gus_iow(uint32_t Addr,uint8_t Data);