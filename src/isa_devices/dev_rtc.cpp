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

  Emulates AST/NS MM58167AN: SixPakPlus V1, Clone I/O boards
  Most of the code by Matthew Dovey
*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "../pm_debug.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType
#include "../pm_libs/pico_rtc.h"

#include <sys/time.h>
#include <time.h>
#include <ctime>
#include "hardware/i2c.h"
#include "pfc85263_i2c.h"
#include "dev_rtc.h"

#define USE_RTC_I2C 0

#define RTC_I2C     0x32

#define RTC_PORT    0x2C0
#define RAM_SIZE    0x10

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

dev_rtc_t dev_rtc;

uint8_t ram[RAM_SIZE];
static uint8_t dotw_lut[] = {
  0, 1, 2, 0, 3, 0, 0, 0, 
  4, 0, 0, 0, 0, 0, 0, 0, 
  5, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 
  6, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0
};

uint8_t bcd2dec(uint8_t x) {
  return (x>>4)*10 + (x&0x0f);
}

uint8_t dec2bcd(uint8_t x) {
  return (x/10)*0x10 + x%10;
}

uint8_t read_register(uint8_t reg)
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
            return ram[reg];
    }
}

void write_register(uint8_t reg, uint8_t data)
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
        case REG_NS_MS:
            tv.tv_usec = 100 * bcd2dec(data);
            break;
        case REG_NS_HS:
            tv.tv_usec = 10000 * bcd2dec(data); 
            break; 
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
    ram[reg]= data;
    
    tv.tv_sec = mktime(t);
    tv.tv_usec = 0; /* microseconds */
    settimeofday(&tv, nullptr);
}

uint8_t dev_rtc_install()
{
  if (!dev_rtc.active)
    {
     SetPortType(RTC_PORT,DEV_RTC,RAM_SIZE/0x08);
     dev_rtc.baseport=RTC_PORT;
     dev_rtc.do_update=false;
    }

#if BOARD_PM15
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
  SetPortType(RTC_PORT,DEV_NULL,RAM_SIZE/0x08);
}

// Return true if the Joystick is installed
bool dev_rtc_installed()
{
  return (GetPortType(RTC_PORT)==DEV_RTC);
}

// Started in the Main Command Wait Loop
void dev_rtc_update()
{
#if BOARD_PM15
 rtc_i2c_setdatetime();
#endif
}

bool dev_rtc_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
  *Data = read_register(CTRL_AL8&(RAM_SIZE-1));
  printf("R%X:%X ",CTRL_AL8,*Data);
  return true;
}

void dev_rtc_iow(uint32_t CTRL_AL8,uint32_t ISAIOW_Data)
{
  write_register(CTRL_AL8&(RAM_SIZE-1), ISAIOW_Data&0x0FF);
  dev_rtc.do_update=true;
  printf("W%X:%X ",CTRL_AL8,ISAIOW_Data);
}


