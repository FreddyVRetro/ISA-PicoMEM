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

extern void Send_PC_CMD(uint8_t CommandNB);  // Send a Command, don't touch parameters and wait for its start
extern bool PC_Wait_Ready_Timeout(uint32_t delay);
extern bool PC_Wait_End_Timeout(uint32_t delay);
extern void PC_WaitCMDCompleted();
extern void PC_End();

extern void PC_printstr(char *StrToSend);							// The PC will Display the string using the BIOS
extern void PC_printf(const char *format, ...);
extern void PC_Getpos(uint8_t *X, uint8_t *Y);
extern void PC_Setpos(uint8_t X, uint8_t Y);
extern void PC_GetDMARegs(uint8_t channel, uint8_t *Page, uint16_t *Offset, uint16_t *Size);
extern void PC_SetDMARegs(uint8_t channel, uint16_t Offset, uint16_t Size);

extern void PC_MW8(uint32_t addr, uint8_t data);
extern void PC_MW16(uint32_t addr, uint16_t data);
extern uint16_t  PC_MR16(uint32_t addr);

extern uint8_t PC_IN8(uint16_t port);
extern uint16_t PC_IN16(uint16_t port);
extern void PC_OUT8(uint16_t port, uint8_t data);
extern void PC_OUT16(uint16_t port, uint16_t data);

extern void PC_MemCopy(uint32_t Src, uint32_t Dest, uint16_t Size); // Copy xx Bytes from Src to Dest Address  (PC RAM @)
extern void PC_MemCopyW_512b(uint32_t Src, uint32_t Dest);          // Copy 512 Bytes from Src to Dest Address  (PC RAM @)

uint8_t PC_Checksum(uint16_t segment, uint16_t size);