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
#include "pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType

#include "dev_audiomix.h"
#include "square/square.h"
tandysound_t *tandysound;   // Tandy Emulation object

bool dev_ps1_active=false;             // True if configured
volatile uint8_t dev_ps1_delay=0;      // counter for the Nb of second since last I/O

uint8_t dev_ps1_install()
{
 if (!dev_ps1_active)   // Don't re enable if active
  {  
   PM_INFO("Install PS1 (0x200)\n");
   if (GetPortType(0x200)!=DEV_NULL)
     {
      PM_ERROR("Port already used (%d)\n",GetPortType(0x200));
      return CMDERR_PORTUSED;
     }

   tandysound=new tandysound_t();

   SetPortType(0x200,DEV_TANDY,1);
   dev_ps1_active=true;
   dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_TDY;
  }
 return 0;
}

void dev_ps1_remove()
{
 if (dev_ps1_active)   // Don't stop if not active
  {  
   dev_ps1_active=false;
   dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_TDY;   
   PM_INFO("Remove PS1 (0x200)\n");

   delete tandysound;
   DelPortType(DEV_TANDY);
  }  
}

// Return true if PS1 is installed
bool dev_ps1_installed()
{
  return (GetPortType(0x200)==DEV_TANDY);
}

// Started in the Main Command Wait Loop
void dev_ps1_update()
{
}

bool dev_ps1_ior(uint32_t Addr,uint8_t *Data )
{
  *Data=ps1_audio_read(Addr);
  return true;
}

void dev_ps1_iow(uint32_t Addr,uint8_t Data)
{
  if ((Addr&0x07)==5) // Port 5 is for the 
   {  
    tandysound->write_register(0, Data);    
    dev_audiomix.dev_active = dev_audiomix.dev_active | AD_PS1; // Enable mixind
    dev_ps1_delay=0;       // Reset the last access delay 
   } else ps1_audio_write(Addr, Data);
}
//#endif
