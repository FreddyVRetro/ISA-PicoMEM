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

/* dev_covox.cpp : PicoMEM Covox, Disney Sound Source emulation

Covox Speech Thing : Parallel port Simple DAC
Disney Sound Source : Parallel port DAC with small FIFO
Stereo in One : LPT DAC With Right/Left Channel, can be auto detected
Covox Voice Master :
Covox Sound Master : AY-3-8910 (PSG), Covox DAC and DMA via the AY8930 Timer
Covox Sound Master + : OPL2 FM, Covox DAC and Covox via DMA
 */ 

//#if USE_AUDIO
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType
#include "covox/covox_dac.h"
#include "dev_audiomix.h"
#include "dev_covox.h"

dev_cvx_t dev_cvx {false,0,0};

uint8_t dev_cvx_install(uint8_t type, uint16_t baseport)
{
 uint8_t res=0;

 if (!dev_cvx.active)   // Don't re enable if active
  {
   PM_INFO("Install Covox (%x)\n",baseport);

   if (GetPortType(baseport)!=DEV_NULL)
     {
      PM_ERROR("Port already used (%d)\n",GetPortType(baseport));
      return CMDERR_PORTUSED;
     }
   SetPortType(baseport,DEV_CVX,1);
   res=cvx_enable(type,baseport,0);
   if (res!=0) { 
     PM_INFO("cvx_enable Err:%x\n",res);
     return res; // Return the error code
    }    
   dev_cvx.baseport=baseport;
   dev_cvx.active=true;
 }  
  else  // Check if the port need to be changed
    if (baseport!=dev_cvx.baseport)
     {
      PM_INFO("Change CVX Port (%x)\n",baseport);
      SetPortType(dev_cvx.baseport,DEV_NULL,1);
      SetPortType(baseport,DEV_CVX,1);
      dev_cvx.baseport=baseport;
     }
  dev_cvx_disable_mix(); // Disable Covox mixing
  return res;
}

void dev_cvx_remove()
{
 if (dev_cvx.active)   // Don't stop if not active
  {  
   dev_cvx.active=false;
   PM_INFO("Remove CVX (%X)\n",dev_cvx.baseport);

   SetPortType(dev_cvx.baseport,DEV_NULL,1);
   dev_cvx_disable_mix(); // Disable Covox mixing 
  }  
}

// Return true if CMS is installed
bool dev_cvx_installed()
{
  return (GetPortType(dev_cvx.baseport)==DEV_CVX);
}

// Started in the Main Command Wait Loop
void dev_cvx_update()
{
}

bool dev_cvx_ior(uint32_t Addr,uint8_t *Data )
{
  return false;  // Nothing to read
}

void dev_cvx_iow(uint32_t Addr,uint8_t Data)
{
  if (!(dev_audiomix.dev_active & AD_CVX))
     {
      PM_INFO("CVX Start\n");
      cvx_start_dac();
     }
  dev_cvx_enable_mix(); // Enable Covox mixing
  dev_cvx.delay=0;
  cvx_outp(Addr, Data);
}

void dev_cvx_test()
{
dev_cvx_install(CVX_TYPE_DAC,0x378);
cvx_test();
dev_cvx_remove();
for (;;) {}
}

//#endif