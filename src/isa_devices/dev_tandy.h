#pragma once

extern bool dev_tdy_playing;   // True if playing
extern uint8_t dev_tdy_delay;  // counter for the Nb of second since last I/O

extern uint8_t dev_tdy_install(uint16_t baseport);
extern void dev_tdy_remove();
extern bool dev_tdy_installed();
extern void dev_tdy_update();
extern bool dev_tdy_ior(uint32_t CTRL_AL8,uint8_t *Data );
extern void dev_tdy_iow(uint32_t CTRL_AL8,uint8_t Data);