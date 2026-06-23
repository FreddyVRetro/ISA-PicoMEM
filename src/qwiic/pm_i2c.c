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

// on the PicoMEM 1.5+, the QwiiC I2C interface has also the RTC
// 0x70: ht16k33 - 4 Digit LED 
// 0x51: Real time Clock

#include <stdio.h>
#include "pico/stdlib.h"
#include "pm_i2c.h"
#include "hardware/i2c.h"
#include "pm_board.h"       // PicoMEM Board Definitions (GPIO)
#include "../pm_debug.h"

extern void ht16k33_init();
extern void ht16k33_display_string(const char *str);

bool qwiic_enabled = false;
bool qwiic_4charLCD_enabled = false;

/* Quick helper function for single byte transfers */
int pm_qwiic_writeb(uint8_t addr, uint8_t val) {
 //  return i2c_write_timeout_us(qwiic_i2c, addr, &val, 1, false,100000);
 return i2c_write_blocking(qwiic_i2c, addr, &val, 1, false); 
 }

int pm_qwiic_write(uint8_t addr,uint8_t *buf,uint8_t size)
{
 return i2c_write_blocking(qwiic_i2c, addr, buf, size, false);
}

int pm_qwiic_readb(uint8_t addr,uint8_t *rxdata)
{
  return i2c_read_timeout_us(qwiic_i2c, addr, rxdata, 1, false,100000);
}

#define NT3H_I2C_WRITE_ADDR  0xAA
#define NT3H_I2C_READ_ADDR   0xAB
#define NTAG_MEM_BLOCK_SESSION_REGS 0xFE

/*

static bool NTAG_ReadRegister(uint8_t reg, uint8_t *val)
{
    uint8_t tx[2];
    uint8_t val;
    tx[0] = NTAG_MEM_BLOCK_SESSION_REGS;
    tx[1] = reg;


    // send block number
    i2c_write_blocking(i2c0, NT3H_I2C_WRITE_ADDR, tx, 2, false);
    // ntag->i2cbus->write(ntag->waddr, (const char*)ntag->tx_buffer, 2);

    // receive bytes
    i2c_read_timeout_us(i2c0, NT3H_I2C_READ_ADDR, val, 1, false, 100000);
    //ntag->i2cbus->read(ntag->raddr, (char*)ntag->rx_buffer, 1);

   // *val = ntag->rx_buffer[RX_START + 0];
    return true;
}
*/


void pm_i2c0_scan_print()
{

 int ret;
 uint8_t rxdata;

PM_INFO("\nI2C Bus Scan (i2c0)\n");
PM_INFO("   0 1 2 3 4 5 6 7 8 9 A B C D E F\n");

for (int addr = 0; addr < (1 << 7); ++addr) {
if (addr % 16 == 0) {
PM_INFO("%02x ", addr);
}
 // Perform a 1-byte dummy read from the probe address. If a slave
 // acknowledges this address, the function returns the number of bytes
 // transferred. If the address byte is ignored, the function returns -1.

 ret = i2c_read_blocking(i2c0, addr, &rxdata, 1, false);

 PM_INFO(ret < 0 ? "." : "@");
 PM_INFO(addr % 16 == 15 ? "\n" : " ");
 }
 PM_INFO("Done.\n");
 
}

void pm_qwiic_scan_print(i2c_inst_t *i2cnb)
{

 int ret;
 uint8_t rxdata;

PM_INFO("\nI2C Bus Scan (i2c1)\n");
PM_INFO("   0 1 2 3 4 5 6 7 8 9 A B C D E F\n");

for (int addr = 0; addr < (1 << 7); ++addr) {
if (addr % 16 == 0) {
PM_INFO("%02x ", addr);
}
 // Perform a 1-byte dummy read from the probe address. If a slave
 // acknowledges this address, the function returns the number of bytes
 // transferred. If the address byte is ignored, the function returns -1.

 ret = i2c_read_blocking(i2cnb, addr, &rxdata, 1, false);

 PM_INFO(ret < 0 ? "." : "@");
 PM_INFO(addr % 16 == 15 ? "\n" : " ");
 }
 PM_INFO("Done.\n");
 
}

bool pico_i2c_ping_device(i2c_inst_t *i2cnb, uint8_t addr)
{
 bool ret=false;
 uint8_t rxdata;
 
   if (i2c_read_timeout_us(i2cnb, addr, &rxdata, 1, false,10000)==1) ret=true;
      else // Try a second time
      {
       sleep_ms(1);
       if (i2c_read_timeout_us(i2cnb, addr, &rxdata, 1, false,10000)==1) ret=true;
      }
 PM_INFO("Ping device at address %02x : %s\n", addr, (ret == false ? "No response" : "Device found"));
 return ret;
}

// Scan the PicoMEM supported QwiiC devices
// Use TimeOut to ensure it does not Block
void pm_qwiic_scan()
{
  int ret;
  uint8_t rxdata;

  // Detect the ht13k33 LCD Display controller
   if (i2c_read_timeout_us(qwiic_i2c, 0x70, &rxdata, 1, false,10000)==1)
      {
       qwiic_4charLCD_enabled=true;
       ht16k33_init();
      } 
      else // Try a second time
      {
       sleep_ms(2);
      if (i2c_read_timeout_us(qwiic_i2c, 0x70, &rxdata, 1, false,10000)==1)
        {
         qwiic_4charLCD_enabled=true;
         ht16k33_init();
        }  
      }
}

void pm_qwiic_init(uint32_t i2c_rate)
{
  // 100KHz  : Standard
  // 400KHz  : Fast
  // 1000KHz : Fast plus
  i2c_init(qwiic_i2c, i2c_rate);
  gpio_set_function(PIN_I2C1_SDA, GPIO_FUNC_I2C);    // PicoMEM i2C SDA Pin
  gpio_set_function(PIN_I2C1_SCL, GPIO_FUNC_I2C);    // PicoMEM i2C SCL Pin
  gpio_pull_up(PIN_I2C1_SDA);
  gpio_pull_up(PIN_I2C1_SCL);
  PM_INFO("I2C1 initialized at %d KHz, SDA %d, SCL %d\n", i2c_rate/1000, PIN_I2C1_SDA, PIN_I2C1_SCL);
  qwiic_enabled=true;
}

void pm_i2c0_init(uint32_t i2c_rate)
{
  #if PIN_I2C0_SDA
  // 100KHz  : Standard
  // 400KHz  : Fast
  // 1000KHz : Fast plus
  i2c_init(i2c0, i2c_rate);
  gpio_set_function(PIN_I2C0_SDA, GPIO_FUNC_I2C);    // PicoMEM i2C SDA Pin
  gpio_set_function(PIN_I2C0_SCL, GPIO_FUNC_I2C);    // PicoMEM i2C SCL Pin
  gpio_pull_up(PIN_I2C0_SDA);
  gpio_pull_up(PIN_I2C0_SCL);
  PM_INFO("I2C0 initialized at %d KHz, SDA %d, SCL %d\n", i2c_rate/1000, PIN_I2C0_SDA, PIN_I2C0_SCL);
  #else
  PM_INFO("I2C0 not initialized, no pins defined\n");
  #endif
}
 
void pm_qwiic_stop()
{
  i2c_deinit (qwiic_i2c);
  qwiic_enabled=false;
  qwiic_4charLCD_enabled=false;
}

void qwiic_display_4char(const char *str)
{
 if (qwiic_4charLCD_enabled) ht16k33_display_string(str);
}