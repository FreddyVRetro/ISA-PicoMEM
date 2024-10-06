/* Copyright (C) 2023 Freddy VETELE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
// pm_libs.cpp : Common functions for the PicoMEM Board

#include <stdio.h>
#include "pico/stdlib.h"
#include <stdarg.h>
#include "..\..\pm_debug.h"

#if PM_PICO_W
#include "pico/cyw43_arch.h"
#endif

//#include "dosbox/bios_disk.h"
//#include "ff.h"

#define DISPLAY_TIME 1
//constexpr uint LED_PIN = PICO_DEFAULT_LED_PIN;
#define LED_PIN 25

struct BlinkCode_T
 {
  bool    blinking;
  bool    led_off;
  uint8_t l_cnt;
  uint8_t s_cnt;
  uint8_t loop_cnt;
  uint8_t n_l_total;    // Next Blink code
  uint8_t n_s_total;
  uint8_t n_loop_total;
 };

 BlinkCode_T LEDBlink;

 uint32_t prevtime;

/*
void pm_printf(const char *pcFormat, ...) {
    char pcBuffer[256] = {0};
    va_list xArgs;
    va_start(xArgs, pcFormat);
    vsnprintf(pcBuffer, sizeof(pcBuffer), pcFormat, xArgs);
    va_end(xArgs);
    printf("%s", pcBuffer);
    fflush(stdout);
}
*/

void pm_timefrominit()
{
  #ifdef DISPLAY_TIME
  uint32_t timenow=time_us_32();
  PM_INFO("From powerup: %d ms %d us.\n",timenow/1000,timenow%1000);
  prevtime=time_us_32();
  #endif
}

void pm_timefromlast()
{
  #ifdef DISPLAY_TIME  
  uint32_t timenow=time_us_32();
  timenow=timenow-prevtime;
  PM_INFO("Delta time: %d ms %d us.\n",timenow/1000,timenow%1000);
  #endif  
}

void pm_led(uint8_t onoff)
{
#if PM_PICO_W
cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, onoff);
#else
gpio_put(LED_PIN, 1);
#endif
}

// Blink xx times to show error number

void err_blink(uint8_t nblong, uint8_t nbshort,uint8_t nbrepeat) {


if (!LEDBlink.blinking)
{
 LEDBlink.blinking=true;
 pm_led(0);

 sleep_ms(1000);

 pm_led(1);
 sleep_ms(250);
 pm_led(0);
 sleep_ms(250);

 pm_led(1);  
 LEDBlink.blinking=false;
}

}

