
#include <stdio.h>
#include "string.h"
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "pico/aon_timer.h"
#include <sys/time.h>
#include <time.h>

#include "pico_rtc.h"

void printdatetime(tm *t)
{
PM_INFO(" %d:%02d:%02d %d/%d/%d\n",t->tm_hour,t->tm_min,t->tm_sec,t->tm_mday,t->tm_mon+1,t->tm_year+1900);
}

/* settimeofday()/gettimeofday() come from newlib and are not wired to the
   RP2350 always-on timer, so a time set through them is invisible to
   get_fattime() in FatFs. Drive the always-on timer directly instead: it is
   the same clock FatFs reads, which keeps file timestamps in sync. This is
   the single clock of the card -- the DOS RTC emulation (dev_rtc.cpp) and the
   battery-backed PCF8563 (i2c_pfc85263.cpp) go through it too. */
bool pico_rtc_set_tm(struct tm *t)
{
  struct timespec ts = {};
  ts.tv_sec = mktime(t);
  ts.tv_nsec = 0;
  if (aon_timer_is_running()) return aon_timer_set_time(&ts);
  return aon_timer_start(&ts);
}

bool pico_rtc_get_tm(struct tm *out)
{
  struct timespec ts;
  if (!aon_timer_is_running()) return false;
  aon_timer_get_time(&ts);
  time_t tsec = ts.tv_sec;
  gmtime_r(&tsec, out);
  return true;
}

// Microseconds elapsed in the current second, from the same always-on timer.
// Used by the MM58167 sub-second registers. Returns 0 if the clock is stopped.
uint32_t pico_rtc_get_subsec_us()
{
  struct timespec ts;
  if (!aon_timer_is_running()) return 0;
  aon_timer_get_time(&ts);
  return (uint32_t)(ts.tv_nsec / 1000);
}

void pico_rtc_printdate()
{
  struct tm t;

 PM_INFO("Pico Time/Date: ");
 if (!pico_rtc_get_tm(&t)) { PM_INFO("<clock not running>\n"); return; }
 printdatetime(&t);
}

/* Seed the clock from the build date. The battery-backed PCF8563 is NOT read
   here: pico_rtc_init() runs before the Qwiic I2C bus is initialised, and
   touching I2C at this point hangs the card (and the ISA bus with it). The
   chip is read later by dev_rtc_install(), which runs after the bus is up and
   pushes a valid battery time into the always-on timer. */
void pico_rtc_init()
{
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char s_month[5];
    int month, day, year, hour, min, sec;
    sscanf(__DATE__, "%s %d %d", s_month, &day, &year);
    month = (strstr(month_names, s_month)-month_names)/3;
    sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);
    struct tm t = {
        .tm_sec   = sec,
        .tm_min   = min,
        .tm_hour  = hour,
        .tm_mday  = day,
        .tm_mon   = month,
        .tm_year  = year - 1900,  // struct tm counts years from 1900
        .tm_wday  = 6  // 0 is Sunday
    };

    pico_rtc_set_tm(&t);

    PM_INFO("Pico Default Time/Date: ");
    PM_INFO(" %d:%02d:%02d %d/%d/%d\n",t.tm_hour,t.tm_min,t.tm_sec,t.tm_mday,t.tm_mon+1,t.tm_year+1900); 

#ifdef PM_PRINTF
    printdatetime(&t);
#endif
}

/*
               24                16                 8                 0
+-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
|Y|Y|Y|Y|Y|Y|Y|M| |M|M|M|D|D|D|D|D| |h|h|h|h|h|m|m|m| |m|m|m|s|s|s|s|s|
+-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
 \___________/\________/\_________/ \________/\____________/\_________/
    year        month       day        hour       minute      seconds */
void pico_rtc_setDOSDate(uint32_t ddate)
{
  struct tm t={};

  t.tm_year  = (ddate>>25) + 80;   // DOS counts from 1980, struct tm from 1900
  t.tm_mon   = (ddate>>21 & 0xF) - 1;
  t.tm_mday  = ddate>>16 & 0x1F;
  t.tm_hour  = ddate>>11 & 0x1F;
  t.tm_min   = ddate>>5  & 0x3F;
  t.tm_sec   = (ddate & 0x1F) << 1; /* DOS stores seconds divided by two */
  t.tm_isdst = -1;

  PM_INFO("Set DOS Date to pico Date : ");
  printdatetime(&t);

  pico_rtc_set_tm(&t);

  pico_rtc_printdate();
}

uint32_t pico_rtc_getDOSDate()
{
  uint32_t res;
  struct tm tmv;

  if (!pico_rtc_get_tm(&tmv)) return 0;
  struct tm *t = &tmv;

  res = t->tm_year - 80; /* tm_year is years from 1900, while FAT needs years from 1980 */
  res <<= 4;
  res |= t->tm_mon+1;
  res <<= 5;
  res |= t->tm_mday;
  res <<= 5;
  res |= t->tm_hour;
  res <<= 6;
  res |= t->tm_min;
  res <<= 5;
  res |= (t->tm_sec >> 1); /* DOS stores seconds divided by two */

 return (res);
}
