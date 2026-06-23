#pragma once

#include "../dev_audiomix.h"

typedef struct dev_cms_t {            
    bool     active;          // True if configured
    volatile uint8_t delay;   // counter for the Nb of second since last I/O
    uint16_t baseport;
} dev_cms_t;

extern dev_cms_t dev_cms;

__force_inline void dev_cms_enable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active | AD_CMS;
}

__force_inline void dev_cms_disable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_CMS;
}

extern uint8_t dev_cms_install(uint16_t baseport);
extern void dev_cms_remove();
extern bool dev_cms_installed();
extern void dev_cms_update();
extern bool dev_cms_ior(uint32_t Addr,uint8_t *Data);
extern void dev_cms_iow(uint32_t Addr,uint8_t Data);