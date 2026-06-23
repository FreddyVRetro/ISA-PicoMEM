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

/* pm_gus.cpp : PicoMEM Gravis Ultrasound emulation

 Port list : 
 -----------
 GF1 Page Register I/O R/W          3X2
 GF1/Global Register Select I/O R/W 3X3
 GF1/Global Data Low Byte I/O R,W   3X4
 GF1/Global Data High Byte I/O R/W  3X5
 IRQ Status Register 1=ACTIVE I/O R 2X6
 Timer Control Reg I/O R/W          2X8
 Timer Data I/O W                   2X9
 DRAM I/O R,W                       3X7
 */ 

#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType
#include "dev_gus.h"

#include "gus/gus-x.h"

dev_gus_t dev_gus = {false,0,0};

// Set the IRQ and DMA for the GUS
// IRQ can be 3,5,7
// DMA can be 1,3
uint8_t dev_gus_set_irq_dma(uint8_t irq,uint8_t dma)
{
 GUS_SetIRQDMA(irq,dma);
 if (irq==BV_IRQ)
     {
      PM_ERROR("GUSIRQ = PicoMEM IRQ (%d)\n",irq);
      return 1;
     } 
 return 0;
}

uint8_t dev_gus_install(uint16_t baseport)
{
  PM_INFO("Install GUS (%x)\n",baseport);
  if (!dev_gus.active)   // Don't re enable if active
  {
   if ((GetPortType(baseport)!=DEV_NULL)||(GetPortType(baseport+0x100)!=DEV_NULL))
     {
      PM_ERROR("Port already used (%d)\n",GetPortType(baseport));
      PM_ERROR("Port already used (%d)\n",GetPortType(baseport+0x100));
      return CMDERR_PORTUSED;
     }

   PM_INFO("Initialize GUS\n");
   GUS_OnReset();  // Initialize the GUS emulation
   GUS_Setup();    // Setup Clamp/interpolation (If needed)
   
   SetPortType(baseport,DEV_GUS,2);
   SetPortType(baseport+0x100,DEV_GUS,1);
   dev_gus.baseport=baseport;
   dev_gus.active=true;
   dev_gus_disable_mix();
   dev_gus.delay=0;
 }  
  else  // Check if the port need to be changed
    PM_INFO("GUS already active, check port\n");
    if (baseport!=dev_gus.baseport)
     {
      PM_INFO("Change GUS Port (%x)\n",baseport);
      DelPortType(DEV_GUS);                           // Remove old port     
      SetPortType(baseport,DEV_GUS,2);                // Set new port
      SetPortType(baseport+0x100,DEV_GUS,1);
      dev_gus.baseport=baseport;
     }
  return 0;
}

void dev_gus_remove()
{
 if (dev_gus.active)   // Don't stop if not active
  {  
   dev_gus.active=false;
   dev_gus_disable_mix();
   PM_INFO("Remove GUS (%X)\n",dev_gus.baseport);
   
   DelPortType(DEV_GUS);
   GUS_Shutdown();  // Cleanup GUS emulation
  }
}

// Return true if CMS is installed
bool dev_gus_installed()
{
  return (GetPortType(dev_gus.baseport)==DEV_GUS);
}

// Started in the Main Command Wait Loop
void dev_gus_update()
{
}

bool dev_gus_ior(uint32_t Addr,uint8_t *Data )
{
  *Data=read_gus(Addr-dev_gus.baseport);
   return true;
}

void dev_gus_iow(uint32_t Addr,uint8_t Data)
{
  dev_gus_enable_mix();
  dev_gus.delay=0;
  write_gus(Addr-dev_gus.baseport,Data);
}