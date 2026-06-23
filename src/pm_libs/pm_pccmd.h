#pragma once
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
 * pm_pccmd.h : PC Commands functions definition
 */ 

#include "pico/stdlib.h"  // uint types definitions

#define PCC_PCS_NOTREADY   0x00    // PC Not Waiting for a Command
#define PCC_PCS_WAITCMD    0x01    // PC Waiting for an Command (PCSTATUS=PCC_PCS_WAITCMD)
#define PCC_PCS_INPROGRESS 0X02    // PC Command in Progress
#define PCC_PCS_CMDEND     0X03    // PC Command Ended (Wait for PCCR_PMSTATE=PCC_PMS_CMDACK)
#define PCC_PCS_RESET      0x20    // When the Pi Pico is blocked waiting for the PC, use this to reset

#define PCC_RES_COMPLETED  0X01    // PC Command completed
#define PCC_RES_ERROR      0x10    // PC Command Error
 
// PicoMEM PC Command State Register (PCCR_PMSTATE) definition
#define PCC_PMS_CMDSENT    0x01  // Command Sent by the Pico
#define PCC_PMS_CMDACK     0x02  // Command End Acknowledged by the PC

#define PCC_EndCommand    0    // No more Command
#define PCC_IN8           1	   // IO Read  8Bit
#define PCC_IN16          2    // IO Read  16Bit
#define PCC_OUT8          3    // IO Write 8Bit
#define PCC_OUT16         4    // IO Write 16Bit
#define PCC_MemCopyB      5
#define PCC_MemCopyW      6
#define PCC_MemCopyW_Odd  7
#define PCC_MemCopyW_512b 8
#define PCC_MEMR16        9    // Memory Read 16Bit
#define PCC_MEMW8         10   // Memory Write 16Bit
#define PCC_MEMW16        11   // Memory Write 16Bit
#define PCC_DMA_READ      12   //Read requested DMA channel Page:Offset and size
#define PCC_DMA_WRITE     13   //Update the DMA channel Offset and size
#define PCC_IRQ13         14   //Send an IRQ13 and get the result (BIOS)
#define PCC_IRQ21         15   //Send an IRQ21 and get the result (DOS )

#define PCC_Printf        16   // Display a 0 terminiated string with Int10h
#define PCC_GetScreenMode 17   // Get the currently used Screen Mode
#define PCC_GetPos        18   // Get Screen Offset
#define PCC_SetPos        19   // 
#define PCC_KeyPressed    20   //
#define PCC_ReadKey       21   //
#define PCC_SendKey       22   //
#define PCC_ReadString    23   //

#define PCC_Checksum      24   // Perform a Checksum from one Segment Start with a Size
#define PCC_SetRAMBlock16 25   // Write the same value to a memory Block
#define PCC_MEMWR8        26   // Write then read a byte
#define PCC_MEMWR16       27   // Write then read a word

extern void Send_PC_CMD(uint8_t CommandNB);  // Send a Command, don't touch parameters and wait for its start
extern bool PC_Wait_CMD_End_Timeout(uint32_t delay);
extern bool PC_Wait_End_Timeout(uint32_t delay);
extern bool PC_Wait_CMD_End();
extern void PC_End();

extern void PC_printstr(char *StrToSend);							// The PC will Display the string using the BIOS
extern void PC_printf(const char *format, ...);
extern void PC_Getpos(uint8_t *X, uint8_t *Y);
extern void PC_Setpos(uint8_t X, uint8_t Y);
extern void PC_Int13();

extern void PC_GetDMARegs(uint8_t channel, uint8_t *Page, uint16_t *Offset, uint16_t *Size);
extern void PC_SetDMARegs(uint8_t channel, uint16_t Offset, uint16_t Size, uint8_t ignore);

extern void PC_MW8(uint32_t addr, uint8_t data);
extern void PC_MW16(uint32_t addr, uint16_t data);
extern uint16_t PC_MR16(uint32_t addr);

extern uint8_t PC_IN8(uint16_t port);
extern uint16_t PC_IN16(uint16_t port);
extern void PC_OUT8(uint16_t port, uint8_t data);
extern void PC_OUT16(uint16_t port, uint16_t data);

extern void PC_MemCopy(uint32_t Src, uint32_t Dest, uint16_t Size); // Copy xx Bytes from Src to Dest Address  (PC RAM @)
extern bool PC_MemCopyW_512b(uint32_t Src, uint32_t Dest);          // Copy 512 Bytes from Src to Dest Address  (PC RAM @)

__force_inline bool PC_Wait_CMD_End_Disk()
{
  if (!PC_Wait_CMD_End_Timeout(20000))
    {
     //PM_ERROR("PC_MemCopyW_512b: Timeout 20ms (%x)\n",PCCR_PCSTATE);      
     PM_ERROR("PC_MemCopyW_512b: Timeout 20ms\n"); 
     if (!PC_Wait_CMD_End_Timeout(100000)) { 
       //PM_ERROR("PC_MemCopyW_512b: Timeout 100ms (%x)\n",PCCR_PCSTATE);
       PM_ERROR("PC_MemCopyW_512b: Timeout 100ms\n");       
       return false;
      }
    }

  // if (!PC_Wait_CMD_End()) return false;    
  return true;
}


uint8_t PC_Checksum(uint16_t segment, uint16_t size);