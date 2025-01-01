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

/* pm_adlib.cpp : PicoMEM Adlib emulation

 */ 

#if USE_AUDIO
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType
#include "opl.h"

extern "C" int OPL_Pico_Init(unsigned int);
extern "C" void OPL_Pico_Shutdown(void);
extern "C" void OPL_Pico_PortWrite(opl_port_t, unsigned int);
extern "C" unsigned int OPL_Pico_PortRead(opl_port_t);

bool dev_adlib_active=false;    // True if configured
volatile bool dev_adlib_playing=false;   // True if playing
volatile uint8_t dev_adlib_delay=0;      // counter for the Nb of second since last I/O
uint64_t dev_adlib_lastaccess;  // Last access time (For Audo mute)

uint8_t dev_adlib_install()
{
 if (!dev_adlib_active)   // Don't re enable if active
  {
   PM_INFO("Install OPL2 (0x388)\n");

   OPL_Pico_Init(0x388);
   SetPortType(0x388,DEV_ADLIB,1);   

   dev_adlib_active=true;
   dev_adlib_playing=false;   // It is enabled at the first Adlib IOW
  }
  return 0;
}

void dev_adlib_remove()
{
 if (dev_adlib_active)       // Don't stop if not active
  {
   dev_adlib_active=false;
   dev_adlib_playing=false;
   PM_INFO("Remove OPL2 (0x388)\n");
  
   OPL_Pico_Shutdown();
   SetPortType(0x388,DEV_NULL,1);  
  }
}

// Return true if Adlib is installed
bool dev_adlib_installed()
{
  return dev_adlib_active;
}

// Started in the Main Command Wait Loop
void dev_adlib_update()
{
}

bool dev_adlib_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
  if ((CTRL_AL8&0x07)==0) 
   {
    *Data = (uint8_t) OPL_Pico_PortRead(OPL_REGISTER_PORT); 
  //  printf("OPLR %x-",*Data);
    return true;
   }
  *Data=0xFF;
  return false;
}

void dev_adlib_iow(uint32_t CTRL_AL8,uint8_t Data)
{
  switch (CTRL_AL8&0x07) 
  {
   case 0:
  //   PM_INFO("OPLR 0,%x-",Data);
     OPL_Pico_PortWrite(OPL_REGISTER_PORT, (unsigned int) Data);
     dev_adlib_playing=true;  // enable mixind, start the timer
     dev_adlib_delay=0;     
     return;
     break;
   case 1:
   //  PM_INFO("OPLR 1,%x-",Data);
     OPL_Pico_PortWrite(OPL_DATA_PORT, (unsigned int) Data);
     dev_adlib_playing=true;  // enable mixind, start the timer
     dev_adlib_delay=0;  
     return;
     break;     
  }
    
}
#endif
