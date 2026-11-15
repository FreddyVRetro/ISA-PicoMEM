/*  PCF85263 Real Time Clock (Sodered on the PicoMEM 1.5)
  
 Freddy VETELE for the PicoMEM

*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "../pm_debug.h"
#include <sys/time.h>
#include <time.h>
#include <ctime>
#include "pm_i2c.h"
#include "../pm_libs/pico_rtc.h"

#define pfc85263_addr 0x51
bool PMRTC_enabled = false;

// The RTC shares i2c1 (qwiic_i2c) with the screenpad OLED, which streams frames
// via DMA on the same bus. Plain blocking I2C here can collide with an in-flight
// OLED DMA. This weak hook lets the screenpad inject its wait_oled_dma_idle()
// serialization; on builds without the screenpad it stays a no-op.
extern "C" __attribute__((weak)) void qwiic_i2c1_wait_idle(void) { }

/* register addresses in the rtc */
#define RTCC_SEC_ADDR        0x01
#define RTCC_MIN_ADDR        0x02
#define RTCC_HR_ADDR         0x03
#define RTCC_DAY_ADDR        0x04
#define RTCC_WEEKDAY_ADDR    0x05
#define RTCC_MONTH_ADDR      0x06
#define RTCC_YEAR_ADDR       0x07

#define RTCC_OSC             0x25
// bit 7 = clock invert, 0 = non-inverting, 1 = inverted
// bit 6 = OFFM, offset calibration mode
// bit 5 = 12/24 hour mode, 1 = 12 hour mode 0 = 24 hour mode
// bit 4 = Low jitter mode, 0 = normal, 1 = reduced CLK output jitter
// bit 3 - 2 = drive control, 00 = normal Rs 100k, 01 = low drive Rs 60k, 10 and 11 = high drive Rs 500k
// bit 1 - 0 = load capacitance, 00 = 7.0pF, 01 = 6.0pF, 10 and 11 = 12.5pF
#define RTCC_BATT            0x26
// bit 4 = enable/disable battery switch feature, 0 = enable, 1 = disable
// bit 3 = battery switch refresh rate, 0 = low, 1 = high
// bit 2 - 1 = battery switch mode, 00 = at Vth level, 01 = at Vbat level, 10 = at higher of Vth or Vbat, 11 = at lower of Vth or Vbat
// bit 0 = battery switch threshold voltage, 0 = 1.5V, 1 = 2.8V
#define RTCC_RAM             0x2C
#define RTCC_STOP            0x2E
// bit 7 -1 not used
// bit 0 = run/stop, 0 = run, 1 = stop
#define RTCC_RESET           0x2E
// software reset = 0x2C, triggers CPR and CTS
// clear prescaler = 0xA4
// clear timestamp = 0x25

#define RTCC_CENTURY_MASK       0x80

uint8_t decTobcd(uint8_t val)
{
  return ( (val/10*16) + (val%10) );
}

uint8_t bcdTodec(uint8_t val)
{
  return ( (val/16*10) + (val%16) );
}

extern "C" bool rtc_i2c_detect()
{
  uint8_t rxdata;
  PMRTC_enabled=false;

  qwiic_i2c1_wait_idle();   // serialize with screenpad OLED DMA (shared i2c1)
  if (i2c_read_blocking(qwiic_i2c, pfc85263_addr, &rxdata, 1, false)==1) PMRTC_enabled=true;

  return PMRTC_enabled;
}

// Set the PicoMEM i2c RTC to the Pico time/date
extern "C" bool rtc_i2c_setdatetime()
{
    uint8_t buf[9];

    if (!PMRTC_enabled) return false;

    struct tm tmv;
    if (!pico_rtc_get_tm(&tmv)) return false;
    struct tm *t = &tmv;

    PM_INFO("[RTC] Write Date/Time: %04d-%02d-%02d %02d:%02d:%02d\n",
         t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

    qwiic_i2c1_wait_idle();   // serialize with screenpad OLED DMA (shared i2c1)

    // 1. Stop the clock so the time registers can be loaded (STOP=1 @ 0x2E).
    buf[0] = RTCC_STOP; buf[1] = 0x01;
    i2c_write_blocking(qwiic_i2c, pfc85263_addr, buf, 2, false);
    // 2. Clear the prescaler (Resets register @ 0x2F, CPR = 0xA4).
    buf[0] = 0x2F; buf[1] = 0xA4;
    i2c_write_blocking(qwiic_i2c, pfc85263_addr, buf, 2, false);
    // 3. Time registers EXPLICITLY 0x00 (100th) .. 0x07 (year) -- no wrap dep.
    buf[0] = 0x00;                        // register pointer: 100th-seconds
    buf[1] = 0x00;                        // 0x00 100th = 0
    buf[2] = decTobcd(t->tm_sec);         // 0x01 second
    buf[3] = decTobcd(t->tm_min);         // 0x02 minute
    buf[4] = decTobcd(t->tm_hour);        // 0x03 hour
    buf[5] = decTobcd(t->tm_mday);        // 0x04 day
    buf[6] = decTobcd(t->tm_wday);        // 0x05 weekday (0=Sun)
    buf[7] = decTobcd(t->tm_mon+1);       // 0x06 month (tm_mon is 0-11 -> 1-12)
    buf[8] = (uint8_t)(t->tm_year-80);    // 0x07 year (tm_year counts from 1900)
    i2c_write_blocking(qwiic_i2c, pfc85263_addr, buf, 9, false);
    PM_INFO("[RTC] Write Registers  0:%02d 1:%02d 2:%02d 3:%02d 4:%02d 5:%02d 6:%02d\n",
          buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8]);
    // 4. Restart the clock (STOP=0).
    buf[0] = RTCC_STOP; buf[1] = 0x00;
    i2c_write_blocking(qwiic_i2c, pfc85263_addr, buf, 2, false);

    return true;
}

// Buffer-style read: PCF85263 register 0x01..0x07 -> out[0..6] in chip-native
// format (BCD for everything except year, which is binary year-1980). Does
// NOT touch the Pico POSIX clock. See WonderMCA/doc/rtc_management.md.
extern "C" bool rtc_i2c_read_buf(uint8_t out[7])
{
    // Lazy probe: on WM10 the qwiic init / rtc_i2c_detect() boot path is gated
    // out (#if !BOARD_WM10), and i2c1 is brought up later by the screenpad.
    // Detect on first use so PMRTC_enabled gets set once the bus is alive.
    if (!PMRTC_enabled) rtc_i2c_detect();
    if (!PMRTC_enabled) return false;

    qwiic_i2c1_wait_idle();   // serialize with screenpad OLED DMA (shared i2c1)
    uint8_t reg = RTCC_SEC_ADDR;
    int w = i2c_write_blocking(qwiic_i2c, pfc85263_addr, &reg, 1, true);
    if (w != 1) return false;

    int r = i2c_read_blocking(qwiic_i2c, pfc85263_addr, out, 7, false);
    if (r != 7) return false;

    // Strip the chip control bits that share the BCD nibbles -- caller wants
    // raw BCD digits, not the masked-out flag bits.
    out[0] &= 0x7f;   // seconds: bit 7 = OSC-stop indicator
    out[1] &= 0x7f;   // minutes: bit 7 = reserved
    out[2] &= 0x3f;   // hours (24h mode): bits 5..0 = tens+units
    out[3] &= 0x3f;   // day
    out[4] &= 0x07;   // weekday
    out[5] &= 0x1f;   // month
    // out[6] (year) is binary, no mask needed.
    return true;
}

// Buffer-style write: stop clock, write registers 0x01..0x07 from in[0..6],
// restart clock. Buffer in same format as rtc_i2c_read_buf. Does NOT touch
// the Pico POSIX clock.
extern "C" bool rtc_i2c_write_buf(const uint8_t in[7])
{
    if (!PMRTC_enabled) rtc_i2c_detect();   // lazy probe (WM10 boot path skips qwiic init)
    if (!PMRTC_enabled) return false;

    qwiic_i2c1_wait_idle();   // serialize with screenpad OLED DMA (shared i2c1)
    uint8_t buf[9];

    // 1. Stop the clock so the time registers can be loaded (STOP=1 @ 0x2E).
    buf[0] = RTCC_STOP; buf[1] = 0x01;
    if (i2c_write_blocking(qwiic_i2c, pfc85263_addr, buf, 2, false) != 2)
        return false;

    // 2. Clear the prescaler so the sub-second counter restarts clean
    //    (Resets register @ 0x2F, CPR = 0xA4).
    buf[0] = 0x2F; buf[1] = 0xA4;
    if (i2c_write_blocking(qwiic_i2c, pfc85263_addr, buf, 2, false) != 2)
        return false;

    // 3. Write the time registers EXPLICITLY from 0x00 (100th) .. 0x07 (year).
    //    Does NOT rely on the chip's register-pointer wrap from 0x2F -> 0x00
    //    (the old single-burst-from-0x2E write NACKed on parts that don't wrap).
    buf[0] = 0x00;     // register pointer: 100th-seconds
    buf[1] = 0x00;     // 0x00 100th  = 0
    buf[2] = in[0];    // 0x01 second  (BCD)
    buf[3] = in[1];    // 0x02 minute  (BCD)
    buf[4] = in[2];    // 0x03 hour    (BCD)
    buf[5] = in[3];    // 0x04 day     (BCD)
    buf[6] = in[4];    // 0x05 weekday (0-6)
    buf[7] = in[5];    // 0x06 month   (BCD)
    buf[8] = in[6];    // 0x07 year    (binary, year-1980)
    if (i2c_write_blocking(qwiic_i2c, pfc85263_addr, buf, 9, false) != 9)
        return false;

    // 4. Restart the clock (STOP=0).
    buf[0] = RTCC_STOP; buf[1] = 0x00;
    if (i2c_write_blocking(qwiic_i2c, pfc85263_addr, buf, 2, false) != 2)
        return false;

    return true;
}

// !! Need error check to not return a corrupted date/time
extern "C" bool rtc_i2c_getdatetime()
{
  uint8_t buf[7];

  if (!PMRTC_enabled) return false;       // no chip detected -> keep our own time

  struct tm tmv = {};
  struct tm *t = &tmv;

  buf[0] = RTCC_SEC_ADDR;         // Read from the Second register
  i2c_write_blocking(qwiic_i2c, pfc85263_addr, &buf[0], 1, false);    // Send the register number to read from
  uint8_t readbytes=i2c_read_blocking (qwiic_i2c, pfc85263_addr, &buf[0], 7, false);

  //PM_INFO("RTC: Read %d bytes/7 \n",readbytes);
  if (readbytes != 7) return false; // RTC Error Don't change the Date/Time

  //PM_INFO("Integrity bit: %d\n",buf[0]&0x80);
  if (buf[0]&0x80)
   {
    PM_INFO("[RTC] Initialized : Use Default Date/Time\n");
    rtc_i2c_setdatetime();
    return true;
   } else PM_INFO("[RTC] Success : Use RTC Read Date/Time\n");

  for (int i=0;i<7;i++) PM_INFO("r%d:%X - ",i,buf[i]);
  PM_INFO("\n");

  t->tm_sec  = bcdTodec(buf[0] & 0x7f);
  t->tm_min  = bcdTodec(buf[1] & 0x7f);
  t->tm_hour = bcdTodec(buf[2] & 0x3f);
  t->tm_mday = bcdTodec(buf[3] & 0x3f);
  t->tm_wday = bcdTodec(buf[4] & 0x07);
  t->tm_mon  = bcdTodec(buf[5] & 0x1f)-1;
  t->tm_year = buf[6] + 80;   // chip holds years since 1980, tm_year from 1900
  t->tm_isdst = -1;

  PM_INFO("[RTC] Read Registers  0:%02d 1:%02d 2:%02d 3:%02d 4:%02d 5:%02d 6:%02d\n",
          buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6]);
  PM_INFO("[RTC] Read Date/Time: %04d-%02d-%02d %02d:%02d:%02d\n",
         t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

  // Update the Pico date/time (the always-on timer FatFs reads)
  pico_rtc_set_tm(t);

 return true;
}
