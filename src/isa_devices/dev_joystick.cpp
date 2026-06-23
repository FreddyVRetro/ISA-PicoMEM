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

/* pm_joystick.cpp : PicoMEM Joystick emulation
  Default PC Joystick port is 0x201

	**  Format of the byte to be returned:       
	**                        | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	**                        +-------------------------------+
	**                          |   |   |   |   |   |   |   |
	**  Joystick B, Button 2 ---+   |   |   |   |   |   |   +--- Joystick A, X Axis
	**  Joystick B, Button 1 -------+   |   |   |   |   +------- Joystick A, Y Axis
	**  Joystick A, Button 2 -----------+   |   |   +----------- Joystick B, X Axis
	**  Joystick A, Button 1 ---------------+   +--------------- Joystick B, Y Axis
	**

  Gravis GRiP :  
  https://github.com/prosper00/GRiP-duino
  GamePAD Pro : 
  GRiP mode uses Button 0 as a 20 to 25kHz clock. On each falling edge of this, you can read the state of button 1. Frames are 24 bits long
  And button 1 as data Data packet consists of 24 bytes, as follows: 0 1 1 1 1 1 0 Select Start R2 Blue 0 L2 Green Yellow Red 0 L1 R1 Up Down 0 Right Left

  "Reversed", GamePort Adapter:
  https://github.com/necroware/gameport-adapter

 */ 

#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType
#include "hardware/pwm.h"
#include "dev_joystick.h"
#include "../pm_debug.h"

//#include "hid_dev.h"      // In USB_Host_Driver
joystate_struct_t joystate_struct= {127, 127, 127, 127, 0xF0, 0, 0, 0, /*pl_total*/ 0, {255,0}, {255,0}};
//extern "C" joystate_struct_t joystate_struct;

constexpr float pwm_clkdiv = (float) (PM_SYS_CLK*1000 / 22727.27);  //CPU Clock


void dev_joystick_iow(uint32_t CTRL_AL8,uint8_t Data);

uint8_t dev_joystick_install()
{
  pwm_config pwm_c = pwm_get_default_config();
  // TODO better calibrate this
  pwm_config_set_clkdiv(&pwm_c, pwm_clkdiv);
  // Start the PWM off constantly looping at 0
  pwm_config_set_wrap(&pwm_c, 0);
  pwm_init(0, &pwm_c, true);
  pwm_init(1, &pwm_c, true);
  pwm_init(2, &pwm_c, true);
  pwm_init(3, &pwm_c, true);

  SetPortType(0x201,DEV_JOY,1);

  joystate_struct.button_mask=0xF0;
  joystate_struct.joy1_x=127;
  joystate_struct.joy1_y=127;  
  joystate_struct.joy1_x=127;
  joystate_struct.joy2_y=127;    
  dev_joystick_iow(0x201,0); // Simulate a Joystick IO Write

  return 0;
}

void dev_joystick_remove()
{
  DelPortType(DEV_JOY);
}

// Return true if the Joystick is installed
bool dev_joystick_installed()
{
  return (GetPortType(0x201)==DEV_JOY);
}

// Started in the Main Command Wait Loop
void dev_joystick_update()
{
}

int bitcount=0;

bool dev_joystick_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
  static uint8_t Grip_index;
  if (((CTRL_AL8 & 0x07)==1) && (joystate_struct.pl_total))  // Port 0x201 and players available
   {
gpio_put(43,1);  // Test scope pin    
    if (joystate_struct.mode == 3) 
      { // ** Gravis Gamepad Pro mode **
      // Divide the time in 48 steps of 25us (20kHz clock) to get the current bit index in the 24-bit frame, and return the corresponding bits

      uint8_t Grip_index = (47-((time_us_32()/25) % 48));  // move the clock index every 25us
      uint8_t index=Grip_index;
 //   Grip_index++;
 //    if (Grip_index >= 48) Grip_index=0;
 //    uint8_t index=47-Grip_index;

 //      joystate_struct.Gravis1=0b0111110 << 17 | 0b10001000100010001;
 //     printf(" %024b i%d\n",joystate_struct.Gravis1,index);
       uint8_t p1_value = (joystate_struct.Gravis1 >> (Grip_index>>1) ) & 1;
//       uint8_t p1_value = index>>1 & 1;   // Test
 //      uint8_t p2_value = (index>>2) & 1;   // Test
       uint8_t clock = index & 1;
gpio_put(46,clock);  // Test scope pin
gpio_put(47,p1_value);  // Test scope pin
       *Data = //(clock << 6) | (p2_value << 7)  |
               (clock << 4) | (p1_value << 5) |
               0b1111;
/*               |
               (bool)pwm_get_counter(0)        |
               ((bool)pwm_get_counter(1) << 1) |
               ((bool)pwm_get_counter(2) << 2) |
               ((bool)pwm_get_counter(3) << 3); */
     //   PM_INFO("Value %24b count %d index %d clock %d bit value %d Data:%X",joystate_struct.Gravis1, bitcount, bit_index,clock,bit_value,Data);
      }       
      else 
      { // ** Standard analog Joystick mode **
   // Proportional bits: 1 if counter is still counting, 0 otherwise
       *Data =  (bool)pwm_get_counter(0) |
                ((bool)pwm_get_counter(1) << 1) |
                ((bool)pwm_get_counter(2) << 2) |
                ((bool)pwm_get_counter(3) << 3) |
                joystate_struct.button_mask;
      }           
//  printf("*R %X\n",*Data); 
gpio_put(43,0);  // Test scope pin
   return true;
  }
  *Data=0xFF;
  return false;  // Nothing read
}

void dev_joystick_iow(uint32_t Addr,uint8_t Data)
{
  if ((Addr & 0x07)==1)  // Port 0x201  
   {
  //  printf("*W%X\n",ISAIOW_Data); 
    // Set times in # of cycles (affected by clkdiv) for each PWM slice to count up and wrap back to 0
    // TODO better calibrate this
    // GUS w/ gravis gamestick -
    //   sbdiag: 322/267 16/18 572/520 
    //   gravutil 34/29 2/2 61/56
    //   checkit 556/451 23/25 984/902
    // Noname w/ gravis gamestick
    //   sbdiag: 256/214 7/6 495/435
    //   gravutil 28/23 1/1 53/47
    //   checkit 355/284 6/5 675/585
    pwm_set_wrap(0, 2000 + ((uint16_t)joystate_struct.joy1_x << 6));
    pwm_set_wrap(1, 2000 + ((uint16_t)joystate_struct.joy1_y << 6));
    pwm_set_wrap(2, 2000 + ((uint16_t)joystate_struct.joy2_x << 6));
    pwm_set_wrap(3, 2000 + ((uint16_t)joystate_struct.joy2_y << 6));
    // Convince PWM to run as one-shot by immediately setting wrap to 0. This will take effect once the wrap
    // times set above hit, so after wrapping the counter value will stay at 0 instead of counting again
    pwm_set_wrap(0, 0);
    pwm_set_wrap(1, 0);
    pwm_set_wrap(2, 0);
    pwm_set_wrap(3, 0);
  
 }
}