#pragma once

extern volatile bool dev_adlib_playing;   // True if playing
extern volatile uint8_t dev_adlib_delay;      // counter for the Nb of second since last I/O

extern uint8_t dev_adlib_install();
extern void dev_adlib_remove();
extern bool dev_adlib_installed();
extern void dev_adlib_update();
extern bool dev_adlib_ior(uint32_t CTRL_AL8,uint8_t *Data );
extern void dev_adlib_iow(uint32_t CTRL_AL8,uint8_t Data);