#pragma once

extern bool detect_picogus(void);
extern bool pm_detectport(uint16_t LoopNb);
extern uint16_t pm_irq_cmd(uint8_t cmd);
extern void pm_diag();