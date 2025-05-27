// "applications" running from the PicoMEM with PC in slave
// Running from the BIOS

#include <stdio.h>
#include "pico/stdlib.h"

#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "../pm_pccmd.h"

#include "dev_dma.h"
#include "dev_memory.h"

#define PM_IOBASEPORT 0x2A0

void pm_apps_biosmenu(uint8_t menu,uint8_t sub)
{
  uint8_t X,Y;
  uint16_t Data;

    switch(PM_CmdDataL)
    {
    case 0 :  // PC Info command
          Y=4;
          PC_Setpos(15,Y++);
#ifdef PIMORONI_PICO_PLUS2_RP2350
          PC_printf("PicoMEM CPU Clock (RP2350): %dMHz",PM_SYS_CLK);
#else
          PC_printf("PicoMEM CPU Clock (RP2040): %dMHz",PM_SYS_CLK);
#endif
          PC_Setpos(15,Y++);
          if ((BV_PSRAMInit>0)&&(BV_PSRAMInit<16))
            {
              #ifdef PIMORONI_PICO_PLUS2_RP2350
             PC_printf("QSPI PSRAM: %dMB, %dMHz",BV_PSRAMInit,PM_SYS_CLK/4);
#else
#if USE_PSRAM_DMA
             PC_printf("PSRAM: %dMB, %dMHz",BV_PSRAMInit,220/2);
#else
             PC_printf("PSRAM: %dMB, %dMHz",BV_PSRAMInit,PSRAM_FREQ/2);
#endif            
#endif
            } else PC_printf("PSRAM: Disabled or Error");

          Y++;
          PC_Setpos(15,Y++);
          PC_printf("Detected ROM List:",X,Y);
          for (uint32_t addr=0xC0000;addr<0xF8000;addr+=0x04000)
            {
             Data=PC_MR16(addr);
             if (Data==0xAA55)
               {
                 Data=PC_MR16(addr+2)&0xFF;
                 PC_Setpos(16,Y++);
                 PC_printf("> %X Size: %dKB, Checksum %X",addr>>4,Data>>1,PC_Checksum(addr>>4,Data*512));
               }
            }



          break;

    case 1 :  // Memory test Command

         Y=4;
         PC_Setpos(15,Y);
         PC_printf("Enabled emulation devices details");

         Y+=2;
         PC_Setpos(15,Y++);
         PC_printf("PicoMEM     >  Port: %X-%X  IRQ: %d \n",PM_IOBASEPORT,PM_IOBASEPORT+0x0F,BV_IRQ);

         if (PM_Config->EMS_Port) {
            PC_Setpos(15,Y++);
            PC_printf("EMS         >  Port: %X-%X  Addr: %d \n",EMS_Port_List[PM_Config->EMS_Port]<<3, (EMS_Port_List[PM_Config->EMS_Port]<<3)+0x03, EMS_Addr_List[PM_Config->EMS_Addr]<<14);
           }

         // DMA, IRQ ?

         if (PM_Config->EMS_Port) {
            PC_Setpos(15,Y++);
            PC_printf("ne2000       >  Port: %X-%X  IRQ: %d \n",PM_Config->ne2000Port, PM_Config->ne2000Port+0x1F, PM_Config->ne2000IRQ);
           }

         if (PM_Config->AudioOut)
         {
          if (PM_Config->Adlib) {
            PC_Setpos(15,Y++);
            PC_printf("Adlib       >  Port: 388-389\n");
           } 
          if (PM_Config->TDYPort) {
            PC_Setpos(15,Y++);
            PC_printf("Tandy       >  Port: %X-%X\n",PM_Config->TDYPort, PM_Config->TDYPort+0x07);
           }
          if (PM_Config->CMSPort) {
            PC_Setpos(15,Y++);
            PC_printf("CMS        >  Port: %X-%X\n",PM_Config->CMSPort, PM_Config->CMSPort+0x0F);
           }
          if (PM_Config->MMBPort) {
            PC_Setpos(15,Y++);
            PC_printf("MindScape  >  Port: %X-%X\n",PM_Config->MMBPort, PM_Config->MMBPort+0x0F);
           }           

         }

         break;

   case 2 :  // Memory test Command

         float test_speed;
         uint16_t PC_RamSize = PC_MEM+PC_MEM_H*256;

          Y=4;
          PC_Setpos(15,Y++);
          PC_printf("Memory Test (PC RAM Size %dKB)",PC_RamSize);

         uint32_t test_begin = time_us_32();

         uint32_t test_elapsed = time_us_32() - test_begin;
         test_speed = 1000000.0 * 4* 1024.0 / test_elapsed;
         PC_Setpos(15,Y++);
         PC_printf("PC RAM Write Speed : %d KB/s\n", (uint16_t)test_speed);

    }  // Sub Menu nb

}

void pm_apps_test_dma()
{
  uint8_t  Page;
  uint16_t Offset,Size;

  dev_dma_install();  // Enable the DMA registers emulation (Write)

  PM_INFO("PC_GetDMARegs: Page:%X Offset: %X Size: %X\n",Page,Offset,Size);

 //Write AA to all the DMA Channel 1 registers
  PC_OUT8(0x0C,0);      // Clear DMA FlipFlop
  PC_OUT8(0x83,0xAA);   // Page
  PC_OUT8(0x02,0xAA);   // Address L
  PC_OUT8(0x02,0xAA);   // Address H
  PC_OUT8(0x03,0xAA);   // Size L
  PC_OUT8(0x03,0xAA);   // Size H

  PC_GetDMARegs(1,&Page,&Offset,&Size);
  PM_INFO("PC_GetDMARegs: Page:%X Offset: %X Size: %X\n",Page,Offset,Size);
  PM_INFO("Virtual DMA: Page:%X Offset: %X Size: %X\n",dmachregs[1].page, dmachregs[1].baseaddr, dmachregs[1].basecnt);
 
// Compare

//Write 55 to all the DMA Channel 1 registers
  PC_OUT8(0x0C,0);      // Clear DMA FlipFlop
  PC_OUT8(0x83,0x55);   // Page
  PC_OUT8(0x02,0x55);   // Address L
  PC_OUT8(0x02,0x55);   // Address H
  PC_OUT8(0x03,0x55);   // Size L
  PC_OUT8(0x03,0x55);   // Size H

  PC_GetDMARegs(1,&Page,&Offset,&Size);
  PM_INFO("PC_GetDMARegs: Page:%X Offset: %X Size: %X\n",Page,Offset,Size);
  PM_INFO("Virtual DMA: Page:%X Offset: %X Size: %X\n",dmachregs[1].page, dmachregs[1].baseaddr, dmachregs[1].basecnt);

}