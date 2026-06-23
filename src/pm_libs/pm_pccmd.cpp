/* Copyright (C) 2023 Freddy VETELE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
/* ISA PicoMEM By Freddy VETELE	
 * pm_pccmd.cpp : API to send Command to the PC (BIOS) The BIOS command Slave mode must be enabled.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "pico/stdlib.h"

#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "isa_dma.h"
#include "pm_pccmd.h"

#define PCC_API_END          0
#define PCC_API_WAITCMDEND   1
#define PCC_API_WAITENDACK   2
#define PCC_API_WAITWAITCMD  3
#define PCC_API_WAITCMDSTART 4


uint32_t PCC_API_STATUS;      // What the API is waiting for ? (To track crash)
uint32_t PCC_CMD_IP;          // Command In Progress

// Send a command to the PC, need to check that the PC is ready to receive a command first
void Send_PC_CMD(uint8_t CommandNB)    // Send a Command, don't touch parameters and wait for its start
{  
//  PM_INFO("Send_PC_CMD: Sending Command %d, Current State %d\n",CommandNB,PCCR_PCSTATE);
   if (PCCR_PCSTATE==PCC_PCS_CMDEND) // Acknowledge the end of the previous command
        {
          PCCR_PMSTATE=PCC_PMS_CMDACK;
          PCC_API_STATUS=PCC_API_WAITENDACK;
          do {} while (PCCR_PCSTATE!=PCC_PCS_WAITCMD&&(PCCR_PCSTATE!=PCC_PCS_RESET)); // Wait for the PC to see the Acknowledge
        }
   if (PCCR_PCSTATE!=PCC_PCS_WAITCMD) {
       // PM_INFO("Send_PC_CMD: PC not waiting for a command, current state %d\n",PCCR_PCSTATE);
        PCC_API_STATUS=PCC_API_WAITWAITCMD;
        do {} while (PCCR_PCSTATE!=PCC_PCS_WAITCMD&&(PCCR_PCSTATE!=PCC_PCS_RESET)); // Wait for the PC to see the Acknowledge
        }
   if (PCCR_PCSTATE==PCC_PCS_RESET) return; // Can't send a command if the PC is in Reset State

   PCCR_CMD=CommandNB;             // Send the Command value
   PCCR_PMSTATE=PCC_PMS_CMDSENT;   // Inform the PC that a command is sent
   // Wait the Start of the command (PC no more in Wait CMD State)
   PCC_API_STATUS=PCC_API_WAITCMDSTART;
   do {} while ((PCCR_PCSTATE==PCC_PCS_WAITCMD)&&(PCCR_PCSTATE!=PCC_PCS_RESET)); 
}

// Wait for the PC BIOS to be ready to receive a command
bool PC_Wait_CMD_End()
{
  PCC_API_STATUS=PCC_API_WAITCMDEND;
  //PM_INFO("PC_Wait_CMD_End: Waiting for the end of the command, current state %d\n",PCCR_PCSTATE);
  if (PCCR_PCSTATE!=PCC_PCS_WAITCMD) 
   {
    do {} while ((PCCR_PCSTATE!=PCC_PCS_CMDEND)&&(PCCR_PCSTATE!=PCC_PCS_WAITCMD)&&PCC_PCS_WAITCMD&&(PCCR_PCSTATE!=PCC_PCS_RESET));
    if (PCCR_PCSTATE==PCC_PCS_RESET)  { PCC_API_STATUS=PCC_API_END; return false; } // Reset Status, no more command can be sent
   }
  return true;
}

// Wait for the PC BIOS to be ready to receive a command
// Timeout after the delay value (in uS)
bool PC_Wait_CMD_End_Timeout(uint32_t delay)
{
  uint64_t initial_time;
  if (PCCR_PCSTATE==PCC_PCS_WAITCMD) return true;
  if (PCCR_PCSTATE==PCC_PCS_CMDEND ) return true;
  if (PCCR_PCSTATE==PCC_PCS_RESET  ) return false; // Reset Status, no more command can be sent

  PCC_API_STATUS=PCC_API_WAITCMDEND;
  initial_time=time_us_64();
  do {
      if ((time_us_64()-initial_time)>delay) { PCC_API_STATUS=PCC_API_END; return false; }
     } while ((PCCR_PCSTATE!=PCC_PCS_CMDEND)&&(PCCR_PCSTATE!=PCC_PCS_WAITCMD)&&(PCCR_PCSTATE!=PCC_PCS_RESET));

  if (PCCR_PCSTATE==PCC_PCS_RESET)  { PCC_API_STATUS=PCC_API_END; return false; } // Reset Status, no more command can be sent
  return true;
}

// Wait that the BIOS commands finished
// Timeout after the delay value (in uS)
bool PC_Wait_End_Timeout(uint32_t delay)
{
  uint64_t initial_time;

  switch(PCCR_PCSTATE) {
    case PCC_PCS_NOTREADY:
        return true;  // No command in progress, no need to wait
    case PCC_PCS_WAITCMD:
         PM_ERROR("PC_Wait_End_Timeout: PC is still waiting for a command");
        return false; // PC is waiting for a command, can't continue
    case PCC_PCS_INPROGRESS:
         PCC_API_STATUS=PCC_API_WAITCMDEND;
         initial_time=time_us_64();
         do {
             if ((time_us_64()-initial_time)>delay) return false;
            } while (PCCR_PCSTATE==PCC_PCS_INPROGRESS); // Wait for the end of the "End" Command
         if (PCCR_PCSTATE==PCC_PCS_NOTREADY) return true;
             else if (PCCR_PCSTATE==PCC_RES_COMPLETED) {
               PM_INFO("PC_Wait_End_Timeout: Command was not PC_End (%d)",PCCR_CMD);
               return false; // Command was not PC_End, but completed
             }
        break; // Command in progress, continue to wait
    case PCC_PCS_RESET:
        return false; // Reset Status, no more command can be sent
    default:
          PM_ERROR("PC_Wait_End_Timeout: Unknown PC State %d",PCCR_PCSTATE);
        return false; // Unknown State, can't continue
  }
  return true; // Command ended
}

void PC_End()
{
  PC_Wait_CMD_End();

  Send_PC_CMD(PCC_EndCommand);
}

void PC_printstr(char *StrToSend)
{
  // Need to wait the end of the previous command
  PC_Wait_CMD_End();

  memcpy((char *)PM_DP_RAM+OFFS_PCCMDPARAM,StrToSend,strlen(StrToSend)+1);
  //PM_INFO("Send to PC: %s\n",(char *) &PCCR_Param);
  Send_PC_CMD(PCC_Printf);
  PCC_API_STATUS=PCC_API_END;
}

void PC_printf(const char *format, ...)
{
  va_list argptr;
  va_start(argptr, format);
  
  // Need to wait the end of the previous command
  PC_Wait_CMD_End();

  vsprintf((char *) &PCCR_Param, format, argptr);
  va_end(argptr);
  //PM_INFO("Send to PC: %s\n",(char *) &PCCR_Param);
  Send_PC_CMD(PCC_Printf);
  PC_Wait_CMD_End();
  PCC_API_STATUS=PCC_API_END;  
}

// Set the screen position (Text mode)
void PC_Setpos(uint8_t X, uint8_t Y)
{
  // Need to wait the end of the previous command
  PC_Wait_CMD_End();

  PM_DP_RAM[OFFS_PCCMDPARAM+1]=X;
  PM_DP_RAM[OFFS_PCCMDPARAM]=Y;
  Send_PC_CMD(PCC_SetPos);
  PC_Wait_CMD_End();
  PCC_API_STATUS=PCC_API_END;  
}

// Get the screen position (Text mode)
// Wait for completion
void PC_Getpos(uint8_t *X, uint8_t *Y)
{
  // Need to wait the end of the previous command
  PC_Wait_CMD_End();
  Send_PC_CMD(PCC_GetPos);
  PC_Wait_CMD_End();
  *X=PM_DP_RAM[OFFS_PCCMDPARAM+1];
  *Y=PM_DP_RAM[OFFS_PCCMDPARAM];
  PCC_API_STATUS=PCC_API_END;  
}

// Send the BIOS Interrupt using registers and return in registers
extern void PC_Int13()
{
  PC_Wait_CMD_End();
  Send_PC_CMD(PCC_IRQ13);
  PC_Wait_CMD_End();  
  PCC_API_STATUS=PCC_API_END;
}

bool PC_KeyPressed()
{
  // Need to wait the end of the previous command
  PC_Wait_CMD_End();
  Send_PC_CMD(PCC_KeyPressed);  
  PC_Wait_CMD_End();
  return PM_DP_RAM[OFFS_PCCMDPARAM];
  PCC_API_STATUS=PCC_API_END;  
}

// Return the Scan Code and Char
uint8_t PC_ReadKey(char *retchar)
{
  // Need to wait the end of the previous command
  PC_Wait_CMD_End();
  Send_PC_CMD(PCC_ReadKey);  
  PC_Wait_CMD_End();
  *retchar=PM_DP_RAM[OFFS_PCCMDPARAM+1];
  return PM_DP_RAM[OFFS_PCCMDPARAM];
  PCC_API_STATUS=PCC_API_END;  
}

// Return null string if cancelled
// Wait for completion
char * PC_ReadString(uint8_t x, uint8_t y, uint8_t Attrib, uint8_t charnb)
{
  // Need to wait the end of the previous command
  PC_Wait_CMD_End();

  PM_DP_RAM[OFFS_PCCMDPARAM+1]=x;
  PM_DP_RAM[OFFS_PCCMDPARAM]=y;
  PM_DP_RAM[OFFS_PCCMDPARAM+2]=Attrib;
  PM_DP_RAM[OFFS_PCCMDPARAM+3]=charnb;   
  Send_PC_CMD(PCC_ReadString);
  PC_Wait_CMD_End();
  PCC_API_STATUS=PCC_API_END;  
  return (char *) &PM_DP_RAM[OFFS_PCCMDPARAM];
}

// Read the DMA page, offset and size
// channel must be 1 or 3 only !
// Wait for completion
void PC_GetDMARegs(uint8_t channel, uint8_t *Page, uint16_t *Offset, uint16_t *Size)
{
  // Need to wait the end of the previous command
  PC_Wait_CMD_End();

  PM_DP_RAM[OFFS_PCCMDPARAM]=channel;

  Send_PC_CMD(PCC_DMA_READ);
  PC_Wait_CMD_End();

  *Page=PM_DP_RAM[OFFS_PCCMDPARAM+1];
  *Offset=PM_DP_RAM[OFFS_PCCMDPARAM+2]+(PM_DP_RAM[OFFS_PCCMDPARAM+3]<<8);  // Can be exchanged to go faster
  *Size=PM_DP_RAM[OFFS_PCCMDPARAM+4]+(PM_DP_RAM[OFFS_PCCMDPARAM+5]<<8);
  PCC_API_STATUS=PCC_API_END;  
}

// Update the DMA offset and remaining size (No need to update the page)
// channel must be 1 or 3 only !
// Wait for completion
void PC_SetDMARegs(uint8_t channel, uint16_t Offset, uint16_t Size, uint8_t ignore)
{
  // Need to wait the end of the previous command
  PC_Wait_CMD_End();

  PM_DP_RAM[OFFS_PCCMDPARAM]=channel;
  PM_DP_RAM[OFFS_PCCMDPARAM+2]=(uint8_t)Offset&0xFF;
  PM_DP_RAM[OFFS_PCCMDPARAM+3]=(uint8_t)(Offset>>8);
  PM_DP_RAM[OFFS_PCCMDPARAM+4]=(uint8_t)Size&0xFF;
  PM_DP_RAM[OFFS_PCCMDPARAM+5]=(uint8_t)(Size>>8);
  BV_DMANoUpdate=ignore;  // Inform the Virtual DMA that the PicoMEM is sending the update (then, ignore it)
  Send_PC_CMD(PCC_DMA_WRITE);
  PC_Wait_CMD_End();
  BV_DMANoUpdate=0;
  PCC_API_STATUS=PCC_API_END;  
}

// Doesn't Wait for completion
void PC_Do_MemCopyB(uint32_t Src, uint32_t Dest,uint16_t Len)
{
  if (Len!=0)
  {
  PC_Wait_CMD_End();
  // Source Offset
  PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)Src&0xF;
  PM_DP_RAM[OFFS_PCCMDPARAM+1]=0;
  // Source Segment
  PM_DP_RAM[OFFS_PCCMDPARAM+2]=(uint8_t)(Src>>4); 
  PM_DP_RAM[OFFS_PCCMDPARAM+3]=(uint8_t)(Src>>12);
  // Destination Offset
  PM_DP_RAM[OFFS_PCCMDPARAM+4]=(uint8_t)Dest&0xF;
  PM_DP_RAM[OFFS_PCCMDPARAM+5]=0;
  // Destination Segment
  PM_DP_RAM[OFFS_PCCMDPARAM+6]=(uint8_t)(Dest>>4);
  PM_DP_RAM[OFFS_PCCMDPARAM+7]=(uint8_t)(Dest>>12);  
  // Length
  PM_DP_RAM[OFFS_PCCMDPARAM+8]=(uint8_t)Len;
  PM_DP_RAM[OFFS_PCCMDPARAM+9]=(uint8_t)(Len>>8);   
  Send_PC_CMD(PCC_MemCopyB);
  }
  PCC_API_STATUS=PCC_API_END;  
}

// Doesn't Wait for completion
void PC_Do_MemCopyW(uint32_t Src, uint32_t Dest,uint16_t Len)
{
  if (Len!=0)
  {
   PC_Wait_CMD_End();
   // Source Offset
   PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)Src&0xF;
   PM_DP_RAM[OFFS_PCCMDPARAM+1]=0;
   // Source Segment
   PM_DP_RAM[OFFS_PCCMDPARAM+2]=(uint8_t)(Src>>4);
   PM_DP_RAM[OFFS_PCCMDPARAM+3]=(uint8_t)(Src>>12);
   // Destination Offset
   PM_DP_RAM[OFFS_PCCMDPARAM+4]=(uint8_t)Dest&0xF;
   PM_DP_RAM[OFFS_PCCMDPARAM+5]=0;
   // Destination Segment
   PM_DP_RAM[OFFS_PCCMDPARAM+6]=(uint8_t)(Dest>>4);
   PM_DP_RAM[OFFS_PCCMDPARAM+7]=(uint8_t)(Dest>>12);  
   // Length
   PM_DP_RAM[OFFS_PCCMDPARAM+8]=(uint8_t)Len;
   PM_DP_RAM[OFFS_PCCMDPARAM+9]=(uint8_t)(Len>>8);   
   
   Send_PC_CMD(PCC_MemCopyW);
  }
  PCC_API_STATUS=PCC_API_END;  
}

// Doesn't Wait for completion
void PC_Do_MemCopyW_Odd(uint32_t Src, uint32_t Dest,uint16_t Len)
{
  if (Len!=0)
  {
   PC_Wait_CMD_End();
   // Source Offset
   PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)Src&0xF;
   PM_DP_RAM[OFFS_PCCMDPARAM+1]=0;
   // Source Segment
   PM_DP_RAM[OFFS_PCCMDPARAM+2]=(uint8_t)(Src>>4);
   PM_DP_RAM[OFFS_PCCMDPARAM+3]=(uint8_t)(Src>>12);
   // Destination Offset
   PM_DP_RAM[OFFS_PCCMDPARAM+4]=(uint8_t)Dest&0xF;
   PM_DP_RAM[OFFS_PCCMDPARAM+5]=0;
   // Destination Segment
   PM_DP_RAM[OFFS_PCCMDPARAM+6]=(uint8_t)(Dest>>4);
   PM_DP_RAM[OFFS_PCCMDPARAM+7]=(uint8_t)(Dest>>12);  
   // Length
   PM_DP_RAM[OFFS_PCCMDPARAM+8]=(uint8_t)Len;
   PM_DP_RAM[OFFS_PCCMDPARAM+9]=(uint8_t)(Len>>8);   
   
   Send_PC_CMD(PCC_MemCopyW_Odd);
  }
  PCC_API_STATUS=PCC_API_END;  
}

// Copy Len Bytes from Src to Dest Address  (PC RAM @)
// Perform the fastest copy method
extern void PC_MemCopy(uint32_t Src, uint32_t Dest, uint16_t Len)
{
// Perform the fastest copy method
    if (Len<=3) PC_Do_MemCopyB(Src,Dest,Len);
     else if (Len & 0xFFFE) PC_Do_MemCopyW_Odd(Src,Dest,Len>>1);
      else PC_Do_MemCopyW(Src,Dest,Len>>1);
  PCC_API_STATUS=PCC_API_END;      
}

// Copy 512 Bytes from Src to Dest Address  (PC RAM @)
// Perform a timeout check (20ms then 50ms)
bool PC_MemCopyW_512b(uint32_t Src, uint32_t Dest)
{
 
  if (!PC_Wait_CMD_End_Disk()) return false;

  // Source and destination are 32Bit address (not Segment/Offset)  
  // If the previous command parameter has not been read, this may fail !
  // Source Offset
  PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)Src&0xF;
  PM_DP_RAM[OFFS_PCCMDPARAM+1]=0;
  // Source Segment
  PM_DP_RAM[OFFS_PCCMDPARAM+2]=(uint8_t)(Src>>4);
  PM_DP_RAM[OFFS_PCCMDPARAM+3]=(uint8_t)(Src>>12);
  // Destination Offset
  PM_DP_RAM[OFFS_PCCMDPARAM+4]=(uint8_t)Dest&0xF;
  PM_DP_RAM[OFFS_PCCMDPARAM+5]=0;
  // Destination Segment
  PM_DP_RAM[OFFS_PCCMDPARAM+6]=(uint8_t)(Dest>>4);
  PM_DP_RAM[OFFS_PCCMDPARAM+7]=(uint8_t)(Dest>>12);
  Send_PC_CMD(PCC_MemCopyW_512b);
  PCC_API_STATUS=PCC_API_END;
  return true;
}

uint8_t PC_IN8(uint16_t port)
{
  PC_Wait_CMD_End();  
  PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)port&0xFF;
  PM_DP_RAM[OFFS_PCCMDPARAM+1]=(uint8_t)(port>>8);
  Send_PC_CMD(PCC_IN8);
  PC_Wait_CMD_End();
  return PM_DP_RAM[OFFS_PCCMDPARAM];
  PCC_API_STATUS=PCC_API_END;  
}

uint16_t PC_IN16(uint16_t port)
{
  PC_Wait_CMD_End();
  PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)port&0xFF;
  PM_DP_RAM[OFFS_PCCMDPARAM+1]=(uint8_t)(port>>8);
  Send_PC_CMD(PCC_IN16);
  PC_Wait_CMD_End();
  return (PM_DP_RAM[OFFS_PCCMDPARAM]+(PM_DP_RAM[OFFS_PCCMDPARAM+1]<<8));
  PCC_API_STATUS=PCC_API_END;  
}

void PC_OUT8(uint16_t port, uint8_t data)
{
  PC_Wait_CMD_End();
  PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)port&0xFF;
  PM_DP_RAM[OFFS_PCCMDPARAM+1]=(uint8_t)(port>>8);
  PM_DP_RAM[OFFS_PCCMDPARAM+2]=data;
  Send_PC_CMD(PCC_OUT8);
  PCC_API_STATUS=PCC_API_END;  
}

void PC_OUT16(uint16_t port, uint16_t data)
{
  PC_Wait_CMD_End();  
  PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)port&0xFF;
  PM_DP_RAM[OFFS_PCCMDPARAM+1]=(uint8_t)(port>>8);
  PM_DP_RAM[OFFS_PCCMDPARAM+2]=(uint8_t)data&0xFF;
  PM_DP_RAM[OFFS_PCCMDPARAM+3]=(uint8_t)(data>>8);
  Send_PC_CMD(PCC_OUT16);
  PCC_API_STATUS=PCC_API_END;  
}

void PC_MW8(uint32_t addr, uint8_t data)
{
  PC_Wait_CMD_End();
  // Offset
  PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)addr&0xF;
  PM_DP_RAM[OFFS_PCCMDPARAM+1]=0; 
  // Segment
  PM_DP_RAM[OFFS_PCCMDPARAM+2]=(uint8_t)(addr>>4);
  PM_DP_RAM[OFFS_PCCMDPARAM+3]=(uint8_t)(addr>>12);
  // Data
  PM_DP_RAM[OFFS_PCCMDPARAM+4]=data;
  Send_PC_CMD(PCC_MEMW8);
  PCC_API_STATUS=PCC_API_END;  
}

void PC_MW16(uint32_t addr, uint16_t data)
{
  PC_Wait_CMD_End();  
  // Offset
  PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)addr&0xF;
  PM_DP_RAM[OFFS_PCCMDPARAM+1]=0; 
  // Segment
  PM_DP_RAM[OFFS_PCCMDPARAM+2]=(uint8_t)(addr>>4);
  PM_DP_RAM[OFFS_PCCMDPARAM+3]=(uint8_t)(addr>>12);
  // Data
  PM_DP_RAM[OFFS_PCCMDPARAM+4]=(uint8_t)data&0xFF;
  PM_DP_RAM[OFFS_PCCMDPARAM+5]=(uint8_t)data>>8;
  Send_PC_CMD(PCC_MEMW16);
  PCC_API_STATUS=PCC_API_END;  
}

uint16_t PC_MR16(uint32_t addr)
{
  PC_Wait_CMD_End();  // Need to wait the end of the previous command
  // Offset
  PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)addr&0xF;
  PM_DP_RAM[OFFS_PCCMDPARAM+1]=0; 
  // Segment
  PM_DP_RAM[OFFS_PCCMDPARAM+2]=(uint8_t)(addr>>4);
  PM_DP_RAM[OFFS_PCCMDPARAM+3]=(uint8_t)(addr>>12);
  Send_PC_CMD(PCC_MEMR16);
  PC_Wait_CMD_End();
  PCC_API_STATUS=PCC_API_END;  
  return (PM_DP_RAM[OFFS_PCCMDPARAM]+(PM_DP_RAM[OFFS_PCCMDPARAM+1]<<8));
}

// Write the Read a byte (For RAM Test)
uint8_t PC_MWR8(uint32_t addr, uint8_t data)
{
  PC_Wait_CMD_End();  // Need to wait the end of the previous command
  // Offset
  PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)addr&0xF;
  PM_DP_RAM[OFFS_PCCMDPARAM+1]=0; 
  // Segment
  PM_DP_RAM[OFFS_PCCMDPARAM+2]=(uint8_t)(addr>>4);
  PM_DP_RAM[OFFS_PCCMDPARAM+3]=(uint8_t)(addr>>12);
  // Data
  PM_DP_RAM[OFFS_PCCMDPARAM+4]=data;
  Send_PC_CMD(PCC_MEMWR8);
  PC_Wait_CMD_End();  
  PCC_API_STATUS=PCC_API_END;  
  return PM_DP_RAM[OFFS_PCCMDPARAM];  
}

// Write the Read a word (For RAM Test)
uint16_t PC_MWR16(uint32_t addr, uint16_t data)
{
  PC_Wait_CMD_End();  // Need to wait the end of the previous command  
  // Offset
  PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)addr&0xF;
  PM_DP_RAM[OFFS_PCCMDPARAM+1]=0; 
  // Segment
  PM_DP_RAM[OFFS_PCCMDPARAM+2]=(uint8_t)(addr>>4);
  PM_DP_RAM[OFFS_PCCMDPARAM+3]=(uint8_t)(addr>>12);
  // Data
  PM_DP_RAM[OFFS_PCCMDPARAM+4]=(uint8_t)data&0xFF;
  PM_DP_RAM[OFFS_PCCMDPARAM+5]=(uint8_t)data>>8;
  Send_PC_CMD(PCC_MEMWR16);
  PC_Wait_CMD_End();    
  PCC_API_STATUS=PCC_API_END;
  return (PM_DP_RAM[OFFS_PCCMDPARAM]+(PM_DP_RAM[OFFS_PCCMDPARAM+1]<<8));
}

uint8_t PC_Checksum(uint16_t segment, uint16_t size)
{
  PC_Wait_CMD_End();  // Need to wait the end of the previous command

  PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)segment&0xFF;
  PM_DP_RAM[OFFS_PCCMDPARAM+1]=(uint8_t)(segment>>8);
  PM_DP_RAM[OFFS_PCCMDPARAM+2]=(uint8_t)size&0xFF;
  PM_DP_RAM[OFFS_PCCMDPARAM+3]=(uint8_t)(size>>8);
  Send_PC_CMD(PCC_Checksum);
  PC_Wait_CMD_End();
  PCC_API_STATUS=PCC_API_END;  
  return (PM_DP_RAM[OFFS_PCCMDPARAM]);
}

// Set a block of 16Bit Memory to a value (Len is the total size in word)
void PC_memsetW(uint32_t Src, uint32_t Dest,uint16_t Len, uint16_t data)
{
  if (Len!=0)
  {
   // If the previous command parameter has not been read, this may fail !
   // Source Offset
   PM_DP_RAM[OFFS_PCCMDPARAM]=(uint8_t)Src&0xF;
   PM_DP_RAM[OFFS_PCCMDPARAM+1]=0;
   // Source Segment
   PM_DP_RAM[OFFS_PCCMDPARAM+2]=(uint8_t)(Src>>4);
   PM_DP_RAM[OFFS_PCCMDPARAM+3]=(uint8_t)(Src>>12);
   // Destination Offset
   PM_DP_RAM[OFFS_PCCMDPARAM+4]=(uint8_t)Dest&0xF;
   PM_DP_RAM[OFFS_PCCMDPARAM+5]=0;
   // Destination Segment
   PM_DP_RAM[OFFS_PCCMDPARAM+6]=(uint8_t)(Dest>>4);
   PM_DP_RAM[OFFS_PCCMDPARAM+7]=(uint8_t)(Dest>>12);  
   // Length
   PM_DP_RAM[OFFS_PCCMDPARAM+8]=(uint8_t)Len;
   PM_DP_RAM[OFFS_PCCMDPARAM+9]=(uint8_t)(Len>>8);   
   // Data
   PM_DP_RAM[OFFS_PCCMDPARAM+10]=(uint8_t)data&0xFF;
   PM_DP_RAM[OFFS_PCCMDPARAM+11]=(uint8_t)data>>8;
  Send_PC_CMD(PCC_SetRAMBlock16);
  }
  PCC_API_STATUS=PCC_API_END;  
}

/*
uint32_t PC_CP_PCAddr;
uint32_t PC_CP_total;
uint32_t PC_CP_done;
uint8_t  PC_CP_MemType;   // Type of RAM of the next buffer to send
bool     PC_To_Pico;      // True PC to Pico False Pico to PC

uint32_t PC_DB_Offset;

// Copy Data from the PC Memory to the PicoMEM, via PC Commands
// Source, Size, Block size
void memcp_PC_to_PM(uint32_t Src,uint32_t Size)
{
  PC_DB_Offset=0;  
  PC_CP_done=0;
  PC_CP_PCAddr=Src;
  PC_CP_total=Size;
  PC_To_Pico=true;
}

void memcp_PM_to_PC(uint32_t Dest,uint32_t Size)
{
  PC_DB_Offset=0;
  PC_CP_done=0;
  PC_CP_PCAddr=Dest;
  PC_CP_total=Size;
  PC_To_Pico=false;
}

// Get a pointer to the picoMEM adress to use for the Data transfer
uint8_t *memcp_GetFreeBuffer()
{
uint8_t mt_beg;
uint8_t mt_end;

// Current 512b Buffer Memory type (Begining and End)
mt_beg = GetMEMType(PC_CP_PCAddr);
mt_end = GetMEMType(PC_CP_PCAddr+512-1);
if (mt_beg!=mt_end)
 { 
  PM_ERROR("! Diff Mem Type %d %d > ",mt_beg,mt_end);
  if ((mt_beg<=MEM_RAM) && (mt_end<=MEM_RAM))
     {						      // MEM Begining and End not in PSRAM
      mt_beg=MEM_NULL;  // Force Standard BIOS Copy
     }
    else
     {					      // MEM Begining or End in PSRAM/EMS
      mt_beg=0xFF;    // Force Slow MEM Copy with uSD Disable
     }  
  PM_INFO("%d > ",mt_beg);
 }
#ifdef PIMORONI_PICO_PLUS2_RP2350
if (mt_beg!=MEM_RAM) mt_beg=MEM_NULL;  // Can go back to use Copy via the CPU
#endif
PC_CP_MemType=mt_beg;

switch(PC_CP_MemType)
 {
  case MEM_NULL:  // ** Disk Read to the PC Ram (Disk Buffer)
   #if PM_PRINTF
     putchar('m');
   #endif
     return &PM_PTR_DISKBUFFER[PC_DB_Offset];   // PC BIOS RAM address (Pico SRAM)
   break;

  case MEM_RAM:  // ** Disk Read to the PC RAM emulated in the Pico SRAM
   #if PM_PRINTF
     putchar('s');
   #endif
     return &PM_Memory[(PC_CP_PCAddr - RAM_InitialAddress)];  // Pico SRAM address
    break;

  case MEM_PSRAM:   // ** Disk Read to the PC RAM emulated in the Pico PSRAM
  case MEM_EMS:     // ** Disk Read to the PC EMS emulated in the Pico PSRAM
   #if USE_SPI_PSRAM
   #if PM_PRINTF
    putchar('p');
   #endif
    // Use also the PC BIOS RAM for Data Transfer
    return &PM_PTR_DISKBUFFER[PC_DB_Offset];   // PC BIOS RAM address (Pico SRAM)
   #endif
    break;
 }
}

// Send the full buffer (Previously taken with GetFreeBuffer) to the PC
memcp_SendFullBuffer()
{

  switch(PC_CP_MemType)
  {
   case MEM_NULL:  // ** Disk Read to the PC Ram (Disk Buffer)
      // The PC Will copy the data to the destination Address, the Pico can fill the next buffer
      PC_Wait_CMD_End();
      PC_MemCopyW_512b(PC_DB_Start+PC_DB_Offset,PCBuffer_Start);
      break;
 
   case MEM_RAM:  // ** Disk Read to the PC RAM emulated in the Pico SRAM
      // Nothing to do, the Data is already sent to the Pc (Emulated RAM)
      break;
 
   case MEM_PSRAM:   // ** Disk Read to the PC RAM emulated in the Pico PSRAM
   case MEM_EMS:     // ** Disk Read to the PC EMS emulated in the Pico PSRAM
 
      break;
  }

 BOffset = (BOffset==0) ? 512 : 0;
 PCBuffer_Start+=512;
}

memcp_ReceiveBuffer()
{

}


switch(mt_beg)
{
case MEM_NULL:  // ** Disk Read to the PC Ram (Disk Buffer)
#if PM_PRINTF
  putchar('m');
#endif
  last_status = imageDiskList[drivenum]->Read_AbsoluteSector(sect_lba+sn, &PM_PTR_DISKBUFFER[BOffset]);	
  if ((last_status != 0x00) || (killRead)) 
    {				
     PM_ERROR("HDD Read Error");				 
     killRead = false;
     reg_ah = last_status;  // 4 if CF not accessible
     reg_SetCF();
     return CBRET_NONE;
    }	
        // ** Ask the BIOS to Copy the data to the application buffer (in Background)
   PC_Wait_CMD_End();
   PC_MemCopyW_512b(PC_DB_Start+BOffset,PCBuffer_Start);
      if (PCCR_PCSTATE==PCC_PCS_RESET) return CBRET_NONE;		// Quit if Reset asked
 break;
case MEM_RAM:  // ** Disk Read to the PC RAM emulated in the Pico SRAM
#if PM_PRINTF
  putchar('s');
#endif			
  last_status = imageDiskList[drivenum]->Read_AbsoluteSector(sect_lba+sn, &PM_Memory[(PCBuffer_Start - RAM_InitialAddress)]);			
  if ((last_status != 0x00) || (killRead)) 
    {		
     PM_ERROR("HDD Read Error");				 
     killRead = false;
     reg_ah = last_status;  // 4 if CF not accessible
     reg_SetCF();
     return CBRET_NONE;
    }
   break;
 case MEM_PSRAM:   // ** Disk Read to the PC RAM emulated in the Pico PSRAM
 case MEM_EMS:     // ** Disk Read to the PC EMS emulated in the Pico PSRAM
#if USE_SPI_PSRAM
#if PM_PRINTF
  putchar('p');
#endif				
//                dev_audiomix_pause();     // Need to pause audio during uSD Access  
  last_status = imageDiskList[drivenum]->Read_AbsoluteSector(sect_lba+sn, &PM_PTR_DISKBUFFER[BOffset]);
  if ((last_status != 0x00) || (killRead)) 
    {
     PM_ERROR("HDD Read Error");				 
     killRead = false;
     reg_ah = last_status;  // 4 if CF not accessible
     reg_SetCF();
     return CBRET_NONE;
    }

// Copy to PSRAM
   PM_PICO_EnablePSRAM();
        // Compute the address to use in PSRAM
   uint32_t PSRAM_Addr;  			
   if (mt_beg==MEM_EMS) PSRAM_Addr=EMS_Base[(PCBuffer_Start>>14)&0x03]+(PCBuffer_Start & 0x3FFF);	
       else PSRAM_Addr=PCBuffer_Start;
// Copy the data to PSRAM
   if (PSRAM_Addr&0x03)
     {  // Not 32Bit aligned, write in 8Bit to avoid problem
 
#if PM_PRINTF
      putchar('8');
#endif				 
      for (int i=0;i<512;i++)	// ** Copy the Disk Data to the PSRAM  (Need to find a faster way)
       { 
        psram_write8(&psram_spi,PSRAM_Addr+i,PM_PTR_DISKBUFFER[BOffset+i]);
       }
     }
   else
     {  // If 32Bit aligned, copy in 32Bit
     for (int i=0;i<128;i++)	// ** Copy the Disk Data to the PSRAM  (Need to find a faster way)
        { 
         uint32_t tmp=((uint32_t)PM_PTR_DISKBUFFER[BOffset+(i<<2)])+((uint32_t)PM_PTR_DISKBUFFER[BOffset+(i<<2)+1]<<8)+((uint32_t)PM_PTR_DISKBUFFER[BOffset+(i<<2)+2]<<16)+((uint32_t)PM_PTR_DISKBUFFER[BOffset+(i<<2)+3]<<24);
         psram_write32(&psram_spi,PSRAM_Addr+(i<<2),tmp);
        }
      }

PM_EnableSD();
//                dev_audiomix_resume();				
   break;
#endif			   
case 0xFF:  // "Slow" case : Copy via the PC CPU with PSRAM re enabled (when mixed RAM)
#if PM_PRINTF			 
putchar('c');
#endif				
//                dev_audiomix_pause();     // Need to pause audio during uSD Access  
  last_status = imageDiskList[drivenum]->Read_AbsoluteSector(sect_lba+sn, &PM_PTR_DISKBUFFER[BOffset]);
//                dev_audiomix_resume();
  if ((last_status != 0x00) || (killRead)) 
    {
    PM_ERROR("HDD Read Error");
   killRead = false;
   reg_ah = last_status;  // 4 if CF not accessible
   reg_SetCF();
   return CBRET_NONE;
    }
PM_PICO_EnablePSRAM();
    PC_Wait_CMD_End();
PC_MemCopyW_512b(PC_DB_Start+BOffset,PCBuffer_Start);
    PC_Wait_CMD_End();
        if (PCCR_PCSTATE==PCC_PCS_RESET) return CBRET_NONE;						
PM_EnableSD();				
 break;
} //End switch(mt_beg)

 BOffset = (BOffset==0) ? 512 : 0;
 PCBuffer_Start+=512;
PCBuffer_End+=512;
#if PM_PRINTF
//          PM_INFO("E ");		  
#endif
}  // End For (Read Loop)

PC_Wait_CMD_End();
if (PCCR_PCSTATE==PCC_PCS_RESET) return CBRET_NONE;


*/

// Create the Memory type table, 512Kb Block (64Kb max > 128 Block)
// MEM_NULL : From/to the PC RAM (Via Disk Buffer)
// MEM_RAM  : From/to the SRAM emulated RAM (Internal)
// MEM_PSRAM: From/to the PSRAM emulated RAM (External)
// MEM_EMS  : From/to the PSRAM emulated EMS (External)
// 0xFF     : Mix of different RAM with PSRAM > Via Disk Buffer and PSRAM Enabled (No uSD transfer)
void memcp_initBuffMemType()
{
	uint16_t mt_beg;
	uint16_t mt_end;
	uint32_t PCBuffer_Start;
	uint32_t PCBuffer_End;
  uint8_t bn_nb;
  
  bn_nb=20;          // To do
  PCBuffer_Start=0;  // To do

  PCBuffer_End=PCBuffer_Start+512-1; //Current 512b PC Buffer End (-1 as we need the end, not the @ just after the end)
/*
  for(uint8_t bn=0;bn<bn_nb;bn++) 
  
  {
  mt_beg = GetMEMType(PCBuffer_Start);
  mt_end = GetMEMType(PCBuffer_End);
	if (mt_beg!=mt_end)
		  { 
			 PM_ERROR("! Diff Mem Type %d %d > ",mt_beg,mt_end);
			if ((mt_beg<=MEM_RAM) && (mt_end<=MEM_RAM))
			   {						          // MEM Begining and End not in PSRAM
			        mt_beg=MEM_NULL;  // Force Standard BIOS Copy
				 }
				 else
			   {					// MEM Begining or End in PSRAM/EMS
			        mt_beg=0xFF;    // Force Slow MEM Copy with uSD Disable
				 }
#if PM_PRINTF				
			  PM_INFO("%d > ",mt_beg);
#endif				  
			 }
  }

   CopyBuffer_MemType[bn]=mt_beg;
   PCBuffer_Start+=512;
	 PCBuffer_End+=512;

*/   
}

void memcp_PM_to_PC()
{

}

