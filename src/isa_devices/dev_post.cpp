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

/* pm_post.cpp : PicoMEM POST Code
 */ 

#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "dev_picomem_io.h"
#include "pm_i2c.h"
#include "pm_display.h"

hostio_t HostIO;

bool dev_post_toinstall=false;   // Tell if we need to install or not the POST Code (If display exist)

void dev_post_install()
{
 PM_INFO("Install POST\n");
 SetPortType(0x80,DEV_POST,1);
 if (HostIO.Post2Port) 
    {
      PM_INFO("PostPort: %X",HostIO.Post2Port);
      SetPortType(HostIO.Post2Port,DEV_POST,1);
    }
}

void dev_post_setPort2(uint16_t BasePort)
{
  HostIO.Post2Port=BasePort;
}


void dev_post_remove()
{
 PM_INFO("Unistall POST\n");   
 DelPortType(DEV_POST);
}

void dev_post_update()
{
 if (HostIO.Port80h_Updated||HostIO.PortPost2_Updated)
  {
   HostIO.Port80h_Updated=false;
   char poststr[5];
   poststr[4]=0;
   sprintf(&poststr[0],"Bp%X",HostIO.Port80h);
//   PM_INFO("%s",poststr);
   qwiic_display_4char(poststr);                        // Send the POST Code to the External LCD
   #if USE_OLED
   pm_display_event(D_PCPOST,HostIO.Port80h,HostIO.PortPost2,NULL);  // Send the POST Code to the OLED
   #endif
  }

 if (HostIO.Port81h_Updated)
  {
   HostIO.Port81h_Updated=false;
   char poststr[5];
   poststr[4]=0;
   sprintf(&poststr[0],"Pp%X",HostIO.Port81h);
//   PM_INFO("%s",poststr);
   qwiic_display_4char(poststr);       // Send the POST Code to the External LCD
   #if USE_OLED   
   pm_display_event(D_PMPOST,HostIO.Port81h,0,NULL);  // Send the POST Code to the OLED   
   #endif
  }  
}

void dev_post_iow(uint32_t Addr,uint8_t Data)
{
//   PM_INFO("p%x %x",Addr,Data);
   if ((Addr & 0xFFF)==HostIO.Post2Port)
    {
      HostIO.PortPost2=Data;
		HostIO.PortPost2_Updated=true;
      PM_INFO("B2%x",Data);      
      return;
    }
   switch (Addr & 0xF)
   {
    case 0: HostIO.Port80h=Data;
			   HostIO.Port80h_Updated=true;
//            PM_INFO("BP%x",Data);
            break;
    case 1: HostIO.Port81h=Data;
			   HostIO.Port81h_Updated=true;
//            PM_INFO("PP%x",Data);
  //          PM_INFO("Ib%x",Data);       // Debug display for IRQ Begin/end
            break;           
 //   case 2: PM_INFO("Ie%x",Data);
   }
}