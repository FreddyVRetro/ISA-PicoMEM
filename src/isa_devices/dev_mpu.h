#pragma once

extern volatile uint8_t dev_mpu_delay;      // counter for the Nb of second since last I/O

extern uint8_t dev_mpu_install(uint16_t port);
extern bool dev_mpu_installed();
extern void dev_mpu_remove();
extern void dev_mpu_update();                      // Update: to execute in the Core0 loop
extern bool dev_mpu_ior(uint32_t Addr,uint8_t *Data);
extern void dev_mpu_iow(uint32_t Addr,uint8_t Data);

// MPU specific commands
extern int dev_mpu_get_active_voices(void);
extern uint8_t dev_mpu_define_renderer(uint8_t renderer);

extern void dev_mpu_test();