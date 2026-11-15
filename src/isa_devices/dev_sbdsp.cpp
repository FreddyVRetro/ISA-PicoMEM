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
#include "pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType
#include "dev_audiomix.h"
#include "dev_adlib.h"
#include "opl.h"
#include "sbdsp/sbdsp.h"
#include "dev_sbdsp.h"

extern bool sbdsp_write(uint8_t address, uint8_t value);
extern uint8_t sbdsp_read(uint8_t address);
extern void sbdsp_init();
extern void sbdsp_process();
extern void sbdsp_dma_worker();

bool dev_sbdsp_active=false;    // True if configured
volatile uint8_t dev_sbdsp_delay=0;      // counter for the Nb of second since last I/O
uint16_t dev_sbdsp_baseport;

// Set the IRQ and DMA for the SB
// IRQ can be 3,5,7
// DMA can be 1,3
uint8_t dev_sbdsp_set_irq_dma(uint8_t irq,uint8_t dma)
{
 PM_INFO("[SB] Set IRQ %d DMA %d\n",irq,dma);
 if ((irq!=3) && (irq!=5) && (irq!=7))
    {
     PM_ERROR("[SB] Error: IRQ %d not valid\n",irq);
     return CMDERR_WRONG_ARG;
    }
 if ((dma!=1) && (dma!=3))
      {
      PM_ERROR("[SB] Error: DMA %d not valid\n",dma);
      return CMDERR_WRONG_ARG;
      }
 sbdsp.dma8=dma;
 sbdsp.irq=irq;
 if (irq==BV_IRQ)
     {
      PM_ERROR("[SB] Error: SBIRQ = PicoMEM IRQ (%d)\n",irq);
      return CMDERR_PM_IRQ;
     } 
 return 0;
}

uint8_t dev_sbdsp_install(uint16_t baseport)
{
 // TRACE entry state — lets us see when install short-circuits because
 // dev_sbdsp_active is already true (= no SetPortType, IO_Index_T stale).
 PM_INFO("dev_sbdsp_install(%x) active=%d cur_base=%x\n",
         baseport, (int)dev_sbdsp_active, dev_sbdsp_baseport);

 if (!dev_sbdsp_active)   // Don't re enable if active
  {
   PM_INFO("Install Sound Blaster (%x)\n",baseport);
   if (GetPortType(baseport)!=DEV_NULL)
     {
      PM_ERROR("Port already used (%d)\n",GetPortType(baseport));
      return CMDERR_PORTUSED;
     }

//   OPL_Pico_Init(0x388);

   SetPortType(baseport,DEV_SBDSP,2);

   sbdsp.type=SBT_2;
   dev_sbdsp_active=true;
   dev_sbdsp_disable_mix();
  }
  else  // Check if the port need to be changed
    if (baseport!=dev_sbdsp_baseport)
     {
      PM_INFO("Change Sound Blaster Port (%x)\n",baseport);
      DelPortType(DEV_SBDSP);  // 16 Port Address
      SetPortType(baseport,DEV_SBDSP,2);
     }
  dev_sbdsp_baseport=baseport;
  sbdsp_init();

  // TRACE: verify SetPortType actually wrote the table the bus handler
  // reads. For baseport=0x220, expect IO_Index_T[0x44]=IO_Index_T[0x45]
  // =DEV_SBDSP. If these read 0, the bus handler will io_miss on SB
  // cycles and the slave bus floats (0xFF on IN).
  PM_INFO("IO_Index_T[%x..%x] = %d %d (DEV_SBDSP=%d)\n",
          baseport>>3, (baseport>>3)+1,
          IO_Index_T[baseport>>3], IO_Index_T[(baseport>>3)+1],
          DEV_SBDSP);
  return 0;
}

void dev_sbdsp_remove()
{
 if (dev_sbdsp_active)       // Don't stop if not active
  {
   dev_sbdsp_active=false;
   dev_sbdsp_disable_mix();
   PM_INFO("Remove SB (%x)\n",dev_sbdsp_baseport);
   DelPortType(DEV_SBDSP);
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

  sbdsp_dma_worker();
}

bool dev_sbdsp_ior(uint32_t Port,uint8_t *Data )
{
  // No PM_INFO here -- this runs on Core 1 inside the IOR cycle window
  // while CHRDY is asserted. printf is backed by stdio_uart/stdio_usb
  // and blocks once its ring fills. On a burst (e.g. DSP-reset sequence
  // on SBDIAG key-press) it stalls dev_sbdsp_ior, RP_REL never fires,
  // CHRDY stays LOW, bus master hangs.
  dev_sbdsp_enable_mix();
  dev_sbdsp_delay=0;
  if ((Port&0x0F)==8) // Adlib part of SB Status
   {
    *Data = (uint8_t) OPL_Pico_PortRead(OPL_REGISTER_PORT);
    return true;
   }
   else
   {
    sbdsp_process();
    *Data=sbdsp_read((uint8_t) Port&0x0F);
     sbdsp_process();
     return true;
   }
}

void dev_sbdsp_iow(uint32_t Port,uint8_t Data)
{
  // No PM_INFO here -- see dev_sbdsp_ior comment. printf blocks on
  // UART/CDC ring saturation and hangs the bus cycle.
  static uint16_t sbopl_addr;
  sbdsp_process();
  dev_sbdsp_enable_mix();
  dev_sbdsp_delay=0;
  
  if (sbdsp_write((uint8_t) Port&0x0F, Data))
     sbdsp_process();  // Doesn't process if nothing written
    else  // If not SB, May be adlib
      switch (Port&0x0F) 
   {
   case 0x08:
     sbopl_addr = Data;
     dev_adlib_enable_mix();   // enable the mixing
     dev_adlib.delay=0;     
     return;
     break;
   case 0x0A:
     sbopl_addr = 0x100 | Data;
     dev_adlib_enable_mix();   // enable the mixing
     dev_adlib.delay=0;     
     return;
     break;
   case 0x09:  // Bank 1 Data
   case 0x0B:  // Bank 2 Data (OPL3)
     OPL_Pico_WriteRegister(sbopl_addr, Data);
     return;
     break;
  }
}
#endif
