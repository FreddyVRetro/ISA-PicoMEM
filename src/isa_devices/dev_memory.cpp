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
#include "dev_picomem_io.h"

#ifdef PIMORONI_PICO_PLUS2_RP2350
#include "pico_psram.h"

uint32_t EMS_PCBaseAddress;   // 0xD0000 or 0XE0000 (For QSPI PSRAM EMS)
#define PSRAM_EMS_START __psram_start__              // First 4Mb
#define PSRAM_RAM_START __psram_start__+4*1024*1024  // PSRAM extention in the first 1Mb 
#endif

extern uint8_t  EMS_Bank[4];
extern uint32_t EMS_Base[4];
volatile uint16_t PM_BIOSADDRESS=0;

#if BOARD_PM15

#endif

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

void dev_memory_init(uint32_t biosaddr)
{

for (int i=0;i<MEM_T_Size;i++) 
    {
      MEM_Index_T[i]=MEM_I_NULL;
      MEM_Type_T[i]=MEM_NULL;
    }
/*
SetMEMType(biosaddr,MEM_BIOS,MEM_S_16k);
SetMEMIndex(biosaddr,MEM_BIOS,MEM_S_16k);
RAM_Offset_Table[MEM_BIOS]=&PMBIOS[-biosaddr];

SetMEMType(biosaddr+0x4000,MEM_DISK,1);  // 8kb only
SetMEMIndex(biosaddr+0x4000,MEM_DISK,1);
RAM_Offset_Table[MEM_DISK]=&PM_DP_RAM[-(biosaddr+0x4000)];
*/
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

PM_INFO("dev_memory_install(%d,%x);\n",MEM_Type,Pico_Offset);

switch(MEM_Type)
      {
       case MEM_RAM:         // RAM No Wait State  128Kb Maximum
               mem_index=MEM_RAM;
               RAM_InitialAddress=0;
               int InitialIndex;
                if (BV_TdyRAM==0)   // IF Tandy RAM <>0 PMRAM is already used
                 for (int i=0;i<64;i++)  
                   { 
                    if ((BV_Tandy!=0)&&(i<0x0A*4)) continue;  
                    uint32_t MEM_Addr=i<<14;  // Shift 14 for 16Kb Block         
                    if (PM_Config->MEM_Map[i]==MEM_RAM)
                       {
                        if (RAM_InitialAddress==0) 
                           {
                            RAM_InitialAddress=i<<14;  // Skip conv memory add if Tandy (Done by BIOS)
                            RAM_Offset_Table[mem_index]=&PM_Memory[-RAM_InitialAddress];
                            PM_INFO("RAM_Offset_Table[mem_index] %x",RAM_Offset_Table[mem_index]);                             
                            InitialIndex=i;
                           }
                        if ((i-InitialIndex)<MAX_PMRAM)
                           {
                            SetMEMType(MEM_Addr,MEM_RAM,MEM_S_16k);     // Define a 16Kb RAM Block Type
                            SetMEMIndex(MEM_Addr,mem_index,MEM_S_16k);  // Define a 16Kb RAM Block Index
                           }
                       }
                   }
               break;
       case MEM_DISK:               //  RAM No Wait State, 16Kb
               mem_index=MEM_DISK;  // Already defined somewhere else
               break;
       case MEM_PSRAM:              // RAM With Wait State any address possible (1Mb)
            //   mem_index=MEM_PSRAM;
#ifdef PIMORONI_PICO_PLUS2_RP2350   // Define also the memory Address for this Index
               RAM_Offset_Table[MEM_PSRAM]=(uint8_t *) (PSRAM_RAM_START);
#endif                
               for (int i=0;i<64;i++)  
                        { 
                        if ((BV_Tandy!=0)&&(i<0x0A*4)) continue;  // Skip conv memory add if Tandy (Done by BIOS)
                        uint32_t MEM_Addr=i<<14;  // Shift 14 for 16Kb Block         
                        if ((PM_Config->MEM_Map[i]==MEM_PSRAM)) 
                         {
                          SetMEMType(MEM_Addr,MEM_PSRAM,MEM_S_16k);   // Define a 16Kb RAM Block Type
                          SetMEMIndex(MEM_Addr,MEM_PSRAM,MEM_S_16k);  // Define a 16Kb RAM Block Index                     
                         }
                        }
               break;

       default :
               return 1;
               break;                 
       }
return 0;
}

bool dev_ems_install(uint16_t port, uint32_t base_addr)
{
#ifdef PIMORONI_PICO_PLUS2_RP2350
  if (port!=0)
      {
        PM_INFO("dev_ems_install (Pimoroni): EMS Port:%d",PM_Config->EMS_Port,port);

        SetPortType(port,DEV_LTEMS,1); // Set the EMS device IO Port
        EMS_PCBaseAddress=base_addr;

        // Set the EMS @ decoding
        SetMEMType (base_addr          ,MEM_EMS,MEM_S_16k*4);    //64Kb (16Kx4) Memory Type

        SetMEMIndex(base_addr          ,MEM_I_EMS0,MEM_S_16k);   //16Kb Index (for the Offset table)
        SetMEMIndex(base_addr+16*1024  ,MEM_I_EMS1,MEM_S_16k);   //16Kb
        SetMEMIndex(base_addr+2*16*1024,MEM_I_EMS2,MEM_S_16k);   //16Kb
        SetMEMIndex(base_addr+3*16*1024,MEM_I_EMS3,MEM_S_16k);   //16Kb

        RAM_Offset_Table[MEM_I_EMS0]=(uint8_t *) (PSRAM_EMS_START-base_addr);
        RAM_Offset_Table[MEM_I_EMS1]=(uint8_t *) (PSRAM_EMS_START-base_addr);
        RAM_Offset_Table[MEM_I_EMS2]=(uint8_t *) (PSRAM_EMS_START-base_addr);
        RAM_Offset_Table[MEM_I_EMS3]=(uint8_t *) (PSRAM_EMS_START-base_addr);

        EMS_Bank[0]=EMS_Bank[1]=EMS_Bank[2]=EMS_Bank[3]=0;
        PM_INFO(" @:%x RAM_Offset %x\n",base_addr,RAM_Offset_Table[MEM_I_EMS0]);
      } else return false;
#else 
#if USE_PSRAM
  if (port!=0)
      { 
        PM_INFO("EMS Port:%d %x ",PM_Config->EMS_Port,port);
         
        SetPortType(port,DEV_LTEMS,1); // Set the EMS device IO Port
        // Set the EMS @ decoding
        SetMEMType(base_addr,MEM_EMS,MEM_S_16k*4);     //64Kb (126Kx4)
        SetMEMIndex(base_addr,MEM_EMS,MEM_S_16k*4);    //64Kb (126Kx4)         

        PM_INFO(" @:%x\n",base_addr);
      } else return false;
#endif  

#endif
return true;
}

// Remove all the Emulated RAM (PSRAM, RAM, EMS)
void dev_memory_remove_ram()
{
 int Addr=0;
 uint8_t MT;
// PM_INFO("dev_memory_remove_ram");
  do
 {
//  busy_wait_ms(5); // Little wait for the display
//  PM_INFO("@%x, ",Addr);
  if ((BV_Tandy!=0)&&(Addr<640*1024)) 
    { 
//      PM_INFO("Sk ");
      Addr+=4096;
      continue;  // Skiv Conv memory emulation removal if Tandy
    }
  MT=GetMEMType(Addr);
  if ((MT==MEM_PSRAM)||(MT==MEM_RAM)||(MT==MEM_EMS))
     {
//       PM_INFO("Clean 4Kb");
       SetMEMType(Addr,MEM_NULL,1);
       SetMEMIndex(Addr,MEM_I_NULL,1);
     }
  Addr+=4096;
 } while (Addr<1024*1024);

}

// Remove one type of Emulated RAM/ROM
void dev_memorytype_remove(uint8_t type)  // Used only for BIOS and disk for the moment, so no Tandy Code needed
{
 for (int i=0;i<MEM_T_Size;i++)
     { 
//       if ((BV_Tandy!=0)&&(i<10*8)) continue;  // Skiv Conv memory emulation removal if Tandy (8k block)
       if (MEM_Type_T[i]==type)
           {
             MEM_Index_T[i]=MEM_I_NULL;
             MEM_Type_T[i]=MEM_NULL;
            }
      }
}

bool dev_ems_ior(uint32_t CTRL_AL8,uint8_t *Data )
{

*Data=EMS_Bank[(uint8_t) CTRL_AL8 & 0x03];  // Read the Bank Register

return true;
}

void dev_ems_iow(uint32_t CTRL_AL8,uint8_t Data)
{
 if ((CTRL_AL8 & 0x07)<4)
  {
#ifdef PIMORONI_PICO_PLUS2_RP2350
   uint8_t bank=CTRL_AL8 & 0x03;
   EMS_Bank[(uint8_t) CTRL_AL8 & 0x03]=Data;   // Write the Bank Register
   RAM_Offset_Table[MEM_I_EMS0+bank]=(uint8_t *) (PSRAM_EMS_START-EMS_PCBaseAddress+(Data-bank)*16*1024);  // Address is EMS Start + Parameter*16KB
//   printf("B %d Addr %x\n",bank,RAM_Offset_Table[MEM_I_EMS0+bank]);
#else
   EMS_Bank[(uint8_t) CTRL_AL8 & 0x03]=Data;   // Write the Bank Register
   EMS_Base[(uint8_t) CTRL_AL8 & 0x03]=((uint32_t) Data*16*1024)+1024*1024; // EMS Bank base Address : 1Mb + Bank number *16Kb (in SPI RAM)
#endif
  }
}