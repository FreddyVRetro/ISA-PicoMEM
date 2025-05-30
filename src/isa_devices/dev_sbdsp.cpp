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

/* pm_sbdsp.cpp : PicoMEM sbdsp emulation

 */ 

#if USE_AUDIO
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType
#include "dev_audiomix.h"
#include "opl.h"

extern bool sbdsp_write(uint8_t address, uint8_t value);
extern uint8_t sbdsp_read(uint8_t address);
extern void sbdsp_init(uint8_t sbirq);
extern void sbdsp_process();
extern void sbdsp_ml_command();

// Adlib part of the SB definitions
extern "C" void OPL_Pico_PortWrite(opl_port_t, unsigned int);
extern "C" unsigned int OPL_Pico_PortRead(opl_port_t);
extern volatile uint8_t dev_adlib_delay;      // counter for the Nb of second since last I/O

bool dev_sbdsp_active=false;    // True if configured
volatile bool dev_sbdsp_playing=false;   // True if playing
volatile uint8_t dev_sbdsp_delay=0;      // counter for the Nb of second since last I/O
uint64_t dev_sbdsp_lastaccess;  // Last access time (For Audo mute)
uint16_t dev_sbdsp_baseport;

uint8_t dev_sbdsp_install(uint16_t baseport,uint8_t sbirq)
{
 if (!dev_sbdsp_active)   // Don't re enable if active
  {
   PM_INFO("Install Sound Blaster (%x)\n",baseport);
   if (GetPortType(baseport)!=DEV_NULL)
     {
      PM_ERROR("Port already used (%d)\n",GetPortType(baseport));
      return 1;
     }
   if (sbirq==BV_IRQ)
     {
      PM_ERROR("SBIRQ = PicoMEM IRQ (%d)\n",sbirq);
      return 1;
     }

//   OPL_Pico_Init(0x388);

   SetPortType(baseport,DEV_SBDSP,2);

   dev_sbdsp_active=true;
   dev_sbdsp_playing=false;   // It is enabled at the first sbdsp IOW
   dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_SBDSP;
  }
  else  // Check if the port need to be changed
    if (baseport!=dev_sbdsp_baseport)
     {
      PM_INFO("Change Sound Blaster Port (%x)\n",baseport);
      SetPortType(dev_sbdsp_baseport,DEV_NULL,2);  // 16 Port Address
      SetPortType(baseport,DEV_SBDSP,2);
     } 
  dev_sbdsp_baseport=baseport;
  sbdsp_init(sbirq);
  return 0;
}

void dev_sbdsp_remove()
{
 if (dev_sbdsp_active)       // Don't stop if not active
  {
   dev_sbdsp_active=false;
   dev_sbdsp_playing=false;
   dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_SBDSP;
   PM_INFO("Remove SB (%x)\n",dev_sbdsp_baseport);
  
//   DSP Stop ?
   SetPortType(dev_sbdsp_baseport,DEV_NULL,1);
  }
}

// Return true if sbdsp is installed
bool dev_sbdsp_installed()
{
  return dev_sbdsp_active;
}

// Started in the Main Command Wait Loop
void dev_sbdsp_update()
{
  sbdsp_ml_command();  
}

bool dev_sbdsp_ior(uint32_t Port,uint8_t *Data )
{

  if ((Port&0x0F)==8) // Adlib part of SB Status
   {
    *Data = (uint8_t) OPL_Pico_PortRead(OPL_REGISTER_PORT); 
  //  printf("OPLR %x-",*Data);
    return true;
   } 
   else
   {
    sbdsp_process();
    *Data=sbdsp_read((uint8_t) Port&0x0F);        
     sbdsp_process();
 //    PM_INFO("SBR %x>%x",Port,Data);
     return true;
   }
}

void dev_sbdsp_iow(uint32_t Port,uint8_t Data)
{
  //   PM_INFO("SBW %x>%x",Port,Data);
  sbdsp_process();
  dev_sbdsp_playing=true;  // enable mixind, start the timer
  dev_audiomix.dev_active = dev_audiomix.dev_active | AD_SBDSP;
  dev_sbdsp_delay=0;
  
  if (sbdsp_write((uint8_t) Port&0x0F, Data))
     sbdsp_process();  // Doesn't process if nothing written
    else  // If not SB, May be adlib
     switch (Port&0x0F)
     {
     case 8:
    //   PM_INFO("OPLR 0,%x-",Data);
       OPL_Pico_PortWrite(OPL_REGISTER_PORT, (unsigned int) Data);
       dev_audiomix.dev_active = dev_audiomix.dev_active | AD_ADLIB;   // enable the mixing       
       dev_adlib_delay=0;     
       return;
       break;
     case 9:
     //  PM_INFO("OPLR 1,%x-",Data);
       OPL_Pico_PortWrite(OPL_DATA_PORT, (unsigned int) Data);
       dev_audiomix.dev_active = dev_audiomix.dev_active | AD_ADLIB;   // enable the mixing       
       dev_adlib_delay=0;  
       return;
       break;     
     }
}
#endif
