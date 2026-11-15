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
#include "dev_memory.h"       // PSRAM_GUS_START + pico_psram (ps_size)

#include "gus/gus-x.h"

dev_gus_t dev_gus = {false,0,0};

// GUS wave RAM lives at PSRAM_GUS_START (offset 5 MB) and spans 1 MB, so the
// map needs 6 MB of PSRAM. Guard against installing on a board whose detected
// PSRAM is too small -- otherwise GUS would read/write past the end of PSRAM
// and silently corrupt. 6 MB = 5 MB (GUS start offset) + 1 MB (GUS RAM).
#define GUS_PSRAM_NEEDED_BYTES  (6u*1024u*1024u)

// Set the IRQ and DMA for the GUS
// IRQ can be 3,5,7
// DMA can be 1,3
uint8_t dev_gus_set_irq_dma(uint8_t irq,uint8_t dma)
{
 GUS_SetIRQDMA(irq,dma);
 if (irq==BV_IRQ)
     {
      PM_ERROR("Error: GUSIRQ = PicoMEM IRQ (%d)\n",irq);
      return CMDERR_PM_IRQ;
     } 
 return 0;
}

uint8_t dev_gus_install(uint16_t baseport)
{
  PM_INFO("Install GUS (%x)\n",baseport);
#if USE_RP2350_PSRAM
  if (ps_size < GUS_PSRAM_NEEDED_BYTES)
    {
     PM_ERROR("GUS needs %u MB PSRAM, only %u MB detected -- not installing\n",
              (unsigned)(GUS_PSRAM_NEEDED_BYTES/(1024u*1024u)),
              (unsigned)(ps_size/(1024u*1024u)));
     return CMDERR_PORTUSED;
    }
#endif
  if (!dev_gus.active)   // Don't re enable if active
  {
#if BOARD_WM10
   // WM10: the 0x220 low window is the SHARED DEV_SBDSP "sound-card" block --
   // the MCA bus handler routes it to GUS via the dev_gus.active flag (SB and
   // GUS are mutually exclusive). Only the 0x320 (3X0) window is GUS-exclusive.
   // So accept baseport already being DEV_SBDSP; only 0x320 must be free.
   if (GetPortType(baseport+0x100)!=DEV_NULL)
     {
      PM_ERROR("GUS 3X0 port already used (%d)\n",GetPortType(baseport+0x100));
      return CMDERR_PORTUSED;
     }
   PM_INFO("Initialize GUS (WM10, shared 0x220 + GUS 0x320)\n");
   GUS_OnReset();
   GUS_Setup();
   // Both windows use the shared DEV_SBDSP marker; the MCA bus handler routes
   // them to GUS via the dev_gus.active flag (no separate DEV_GUS dispatch on
   // WM10). 0x220 low window (2 blocks) + 0x320 3X0 window (1 block).
   SetPortType(baseport,DEV_SBDSP,2);
   SetPortType(baseport+0x100,DEV_SBDSP,1);
#else
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
#endif
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
#if BOARD_WM10
      // Shared DEV_SBDSP marker + active-flag routing (see dev_gus_install).
      SetPortType(dev_gus.baseport+0x100,DEV_NULL,1);  // Free old 0x320 window
      SetPortType(baseport,DEV_SBDSP,2);               // Set new ports
      SetPortType(baseport+0x100,DEV_SBDSP,1);
#else
      DelPortType(DEV_GUS);                           // Remove old port
      SetPortType(baseport,DEV_GUS,2);                // Set new port
      SetPortType(baseport+0x100,DEV_GUS,1);
#endif
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
#if BOARD_WM10
   // WM10: ports were registered as the shared DEV_SBDSP marker (see
   // dev_gus_install). Free only the GUS-exclusive 0x320 (3X0) window;
   // leave the shared 0x220 low window -- if SB is being activated next,
   // PM_sbdsp_OnOff re-marks it, otherwise PM_Audio_OnOff turns it off.
   SetPortType(dev_gus.baseport+0x100,DEV_NULL,1);
#else
   DelPortType(DEV_GUS);
#endif
   GUS_Shutdown();  // Cleanup GUS emulation
  }
}

// Return true if the GUS is installed
bool dev_gus_installed()
{
#if BOARD_WM10
  // WM10 routes the shared DEV_SBDSP block to GUS via the active flag.
  return dev_gus.active;
#else
  return (GetPortType(dev_gus.baseport)==DEV_GUS);
#endif
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