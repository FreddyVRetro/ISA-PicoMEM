#pragma once

extern volatile bool dev_sbdsp_playing;   // True if playing
extern uint8_t dev_sbdsp_delay;  // counter for the Nb of second since last I/O

extern uint8_t dev_sbdsp_install(uint16_t baseport,uint8_t sbirq);
extern void dev_sbdsp_remove();
extern bool dev_sbdsp_installed();
extern void dev_sbdsp_update();
extern bool dev_sbdsp_ior(uint32_t Addr,uint8_t *Data);
extern void dev_sbdsp_iow(uint32_t Addr,uint8_t Data);