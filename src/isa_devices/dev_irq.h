#pragma once

extern void dev_irq_install();
extern void dev_irq_remove();
extern void dev_irq_update();                      // Update: to execute in the Core0 loop
extern void dev_irq_iow(uint32_t Addr,uint8_t Data);