#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bool rtc_i2c_detect();
// One-time chip configuration (load capacitance + battery switch-over).
// Set-once, NOT part of the per-read/write runtime path; run automatically
// by rtc_i2c_detect() the first time the chip answers. Safe to call again
// (idempotent -- guarded so it only writes once).
extern bool rtc_i2c_init();
extern bool rtc_i2c_setdatetime();
extern bool rtc_i2c_getdatetime();

// Buffer-style accessors that do NOT touch the Pico POSIX clock (no
// gettimeofday/settimeofday side effects). Used by the PMBIOS RTC menu
// page via CMD_RTC_Get / CMD_RTC_Set. Buffer layout matches the chip:
//   [0]=sec(BCD) [1]=min(BCD) [2]=hour(BCD,24h) [3]=day(BCD)
//   [4]=weekday(0=Sun) [5]=month(BCD) [6]=year-1980(binary)
// See WonderMCA/doc/rtc_management.md for the wire protocol.
// Return false on I2C failure (chip not detected, NAK, short read).
extern bool rtc_i2c_read_buf(uint8_t out[7]);
extern bool rtc_i2c_write_buf(const uint8_t in[7]);

#ifdef __cplusplus
}
#endif
