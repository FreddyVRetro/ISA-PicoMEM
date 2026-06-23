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
#include "pm_defines.h"
#include "../pm_cmd.h"    //PM_Config Definition 
#include "dev_memory.h"
#include "dev_ltemm.h"
#include "dev_picomem_io.h"
#include "pm_board.h"       // PicoMEM Board Definitions (GPIO, PSRAM...)

#if USE_RP2350_PSRAM  // Use the RP2350 internal PSRAM
uint32_t EMS_PCBaseAddress;   // 0xD0000 or 0XE0000 (For QSPI PSRAM EMS)
#endif


volatile uint16_t PM_BIOSADDRESS=0;

// Lotech like EMS Regs
uint8_t  EMS_Bank[4]={0,1,2,3};
uint32_t EMS_Base[4]={0,1*16*1024,2*16*1024,3*16*1024};

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
#if USE_RP2350_PSRAM   // Define also the memory Address for this Index
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
                          BV_SPIPSRAM=1;                              // Tell that SPI PSRAM is used
                          BV_Int13hCLI=1;                             // Block interrupt during Int13h
                         }
                        }
               break;
       default :
               return 1;
               break;                 
       }
return 0;
}

// Install all the RAM defined in the Memory MAP
void dev_memory_install_all()
{

  PM_INFO("PC RAM: %dKb\n",PC_MEM+256*PC_MEM_H);
/*
    PM_INFO("Mem Type: ");
    for (int i=0;i<MEM_T_Size;i++)
        { if (i==10*8) PM_INFO("- "); 
          PM_INFO("%d ",GetMEMType(i*8*1024));
        }

    PM_INFO("Mem Index: ");
    for (int i=0;i<MEM_T_Size;i++)
        { if (i==10*8) PM_INFO("- "); 
          PM_INFO("%d ",GetMEMIndex(i*8*1024));
        }
*/
    // 1- Remove all emulated RAM

  dev_memory_remove_ram();
/* 
    PM_INFO("\nAfter Clean: ");
    for (int i=0;i<MEM_T_Size;i++)
        { if (i==10*8) PM_INFO("- "); 
          PM_INFO("%d ",GetMEMType(i*8*1024));
        }
    PM_INFO("\n");
*/
    // 2- Initialize the EMS Port and RAM

  DelPortType(DEV_LTEMS);
  dev_ems_install(EMS_Port_List[PM_Config->EMS_Port]<<3,EMS_Addr_List[PM_Config->EMS_Addr]<<14);

    // 3- Configure the RAM and PSRAM
  dev_memory_install(MEM_RAM,0);
  dev_memory_install(MEM_DISK,0);
//#if USE_SPI_PSRAM
  dev_memory_install(MEM_PSRAM,0);
//#endif

  PM_INFO("Mem Type: ");
  for (int i=0;i<MEM_T_Size;i++)
        { if (i==10*8) PM_INFO("- "); 
          PM_INFO("%d ",MEM_Type_T[i]);
        }  

  PM_INFO ("\n!! BV_SPIPSRAM=%d\n",BV_SPIPSRAM);
}

bool dev_ems_install(uint16_t port, uint32_t base_addr)
{
#if USE_RP2350_PSRAM
  if (port!=0)
      {
        PM_INFO("dev_ems_install (RP2350 PSRAM): EMS Port:%d",PM_Config->EMS_Port,port);

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
#if USE_SPI_PSRAM
  if (port!=0)
      { 
        PM_INFO("dev_ems_install (SPI PSRAM): EMS Port:%d",PM_Config->EMS_Port,port);
         
        SetPortType(port,DEV_LTEMS,1); // Set the EMS device IO Port
        // Set the EMS @ decoding
        SetMEMType(base_addr,MEM_EMS,MEM_S_16k*4);     //64Kb (126Kx4)
        SetMEMIndex(base_addr,MEM_EMS,MEM_S_16k*4);    //64Kb (126Kx4)         

        PM_INFO(" @:%x\n",base_addr);

        PM_INFO("Set BV_SPIPSRAM \n");
        BV_SPIPSRAM=1;                // Tell that SPI PSRAM is used
        BV_Int13hCLI=1;               // Block interrupt during Int13h
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
#if USE_RP2350_PSRAM
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