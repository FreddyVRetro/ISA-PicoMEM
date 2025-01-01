#pragma once

extern volatile bool dev_cvx_playing;   // True if playing
extern volatile uint8_t dev_cvx_delay;  // counter for the Nb of second since last I/O

extern uint8_t dev_cvx_install(uint16_t baseport);
extern void dev_cvx_remove();
extern bool dev_cvx_installed();
extern void dev_cvx_update();
extern bool dev_cvx_ior(uint32_t CTRL_AL8,uint8_t *Data );
extern void dev_cvx_iow(uint32_t CTRL_AL8,uint8_t Data);