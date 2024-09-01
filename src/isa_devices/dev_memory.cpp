/* Copyright (C) 2023 Freddy VETELE

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

/* pm_memory.cpp : RAM/ROM emulation Code
 */ 


#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "../pm_cmd.h"    //PM_Config Definition 
#include "dev_memory.h"

volatile uint16_t PM_BIOSADDRESS=0;

/*
void SetMEMType(uint32_t Address,uint8_t Type, uint8_t NbBloc)
{ 	
  for (int i=0;i<NbBloc;i++) 
  { MEM_Type_T[(((Address+i*MEM_BlockSize)^0xF0000)>>MT_Shift)] = Type; }
}

void SetMEMIndex(uint32_t Address,uint8_t Index, uint8_t NbBloc)
{ 	
  for (int i=0;i<NbBloc;i++) 
  { MEM_Index_T[(((Address+i*MEM_BlockSize)^0xF0000)>>MT_Shift)] = Type; }
}
*/

//uint8_t volatile *RAM_Offset_Table[16];

// Debug
void display_memmap()
{
PM_INFO("\nBIOS MP        :");
for (int i=0;i<64;i++) PM_INFO("%d ",PM_Config->MEM_Map[i]); 
}

void display_memindex()
{
PM_INFO("\nMEM I (W  Mask):");
for (int i=0;i<128;i++) PM_INFO("%d ",MEM_Index_T[i]);
PM_INFO("\nMEM T (No Mask):");
for (int i=0;i<128;i++) PM_INFO("%d ",GetMEMType(i<<13));
PM_INFO("\n");
}

void dev_memory_init()
{

}

// To use for MEM_RAM and MEM_DISK MI_Type is ignored
// Return :
// 1: PC Address already in use
// 2: No Memory index available
uint8_t dev_memory_install(uint8_t MEM_Type, uint32_t Pico_Offset)
{
bool found=false;
uint8_t index;
uint8_t mem_index;  // Index value to use

printf("dev_memory_install(%d,%x);\n",MEM_Type,Pico_Offset);

switch(MEM_Type)
      {
       case MEM_RAM:         // RAM No Wait State  128Kb Maximum
               mem_index=MEM_RAM;
               RAM_InitialAddress=0;
               int InitialIndex;
                if (BV_TdyRAM==0)   // IF Tandy RAM <>0 PMRAM is already used
                 for (int i=0;i<64;i++)  
                   { 
                    if ((BV_Tandy!=0)&&(i<10*4)) continue;  
                    uint32_t MEM_Addr=i<<14;  // Shift 14 for 16Kb Block         
                    if (PM_Config->MEM_Map[i]==MEM_RAM)
                       { 
                        if (RAM_InitialAddress==0) 
                           {
                            RAM_InitialAddress=i<<14;  // Skip conv memory add if Tandy (Done by BIOS)
                            RAM_Offset_Table[mem_index]=&PM_Memory[-RAM_InitialAddress];
                            printf("RAM_Offset_Table[mem_index] %x",RAM_Offset_Table[mem_index]);                             
                            InitialIndex=i;
                           }
                        if ((i-InitialIndex)<MAX_PMRAM)
                           {
                            SetMEMType(MEM_Addr,MEM_RAM,MEM_S_16k);     // Define a 16Kb RAM Block
                            SetMEMIndex(MEM_Addr,mem_index,MEM_S_16k);  // Define a 16Kb RAM Block                            
                           }
                       }
                   }                 
               break;
       case MEM_DISK:          //  RAM No Wait State, 16Kb
               mem_index=MEM_DISK;  // Already defined somewhere else
               break;
       case MEM_PSRAM:          // RAM With Wait State any address possible (1Mb)
               mem_index=MEM_PSRAM;
               for (int i=0;i<64;i++)  
                        { 
                        if ((BV_Tandy!=0)&&(i<10*4)) continue;  // Skip conv memory add if Tandy (Done by BIOS)
                        uint32_t MEM_Addr=i<<14;  // Shift 14 for 16Kb Block         
                        if ((PM_Config->MEM_Map[i]==MEM_PSRAM)) 
                         {
                          SetMEMType(MEM_Addr,MEM_PSRAM,MEM_S_16k);  // Define a 16Kb RAM Block
                          SetMEMIndex(MEM_Addr,mem_index,MEM_S_16k);  // Define a 16Kb RAM Block
                         }
                        }
               break;

       default :
               return 1;
               break;                 
       }
return 0;
}

// Remove all the Emulated RAM (PSRAM, RAM, EMS)
void dev_memory_remove()
{
 int Addr=0;
 uint8_t MT;
 do
 {
  if ((BV_Tandy!=0)&&(Addr<640*1024)) continue;  // Skiv Conv memory emulation removal if Tandy
  MT=GetMEMType(Addr);
  if ((MT==MEM_PSRAM)||(MT==MEM_RAM)||(MT==MEM_EMS))
     {
       SetMEMType(Addr,MEM_NULL,1);
       SetMEMIndex(Addr,0,1);
//       MEM_Index_T[i]=0;
//       MEM_Type_T[i]=MEM_NULL;
     } 
  Addr+=4096;
 } while (Addr<1024*1024);
}

// Remove one type of Emulated RAM/ROM
void dev_memorytype_remove(uint8_t type)
{
 for (int i=0;i<MEM_T_Size;i++)
     { 
       if ((BV_Tandy!=0)&&(i<10*8)) continue;  // Skiv Conv memory emulation removal if Tandy (8k block)
       if (MEM_Type_T[i]==type)
           {
             MEM_Index_T[i]=0;
             MEM_Type_T[i]=MEM_NULL;
            }
      }
}