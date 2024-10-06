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

/* pm_cms.cpp : PicoMEM CMS emulation

 */ 

//#if USE_AUDIO
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType

#ifdef MAME_CMS
#include "saa1099/saa1099.h"
saa1099_device *saa0, *saa1; // CMS Emulation object
#else
#include "square/square.h"
cms_t *cms;                  // CMS Emulation object
#endif  //MAME_CMS

bool dev_cms_active=false;    // True if configured
volatile bool dev_cms_playing=false;   // True if playing
volatile uint8_t dev_cms_delay=0;      // counter for the Nb of second since last I/O
uint16_t dev_cms_baseport;    // CMS use an address range of 0 to B
uint8_t cms_detect;           // Used for CMS detection

uint8_t dev_cms_install(uint16_t baseport)
{
 if (!dev_cms_active)   // Don't re enable if active
  {  
   PM_INFO("Install CMS (%x)\n",baseport);

   if (GetPortType(baseport)!=DEV_NULL)
     {
      PM_ERROR("Port already used (%d)\n",GetPortType(baseport));
      return 1;
     }

#ifdef MAME_CMS
//   PM_INFO("Creating SAA1099 1");
   saa0 = new saa1099_device(7159090);  //7159090 is the CMS Chip clock, not the buffer frequency
//   PM_INFO("Creating SAA1099 2");
   saa1 = new saa1099_device(7159090);
#else
//   PM_INFO("Creating CMS");
   cms=new cms_t();
#endif  //MAME_CMS

   SetPortType(baseport,DEV_CMS,2);
   dev_cms_baseport=baseport;
   dev_cms_active=true;
   dev_cms_playing=false;
   cms_detect=0xFF;   
 }  
  else  // Check if the port need to be changed
    if (baseport!=dev_cms_baseport)
     {
      PM_INFO("Change CMS Port (%x)\n",baseport);
      SetPortType(dev_cms_baseport,DEV_NULL,2);
      SetPortType(baseport,DEV_CMS,2);
      dev_cms_baseport=baseport;
     }

  return 0;
}

void dev_cms_remove()
{
 if (dev_cms_active)   // Don't stop if not active
  {  
   dev_cms_active=false;
   dev_cms_playing=false;   
   PM_INFO("Remove CMS (%X)\n",dev_cms_baseport);
#ifdef MAME_CMS
   PM_INFO("Delete SAA1099 1\n");
   delete saa0;
   PM_INFO("Delete SAA1099 2\n");
   delete saa1;
#else
//   PM_INFO("Delete CMS\n");
   delete cms;
#endif  //MAME_CMS

   SetPortType(dev_cms_baseport,DEV_NULL,2);
  }  
}

// Return true if CMS is installed
bool dev_cms_installed()
{
  return (GetPortType(dev_cms_baseport)==DEV_CMS);
}

// Started in the Main Command Wait Loop
void dev_cms_update()
{
}

bool dev_cms_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
    // CMS ior are used for autodetect
    switch (CTRL_AL8&0x0F) {
    case 0x4:
        *Data=0x7F;
        break;
    case 0xa:
    case 0xb:
        *Data=cms_detect;
        break;
    }

//   PM_INFO("CMSR %x,%x-",CTRL_AL8,*Data);
   return true;
}

void dev_cms_iow(uint32_t CTRL_AL8,uint32_t ISAIOW_Data)
{
  uint8_t addr=CTRL_AL8&0x07;

#ifdef MAME_CMS  
  switch (addr) 
  {
   case 0:
     PM_INFO("CMSW 0,%x-",ISAIOW_Data);
     saa0->data_w(cmd.data);
     break;
   case 1:
     PM_INFO("CMSW 1,%x-",ISAIOW_Data);
     saa0->control_w(cmd.data);
     break; 
   case 2:
     PM_INFO("CMSW 2,%x-",ISAIOW_Data);
     saa1->data_w(cmd.data);
     break; 
   case 3:
     PM_INFO("CMSW 3,%x-",ISAIOW_Data);
     saa1->control_w(cmd.data);
     break;
    case 0x6:
    case 0x7:
             cms_detect = ISAIOW_Data & 0xFF;
             break;     
  }
#else // MAME_CMS
//  PM_INFO("CMSW %x,%x-",CTRL_AL8,ISAIOW_Data);
  switch (addr) 
  {
    case 0x0:
    case 0x1:
    case 0x2:
    case 0x3:
             if (addr & 1) {
              cms->write_addr(addr, ISAIOW_Data);
              } else {
              cms->write_data(addr, ISAIOW_Data);
              }
             dev_cms_playing=true;  // enable mixind, start the timer
             dev_cms_delay=0;
             break;
    case 0x6:
    case 0x7:
             cms_detect = ISAIOW_Data & 0xFF;
             break;
  }
#endif // MAME_CMS

}
//#endif
