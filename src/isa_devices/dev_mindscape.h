#pragma once

extern volatile uint8_t dev_mmb_delay;      // counter for the Nb of second since last I/O

extern uint8_t dev_mmb_install(uint16_t baseport);
extern void dev_mmb_remove();
extern bool dev_mmb_installed();
extern void dev_mmb_update();
extern bool dev_mmb_ior(uint32_t Addr,uint8_t *Data);
extern void dev_mmb_iow(uint32_t Addr,uint8_t Data);