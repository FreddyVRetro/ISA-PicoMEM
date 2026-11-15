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
#include "pm_bluetooth.h"
#include "pm_bluetooth_scan.h"

#include "dev_dma.h"
#include "dev_memory.h"

//#include "BIOS.H"

#define DISP_X 18

#define PM_IOBASEPORT 0x2A0

extern uint32_t PC_DB_Start;  // Start of the Shared disk buffer in the Host (PC) RAM

static pm_bluetooth_device_t bluetooth_devices[PM_BLUETOOTH_MAX_DEVICES];
static uint8_t bluetooth_device_count;

static bool pm_bluetooth_type_is_specific(pm_bluetooth_device_type_t type)
{
  return type != PM_BLUETOOTH_DEVICE_UNKNOWN &&
         type != PM_BLUETOOTH_DEVICE_HID;
}

void pm_bluetooth_record_device(const uint8_t address[6], const char *name, int8_t rssi)
{
  pm_bluetooth_record_device_info(address, name, rssi, 0, PM_BLUETOOTH_DEVICE_UNKNOWN,
                                  0xffff, 0, 0);
}

void pm_bluetooth_record_device_info(const uint8_t address[6], const char *name, int8_t rssi,
                                     uint8_t address_type, pm_bluetooth_device_type_t type,
                                     uint16_t appearance, uint8_t hid_service,
                                     uint8_t advertisement_length)
{
  uint8_t index;

  for (index = 0; index < bluetooth_device_count; ++index) {
    if (memcmp(bluetooth_devices[index].address, address, sizeof(bluetooth_devices[index].address)) == 0) {
      break;
    }
  }

  if (index == bluetooth_device_count) {
    if (bluetooth_device_count == PM_BLUETOOTH_MAX_DEVICES) {
      return;
    }
    bluetooth_devices[index].appearance = 0xffff;
    bluetooth_devices[index].hid_service = 0;
    ++bluetooth_device_count;
  }

  memcpy(bluetooth_devices[index].address, address, sizeof(bluetooth_devices[index].address));
  if (name && strcmp(name, "Unknown") != 0) {
    strncpy(bluetooth_devices[index].name, name, PM_BLUETOOTH_NAME_LEN - 1);
    bluetooth_devices[index].name[PM_BLUETOOTH_NAME_LEN - 1] = '\0';
  } else if (bluetooth_devices[index].name[0] == '\0') {
    strcpy(bluetooth_devices[index].name, "Unknown");
  }
  bluetooth_devices[index].rssi = rssi;
  bluetooth_devices[index].address_type = address_type;
  if (appearance != 0xffff) {
    bluetooth_devices[index].appearance = appearance;
  }
  bluetooth_devices[index].hid_service |= hid_service;
  bluetooth_devices[index].advertisement_length = advertisement_length;
  /*
   * Appearance and service UUIDs are optional and are not necessarily
   * repeated in every advertising packet. Keep the strongest classification
   * observed instead of downgrading a known device on a sparse packet.
   */
  if (pm_bluetooth_type_is_specific(type) ||
      !pm_bluetooth_type_is_specific(bluetooth_devices[index].type)) {
    bluetooth_devices[index].type = type;
  }
}

uint8_t pm_bluetooth_device_count(void)
{
  return bluetooth_device_count;
}

const pm_bluetooth_device_t *pm_bluetooth_get_device(uint8_t index)
{
  return index < bluetooth_device_count ? &bluetooth_devices[index] : NULL;
}

void pm_app_bluetooth()
{
  uint8_t y = 4;
  uint8_t device_count = pm_bluetooth_device_count();

  PC_Setpos(DISP_X, y++);
  PC_printf("Bluetooth devices:");

  pm_bluetooth_scan_start();
  PC_Setpos(DISP_X, y++);
  PC_printf("Status: %s", pm_bluetooth_mouse_status());
  PC_Setpos(DISP_X, y++);
  PC_printf("Mouse: %s  Keyboard: %s  Joypad: %s",
            pm_bluetooth_slot_status(PM_BT_SLOT_MOUSE),
            pm_bluetooth_slot_status(PM_BT_SLOT_KEYBOARD),
            pm_bluetooth_slot_status(PM_BT_SLOT_JOYPAD));

  if (!device_count) {
    PC_Setpos(DISP_X, y);
    PC_printf("No devices discovered");
    return;
  }

  for (uint8_t index = 0; index < device_count; ++index) {
    const pm_bluetooth_device_t *device = pm_bluetooth_get_device(index);
    char appearance[5];
    const char *type = device->type == PM_BLUETOOTH_DEVICE_MOUSE ? "Mouse" :
               device->type == PM_BLUETOOTH_DEVICE_KEYBOARD ? "Keyboard" :
               device->type == PM_BLUETOOTH_DEVICE_JOYPAD ? "Joypad" :
               device->type == PM_BLUETOOTH_DEVICE_HEADPHONES ? "Headphones" :
               device->type == PM_BLUETOOTH_DEVICE_SPEAKER ? "Speaker" :
               device->type == PM_BLUETOOTH_DEVICE_PHONE ? "Phone" :
               device->type == PM_BLUETOOTH_DEVICE_COMPUTER ? "Computer" :
               device->type == PM_BLUETOOTH_DEVICE_WATCH ? "Watch" :
               device->type == PM_BLUETOOTH_DEVICE_HEART_RATE ? "Heart rate" :
               device->type == PM_BLUETOOTH_DEVICE_THERMOMETER ? "Thermometer" :
               device->type == PM_BLUETOOTH_DEVICE_HID ? "HID" : "Unknown";

    if (device->appearance == 0xffff) {
      strcpy(appearance, "----");
    } else {
      snprintf(appearance, sizeof(appearance), "%04X", device->appearance);
    }
    PC_Setpos(DISP_X, y++);
    PC_printf("%d. %s %s %ddBm A:%s H:%d L:%d T:%d",
              index + 1,
              device->name,
              type,
              device->rssi,
              appearance,
              device->hid_service,
              device->advertisement_length,
              device->address_type);
    PM_INFO("%d. %s %s %ddBm", index + 1, device->name, type, device->rssi);
  }
}

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

void pm_app_diskinfo()
{
/*
    struct diskinfo {
        uint8_t char drive;      // Drive number (0=A, 1=B, etc.)
        uint8_t type;            // Drive type (0=unknown, 1=floppy, 2=hard disk)
        uint8_t short spt;       // Sectors per track
        uint8_t short heads;     // Number of heads
        uint8_t long cylinders;  // Number of cylinders
        uint8_t long total_sectors;
    } di; 

    _ibm_diskinfo_t disk;
    
    disk.drive = 0x80;  // First hard disk
    if (_bios_disk(_DISK_STATUS, &disk) != 0) {
        PC_printf("Disk not ready\n");
        return;
    }

  reg_dl = (uint8_t)disk.drive;
  reg_ah = 0x8h;  // Get drive parameters
  reg_esl = 0;
  reg_esh = 0;
  PC_Int13();
  if (reg_ah != 0) {
      PC_printf("Error getting drive parameters: %02X\n", reg_ah);
      return;
  
  

  }

  */
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

  // Set text attribute to Black on Grey
  PC_SetTextAttr(0x70);

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
            PC_printf("RTC         >  Port: 2C0-2D4\n");
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
         uint32_t test_begin, test_elapsed;

         PC_Setpos(DISP_X,4);
         PC_printf("Not Implemented Yet");
   /*
         float test_speed;

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
*/
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
    case 4: // Disk info/Copy app
          pm_app_diskinfo();
         break;
    case 5: // Bluetooth devices pairing
         pm_app_bluetooth();
         break;
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13: // Bluetooth device selection: cached index 0 through 7
          if (!pm_bluetooth_connect_device(PM_CmdDataL - 6)) {
         PM_INFO("[BT] Invalid Bluetooth device selection %d\n", PM_CmdDataL - 6);
          }
          pm_app_bluetooth();
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