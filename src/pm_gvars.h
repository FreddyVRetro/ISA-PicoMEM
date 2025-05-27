/*
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
// *** PicoMEM Global Vars definitions (From picomem.cpp)
// Used by bios_disk.cpp, picomem.cpp
#pragma once

// Exported functions // (For CPP and C)
extern "C" bool PM_EnablePSRAM();
extern "C" bool PM_EnableSD();

// Exported variables //
extern volatile uint8_t PM_Command;    // Write at port 0 > Command to send to the Pico (Can be changed only if status is 0 Success)
extern volatile uint8_t PM_Status;     // Read at Port 0  > Command and Pico Status
extern volatile uint8_t PM_CmdDataH;
extern volatile uint8_t PM_CmdDataL;
extern volatile bool PM_CmdReset;      // Is set to true when trying to reset a "locked" command

extern volatile bool PM_DoSectorCount; // Increment the read/Written sector count in Disk function

extern uint32_t EMS_Base[4];

extern uint8_t PM_Memory[128*1024];
extern volatile uint8_t PM_DP_RAM[16*1024];   // "Dual Port" RAM for Disk I/O Buffer

extern volatile uint8_t MEM_Type_T[128];
extern volatile uint8_t MEM_Index_T[128];     // Table used to define the Memory index per 8k bloc
extern volatile uint8_t PORT_Table[128];
extern volatile uint32_t RAM_InitialAddress;  // Initial Address of the Pico Internal RAM Emulation (PC @)

extern uint8_t *PM_PTR_DISKBUFFER;

// Debug var 
#if DISP32
extern volatile uint32_t To_Display32;
extern volatile uint32_t To_Display32_2;
extern volatile uint32_t To_Display32_mask;
extern volatile uint32_t To_Display32_2_mask;
#endif