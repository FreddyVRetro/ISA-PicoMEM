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
#include "pico/stdlib.h"

#include "pm_gvars.h"
#include "pm_defines.h"

void Send_PCC_CMD(uint8_t CommandNB)  // Send a Command, don't touch parameters and wait for its start
{  
     PCCR_PMSTATE=PCC_PMS_DOWAITCMD;  // Ask the PC to go in Wait CMD State
 
 // The PC Must be in Wait State before sending a CMD
 /*    uint16_t count=0;
     do {count++;} while ((PCCR_PCSTATE!=PCC_PCS_WAITCMD) && (count!=20000));         
     if (count==20000) {printf("Send_PCC_CMD: Wait TimeOut %d\n",PCCR_PCSTATE);}  */
     do {} while ((PCCR_PCSTATE!=PCC_PCS_WAITCMD)&&(PCCR_PCSTATE!=PCC_PCS_RESET));

     PCCR_CMD=CommandNB;                 // Send the Command value
     PCCR_PMSTATE=PCC_PMS_COMMAND_SENT;  // Inform the PC that a command is sent

     do {} while ((PCCR_PCSTATE==PCC_PCS_WAITCMD)&&(PCCR_PCSTATE!=PCC_PCS_RESET)); // Wait the Start of the command (PC no more in Wait CMD State)
}

// Wait for the PC Command to End or Reset Status
void PCC_WaitCMDCompleted()
{
  /*   uint32_t count=0;
     do {count++;} while ((PCCR_PCSTATE==PCC_PCS_INPROGRESS) && (count!=600000));
     if (count==600000) {printf("PCC_WaitCMDCompleted: TimeOut %d\n",PCCR_PCSTATE);} */
     do {} while ((PCCR_PCSTATE==PCC_PCS_INPROGRESS)&&(PCCR_PCSTATE!=PCC_PCS_RESET));     
}

void Send_PCC_End()
{
  Send_PCC_CMD(PCC_EndCommand);
  PCCR_PMSTATE=PCC_PMS_DOWAITCMD;  // Put back to the Wait State
}

void Send_PCC_Printf(char *StrToSend)
{
     memcpy((char *)PM_DP_RAM+OFFS_PCCMDPARAM,StrToSend,strlen(StrToSend)+1);
     Send_PCC_CMD(PCC_Printf);   
     // Need to wait the end of the command, to Avoid the next command to erade the string
     do {} while ((PCCR_PCSTATE==PCC_PCS_INPROGRESS)&&(PCCR_PCSTATE!=PCC_PCS_RESET)); // Wait the Start of the command (PC no more in Wait CMD State)
}

void Send_PCC_Do_MemCopyB(uint32_t Src, uint32_t Dest,uint16_t Len)
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
  Send_PCC_CMD(PCC_MemCopyB);
}

void Send_PCC_Do_MemCopyW(uint32_t Src, uint32_t Dest,uint16_t Len)
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
  Send_PCC_CMD(PCC_MemCopyW);
}

void Send_PCC_MemCopyW_512b(uint32_t Src, uint32_t Dest)
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
  Send_PCC_CMD(PCC_MemCopyW_512b);
}

uint8_t CopyBuffer_MemType[128];
uint8_t DPRAMBuffer_Busy[8];

// Copy Data from the PC Memory to the PicoMEM, via PC Commands
// Source, Size, Block size
void memcp_PC_to_PM()
{

}

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
			  printf("%d > ",mt_beg);
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