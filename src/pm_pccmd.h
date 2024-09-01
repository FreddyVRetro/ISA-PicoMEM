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

extern void Send_PCC_CMD(uint8_t CommandNB);  // Send a Command, don't touch parameters and wait for its start
extern void PCC_WaitCMDCompleted();
extern void Send_PCC_End();
extern void Send_PCC_Printf(char *StrToSend);							// The PC will Display the string using the BIOS
extern void Send_PCC_MemCopyW_512b(uint32_t Src, uint32_t Dest);        // Copy 512 Bytes from Src to Dest Address  (PC RAM @)