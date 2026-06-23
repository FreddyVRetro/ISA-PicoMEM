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

/* pm_adlib.cpp : PicoMEM Adlib emulation */

#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType
#include "dev_audiomix.h"
#include "dev_adlib.h"
#include "opl.h"

dev_adl_t dev_adlib = {false,0};

uint8_t dev_adlib_install()
{
 if (!dev_adlib.active)   // Don't re enable if active
  {
   PM_INFO("Install OPL2 (0x388)\n");

   OPL_Pico_Init(0x388);
   SetPortType(0x388,DEV_ADLIB,1);   
   dev_adlib.active=true;
   dev_adlib_disable_mix();
  }
  return 0;
}

void dev_adlib_remove()
{
 if (dev_adlib.active)       // Don't stop if not active
  {
   PM_INFO("Remove OPL2 (0x388)\n");

   OPL_Pico_Shutdown();
   DelPortType(DEV_ADLIB);  
   dev_adlib.active=false;
   dev_adlib_disable_mix();  
  }
}

// Return true if Adlib is installed
bool dev_adlib_installed()
{
  return dev_adlib.active;
}

// Started in the Main Command Wait Loop (Not needed for Adlib)
void dev_adlib_update()
{
}

bool dev_adlib_ior(uint32_t Addr,uint8_t *Data )
{
  if ((Addr&0x07)==0) 
   {
    *Data = (uint8_t) OPL_Pico_PortRead(OPL_REGISTER_PORT); 
  //  printf("OPLR %x-",*Data);
    return true;
   }
  *Data=0xFF;
  return false;
}

void dev_adlib_iow(uint32_t Addr,uint8_t Data)
{
  static uint16_t opl_addr;

  switch (Addr&0x07) 
  {
   case 0:  // Bank 1 Address
     opl_addr = Data;
     dev_adlib_enable_mix();   // enable the mixing
     dev_adlib.delay=0;
     return;
     break;
   case 2:  // Bank 2 Address (OPL3)
     opl_addr = 0x100 | Data;
     dev_adlib_enable_mix();   // enable the mixing
     dev_adlib.delay=0;
     return;
     break;
   case 1:  // Bank 1 Data
   case 3:  // Bank 2 Data (OPL3)
     OPL_Pico_WriteRegister(opl_addr, Data);
     return;
     break;
  }
}
