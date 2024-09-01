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

/* pm_cms.cpp : PicoMEM Tandy emulation

 */ 

#if USE_AUDIO
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "../pm_port_f.h"   // SetPortType / GetPortType
#include "opl.h"

//extern "C" int OPL_Pico_Init(unsigned int);

bool dev_tdy_active=false;

uint8_t dev_tdy_install()
{

 if (!dev_tdy_active)   // Don't re enable if active
  {  
   PM_INFO("Install TDY ( )\n");

   SetPortType(0x388,DEV_TDY,1);   
   dev_tdy_active=true;
  }
  return 0;
}

void dev_tdy_remove()
{
  PM_INFO("Remove TDY ( )\n");

  SetPortType(0x388,DEV_NULL,1);
}

// Return true if Adlib is installed
bool dev_tdy_installed()
{
  return dev_tdy_active;
}

// Started in the Main Command Wait Loop
void dev_tdy_update()
{
}

bool dev_tdy_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
   if ((CTRL_AL8&0x07)==0) 
   {

  //  printf("CMSR %x-",*Data);
   }
   else *Data=0xFF;
   return true;
}

void dev_tdy_iow(uint32_t CTRL_AL8,uint32_t ISAIOW_Data)
{
  switch (CTRL_AL8&0x07) 
  {
   case 0:
  //   PM_INFO("CMSW 0,%x-",ISAIOW_Data);

     return;
     break;
   case 1:
   //  PM_INFO("CMSW 1,%x-",ISAIOW_Data);

     return;
     break;     
  }
    
}
#endif
