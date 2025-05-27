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

/* pm_tdy.cpp : PicoMEM Tandy emulation

 */ 

//#if USE_AUDIO
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType

#include "dev_audiomix.h"
#include "square/square.h"
tandysound_t *tandysound;   // Tandy Emulation object

bool dev_tdy_active=false;             // True if configured
volatile uint8_t dev_tdy_delay=0;      // counter for the Nb of second since last I/O
uint16_t dev_tdy_baseport;

uint8_t dev_tdy_install(uint16_t baseport)
{
 if (!dev_tdy_active)   // Don't re enable if active
  {  
   PM_INFO("Install Tandy (%x)\n",baseport);
   if (GetPortType(baseport)!=DEV_NULL)
     {
      PM_ERROR("Port already used (%d)\n",GetPortType(baseport));
      return 1;
     }

   tandysound=new tandysound_t();

   SetPortType(baseport,DEV_TANDY,1);
   dev_tdy_active=true;
   dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_TDY;
  }
  else  // Check if the port need to be changed
    if (baseport!=dev_tdy_baseport)
     {
      PM_INFO("Change TDY Port (%x)\n",baseport);
      SetPortType(dev_tdy_baseport,DEV_NULL,2);
      SetPortType(baseport,DEV_TANDY,2);
     }
  dev_tdy_baseport=baseport;
  return 0;
}

void dev_tdy_remove()
{
 if (dev_tdy_active)   // Don't stop if not active
  {  
   dev_tdy_active=false;
   dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_TDY;   
   PM_INFO("Remove TDY (%d)\n",dev_tdy_baseport);

   delete tandysound;
   SetPortType(dev_tdy_baseport,DEV_TANDY,1);
  }  
}

// Return true if Adlib is installed
bool dev_tdy_installed()
{
  return (GetPortType(dev_tdy_baseport)==DEV_TANDY);
}

// Started in the Main Command Wait Loop
void dev_tdy_update()
{
}

bool dev_tdy_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
  *Data=0xFF;
  return false;  // Nothing read
}

void dev_tdy_iow(uint32_t CTRL_AL8,uint8_t Data)
{
  if ((CTRL_AL8&0x07)==0) 
   {  
    tandysound->write_register(0, Data);    
    dev_audiomix.dev_active = dev_audiomix.dev_active | AD_TDY; // Enable mixind
    dev_tdy_delay=0;       // Reset the last access delay
//    PM_INFO("TDYW %x ",(uint8_t) Data);    
   }
}
//#endif
