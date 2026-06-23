
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
PM_INFO(" %d:%d:%d %d/%d/%d",t->tm_hour,t->tm_min,t->tm_sec,t->tm_mon,t->tm_mday,t->tm_year);
}

// Initialize the RTC with a default value (Compile Date/Time)
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
        .tm_year  = year - 1900,
        .tm_wday  = 6  // 0 is Sunday
    };

    aon_timer_start_calendar(&t);
    PM_INFO("Pico Default Time/Date: ");
    PM_INFO(" %d:%02d:%02d %d/%d/%d\n",t.tm_hour,t.tm_min,t.tm_sec,t.tm_mday,t.tm_mon+1,t.tm_year+1900);  
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
    // Start on Friday 5th of June 2020 15:45:00
  struct timeval tv;
  time_t tsec = tv.tv_sec;
  struct tm t={};

  t.tm_year  = (ddate>>25) + 1980;
  t.tm_mon   = ddate>>21 & 0xF;
  t.tm_mday  = ddate>>16 & 0x1F;
  t.tm_hour  = ddate>>10 & 0x1F;  
  t.tm_min   = ddate>>5  & 0x3F;
  t.tm_sec   = (ddate & 0x1F) << 1; /* DOS stores seconds divided by two */
  t.tm_isdst = -1;

  PM_INFO("DOS Date to pico Date : ");
  printdatetime(&t);

  tv.tv_sec = mktime(&t);
  tv.tv_usec = 0; /* microseconds */
  settimeofday(&tv, nullptr);

  pico_rtc_printdate();
}

uint32_t pico_rtc_getDOSDate()
{
    // Start on Friday 5th of June 2020 15:45:00
  uint32_t res;

  struct timeval tv;
  gettimeofday(&tv, nullptr);
  time_t tsec = tv.tv_sec;
  struct tm *t = gmtime(&tsec);

  res = t->tm_year - 1980; /* tm_year is years from 1900, while FAT needs years from 1980 */
  res <<= 4;
  res |= t->tm_mon;
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

void pico_rtc_printdate()
{
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  time_t tsec = tv.tv_sec;
  struct tm *t = gmtime(&tsec);

 PM_INFO("Pico Time/Date: ");
 printdatetime(t);
 PM_INFO("\n");
}