#pragma once

typedef struct dev_mmb_t {            
    bool     active;          // True if configured
    volatile uint8_t delay;   // counter for the Nb of second since last I/O
    uint16_t baseport;
} dev_mmb_t;

extern dev_mmb_t dev_mmb;

__force_inline void dev_mmb_enable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active | AD_MMB;
}

__force_inline void dev_mmb_disable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_MMB;
}

extern uint8_t dev_mmb_install(uint16_t baseport);
extern void dev_mmb_remove();
extern bool dev_mmb_installed();
extern void dev_mmb_update();
extern bool dev_mmb_ior(uint32_t Addr,uint8_t *Data);
extern void dev_mmb_iow(uint32_t Addr,uint8_t Data);