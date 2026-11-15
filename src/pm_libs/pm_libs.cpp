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
// pm_libs.cpp : Common functions for the PicoMEM Board and tests fonctions

#include <stdio.h>
#include "pico/stdlib.h"
#include <stdarg.h>
#include "hardware/pio.h"     // pio0/pio1, pio_sm_is_claimed
#include "../../pm_debug.h"
#include "pm_gvars.h"
#include "pm_defines.h"
#include "pm_board.h"     // PicoMEM Board Definitions (GPIO)
#include "../isa_devices/dev_joystick.h"  // dev_joystick_install/_ior, joystate_struct

#ifdef PIMORONI_PICO_PLUS2_RP2350
#include "pico_psram.h"   // __psram_start__
#endif

#if PM_WIFI_LED
#include "pico/cyw43_arch.h"    // Needed to blink the LED for Pico W
#endif

//#include "ff.h"

#define DISPLAY_TIME 1

uint8_t pm_led_state;

struct BlinkCode_T
 {
  bool    blinking;
  bool    led_off;
  uint8_t l_cnt;        // Current Blink code
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
  PM_INFO("[BOOT] From powerup: %d ms %d us.\n",timenow/1000,timenow%1000);
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

// input : 0 Off, 1 On
void pm_led(uint8_t onoff)
{
 pm_led_state=onoff;
#if PM_WIFI_LED
PM_INFO("WLED %d ",onoff);
if (BV_WifiInit==INIT_DONE) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, onoff);
#else
//PM_INFO("LED %d ",onoff);
gpio_put(LED_PIN, onoff);
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


void gpio_move_5s (uint8_t pin)
{
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_OUT);
  for (int i=0;i<5;i++) {
    gpio_put(pin, 1);
    sleep_ms(250);
    gpio_put(pin, 0);
    sleep_ms(250);
    gpio_put(pin, 1);
    sleep_ms(250);
    gpio_put(pin, 0);
    sleep_ms(250);
  }
}


//#include "pm_debug.h"
void pm_display_claimed_pio_sm()
{
    if (pio_sm_is_claimed(pio0, 0)) PM_ERROR("pio/sm pio0/0 already claimed\n");
       else PM_INFO("PIO pio0 SM 0 free\n");
    if (pio_sm_is_claimed(pio0, 1)) PM_ERROR("pio/sm pio0/1 already claimed\n");
       else PM_INFO("PIO pio0 SM 1 free\n");
    if (pio_sm_is_claimed(pio0, 2)) PM_ERROR("pio/sm pio0/2 already claimed\n");
       else PM_INFO("PIO pio0 SM 2 free\n");
    if (pio_sm_is_claimed(pio0, 3)) PM_ERROR("pio/sm pio0/3 already claimed\n");
        else PM_INFO("PIO pio0 SM 3 free\n");

    if (pio_sm_is_claimed(pio1, 0)) PM_ERROR("pio/sm pio1/0 already claimed\n");
       else PM_INFO("PIO pio1 SM 0 free\n");
    if (pio_sm_is_claimed(pio1, 1)) PM_ERROR("pio/sm pio1/1 already claimed\n");
       else PM_INFO("PIO pio1 SM 1 free\n");
    if (pio_sm_is_claimed(pio1, 2)) PM_ERROR("pio/sm pio1/2 already claimed\n");
       else PM_INFO("PIO pio1 SM 2 free\n");
    if (pio_sm_is_claimed(pio1, 3)) PM_ERROR("pio/sm pio1/3 already claimed\n");
        else PM_INFO("PIO pio1 SM 3 free\n");              
}

#if defined(RASPBERRYPI_PICO2) || defined(PIMORONI_PICO_PLUS2_RP2350)

 #include "hardware/pll.h"
 #include "hardware/clocks.h"
 #include "hardware/structs/pll.h"
 #include "hardware/structs/clocks.h"

void measure_freqs(void) {
     uint f_pll_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
     uint f_pll_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY);
     uint f_rosc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC);
     uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
     uint f_clk_peri = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI);
     uint f_clk_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB);
     uint f_clk_adc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC);
 #ifdef CLOCKS_FC0_SRC_VALUE_CLK_RTC
     uint f_clk_rtc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_RTC);
 #endif
 
     PM_INFO("pll_sys  = %dkHz\n", f_pll_sys);
     PM_INFO("pll_usb  = %dkHz\n", f_pll_usb);
     PM_INFO("rosc     = %dkHz\n", f_rosc);
     PM_INFO("clk_sys  = %dkHz\n", f_clk_sys);
     PM_INFO("clk_peri = %dkHz\n", f_clk_peri);
     PM_INFO("clk_usb  = %dkHz\n", f_clk_usb);
     PM_INFO("clk_adc  = %dkHz\n", f_clk_adc);
 #ifdef CLOCKS_FC0_SRC_VALUE_CLK_RTC
     PM_INFO("clk_rtc  = %dkHz\n", f_clk_rtc);
 #endif
 
    // Can't measure clk_ref / xosc as it is the ref
}

void p2_psram_test()
{

volatile uint8_t *test;
volatile uint16_t *test16;
volatile uint32_t *test32;

uint16_t result16;
uint32_t result32;

float psram_speed;

//measure_freqs();

 test=(uint8_t *) __psram_start__;
 test16=(uint16_t *) __psram_start__;
 test32=(uint32_t *) __psram_start__;


 PM_INFO("---- 8bit test (4MB):\n>Write Data (4MB) (Low byte of the adress)\n");
 for (uint32_t addr = 0; addr < 4*(1024 * 1024); ++addr) {
     test[addr]= (uint8_t) (addr & 0xFF);
    }

 PM_INFO(">Read Data\n");
 uint32_t ErrNb=0;
 uint32_t psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < 4*(1024 * 1024); ++addr) {
     uint8_t result = test[addr];
     if (static_cast<uint8_t>((addr & 0xFF)) != result) 
         {
          PM_INFO("PSRAM failure at address %x (%x != %x), 2nd read: %x\n", addr, addr & 0xFF, result, test[addr]);
         if (ErrNb++==10) break;
         }
 }
 uint32_t psram_elapsed = time_us_32() - psram_begin;
 psram_speed = 1000000.0 * 4* 1024.0 * 1024 / psram_elapsed;
 PM_INFO("8 bit : PSRAM read 4MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);

 
 PM_INFO("---- 16bit test (4MB):\n>Write Data (Low word of the adress)\n");
 for (uint32_t addr = 0; addr < 2*(1024 * 1024); ++addr) { // 2MB, 16Bit=4MB
     test16[addr]= (uint16_t) (addr & 0xFFFF);
    }

 PM_INFO(">Read Data\n");
 psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < 2*(1024 * 1024); ++addr) {
     result16 = test16[addr];
     if (static_cast<uint16_t>((addr & 0xFFFF)) != result16) 
         {
         PM_INFO("PSRAM failure at address %x (%x != %x), 2nd read: %x\n", addr, addr & 0xFFFF, result16, (uint16_t) test[addr]);
         if (ErrNb++==20) break;
         }
 }
 psram_elapsed = time_us_32() - psram_begin;
 psram_speed = 1000000.0 * 4* 1024.0 * 1024 / psram_elapsed;
 PM_INFO("16 bit : PSRAM read 1MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);

 /*
 PM_INFO("---- 32bit test (4MB):\n>Write Data (0xAAAAAAAA)\n");
 for (uint32_t addr = 0; addr < 1*(1024 * 1024); ++addr) {
     test32[addr]= 0xAAAAAAAA;
    }

 PM_INFO(">Read Data\n");
 uint32_t psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < 1*(1024 * 1024); ++addr) {
     result32 = test32[addr];
     if (result32!=0xAAAAAAAA)
         {
         printf("PSRAM failure at address %x (%x != 0xAAAAAAAA), 2nd read: %x\n", addr, result32, test32[addr]);
         if (ErrNb++==30) break;
         }
 PM_INFO("---- 32bit test End\n");

 PM_INFO("---- 32bit test (4MB):\n>Write Data (0x55555555)\n"); 
  for (uint32_t addr = 0; addr < 4*(1024 * 1024); ++addr) { // 2MB, 16Bit=4MB
       test[addr]= 0x55;
      } 

  PM_INFO(">Read Data\n");
 uint32_t psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < 1*(1024 * 1024); ++addr) {
     result32 = (uint32_t) test[addr];
     if (result32!=0x55555555)
         {
         printf("PSRAM failure at address %x (%x != 0x55555555), 2nd read: %x\n", addr, result32, (uint32_t) test[addr]);
         if (ErrNb++==40) break;
         }
 PM_INFO("---- 32bit test End\n");

 PM_INFO("---- 32bit test (4MB):\n>Write Data (0xFFFFFFFF)\n"); 
  for (uint32_t addr = 0; addr < 4*(1024 * 1024); ++addr) { // 2MB, 16Bit=4MB
       test[addr]= 0xFF;
      } 

 PM_INFO(">Read Data\n");
 uint32_t psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < 1*(1024 * 1024); ++addr) {
     result32 = (uint16_t) test[addr];
     if (result32!=0xFFFFFFFF)
         {
         printf("PSRAM failure at address %x (%x != 0xFFFFFFFF), 2nd read: %x\n", addr, result32, (uint32_t) test[addr]);
         if (ErrNb++==50) break;
         }
 PM_INFO("---- 32bit test End\n");

 PM_INFO("---- 32bit test (4MB):\n>Write Data (0)\n"); 
  for (uint32_t addr = 0; addr < 4*(1024 * 1024); ++addr) { // 2MB, 16Bit=4MB
       test[addr]= 0;
      } 

 PM_INFO(">Read Data\n");
 uint32_t psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < 1*(1024 * 1024); ++addr) {
     result32 = (uint32_t) test[addr];
     if (result32!=0)
         {
         PM_INFO("PSRAM failure at address %x (%x != 0), 2nd read: %x\n", addr, result32, (uint32_t) test[addr]);
         if (ErrNb++==60) break;
         }
 PM_INFO("---- 32bit test End\n");

*/

if (ErrNb==0) {PM_INFO("PSRAM Test OK :) \n");}
   else {PM_INFO("PSRAM Test FAIL :( \n");}

/*
PM_INFO("ps_total_space: %d\n", ps_total_space());
PM_INFO("ps_total_used: %d\n", ps_total_used());
PM_INFO("ps_largest_free_block %d\n",ps_largest_free_block());
*/

/*
test8=(uint8_t*) ps_malloc(1024);
if (test8!=NULL)
{
  PM_INFO("Allocated : Pointer to a 1024 byte table: %p\n",test8);
  PM_INFO("Write/Read 256 bytes:\n",test8);
  for (int i=0;i<256;i++)
  {
    test8[i]=(uint8_t) i;
    PM_INFO("%d,",test8[i]);
  }
} else PM_INFO("! Pointer not allocated\n");

PM_INFO("PSRAM Size :%d \n",ps_size);
PM_INFO("ps_total_space: %d\n", ps_total_space());
PM_INFO("ps_total_used: %d\n", ps_total_used());
PM_INFO("ps_largest_free_block %d\n",ps_largest_free_block());

test8=(uint8_t *) __psram_start__;
for (uint8_t i=0;i<255;i++)
  {
    test8[i*16*1024]=i;
    test8[i*16*1024+1]=i;
    test8[i*16*1024+2]=i;
    test8[i*16*1024+3]=i;
  }
*/
}

#endif

void joy_test()
{
PM_INFO("Test Joystick\n");

dev_joystick_install();
joystate_struct.mode=3;
joystate_struct.pl_total=1;

gpio_set_dir(43,GPIO_OUT);
gpio_set_dir(46,GPIO_OUT);
gpio_set_dir(47,GPIO_OUT);
uint8_t d;
for (;;)
{
//  gpio_put(43,1);  // Test scope pin
  busy_wait_us(10);
//  gpio_put(46,0);  // Test scope pin  
  dev_joystick_ior(0x201,&d);
}  
}
