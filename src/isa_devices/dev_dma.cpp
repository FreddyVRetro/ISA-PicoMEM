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

/* dev_dma.cpp : PicoMEM DMA port write capture

PORT 0000-001F - DMA 1 - FIRST DIRECT MEMORY ACCESS CONTROLLER (8237)
SeeAlso: PORT 0080h-008Fh"DMA",PORT 00C0h-00DFh

0000  R-  DMA channel 0	current address		byte  0, then byte 1
0000  -W  DMA channel 0	base address		byte  0, then byte 1
0001  RW  DMA channel 0 word count		byte 0, then byte 1
0002  R-  DMA channel 1	current address		byte  0, then byte 1
0002  -W  DMA channel 1	base address		byte  0, then byte 1
0003  RW  DMA channel 1 word count		byte 0, then byte 1
0004  R-  DMA channel 2	current address		byte  0, then byte 1
0004  -W  DMA channel 2	base address		byte  0, then byte 1
0005  RW  DMA channel 2 word count		byte 0, then byte 1
0006  R-  DMA channel 3	current address		byte  0, then byte 1
0006  -W  DMA channel 3	base address		byte  0, then byte 1
0007  RW  DMA channel 3 word count		byte 0, then byte 1

0008  R-  DMA channel 0-3 status register (see #P0001)
0008  -W  DMA channel 0-3 command register (see #P0002)
0009  -W  DMA channel 0-3 write request register (see #P0003)
000A  RW  DMA channel 0-3 mask register (see #P0004)
000B  -W  DMA channel 0-3 mode register (see #P0005)

000C  -W  DMA channel 0-3 clear byte pointer flip-flop register
	  any write clears LSB/MSB flip-flop of address and counter registers
000D  R-  DMA channel 0-3 temporary register
000D  -W  DMA channel 0-3 master clear register
	  any write causes reset of 8237
000E  -W  DMA channel 0-3 clear mask register
	  any write clears masks for all channels
000F  rW  DMA channel 0-3 write mask register (see #P0006)
Notes:	the temporary register is used as holding register in memory-to-memory
	  DMA transfers; it holds the last transferred byte
	channel 2 is used by the floppy disk controller
	on the IBM PC/XT channel 0 was used for the memory refresh and
	  channel 3 was used by the hard disk controller
	on AT and later machines with two DMA controllers, channel 4 is used
	  as a cascade for channels 0-3
	command and request registers do not exist on a PS/2 DMA controller

Bitfields for DMA channel 0-3 status register:
Bit(s)	Description	(Table P0001)
 7	channel 3 request active
 6	channel 2 request active
 5	channel 1 request active
 4	channel 0 request active
 3	channel terminal count on channel 3
 2	channel terminal count on channel 2
 1	channel terminal count on channel 1
 0	channel terminal count on channel 0
SeeAlso: #P0002,#P0481

Bitfields for DMA channel 0-3 command register:
Bit(s)	Description	(Table P0002)
 7	DACK sense active high
 6	DREQ sense active high
 5	=1 extended write selection
	=0 late write selection
 4	rotating priority instead of fixed priority
 3	compressed timing (two clocks instead of four per transfer)
	=1 normal timing (default)
	=0 compressed timing
 2	=1 enable controller
	=0 enable memory-to-memory
 1-0	channel number
SeeAlso: #P0001,#P0004,#P0005,#P0482

Bitfields for DMA channel 0-3 request register:
Bit(s)	Description	(Table P0003)
 7-3	reserved (0)
 2	=0 clear request bit
	=1 set request bit
 1-0	channel number
	00 channel 0 select
	01 channel 1 select
	10 channel 2 select
	11 channel 3 select
SeeAlso: #P0004

Bitfields for DMA channel 0-3 mask register:
Bit(s)	Description	(Table P0004)
 7-3	reserved (0)
 2	=0 clear mask bit
	=1 set mask bit
 1-0	channel number
	00 channel 0 select
	01 channel 1 select
	10 channel 2 select
	11 channel 3 select
SeeAlso: #P0001,#P0002,#P0003,#P0484

Bitfields for DMA channel 0-3 mode register:
Bit(s)	Description	(Table P0005)
 7-6	transfer mode
	00 demand mode
	01 single mode
	10 block mode
	11 cascade mode
 5	direction
	=0 increment address after each transfer
	=1 decrement address
 3-2	operation
	00 verify operation
	01 write to memory
	10 read from memory
	11 reserved
 1-0	channel number
	00 channel 0 select
	01 channel 1 select
	10 channel 2 select
	11 channel 3 select
SeeAlso: #P0002,#P0485

Bitfields for DMA channel 0-3 write mask register:
Bit(s)	Description	(Table P0006)
 7-4	reserved
 3	channel 3 mask bit
 2	channel 2 mask bit
 1	channel 1 mask bit
 0	channel 0 mask bit
Note:	each mask bit is automatically set when the corresponding channel
	  reaches terminal count or an extenal EOP sigmal is received
SeeAlso: #P0004,#P0486

*/

//#if USE_AUDIO
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType

#include "dev_dma.h"

bool dev_dma_active =false;         // True if configured
bool dev_dma_emulate=false;         // True if respond to IOR (For Tandy 1000 EX/HX)
bool dma_flipflop   =false;
#define dev_dma_baseport 0x0        // Port to the DMA controller registers
#define dev_dma_pageport 0x80       // Port to the DMA pages

volatile bool dma_ignorechange;

/*
volatile uint8_t dma_status;
volatile uint8_t dma_command;
volatile uint8_t dma_request;
volatile uint8_t dma_mask;
volatile uint8_t dma_mode;
*/
volatile uint8_t dev_dma_mask;
dma_regs_t dmachregs[4];

// Need multiple mode :
// Page Only (Read)
// Page + Other (Write Only or Read/Write)
uint8_t dev_dma_install()
{
 if (!dev_dma_active)   // Don't re enable if active
  {  
   PM_INFO("Install DMA (%x)\n",dev_dma_baseport);

   if (GetPortType(dev_dma_baseport)!=DEV_NULL)
     {
      PM_ERROR("Port already used (%d)\n",GetPortType(dev_dma_baseport));
      dev_dma_active=false;
      return 1;
     }

   SetPortType(dev_dma_baseport,DEV_DMA,2);  // 2 is needed for Port 0C
   SetPortType(dev_dma_pageport,DEV_DMA,1);  //
   dev_dma_emulate=false;
   dev_dma_active=true;
 }
  return 0;
}

void dev_dma_remove()
{
 if (dev_dma_active)   // Don't stop if not active
  {  
   dev_dma_active=false;  
   PM_INFO("Remove DMA (%X)\n",dev_dma_baseport);

   SetPortType(dev_dma_baseport,DEV_NULL,2);
  }  
}

// Return true if CMS is installed
bool dev_dma_installed()
{
  return (GetPortType(dev_dma_baseport)==DEV_DMA);
}

// Started in the Main Command Wait Loop
void dev_dma_update()
{
}

bool dev_dma_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
  if (dev_dma_emulate)
  {

    return false;
  } else return false;  // Nothing to read
}

void dev_dma_iow(uint32_t CTRL_AL8,uint8_t Data)
{
  uint8_t addr=CTRL_AL8&0xFF;
  uint8_t channel;

//  PM_INFO("DMA %x,%x",CTRL_AL8,Data);
 if (!dma_ignorechange)  // Ignore the change when the PicoMEM update the registers (in pm_pccmd.cpp)
 {
  switch(addr)
      {
      // Base address
        case 0x0:case 0x2:case 0x4:case 0x6:
          channel=addr>>1;
          dma_flipflop=!dma_flipflop;
          if (dma_flipflop)
            {
             dmachregs[channel].baseaddr=(uint16_t)((dmachregs[channel].baseaddr&0xff00)|Data);
            }
            else
            {
             dmachregs[channel].baseaddr=(uint16_t)((dmachregs[channel].baseaddr&0x00ff)|(Data << 8));
             dmachregs[channel].changed=true;  
            }
          break;
      // Transfer count
	    case 0x1:case 0x3:case 0x5:case 0x7:
          channel=addr>>1;
          dma_flipflop=!dma_flipflop;
          if (dma_flipflop)
            {
             dmachregs[channel].basecnt=(uint16_t)((dmachregs[channel].basecnt&0xff00)|Data);
            }
            else
            {
             dmachregs[channel].basecnt=(uint16_t)((dmachregs[channel].basecnt&0x00ff)|(Data << 8));
             dmachregs[channel].changed=true;              
            }
          break;
		case 0x8:   // Command register
		     break;
    case 0x0A:	// mask register (1 when stopped, 0 when running) ?
            dmachregs[Data&0x03].mask=(Data & 0x4)>0;
         break;         
    case 0x0B:	// mode register
            dmachregs[Data&0x03].autoinit=(Data & 0x10)>0;
         break;         
	  case 0xc:	// Clear flip flop
		        dma_flipflop=false;
		     break;
		case 0x81:  // Channel 2 page
            dmachregs[2].page=Data;
            dmachregs[2].changed=true;            
            break;
		case 0x82:  // Channel 3 page
            dmachregs[3].page=Data;
            dmachregs[3].changed=true;            
            break;
		case 0x83:  // Channel 1 page
            dmachregs[1].page=Data;
            dmachregs[1].changed=true;
            break;
		case 0x87:  // Channel 0 page
            dmachregs[0].page=Data;
            dmachregs[0].changed=true;
            break;
      }
 }
}
//#endif