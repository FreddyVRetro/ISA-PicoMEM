#pragma once

extern volatile bool dev_cms_playing;   // True if playing
extern volatile uint8_t dev_cms_delay;      // counter for the Nb of second since last I/O

extern uint8_t dev_cms_install(uint16_t baseport);
extern void dev_cms_remove();
extern bool dev_cms_installed();
extern void dev_cms_update();
extern bool dev_cms_ior(uint32_t CTRL_AL8,uint8_t *Data );
extern void dev_cms_iow(uint32_t CTRL_AL8,uint32_t ISAIOW_Data);