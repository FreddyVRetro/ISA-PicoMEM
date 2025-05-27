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
#include "qwiic.h"

#define pfc85263_addr 0x51
bool PMRTC_enabled = false;

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

bool rtc_i2c_detect()
{
  uint8_t rxdata;
  PMRTC_enabled=false;

  if (i2c_read_blocking(qwiic_i2c, pfc85263_addr, &rxdata, 1, false)==1) PMRTC_enabled=true;

  return PMRTC_enabled;
}

// Set the PicoMEM i2c RTC to the Pico time/date
bool rtc_i2c_setdatetime()
{
    uint8_t thisyear;
    uint8_t buf[11];

    if (!PMRTC_enabled) return false;

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    time_t tsec = tv.tv_sec;
    struct tm *t = gmtime(&tsec);

    thisyear=t->tm_year;
   // if ((thisyear<1980)||(thisyear>2179)) return false;

    buf[0] = RTCC_STOP;             // Write from the Stop register
    buf[1] = 0x01;                  // Write 1 to the STOP register (Stop Clock)
    buf[2] = 0xA4;                  // write A4 to the Prescale register
    buf[3] = 0x00;                  // 100th of seconds (Register index back to 0)
    buf[4] = decTobcd(t->tm_sec);
    buf[5] = decTobcd(t->tm_min);
    buf[6] = decTobcd(t->tm_hour);
    buf[7] = decTobcd(t->tm_mday);
    buf[8] = decTobcd(t->tm_wday);
    buf[9] = decTobcd(t->tm_mon);
    /* If 2000 set the century bit. */
    buf[10]= thisyear-1980;

//    for (int i=4;i<11;i++) printf("r%d :%X - ",i,buf[i]);
//    printf("\n");

    i2c_write_blocking(i2c1, pfc85263_addr, &buf[0], 11, false);

    buf[0] = RTCC_STOP;         // Write from the Stop register
    buf[1] = 0x00;              // Write 0 to the STOP register (Start the Clock)
    i2c_write_blocking(i2c1, pfc85263_addr, &buf[0], 2, false);

    return true;
}

// !! Need error check to not return a corrupted date/time
bool rtc_i2c_getdatetime()
{
  uint8_t buf[7];

  if (!PMRTC_enabled) return false;

  struct timeval tv;
  gettimeofday(&tv, nullptr);
  time_t tsec = tv.tv_sec;
  struct tm *t = gmtime(&tsec);

  buf[0] = RTCC_SEC_ADDR;         // Read from the Second register
  i2c_write_blocking(i2c1, pfc85263_addr, &buf[0], 1, false);    // Send the register number to read from
  uint8_t readbytes=i2c_read_blocking (i2c1, pfc85263_addr, &buf[0], 7, false);

//  PM_INFO("RTC: Read %d bytes/7 \n",readbytes);
  if (readbytes != 7) return false; // RTC Error Don't change the Date/Time

 // PM_INFO("Integrity bit: %d\n",buf[0]&0x80);
  if (buf[0]&0x80)
   {
    PM_INFO("RTC was intitalized : Don't change the Date/Time\n");
    rtc_i2c_setdatetime();
    return true;
   }

//  for (int i=0;i<7;i++) PM_INFO("r%d:%X - ",i,buf[i]);
//  PM_INFO("\n");
  
  t->tm_sec  = bcdTodec(buf[0] & 0x7f);
  t->tm_min  = bcdTodec(buf[1] & 0x7f);
  t->tm_hour = bcdTodec(buf[2] & 0x3f);
  t->tm_mday = bcdTodec(buf[3] & 0x3f);
  t->tm_wday = bcdTodec(buf[4] & 0x07);
  t->tm_mon  = bcdTodec(buf[5] & 0x1f);
  t->tm_year = buf[6] + 1980;
  t->tm_isdst = -1;

  tv.tv_sec = mktime(t);
  tv.tv_usec = 0; /* microseconds */
  settimeofday(&tv, nullptr);

 return true;
}
