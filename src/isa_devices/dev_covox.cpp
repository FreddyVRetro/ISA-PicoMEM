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

/* dev_covox.cpp : PicoMEM Covox emulation

 */ 

//#if USE_AUDIO
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType


bool dev_cvx_active=false;    // True if configured
volatile bool dev_cvx_playing=false;   // True if playing
volatile uint8_t dev_cvx_delay=0;      // counter for the Nb of second since last I/O
uint16_t dev_cvx_baseport=0x378;       // Port to LPT1

uint8_t dev_cvx_install(uint16_t baseport)
{
 if (!dev_cvx_active)   // Don't re enable if active
  {  
   PM_INFO("Install Covox (%x)\n",baseport);

   if (GetPortType(baseport)!=DEV_NULL)
     {
      PM_ERROR("Port already used (%d)\n",GetPortType(baseport));
      return 1;
     }


   SetPortType(baseport,DEV_CVX,1);
   dev_cvx_baseport=baseport;
   dev_cvx_active=true;
   dev_cvx_playing=false;
 }  
  else  // Check if the port need to be changed
    if (baseport!=dev_cvx_baseport)
     {
      PM_INFO("Change CVX Port (%x)\n",baseport);
      SetPortType(dev_cvx_baseport,DEV_NULL,1);
      SetPortType(baseport,DEV_CVX,1);
      dev_cvx_baseport=baseport;
     }

  return 0;
}

void dev_cvx_remove()
{
 if (dev_cvx_active)   // Don't stop if not active
  {  
   dev_cvx_active=false;
   dev_cvx_playing=false;   
   PM_INFO("Remove CVX (%X)\n",dev_cvx_baseport);

   SetPortType(dev_cvx_baseport,DEV_NULL,2);
  }  
}

// Return true if CMS is installed
bool dev_cvx_installed()
{
  return (GetPortType(dev_cvx_baseport)==DEV_CVX);
}

// Started in the Main Command Wait Loop
void dev_cvx_update()
{
}

bool dev_cvx_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
  return false;  // Nothing to read
}

void dev_cms_iow(uint32_t CTRL_AL8,uint8_t Data)
{
  uint8_t addr=CTRL_AL8&0x07;

  if (dev_cvx_playing==0)
     {
      //ADD Audio fifo init
      dev_cvx_playing=1;
     }
  

}
//#endif
