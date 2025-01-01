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

 */ 

#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType
#include "hardware/pwm.h"

#include "hid_dev.h"      // In USB_Host_driver

constexpr float pwm_clkdiv = (float) (280000 / 22727.27);  //CPU Clock

extern "C" joystate_struct_t joystate_struct;

void dev_joystick_iow(uint32_t CTRL_AL8,uint8_t Data);

uint8_t dev_joystick_install()
{
  // Init joystick as centered with no buttons pressed
  joystate_struct = {127, 127, 127, 127, 0xf};
 // puts("Config joystick PWM\n");
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

  dev_joystick_iow(0x201,0); // Simulate a Joystick IO Write
  joystate_struct.button_mask=0xF0;

  return 0;
}

void dev_joystick_remove()
{
  SetPortType(0x201,DEV_NULL,1);
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

bool dev_joystick_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
  if ((CTRL_AL8 & 0x07)==1)  // Port 0x201
   {
//  printf("*R\n");    
   // Proportional bits: 1 if counter is still counting, 0 otherwise
   *Data =  (bool)pwm_get_counter(0) |
           ((bool)pwm_get_counter(1) << 1) |
           ((bool)pwm_get_counter(2) << 2) |
           ((bool)pwm_get_counter(3) << 3) |
           joystate_struct.button_mask;
//  printf("*R %X\n",*Data); 
   return true;
  }
  *Data=0xFF;
  return false;  // Nothing read
}

void dev_joystick_iow(uint32_t CTRL_AL8,uint8_t Data)
{
  if ((CTRL_AL8 & 0x07)==1)  // Port 0x201  
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