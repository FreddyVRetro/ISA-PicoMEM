#pragma once
// *** PicoMEM Library for the QwiiC devices
// 

extern bool qwiic_enabled;
extern bool quiic_4charLCD_enabled;

extern uint8_t pm_qwiic_init(uint32_t i2c_rate);
extern void pm_qwiic_scan_print();                  // Scan all the I2C Port and Display the result
extern uint8_t pm_qwiic_scan();                     // Scan/detect only the supported devices
extern void qwiic_display_4char(const char *str);

extern void ht16k33_demo();