#pragma once

#include "../dev_audiomix.h"

extern uint8_t dev_ps1_delay;  // counter for the Nb of second since last I/O

__force_inline void dev_ps1_enable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active | AD_PS1;
}

__force_inline void dev_ps1_disable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_PS1;
}

extern uint8_t dev_ps1_install(uint16_t baseport);
extern void dev_ps1_remove();
extern bool dev_ps1_installed();
extern void dev_ps1_update();
extern bool dev_ps1_ior(uint32_t Addr,uint8_t *Data);
extern void dev_ps1_iow(uint32_t Addr,uint8_t Data);