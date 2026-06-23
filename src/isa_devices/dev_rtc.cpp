/* Copyright (C) 2024 Freddy VETELE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. 
If not, see <https://www.gnu.org/licenses/>.
*/

/* dev_rtv.cpp : PicoMEM RTC
  Default RTC port is 0x2C0

  Emulates:
   AST/NS MM58167AN: SixPakPlus V1, Clone I/O boards (Most of the code by Matthew Dovey)
   Standard AT RTC/CMOS

  https://wiki.osdev.org/CMOS

  For AT compatible RTC (MC146818A/DS1287):
   Port 70h: Index Register
   Port 71h: Data Register
*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "../pm_debug.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType
#include "../pm_libs/pico_rtc.h"

#include <sys/time.h>
#include <time.h>
#include <ctime>
#include "hardware/i2c.h"
#include "i2c_pfc85263.h"
#include "dev_rtc.h"

#define RTC_MODEL_MM58 1
#define RTC_MODEL_BQ32 2

#define USE_RTC_I2C 0

#define RTC_I2C     0x32

#define RTC_PORT         0x2C0
#define NS_RAM_SIZE      0x10
#define BQ32_RAM_SIZE    128

#define REG_NS_MS        0x00
#define REG_NS_HS        0x01
#define REG_NS_SECONDS   0x02
#define REG_NS_MINUTES   0x03
#define REG_NS_HOURS     0x04
#define REG_NS_DOTW      0x05
#define REG_NS_DAY       0x06
#define REG_NS_MONTH     0x07

#define REG_NS_RAM_MS    0x08 // ms high nibble only, low = 0
#define REG_NS_YEAR      0x0A // offset from 1980 (AST)
#define REG_NS_RAM_WEEK  0x0D // RAM week low nibble only, high = 0

#define REG_BQ32_SEC       0x00
#define REG_BQ32_SEC_ALARM 0x01
#define REG_BQ32_MIN       0x02
#define REG_BQ32_MIN_ALARM 0x03
#define REG_BQ32_HRS       0x04
#define REG_BQ32_HRS_ALARM 0x05
#define REG_BQ32_DOTW      0x06  // Day of the week 1-7
#define REG_BQ32_DOM       0x07  // Day of the month 1-31
#define REG_BQ32_MTH       0x08  // Month 1-12
#define REG_BQ32_YEAR      0x09  // Year 0-99
#define REG_BQ32_REGA      0x0A
#define REG_BQ32_REGB      0x0B
#define REG_BQ32_REGC      0x0C
#define REG_BQ32_REGD      0x0D

uint8_t bq32_reg=0;
bool bq32_binary_mode = true; // false = BCD, true = binary
bool bq32_24hr_mode = true; // true = 24hr, false = 12hr
bool bq32_update_in_progress = false;

dev_rtc_t dev_rtc;

uint8_t bcd2dec(uint8_t x) {
  return (x>>4)*10 + (x&0x0f);
}

uint8_t dec2bcd(uint8_t x) {
  return (x/10)*0x10 + x%10;
}

uint8_t BQ32_read(uint8_t reg)
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    time_t tsec = tv.tv_sec;
    struct tm *t = gmtime(&tsec);
    uint8_t i2c_data;     
//    printdatetime(t);
//    printf("Reg: %x\n",reg);

    switch (reg)
    {
        case REG_BQ32_SEC : if (bq32_binary_mode) return t->tm_sec;
                              else return dec2bcd(t->tm_sec);
        case REG_BQ32_MIN : if (bq32_binary_mode) return t->tm_min;
                              else return dec2bcd(t->tm_min);
        case REG_BQ32_HRS : if (bq32_binary_mode) return t->tm_hour;
                             else return dec2bcd(t->tm_hour);
        case REG_BQ32_DOTW: if  (bq32_binary_mode) return t->tm_wday;
                             else return dec2bcd(t->tm_wday);         
        case REG_BQ32_DOM : if (bq32_binary_mode) return t->tm_mday;
                             else return dec2bcd(t->tm_mday);
        case REG_BQ32_MTH : if (bq32_binary_mode) return t->tm_mon;
                             else return dec2bcd(t->tm_mon);
        case REG_BQ32_YEAR: if (bq32_binary_mode) return t->tm_year%100;
                            else return dec2bcd(t->tm_year%100);
        case REG_BQ32_REGD:
            return 0x80;    // Valid RAM and Time
        default:
            if (reg<128) return dev_rtc.ram[reg];
            else return 0xFF;
    }
}

void BQ32_write(uint8_t reg, uint8_t data)
{
    bool update_time = false;
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    time_t tsec = tv.tv_sec;
    struct tm *t = gmtime(&tsec);
    uint8_t i2c_data[2];

    i2c_data[1] = data;

    if (reg<=REG_NS_YEAR) dev_rtc.do_update=true;  // If Date/Time changed, update the I2C RTC

    switch (reg)
    {
        case REG_BQ32_SEC:
            if (bq32_binary_mode) t->tm_sec = data;
               else t->tm_sec = bcd2dec(data);
            update_time = true;
            break;
        case REG_BQ32_MIN:
            if (bq32_binary_mode) t->tm_min = data;
               else t->tm_min = bcd2dec(data);
            update_time = true;
            break;
        case REG_BQ32_HRS:          
            if (bq32_binary_mode) t->tm_hour = data;
               else t->tm_hour = bcd2dec(data);
            update_time = true;
            break;
        case REG_BQ32_DOTW:
            if (bq32_binary_mode) t->tm_wday = data;
               else t->tm_wday = bcd2dec(data);
            update_time = true;
            break;
        case REG_BQ32_DOM:
            if (bq32_binary_mode) t->tm_mday = data;
               else t->tm_mday = bcd2dec(data);
            update_time = true;
            break;
        case REG_BQ32_MTH:
            if (bq32_binary_mode) t->tm_mon = data;
               else t->tm_mon = bcd2dec(data);
            update_time = true;
            break;
        case REG_BQ32_YEAR:
            if (bq32_binary_mode) t->tm_year = data;
               else t->tm_year = bcd2dec(data); 
            update_time = true;
            break;
        case REG_BQ32_REGB:
            bq32_binary_mode = (data & 0x04) != 0;
            bq32_24hr_mode = (data & 0x02) != 0;
            break;
    }

    if (reg<128) dev_rtc.ram[reg]= data;

    if (update_time) {
       tv.tv_sec = mktime(t);
       tv.tv_usec = 0; /* microseconds */
       settimeofday(&tv, nullptr);
      }
}

uint8_t MM58_read(uint8_t reg)
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    time_t tsec = tv.tv_sec;
    struct tm *t = gmtime(&tsec);
    uint8_t i2c_data;     
//    printdatetime(t);
//    printf("Reg: %x\n",reg);

    switch (reg)
    {
        case REG_NS_MS:
            return ((tv.tv_usec/1000)%10)<<4;   // Low nibble always 0
        case REG_NS_HS:        
            return dec2bcd(tv.tv_usec/10000);
        case REG_NS_SECONDS:
            return dec2bcd(t->tm_sec);
        case REG_NS_MINUTES:
            return dec2bcd(t->tm_min);
        case REG_NS_HOURS:
            return dec2bcd(t->tm_hour);
        case REG_NS_DOTW:
            return dec2bcd(t->tm_wday);         
        case REG_NS_DAY:       
            return dec2bcd(t->tm_mday);
        case REG_NS_MONTH:        
            return dec2bcd(t->tm_mon);
        case REG_NS_YEAR:
            return (t->tm_year < 1980) ? 0 : (t->tm_year - 1980);
        default:
            if (reg<128) return dev_rtc.ram[reg];
              else return 0xFF;
    }
}


void MM58_write(uint8_t reg, uint8_t data)
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    time_t tsec = tv.tv_sec;
    struct tm *t = gmtime(&tsec);
    uint8_t i2c_data[2];

    i2c_data[1] = data;

    if (reg<=REG_NS_YEAR) dev_rtc.do_update=true;  // If Date/Time changed, update the I2C RTC

    switch (reg)
    {
        case REG_NS_SECONDS:
            t->tm_sec = bcd2dec(data);
            break;
        case REG_NS_MINUTES:
            t->tm_min = bcd2dec(data);
            break;
        case REG_NS_HOURS:          
            t->tm_hour = bcd2dec(data);
            break;
        case REG_NS_DOTW:
            t->tm_wday = bcd2dec(data);
            break;
        case REG_NS_DAY:
            t->tm_mday = bcd2dec(data);
            break;        
        case REG_NS_MONTH:
            t->tm_mon = bcd2dec(data);
            break;
        case REG_NS_YEAR:
            t->tm_year = 1980 + data;
            break;
    }

    if (reg==REG_NS_RAM_MS) data=data&0xf0;
    if (reg==REG_NS_RAM_WEEK) data=data&0x0f;
    if (reg<128) dev_rtc.ram[reg]= data;
    
    tv.tv_sec = mktime(t);
    tv.tv_usec = 0; /* microseconds */
    settimeofday(&tv, nullptr);
}

uint8_t dev_rtc_install(uint8_t model)
{
  PM_INFO("Install RTC Device Model %d\n",model);
  if (!dev_rtc.active)
    {
     if (dev_rtc.model==RTC_MODEL_BQ32) 
        {
         PM_INFO("Install BQ32 RTC at port 0x70\n");
         SetPortType(0x70,DEV_RTC,1);
         dev_rtc.baseport=RTC_PORT;
        }
       else {
         PM_INFO("Install MM58167 RTC at port 0x2C0\n");
         SetPortType(RTC_PORT,DEV_RTC,NS_RAM_SIZE/0x08);
        }
     dev_rtc.do_update=false;
     dev_rtc.model=model;
     bq32_update_in_progress = false;
     bq32_binary_mode = true;
     dev_rtc.active=true;
    }

#if ONBOARD_RTC
  if (rtc_i2c_detect()) 
     {
      PM_INFO("PicoMEM RTC found (PCF8563)\n");
      PM_INFO("Get I2C RTC Time\n");
      rtc_i2c_getdatetime();    // Update the time from the PM 1.5+ RTC
      pico_rtc_printdate();
    }
#endif

  return 0;
}

void dev_rtc_remove()
{
 if (dev_rtc.active)
    {
     DelPortType(DEV_RTC);
     dev_rtc.active=false;
    }
}

// Return true if the Joystick is installed
bool dev_rtc_installed()
{
  return (GetPortType(RTC_PORT)==DEV_RTC);
}

// Started in the Main Command Wait Loop
void dev_rtc_update()
{
#if ONBOARD_RTC
 rtc_i2c_setdatetime();
#endif
}

bool dev_rtc_ior(uint32_t CTRL_AL8,uint8_t *Data )
{   
  if (dev_rtc.model==RTC_MODEL_BQ32) {
       if (CTRL_AL8&0x7==1) *Data = BQ32_read(bq32_reg);
         else return false;
       }
     else *Data = MM58_read(CTRL_AL8&(NS_RAM_SIZE-1));
  PM_INFO("rR%X:%X ",CTRL_AL8,*Data);
  return true;
}

void dev_rtc_iow(uint32_t CTRL_AL8,uint32_t ISAIOW_Data)
{
  if (dev_rtc.model==RTC_MODEL_BQ32) {
       if (CTRL_AL8&0x7==0) bq32_reg;
       else if (CTRL_AL8&0x7==1) {
              BQ32_write(bq32_reg, ISAIOW_Data&0x0FF);
              dev_rtc.do_update=true;
             }
       }
     else {    
        MM58_write(CTRL_AL8&(NS_RAM_SIZE-1), ISAIOW_Data&0x0FF);
        dev_rtc.do_update=true;
       }
  PM_INFO("rW%X : %X ",CTRL_AL8,ISAIOW_Data);
}