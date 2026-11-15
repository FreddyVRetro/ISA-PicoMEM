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
#include "dev_picomem_io.h"
#include "pm_board.h"       // PicoMEM Board Definitions (GPIO, PSRAM...)

#if USE_RP2350_PSRAM  // Use the RP2350 internal PSRAM
uint32_t EMS_PCBaseAddress;   // 0xD0000 or 0XE0000 (For QSPI PSRAM EMS)
#endif

// Lotech like EMS Regs
uint8_t  EMS_Bank[4]={0,1,2,3};
uint32_t EMS_Base[4]={0,1*16*1024,2*16*1024,3*16*1024};

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