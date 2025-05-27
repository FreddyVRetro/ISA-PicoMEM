#pragma once

extern bool dev_null_ior(uint32_t CTRL_AL8,uint8_t *Data);
extern void dev_null_iow(uint32_t CTRL_AL8,uint8_t Data);

bool (*dev_ior[12])(uint32_t Addr, uint8_t *Data);
void (*dev_iow[12])(uint32_t Addr, uint8_t Data);
