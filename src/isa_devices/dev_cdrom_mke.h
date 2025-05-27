#pragma once


extern uint8_t dev_cdr_install();
extern void dev_cdr_remove();
extern bool dev_cdr_installed();
extern void dev_cdr_update();
extern bool dev_cdr_ior(uint32_t Addr,uint8_t *Data);
extern void dev_cdr_iow(uint32_t Addr,uint8_t Data);