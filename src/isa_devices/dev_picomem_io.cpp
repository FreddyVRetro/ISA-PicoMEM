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
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "pico/multicore.h"

#include "dev_picomem_io.h"

volatile uint8_t PM_TestRead=0;
uint8_t PM_CMDState=0;  //Command State Machine State (0 : Unlocked; 1 : Intermediate : 2: locked;)

volatile bool PM_DoIOR_B=false;
volatile bool PM_DoIOR_W=false;
volatile bool PM_DoIOW_B=false;
volatile bool PM_DoIOW_W=false;
uint8_t * PM_IORW_Base;   // Pointer to the table used by the IO Read / Write command
volatile uint16_t PM_IORW_Size;      // Max size of the IO Read / Write command


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

bool dev_pmio_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
 switch(CTRL_AL8 & 0x07)  //CTRL_AL8 Remains as a Register even here
  {
    case PORT_PM_CMD_STATUS:   // PicoMEM : Read Status           
                 *Data=PM_Status;
                 break;
    case PORT_PM_CMD_DATAL:    // PicoMEM : Read the Command L Data /Return             
                 *Data=PM_CmdDataL;
                 break;
    case PORT_PM_CMD_DATAH:    // PicoMEM : Read the Command H Data /Return          
                 *Data=PM_CmdDataH;         
//                 printf("r%x",ISA_Data);
                 break;
    case PORT_TEST:            // Test port, Return Test value +1            
                 if (PM_DoIOR_B)
                   {  // Read from a table in memory, index PM_CmdDataH+PM_CmdDataL is auto increment
                    if ((uint16_t)PM_CmdDataL<PM_IORW_Size)
                       { 
                         *Data=PM_IORW_Base[(uint16_t)PM_CmdDataL];
                         (uint16_t)PM_CmdDataL++;
                       }
                       else *Data=0xFF;
                   } else *Data=PM_TestRead++;                 
                 break;
   } 
 return true;
}

void dev_pmio_iow(uint32_t CTRL_AL8,uint8_t ISAIOW_Data)
{
          switch(CTRL_AL8 & 0x07)  //CTRL_AL8 Remains as a Register even here        
 //          switch(ISA_Addr & 0x07) 
            { 
            case PORT_PM_CMD_STATUS:  // PicoMEM : Write Command
			       switch (PM_CMDState)
				       { 
					      case 0:
					        if (ISAIOW_Data==CMD_LockPort) PM_CMDState=2;
                            if ((PM_Status==STAT_READY)||((ISAIOW_Data)==0))  // Ready to receive a command or Command Reset
                            {
                             PM_Command=(uint8_t) ISAIOW_Data;
                             PM_Status=STAT_CMDINPROGRESS;    // Core 1 will execute the command (I hope, at one time or another...)
                             multicore_fifo_push_blocking(0); // Hope it never block
                            }
							    break;
					    case 1:if (ISAIOW_Data==0x94)	PM_CMDState=0;		// Unlock if PM_CMDState=1 and ISAIOW_Data=0x94, otherwise Lock again
					  		        else PM_CMDState=2;
					           break;
					    case 2:if (ISAIOW_Data==0x37) PM_CMDState=1;    // State 2 is Locked
                     break;
					    }
                 break;
            case PORT_PM_CMD_DATAL:    // PicoMEM : Write the Command Data L ! Command Data must be written before
                 PM_CmdDataL=ISAIOW_Data;
                 break;
            case PORT_PM_CMD_DATAH:    // PicoMEM : Write the Command Data H ! Command Data must be written before
//                 printf("w%x",ISAIOW_Data);
                 PM_CmdDataH=ISAIOW_Data;               
                 break;                                                     
            case PORT_TEST:              // Test and 8Bit Data port
  //               gpio_put(PIN_IRQ,1);  // * Send an IRQ, to Debug
                 if (PM_DoIOR_W)
                   {  // Write to a table in memory, index PM_CmdDataH+PM_CmdDataL is auto increment
                    if ((uint16_t)PM_CmdDataL<PM_IORW_Size)
                       { 
                        PM_IORW_Base[(uint16_t)PM_CmdDataL]=ISAIOW_Data;
                        (uint16_t)PM_CmdDataL++; 
                       }

                   }
                 break;
            default: break;
            } // PicoMEM Register Switch
}