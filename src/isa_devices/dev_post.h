#pragma once

extern hostio_t HostIO;

extern bool dev_post_toinstall;

extern void dev_post_install();
extern void dev_post_setPort2(uint16_t BasePort);
extern void dev_post_remove();
extern void dev_post_update();                      // Update: to execute in the Core0 loop
extern void dev_post_iow(uint32_t Addr,uint8_t Data);