#pragma once
// *** PicoMEM Library for the QwiiC devices
// 

#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define qwiic_i2c i2c1

extern bool qwiic_enabled;
extern bool qwiic_4charLCD_enabled;
extern bool PMRTC_enabled;

void pm_qwiic_init(uint32_t i2c_rate);
void pm_i2c0_init(uint32_t i2c_rate);
void pm_qwiic_scan_print(i2c_inst_t *i2cnb);                  // Scan all the I2C Port and Display the result
void pm_i2c0_scan_print();
bool pico_i2c_ping_device(i2c_inst_t *i2cnb, uint8_t addr);
void pm_qwiic_scan();                     // Scan/detect only the supported devices
void qwiic_display_4char(const char *str);

extern void ht16k33_demo();

#ifdef __cplusplus
}  // extern "C"
#endif