/* Copyright (C) 2023 Freddy VETELE

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
/* pm_disk.cpp : PicoMEM Library containing the HDD and Floppy emulation functions
 */ 

// https://learn.adafruit.com/i2c-addresses

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "../pm_gpiodef.h"     // Various PicoMEM GPIO Definitions

extern void ht16k33_init();
extern void ht16k33_display_string(const char *str);

bool qwiic_enabled = false;
bool quiic_4charLCD_enabled = false;

/* Quick helper function for single byte transfers */
int pm_qwiic_writeb(uint8_t addr, uint8_t val) {
 //  return i2c_write_timeout_us(i2c1, addr, &val, 1, false,100000);
 return i2c_write_blocking(i2c1, addr, &val, 1, false); 
 }

int pm_qwiic_write(uint8_t addr,uint8_t *buf,uint8_t size)
{
 return i2c_write_blocking(i2c1, addr, buf, size, false);
}

int pm_qwiic_readb(uint8_t addr,uint8_t *rxdata)
{
  return i2c_read_timeout_us(i2c1, addr, rxdata, 1, false,100000);
}

void pm_qwiic_scan_print()
{

 int ret;
 uint8_t rxdata;

printf("\nI2C Bus Scan\n");
printf(" 0 1 2 3 4 5 6 7 8 9 A B C D E F\n");

for (int addr = 0; addr < (1 << 7); ++addr) {
if (addr % 16 == 0) {
printf("%02x ", addr);
}

 // Perform a 1-byte dummy read from the probe address. If a slave
 // acknowledges this address, the function returns the number of bytes
 // transferred. If the address byte is ignored, the function returns
 // -1.

 // Skip over any reserved addresses.

 ret = i2c_read_blocking(i2c1, addr, &rxdata, 1, false);

 printf(ret < 0 ? "." : "@");
 printf(addr % 16 == 15 ? "\n" : " ");
 }
 printf("Done.\n");
 
}

// Scan the PicoMEM supported QwiiC devices
// Use TimeOut to ensure it does not Block
void pm_qwiic_scan()
{
  int ret;
  uint8_t rxdata;

   if (i2c_read_timeout_us(i2c1, 0x70, &rxdata, 1, false,10000)==1)
      {
       quiic_4charLCD_enabled=true;
      } 
      else // Try a second time
      {
       sleep_ms(2);
      if (i2c_read_timeout_us(i2c1, 0x70, &rxdata, 1, false,10000)==1)
        {
         quiic_4charLCD_enabled=true;
        }  
      }

if (quiic_4charLCD_enabled) 
   {
    printf("Qwiic : 4 Digit LCD Found \n");
    ht16k33_init();
   }

}

uint8_t pm_qwiic_init(uint32_t i2c_rate)
{
  // 100KHz  : Standard
  // 400KHz  : Fast
  // 1000KHz : Fast plus
  i2c_init(i2c1, i2c_rate);
  gpio_set_function(PIN_QWIIC_SDA, GPIO_FUNC_I2C);    // PicoMEM i2C SDA Pin
  gpio_set_function(PIN_QWIIC_SCL, GPIO_FUNC_I2C);    // PicoMEM i2C SCL Pin
  gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
  gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

  // Make the I2C pins available to picotool
  //bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));
  qwiic_enabled=true;
  return 0;
}

void pm_qwiic_stop()
{

}

void qwiic_display_4char(const char *str)
{
 if (quiic_4charLCD_enabled) ht16k33_display_string(str);
}