// "applications" running from the PicoMEM with PC in slave
// Running from the BIOS

#include <stdio.h>
#include "pico/stdlib.h"
#include <string.h>

#include "ff.h"

#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "pm_board.h"       // PicoMEM Board Definitions (GPIO, PSRAM...)
#include "pm_pccmd.h"
#include "pmdev.h"

#include "dev_dma.h"
#include "dev_memory.h"

#define DISP_X 18

#define PM_IOBASEPORT 0x2A0

extern uint32_t PC_DB_Start;  // Start of the Shared disk buffer in the Host (PC) RAM

void PM_RAM_to_file(uint32_t addr, uint32_t sectors, char *filename)
{
  FRESULT fr; /* Return value */
  FIL f;  
  uint bw;    // Nb of Bytes Written
  uint16_t Data;

    PM_EnableSD();  // Enable the SD Card access

    fr = f_open(&f, filename, FA_WRITE | FA_CREATE_ALWAYS);  // Create the file
    if (fr != FR_OK) {
        PC_printf("Can't create %s, Err %d\n",filename,fr);
        f_close(&f);
        PM_PICO_EnablePSRAM();   // Re enable the PSRAM for the Memory emulation
        return;
       }

    for (uint32_t a=0;a<sectors;a++) // Loop on the number of 512 byte sectors
      {
       PC_MemCopyW_512b(addr+(a*512),PC_DB_Start);  // Use the uSD buffer for the transfer (Disk Buffer Start)
       PC_Wait_CMD_End();
       fr = f_write(&f,(void *) &PM_PTR_DISKBUFFER[0],512,&bw);
       if (FR_OK != fr)
           {
            PC_printf("f_write failed : %d\n",fr);
            f_close(&f);
            PM_PICO_EnablePSRAM();   // Re enable the PSRAM for the Memory emulation
            return;
           }
      }
    PM_INFO("Done\n");
    PC_printf("Done");
    f_close(&f);

    PM_PICO_EnablePSRAM();   // Re enable the PSRAM for the Memory emulation
}

void pm_apps_biosmenu(uint8_t menu,uint8_t sub)
{
  uint8_t X,Y;
  uint16_t Data;
  uint32_t addr;
  char filename[25];
  FRESULT fr; /* Return value */
  FIL f;  
  uint bw;    // Nb of Bytes Written

    switch(PM_CmdDataL)
    {
    case 0 :  // PC Info command
          Y=4;
          PC_Setpos(DISP_X,Y++);
          PC_printf("PicoMEM Board ID: %d",BV_BoardID);
          PC_Setpos(DISP_X,Y++);
#ifdef PICO_RP2350
          PC_printf("PCPU Clock (RP2350): %dMHz",PM_SYS_CLK);
#else
          PC_printf("CPU Clock (RP2040): %dMHz",PM_SYS_CLK);
#endif
          PC_Setpos(DISP_X,Y++);
          if ((BV_PSRAMInit>0)&&(BV_PSRAMInit<16))
            {
#if USE_RP2350_PSRAM
             PC_printf("QSPI PSRAM: %dMB, %dMHz",BV_PSRAMInit,PM_SYS_CLK/3);
#else
#if USE_SPI_PSRAM_DMA
             PC_printf("PSRAM: %dMB, %dMHz",BV_PSRAMInit,220/2);
#else
             PC_printf("PSRAM: %dMB, %dMHz",BV_PSRAMInit,220/2);
#endif            
#endif
            } else PC_printf("PSRAM: Disabled or Error");
          
          PC_Setpos(DISP_X,Y++);
          PC_printf("IRQ: %d",BV_IRQ);          
#if BOARD_PM15 || BOARD_PM20
          PC_printf(" All IRQ: ");
          {
            bool found = false;
            for (int i=0;i<8;i++) {
              if (BV_HWIRQ & (1<<i)) {
                PC_printf("%d ", i);
                found = true;
              }
            }
            if (!found) {
              PC_Setpos(DISP_X,Y++);
              PC_printf("  None");
            }
          }
#endif          

          Y++;
          PC_Setpos(DISP_X,Y++);
          PC_printf("Detected ROM List:");
          for (uint32_t addr=0xC0000;addr<0xF8000;addr+=0x04000)
            {
             Data=PC_MR16(addr);
             if (Data==0xAA55)
               {
                 Data=PC_MR16(addr+2)&0xFF;
                 PC_Setpos(DISP_X,Y++);
                 PC_printf("> %X Size: %dKB, Checksum %X",addr>>4,Data>>1,PC_Checksum(addr>>4,Data*512));
               }
            }

          break;

    case 1 :  // Emulated devices info command

         Y=4;
         PC_Setpos(DISP_X,Y);
         PC_printf("Enabled emulation devices details : ");

         Y+=2;
         PC_Setpos(DISP_X,Y++);
         PC_printf("PicoMEM     >  Port: %X-%X  IRQ: %d  BIOS: %X (24KB)\n",PM_IOBASEPORT,PM_IOBASEPORT+0x0F,BV_IRQ,PM_BIOSADDRESS);

         if (PM_Config->EMS_Port) {
            PC_Setpos(DISP_X,Y++);
            PC_printf("EMS         >  Port: %X-%X  Addr: %d \n",EMS_Port_List[PM_Config->EMS_Port]<<3, (EMS_Port_List[PM_Config->EMS_Port]<<3)+0x03, EMS_Addr_List[PM_Config->EMS_Addr]<<14);
           }
         if (PM_Config->UseRTC) {
            PC_Setpos(DISP_X,Y++);
            PC_printf("RTC         >  Port: 2C0-2C7\n");
           }
          if (PM_Config->ne2000Port) {
            PC_Setpos(DISP_X,Y++);
            PC_printf("NE2000      >  Port: %X-%X  IRQ: %d\n",PM_Config->ne2000Port, PM_Config->ne2000Port+0x1F, PM_Config->ne2000IRQ);
           }
         if (PM_Config->AudioOut)
         {
          if (PM_Config->Adlib) {
            PC_Setpos(DISP_X,Y++);
            PC_printf("Adlib       >  Port: 388-389\n");
           } 
          if (PM_Config->TDYPort) {
            PC_Setpos(DISP_X,Y++);
            PC_printf("Tandy       >  Port: %X-%X\n",PM_Config->TDYPort, PM_Config->TDYPort+0x07);
           }
          if (PM_Config->CMSPort) {
            PC_Setpos(DISP_X,Y++);
            PC_printf("CMS         >  Port: %X-%X\n",PM_Config->CMSPort, PM_Config->CMSPort+0x0F);
           }
          if (PM_Config->MMBPort) {
            PC_Setpos(DISP_X,Y++);
            PC_printf("MindScape  >  Port: %X-%X\n",PM_Config->MMBPort, PM_Config->MMBPort+0x0F);
           }
         }

         break;

   case 3 :  // Memory speed test Command

         float test_speed;
         uint32_t test_begin, test_elapsed;
         uint16_t PC_RamSize;
         PC_RamSize = PC_MEM+PC_MEM_H*256;

         Y=4;
         PC_Setpos(DISP_X,Y++);
         PC_printf("Memory Test (PC RAM Size %dKB)",PC_RamSize);

         test_begin = time_us_32();

         test_elapsed = time_us_32() - test_begin;
         test_speed = 1000000.0 * 4* 1024.0 / test_elapsed;
         PC_Setpos(DISP_X,Y++);
         PC_printf("PC RAM R/W Speed  : %d KB/s", (uint16_t)test_speed);
         break;

    case 2 :  // PC ROM DUMP         

          test_begin = time_us_32();
          Y=4;
          PC_Setpos(DISP_X,Y);
          PM_INFO("Dump all the ROM to rom_addr.bin files\n");
          PC_printf("Dump all the ROM to rom_addr.bin files");
          PM_INFO(" SetPOS/Print duration %d us\n",time_us_32() - test_begin);
          Y++;

          // Loop to detect and copy extention ROMs
          for (uint32_t addr=0xC0000;addr<0xF8000;addr+=0x04000)
            {
             test_begin = time_us_32();           
             Data=PC_MR16(addr);
             PM_INFO(" PC_MR16 duration %d us\n",time_us_32() - test_begin);
             if (Data==0xAA55)    // check if it is an extension ROM (0xAA55 at start)
               {       
                 Data=PC_MR16(addr+2)&0xFF;
                 PC_Setpos(DISP_X,++Y);
                 PM_INFO("> DUMP ROM at %X, Size: %dKB ",addr>>4,Data>>1,PC_Checksum(addr>>4,Data*512));
                 PC_printf("> DUMP ROM at %X, Size: %dKB ",addr>>4,Data>>1,PC_Checksum(addr>>4,Data*512));

                 filename[0]=0;
                 sprintf(filename,"0:/ROM_%X.BIN",addr>>12);
                 PM_RAM_to_file(addr, Data, filename);
               }
            }
         Y+=2;
         PC_Setpos(DISP_X,Y);
         PC_printf("> Full RAM Dump to RAM_DUMP.BIN (1MB) ");

         strcpy(filename,"0:/RAM_DUMP.BIN");
         PM_RAM_to_file(0, 1024*2, filename);  // Dump 1Mb from address 0

         Y+=1;
         PC_Setpos(DISP_X,Y);
         PC_printf("> Completed");

         break;
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