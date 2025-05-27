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

/* dev_picomem.cpp : PicoMEM I/O Code
  Default port is "2A0-2A3"
 */ 

#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "pico/multicore.h"

#include "dev_picomem_io.h"

volatile uint8_t PM_TestRead=0;
uint8_t PM_CMDState=0;  //Command State Machine State (0 : Unlocked; 1 : Intermediate : 2: locked;)

volatile uint16_t PM_TR_CNT_L=0;
volatile uint16_t PM_TR_CNT_H=0;
bool PM_RE_ISREAD;
bool PM_TR_IS16B;
uint8_t * PM_TR_PTR;                 // Pointer to the table used by the IO Read / Write command


uint8_t dev_pmio_install()
{
  return 0;
}

void dev_pmio_remove(uint16_t BasePort)
{
}

// Started in the Main Command Wait Loop
void dev_pmio_update()
{
}

void test_pmio_transfer()
{
  uint8_t buff[]={0,1,2,3,5,6,7,8};
  
  dev_pmio_do_rw(buff,8,true,false);

}

// 8Bit IOW Ok
// dev_pmio_do_rw : Copy a buffer from PicoMEM to PC via I/O ports
// This is executed during a commad, to transfer data.
void dev_pmio_do_rw(uint8_t * buffer,uint16_t size,bool isread, bool do16bit)
{

// Buffer pointer and size

PM_INFO("Transfer %d : ",size);


// Transfer type
PM_RE_ISREAD=isread;
PM_TR_IS16B=do16bit;

PM_TR_PTR=buffer;
PM_CmdDataL=(uint8_t) (size&0xFF);  // Tell the PC the transfer length
PM_CmdDataH=(uint8_t) (size>>8);
if (do16bit) 
   {
    PM_TR_CNT_L=size/2;
    PM_TR_CNT_H=PM_TR_CNT_L;
    PM_Status=(isread ? STAT_DATA_READ_W:STAT_DATA_WRITE_W);
   }
 else
   {
    PM_TR_CNT_L=size;
    PM_Status=(isread ? STAT_DATA_READ:STAT_DATA_WRITE);   
   }

 do {
     if (PM_CmdReset) continue;
    } while ((PM_TR_CNT_L!=0)||(PM_TR_CNT_H!=0));

PM_INFO("Transfer End\n");

}

bool dev_pmio_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
 switch(CTRL_AL8 & 0x07)  //CTRL_AL8 Remains as a Register even here
  {
    case PORT_PM_CMD_STATUS:   // PicoMEM : Read Status           
                 *Data=PM_Status;
                 return true;
    case PORT_PM_CMD_DATAL:    // PicoMEM : Read the Command L Data /Return             
                 *Data=PM_CmdDataL;
                 return true;
    case PORT_PM_CMD_DATAH:    // PicoMEM : Read the Command H Data /Return          
                 *Data=PM_CmdDataH;         
//                 printf("r%x",ISA_Data);
                 return true;
    case PORT_TEST:            // Test port, Return Test value +1            
                 *Data=PM_TestRead++; 
                // printf("t");
                 return true;
    case PORT_TR_L:
                 if ((PM_TR_CNT_L!=0)&&(PM_RE_ISREAD)) // counter of 
                   {  // Read from a table in memory, index PM_CmdDataH+PM_CmdDataL is auto increment
                    PM_TR_CNT_L--;
                    *Data=*PM_TR_PTR;
                    PM_TR_PTR++;
                    //printf("CR %d, %X",PM_TR_CNT_L,*Data);
                    return true;
                   }
                 break;

   } 
  *Data=0xFF;
  return false;  // Nothing read
}

void dev_pmio_iow(uint32_t CTRL_AL8,uint8_t Data)
{
  //printf("Wr %x, %X",CTRL_AL8,Data);
  switch(CTRL_AL8 & 0x07)  //CTRL_AL8 Remains as a Register even here        
 //          switch(ISA_Addr & 0x07) 
    {
      case PORT_PM_CMD_STATUS:  // PicoMEM : Write Command
			       switch (PM_CMDState)
				       { 
					      case 0:
					        if (Data==CMD_LockPort) PM_CMDState=2;
                        if ((PM_Status==STAT_READY)||((Data)==0))  // Ready to receive a command or Command Reset
                           {
                            if (Data==0) PM_CmdReset=true;   // Force commands reset (Then add RESET Command in command queue)
                            PM_Command=(uint8_t) Data;
                            PM_Status=STAT_CMDINPROGRESS;    // Core 1 will execute the command (I hope, at one time or another...)
                            multicore_fifo_push_blocking(0); // Hope it never block
                           }
							    break;
					    case 1:if (Data==0x94)	PM_CMDState=0;		// Unlock if PM_CMDState=1 and Data=0x94, otherwise Lock again
					  		        else PM_CMDState=2;
					           break;
					    case 2:if (Data==0x37) PM_CMDState=1;    // State 2 is Locked
                     break;
					    }
                 break;
      case PORT_PM_CMD_DATAL:    // PicoMEM : Write the Command Data L ! Command Data must be written before
                 PM_CmdDataL=Data;
                 break;
      case PORT_PM_CMD_DATAH:    // PicoMEM : Write the Command Data H ! Command Data must be written before
//                 printf("w%x",Data);
                 PM_CmdDataH=Data;               
                 break;                 
      case PORT_TEST:              // Test and 8Bit Data port
  //               gpio_put(PIN_IRQ,1);  // * Send an IRQ, to Debug
                 break;
      case PORT_TR_L:
                 if ((PM_TR_CNT_L!=0)&&(!PM_RE_ISREAD)) // counter of 
                   {  // Read from a table in memory, index PM_CmdDataH+PM_CmdDataL is auto increment
                    PM_TR_CNT_L--;
                    *PM_TR_PTR=Data;
                    PM_TR_PTR++;
                    PM_INFO("CW %d, %X",PM_TR_CNT_L,Data);
                   }
                 break;  
        case 5: if (Data==8) DBG_ON_INT_SB();
                if (Data==3) DBG_ON_INT_DMA();
            //    PM_INFO("b%x,%d",Data,BV_IRQ_Cnt);       // Debug display for IRQ Begin/end
                break;
        case 6: //PM_INFO("e%x",Data);
                if (Data==8) DBG_OFF_INT_SB();
                if (Data==3) DBG_OFF_INT_DMA();
                break;
      default: break;
    } // PicoMEM Register Switch
}