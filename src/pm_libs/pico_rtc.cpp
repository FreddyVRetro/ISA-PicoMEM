
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"

#include <sys/time.h>
#include <time.h>

#include "pico_rtc.h"

void printdatetime(tm *t)
{
printf(" %d:%d:%d %d/%d/%d",t->tm_hour,t->tm_min,t->tm_sec,t->tm_mon,t->tm_mday,t->tm_year);
}

// Initialize the RTC with a default value
void pico_rtc_init()
{
  struct timeval tv;
  time_t tsec = tv.tv_sec;

  struct tm t = {
            .tm_sec   = 00,
            .tm_min   = 00,
            .tm_hour  = 12,
            .tm_mday  = 22,
            .tm_mon   = 05,
            .tm_year  = 2025,
            .tm_wday  = 4  // 0 is Sunday
             };

  tv.tv_sec = mktime(&t);
  tv.tv_usec = 0; /* microseconds */
  settimeofday(&tv, nullptr);

  PM_INFO("Pico Default Time/Date: ");
  printdatetime(&t);
  PM_INFO("\n");  
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

  printf("DOS Date to pico Date : ");
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