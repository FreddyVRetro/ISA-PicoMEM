#pragma once

#include "../dev_audiomix.h"

typedef struct dev_sbdsp_t {            
    bool     active;          // True if configured
    volatile uint8_t delay;   // counter for the Nb of second since last I/O
    uint16_t baseport;
} dev_sbdsp_t;

//extern dev_sbdsp_t dev_sbdsp;

extern volatile bool dev_sbdsp_playing;   // True if playing
extern volatile uint8_t dev_sbdsp_delay;  // counter for the Nb of second since last I/O

__force_inline void dev_sbdsp_enable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active | AD_SBDSP;
}

__force_inline void dev_sbdsp_disable_mix() {
  //PM_INFO("Disable SB mixing\n");
  dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_SBDSP;
}

extern uint8_t dev_sbdsp_install(uint16_t baseport);
extern uint8_t dev_sbdsp_set_irq_dma(uint8_t irq,uint8_t dma);
extern void dev_sbdsp_remove();
extern bool dev_sbdsp_installed();
extern void dev_sbdsp_update();
extern bool dev_sbdsp_ior(uint32_t Addr,uint8_t *Data);
extern void dev_sbdsp_iow(uint32_t Addr,uint8_t Data);