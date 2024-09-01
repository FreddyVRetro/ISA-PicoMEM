#pragma once

extern hostio_t HostIO;

extern void dev_post_install(uint16_t BasePort);
extern void dev_post_remove(uint16_t BasePort);
extern void dev_post_update();                      // Update: to execute in the Core0 loop
extern void dev_post_iow(uint32_t CTRL_AL8,uint32_t ISAIOW_Data);