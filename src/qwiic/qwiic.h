#pragma once
// *** PicoMEM Library for the QwiiC devices
// 

#define qwiic_i2c i2c1

extern bool qwiic_enabled;
extern bool qwiic_4charLCD_enabled;
extern bool PMRTC_enabled;

extern void pm_qwiic_init(uint32_t i2c_rate);
extern void pm_qwiic_scan_print();                  // Scan all the I2C Port and Display the result
extern void pm_qwiic_scan();                     // Scan/detect only the supported devices
extern void qwiic_display_4char(const char *str);

extern void ht16k33_demo();