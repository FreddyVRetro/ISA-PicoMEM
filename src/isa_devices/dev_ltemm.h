#pragma once

extern bool dev_ems_install(uint16_t port, uint32_t base_addr);

bool dev_ems_ior(uint32_t CTRL_AL8,uint8_t *Data );
extern void dev_ems_iow(uint32_t CTRL_AL8,uint8_t Data);
