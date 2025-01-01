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
/* ISA PicoMEM By Freddy VETELE	
 * picomem.cpp : Contains the Main initialisazion, ISA Bus Cycles management and commands executed by the Pi Pico
 */ 

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/irq.h"
#include "hardware/vreg.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/regs/busctrl.h"              /* Bus Priority defines */
#if !defined(RASPBERRYPI_PICO2) && !defined(PIMORONI_PICO_PLUS2_RP2350)
#include "hardware/regs/vreg_and_chip_reset.h"
#endif

#if USE_PSRAM
#if USE_PSRAM_DMA
#include "psram_spi.h"
#else
#include "psram_spi2.h"
#endif
#endif

#ifdef PIMORONI_PICO_PLUS2_RP2350
#include "pico_psram.h"    // Add QSPI PSRAM Code
#else
#if USE_PSRAM              // Add other PSRAM Code if not Pimoroni
#if USE_PSRAM_DMA
#include "psram_spi.h"
#else
#include "psram_spi2.h"
#endif  //USE_PSRAM_DMA
#endif  //USE_PSRAM
#endif  //PIMORONI_PICO_PLUS2_RP2350

#ifdef RASPBERRYPI_PICO_W
#include "pico/cyw43_arch.h"
#include "cyw43.h"
#include "cyw43_stats.h"
#endif

#include "pm_debug.h"

// For Clock Display (Test)
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

//SDCARD and File system definitions
#include "hw_config.h"  /* The SDCARD and SPI definitions */	
#include "f_util.h"
#include "ff.h"
#include "sd_card.h"

//DOSBOX Code
#include "dosbox/bios_disk.h"

//Other PicoMEM Code
#include "pm_dbg.h"
#include "isa_iomem.pio.h"  // PIN Defines
#include "pm_gvars.h"
#include "picomem.h"
#include "pm_gpiodef.h"     // Various PicoMEM GPIO Definitions
#include "pm_defines.h"     // Shared MEMORY variables defines (Registers and PC Commands)
#include "pm_libs.h"
#include "pm_disk/pm_disk.h"
#include "pm_pccmd.h"
#include "qwiic.h"

extern char fdd_dir[];

// PicoMEM emulated devices include
#include "dev_memory.h"
#include "dev_picomem_io.h"
#include "dev_post.h"
#include "dev_joystick.h"

#if USE_AUDIO
#include "dev_adlib.h"
#include "dev_cms.h"
#include "dev_tandy.h"
#include "dev_dma.h"
#include "dev_sbdsp.h"
#include "dev_pmaudio.h"
#include "pm_audio.h"
#else
void pm_audio_pause() {}
void pm_audio_resume() {}
#endif

#ifdef RASPBERRYPI_PICO_W
#if USE_NE2000
extern "C" {
#include "ne2000/dev_ne2000.h"
extern wifi_infos_t PM_Wifi;
}
#endif
#endif

extern "C" {
  extern void PM_IRQ_Lower(void);
  extern uint8_t PM_IRQ_Raise(uint8_t Source,uint8_t Arg);  
//  extern bool PM_EnableSD(void);
//  extern bool PM_StartSD(void);
}

#if USE_USBHOST
// TinyUSB Host
//#include "bsp/board_api.h"
#include "tusb.h"
#include "hid_dev.h"  // USB HID Devices variables : Mouse, KB, Joystick
extern "C" {
#include "usb.h"
}
#endif

#if USE_USBHOST
#if TUSB_OPT_HOST_ENABLED
  #pragma message "picomem.cpp - TUSB_OPT_HOST_ENABLED is set"
#else
  #pragma message "picomem.cpp - TUSB_OPT_HOST_ENABLED is NOT set"
#endif

extern volatile pm_mouse_t pm_mouse;

// TinyUSB Host
void led_blinking_task(void);

CFG_TUSB_MEM_SECTION static char serial_in_buffer[64] = { 0 };

#endif // TinyUSB Host definitions END

#include "pm_libs/pm_e_dma.h"       // Emulated DMA code

//#ifndef USE_ALARM
//#include "pico_pic.h"
//#endif

// ** State Machines ** Order should not be changed, "Hardcoded" (For ISA)
// pio0 / sm0 : isa_bus_sm    : Wait for ALE falling edge, fetch the Address and Data
//
// pio1 / sm0                 : PSRAM, Wifi, I2S
// pio1 / sm1                 : PSRAM, Wifi, I2S
// pio1 / sm2                 : PSRAM, Wifi, I2S

//***********************************************
//*              PicoMEM Variables              *
//***********************************************
							
#define DEV_POST_Enable 1

bool PM_Init_Completed=false;

PMCFG_t *PM_Config;               // Pointer to the PicoMEM Configuration in the BIOS Shared Memory

volatile uint8_t PM_Command;      // Write at port 0  > Command to send to the Pico (Can be changed only if status is 0 Success)
volatile uint8_t PM_Status;       // Read from Port 0 > Command and Pico Status ( ! When a SD Command is in progress, sometimes read FFh)
volatile uint8_t PM_CmdDataH;     // R/W Register
volatile uint8_t PM_CmdDataL;     // R/W Register
volatile bool PM_CmdReset=false;  // Is set to true when trying to reset a "locked" command

volatile uint8_t PM_TestWrite;
         uint8_t PM_SetRAMVal;
         uint8_t PM_TestRAMVal;

// USB Host variables

bool USB_Host_Enabled=false;	// True if USB Host mode is enabled
bool MOUSE_Enabled=false;
bool KEYB_Enabled=false;

// Serial Output variables

bool SERIAL_Enabled=false;	// True is Serial output is enabled (Can do printf)
bool SERIAL_USB=false;   	  // True is Serial is oved USB

// PSRAM Memory variables / Registers

static uint EPSRAM_Size;
bool EPSRAM_Available=false;   // True if the External PSRAM is initialized
bool EPSRAM_Enabled=false;

#define EMS_PORT_NB 5
#define EMS_ADDR_NB 2
const uint8_t EMS_Port_List[EMS_PORT_NB]={0,0x268>>3,0x288>>3,0x298>>3,0x2A8>>3}; // 278 used by LPT
const uint8_t EMS_Addr_List[EMS_ADDR_NB]={0xD0000>>14,0xE0000>>14};

bool SD_Available=false;
bool SD_Enabled=false;

bool IRQ_raised=false;

#define WIFI_STATUS_DELAY 10000000 // Delay in us between 2 Wifi get status call (10s)
uint64_t wifi_getstatus_delay;
uint64_t pm_cmd_1sdelay;
uint8_t wifi_delay_counter;

// Add in registers ?

/*
uint16_t PICO_Freq=280;  //PM_SYS_CLK;
uint8_t  PSRAM_Freq=135; //PM_SYS_CLK/2;
uint8_t  SD_Freq=20;
*/

//********************************************************
//*          IRQ Functions                               *
//********************************************************

uint8_t PM_IRQ_Raise(uint8_t Source,uint8_t Arg)
{
    // Should check if Ok before
    if (Source==IRQ_NE2000) 
      {
        //PM_INFO("w%d ",PM_Config->WifiIRQ);
        PM_INFO("w");

        BV_IRQSource=IRQ_IRQ3;
        switch(PM_Config->WifiIRQ) 
           {
        case 3: BV_IRQSource=IRQ_IRQ3;
               break;
        case 5: BV_IRQSource=IRQ_IRQ5;
               break;
        case 7: BV_IRQSource=IRQ_IRQ7;
               break;               
        case 9: BV_IRQSource=IRQ_None;
               break;      
           }
      }
       else BV_IRQSource=Source;

    PM_INFO("i%d,%d ",BV_IRQSource,BV_IRQStatus);
    switch(BV_IRQStatus)
     {
       case 1:PM_ERROR(" Not Started ");
              break;     
       case 2: PM_ERROR(" In progress ");
              break;
       case 0x12: PM_ERROR(" Disabled ");
              break;
       case 0x13: PM_ERROR(" Invalid ");
              break; 
       case 0x14: PM_ERROR(" Same IRQ ");
              break;
     }

    BV_IRQArg=Arg;
    BV_IRQStatus=IRQ_Stat_REQUESTED;
#if UART_PIN0
#else
    if (IRQ_raised)
       {
        PM_ERROR(" IRQ still Up %d",BV_IRQStatus);
        gpio_put(PIN_IRQ,1);  // IRQ Down
        busy_wait_us(100);
       }
    gpio_put(PIN_IRQ,0);      // IRQ Up
    IRQ_raised=true;
#endif    
    return 0;
}

void PM_IRQ_Lower()
{
//    PM_INFO("l");
#if UART_PIN0
#else    
    gpio_put(PIN_IRQ,1);  // IRQ Down
    IRQ_raised=false;    
#endif    
}

//********************************************************
//*          PSRAM Initialisation and test Code          *
//********************************************************

 // Test with DMA Code : PM 2.0A
 // Clock, Fudge
 // 200, False : Failure then Ok (Before / After SD init) 
 // 8b: 1331827 B/s 32b: 3788536 B/s
 // 210, False : Ko  (With and Without SD)
 // 210, True  : Failure then Ok (Before / After SD init) No SD : Ok Before and After 
 // 8b: 1332057 B/s 32b: 3786416 B/s
 // 240, True  : Failure then Ok (Before / After SD init) No SD : Ok Before and After 
 // 8b: 1425580 B/s 32b: 4125620 B/s
 // 260, True  : Failure then Ok (Before / After SD init) No SD : Ok Before and After 
 // 8b: 1480619 B/s 32b: 4355908 B/s
#define PSRAM_FREQ 220

// PM_StartPSRAM: Initialise the External PSRAM return true if 8Mb detected
// return the memory size in MB or Init error/Disabled code
uint8_t PM_StartPSRAM()
{
#if USE_PSRAM
 PM_INFO("SPI PSRAM Init: ");

#if USE_PSRAM_DMA
 float div = (float)clock_get_hz(clk_sys) / (PSRAM_FREQ*1000000); //
 PM_INFO("PSRAM freq %d, div (DMA) : %f ",PSRAM_FREQ,div);
 psram_spi = psram_spi_init_clkdiv(pio1, -1, div, true);
// Modify again the output pad
 gpio_set_drive_strength(PSRAM_PIN_CS, GPIO_DRIVE_STRENGTH_8MA);
 gpio_set_drive_strength(PSRAM_PIN_SCK, GPIO_DRIVE_STRENGTH_8MA);
 gpio_set_drive_strength(PSRAM_PIN_MOSI, GPIO_DRIVE_STRENGTH_8MA);
 /* gpio_set_slew_rate(PSRAM_PIN_CS, GPIO_SLEW_RATE_FAST); */
 gpio_set_slew_rate(PSRAM_PIN_SCK, GPIO_SLEW_RATE_FAST);
 gpio_set_slew_rate(PSRAM_PIN_MOSI, GPIO_SLEW_RATE_FAST);

#else
 psram_spi = psram_init();
#endif

 uint FirstErr=8; 
 uint i=0;

do   { // "Test 8Mb"
     uint32_t addr=i*1024*1024;
     psram_write8(&psram_spi, addr,0xAA);
     if (psram_read8(&psram_spi, addr)!=0xAA) FirstErr=i;
     psram_write8(&psram_spi, addr,0x55);
     if (psram_read8(&psram_spi, addr)!=0x55) FirstErr=i;
    } while ((FirstErr==8) && (++i<8));

 if (FirstErr==8)
     {
      EPSRAM_Size=i;
      EPSRAM_Available=true;
      EPSRAM_Enabled=true;

      PM_INFO("OK,%d Mb \n",i);
      return i;  
     }
    else 
     {
      PM_ERROR("PSRAM Init Error\n");

      return 0xFD;  // Failed
     }
#else
return 0xFF;   // Disabled
#endif
} // PM_StartPSRAM

// PM_EnablePSRAM: Re configure the PIO pin direction for External PSRAM
bool PM_EnablePSRAM()
{
#if USE_PSRAM  
 if (EPSRAM_Available)
    {
     SD_Enabled=false;
#ifndef PIMORONI_PICO_PLUS2_RP2350     
     gpio_set_function(SD_SPI_SCK, GPIO_FUNC_PIO1);
     gpio_set_function(SD_SPI_MOSI, GPIO_FUNC_PIO1);
     gpio_set_function(SD_SPI_MISO, GPIO_FUNC_PIO1);
// printf(">Enable PSRAM sm %d \n",psram_spi.sm);
#endif
     EPSRAM_Enabled=true;
     return true;
    }
    else
    {
     return false;
    }
#else
EPSRAM_Enabled=false;
return false;
#endif    
}

FATFS SDFS;  // FileSystem Object for the SD

//********************************************************
//*          SDCARD Initialisation and test Code         *
//********************************************************
bool PM_StartSD()
{
 sd_card_t *pSD = sd_get_by_num(0);
 char drive_path[3] = "0:";

 FRESULT fr = f_mount(&SDFS,drive_path, 1);
 if (FR_OK != fr) { 
     PM_ERROR("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
     BV_SDInit=0xFD;  // Fail
     return false;
    }
 else
  {
    SD_Available=true;
    SD_Enabled=true;
    BV_SDInit=0;      // Status in Memory
    return true;
  }    
} // PM_StartSD

#if TEST_SDCARD
bool PM_TestSD()
{
    if (!SD_Enabled) return 0;

    sd_card_t *pSD = sd_get_by_num(0);
    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr) panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);

    PM_ERROR(">Test SDCard file Write (No error if Ok)\n");
    FIL fil;
    const char* const filename = "filename.txt";
    
    for (int i=0; i<20; i++)
    {
    fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr)
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    if (f_printf(&fil, "Hello, world!\n") < 0) {
        PM_ERROR("f_printf failed\n");
    }
    fr = f_close(&fil);
    if (FR_OK != fr) {
        PM_ERROR("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }
    }

//  f_unmount(pSD->pcName);
    puts(">Test End!");

return true;
} // PM_TestSD
#endif // TEST_SDCARD

// PM_EnableSD : Re configure the PIO pin direction for PSRAM
bool PM_EnableSD()
{
//  PM_INFO(">PM_EnableSD : SD Available: %d SD_Enabled %d ",SD_Available,SD_Enabled);
  if (SD_Available)
    {
     if (!SD_Enabled)
      {
       EPSRAM_Enabled=false;
       gpio_set_function(SD_SPI_SCK, GPIO_FUNC_SPI);
       gpio_set_function(SD_SPI_MOSI, GPIO_FUNC_SPI);
       gpio_set_function(SD_SPI_MISO, GPIO_FUNC_SPI);  
 //      PM_INFO(">Enable SD SPI \n");
       SD_Enabled=true;
      }
     return true;
    }
    else
    { 
      SD_Enabled=false;
      return false;
    }
}

// ***********************************
// ***** USB  Start / Stop Code  *****
// ***********************************
// PM_StartUSB: Initialise the USB Host Mode
bool PM_StartUSB()
{
 PM_INFO("USB Host Init: ");	
#if USE_USBHOST
 if (!SERIAL_USB)
  {
   PM_INFO("Ok\r\n");
   // init host stack on configured roothub port
   tuh_init(BOARD_TUH_RHPORT);

//   pm_mouse.div=4;
   USB_Host_Enabled=true;
   BV_USBInit=0;    // Success 
   return true;
  } 
  else
  {
   PM_INFO("Fail : Serial USB is Active\r\n");    
   BV_USBInit=0xFD;  // Failed  
   return false;
  }
#else    //  if !DO_USBHOST
   PM_INFO2("USE_USBHOST!=1 \n ");
   BV_USBInit=0xFD;  // Failed   
return false;
#endif   
}

void PM_StopUSB()
{
 USB_Host_Enabled=false;
 BV_USBInit=0xFF;   // Disabled
}

// *******************************
// ***** Audio On / On Code  *****
// *******************************
void PM_Adlib_OnOff(uint16_t state)
{
 if (state)
     {
      PM_INFO("> Adlib On\n");      
      dev_adlib_install();           // Will not reinstall if already installed
     }
     else
     {
      PM_INFO("> Adlib Off\n");
      dev_adlib_remove();
     }

} //PM_Adlib_OnOff

// Return 0 When Ok
uint8_t PM_CMS_OnOff(uint16_t state)
{

 if (state)
     {
       if (state==1) state=0x220;    // Default CMS Port
       PM_INFO("> CMS On %x\n",state);
       return dev_cms_install(state);
     }
     else
     {
       PM_INFO("> CMS Off\n");      
       dev_cms_remove();
       return 0;
     }
} //PM_CMS_OnOff

// Return 0 When Ok
uint8_t PM_TDY_OnOff(uint16_t state)
{
 if (state)
     {
      if (state==1) state=0x2C0;    // Default Tandy Port
      PM_INFO("> Tandy On %x\n",state);      
      return dev_tdy_install(state);
     }
     else
     {
      dev_tdy_remove();
      PM_INFO("> Tandy Off\n");        
      return 0;
     }
} //PM_TDY_OnOff

// Return 0 When Ok
uint8_t PM_sbdsp_OnOff(uint16_t state)
{
 if (state)
     {
      if (state==1) state=0x220;    // Default SB DSP port
      dev_dma_install();
      PM_INFO("> SB DSP On %x\n",state);
      return dev_sbdsp_install(state);
     }
     else
     {
      dev_sbdsp_remove();
      PM_INFO("> SB DSP Off\n");        
      return 0;
     }
} //PM_sbdsp_OnOff

// PM_Audio_OnOff : Enablez/Disable the Audio device / Sound cards
// state : 0: Off / 1: On
void PM_Audio_OnOff(uint8_t state)
{
  if (state)
    {
      PM_Adlib_OnOff(PM_Config->Adlib);
      PM_CMS_OnOff(PM_Config->CMSPort);
      PM_TDY_OnOff(PM_Config->TDYPort);
  //    PM_sbdsp_OnOff(PM_Config->SBPort);
  //    PM_sbdsp_OnOff(0x220); 

      quiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
      pm_audio_start();
    }
    else
    {
      PM_Adlib_OnOff(0);
      PM_CMS_OnOff(0);
      PM_TDY_OnOff(0);
    //  PM_sbdsp_OnOff(0);      
      pm_audio_stop();
    }
}    

// PM_Joystick_OnOff : Enable/Disable the Joystick
// state : 0: Off / 1: On
// return false it it can't enable it
bool PM_Joystick_OnOff(uint8_t state)
{
 if (state)
    {
     if (USB_Host_Enabled)
      {
       PM_INFO("-Enable Joystick\n");        
       dev_joystick_install();
       return true;
      }
      else return false;
    }
    else 
    {
      PM_INFO("-Disable Joystick\n");
      dev_joystick_remove();
      return true;      
     }
}

// *************************************************************
// *****     Pico MEMP Commands Run from the 2nd core      *****
// *************************************************************
void PM_ExecCMD()
{
//char spcc[]="Now the PicoMEM Can execute commands to the PC\r\n";
//char spcc2[]="Like Printing this :)\r\n ";
//char spcc3[]="Printf3 ";

//printf("CMD %x Data %x > ",PM_Command,PM_CmdDataL);
if (PM_Command!=0) PM_INFO("CMD %X,%X > ",PM_Command,PM_CmdDataL);


 switch(PM_Command)
  {
   case CMD_Reset :         // CMD 0: Clean the Status or Enable/Disable the commands
    PM_INFO("Reset\n");
    PM_CmdReset=false;      // Disable the Reset (It is completed)
    PM_Status=STAT_READY;
    break;
    
   case CMD_SetBasePort :   // CMD 2: Set the board base port !! To do !!
    PM_INFO("SetBasePort\n");
    PM_Status=STAT_READY;
    break;
   case CMD_GetDEVType :    // CMD 3: Get the Device Type (Port)
    if (PM_CmdDataL<128) {PM_CmdDataH=GetPortType(PM_CmdDataL>>3);}
    PM_Status=STAT_READY;
    break;    
   case CMD_SetDEVType :    // CMD 4: Set the Device Type (Port)
    if (PM_CmdDataL<128) {SetPortType(PM_CmdDataL>>3,PM_CmdDataH,1);}
    PM_Status=STAT_READY;
    break;
   case CMD_GetMEMType :    // CMD 5: Get the Memory Type (16K Block)
    if (PM_CmdDataL<64) {PM_CmdDataH=GetMEMType(PM_CmdDataL<<14); } //MEM_Index_T[PM_CmdDataL^(0xF0000>>14)];
       else {PM_CmdDataH=0;}
    PM_Status=STAT_READY;
    break;
   case CMD_SetMEMType :    // CMD 6: Set the Memory Type (16K Block)
    if (PM_CmdDataL<64)     // Used to perform emulation RAM Test (MEM MAP and Diag)
       {
        uint32_t Addr=PM_CmdDataL<<14;
        PM_INFO("A:%x T:%x %x ",PM_CmdDataL,PM_CmdDataH,Addr);

        if (PM_CmdDataH==MEM_RAM) 
           {
            PM_Memory[0]=0x13;      // ID at the begining of the RAM, for the detection
            PM_Memory[0]=0x12;
            RAM_InitialAddress=Addr;
            RAM_Offset_Table[MEM_RAM]=&PM_Memory[-RAM_InitialAddress];            
           }
        SetMEMType(Addr,PM_CmdDataH,MEM_S_16k);  // Define a 16Kb RAM Block 44
        SetMEMIndex(Addr,PM_CmdDataH,MEM_S_16k);           
       }
    PM_Status=STAT_READY;
    break;
   case CMD_MEM_Init :       // CMD 7: Initialize the memory emulation based on the Config
// Started after the Setup / before the Boot
// RAM initialisation can't be done if the BIOS Does not work / is not enabled.
     PM_INFO("MEM Init\n");    

     PM_INFO("PC RAM: %dKb\n",PC_MEM+256*PC_MEM_H);

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

    // 1- Remove all emulated RAM

    dev_memory_remove_ram();
 
    PM_INFO("\nAfter Clean: ");
    for (int i=0;i<MEM_T_Size;i++)
        { if (i==10*8) PM_INFO("- "); 
          PM_INFO("%d ",GetMEMType(i*8*1024));
        }
    PM_INFO("\n");

    // 2- Initialize the EMS Port and RAM

    DelPortType(DEV_LTEMS);
    dev_ems_install(EMS_Port_List[PM_Config->EMS_Port]<<3,EMS_Addr_List[PM_Config->EMS_Addr]<<14);

    // 3- Configure the RAM and PSRAM
    dev_memory_install(MEM_RAM,0);
    dev_memory_install(MEM_DISK,0);
#if USE_PSRAM    
    dev_memory_install(MEM_PSRAM,0);
#endif

    PM_INFO("Mem Type: ");
    for (int i=0;i<MEM_T_Size;i++)
        { if (i==10*8) PM_INFO("- "); 
          PM_INFO("%d ",MEM_Type_T[i]);
        }

     PM_Status=STAT_READY;
    break;

   case CMD_TDY_Init :  // Initialize the Tandy conventionnal RAM emulation
    PM_INFO("Tandy RAM : %dx16Kb\n",BV_TdyRAM);
    uint8_t PC_RamSize_Bloc;
    PC_RamSize_Bloc=(PC_MEM+256*PC_MEM_H)/16;  //Compute the Number of 16 Kb Bloc RAM on the PC
    
    if ((PC_RamSize_Bloc+BV_TdyRAM)>40)
       {
        PM_INFO("Err, Too many RAM, Adjust size %d\n",PC_RamSize_Bloc+BV_TdyRAM);
        BV_TdyRAM=40-PC_RamSize_Bloc;        
       }

     if (BV_TdyRAM!=0)
      {

      if (MAX_PMRAM!=0)      // Extend first with PMRAM
         {
          int AddBlock_NB;
          AddBlock_NB=BV_TdyRAM;
          if (BV_TdyRAM>MAX_PMRAM) AddBlock_NB=MAX_PMRAM;
          PM_INFO("Add %dx16K PMRAM \n",AddBlock_NB);
          RAM_Offset_Table[MEM_RAM]=&PM_Memory[0];        // Place the RAM at the begining
          SetMEMType(0,MEM_RAM,MEM_S_16k*AddBlock_NB);    // MEMType is "for info"
          SetMEMIndex(0,MEM_RAM,MEM_S_16k*AddBlock_NB);   // MEMIndex is used for decoding
          for (int i=0;i<MAX_PMRAM;i++)
           {PM_Config->MEM_Map[i]=MEM_RAM;}               // Update the Memory MAP
         }

#if USE_PSRAM
     if (BV_TdyRAM>MAX_PMRAM) // Then PSRAM
        {  // Add Tandy RAM as PSRAM From the Address MAX_PMRAM*+16KB, size (BV_TdyRAM-MAX_PMRAM)*16KB  
          PM_INFO("Add %dx16K PSRAM, Addr %x \n",BV_TdyRAM-MAX_PMRAM,MAX_PMRAM<<14);     
          uint32_t MEM_Addr=MAX_PMRAM<<14;  // Shift 14 for 16Kb Block
          SetMEMType(MEM_Addr,MEM_PSRAM,MEM_S_16k*(BV_TdyRAM-MAX_PMRAM));
          SetMEMIndex(MEM_Addr,MEM_PSRAM,MEM_S_16k*(BV_TdyRAM-MAX_PMRAM));
          for (int i=MAX_PMRAM;i<BV_TdyRAM;i++)
           {PM_Config->MEM_Map[i]=MEM_PSRAM;}           // Update the Memory MAP
        }
      for (int i=BV_TdyRAM;i<(BV_TdyRAM+PC_RamSize_Bloc);i++)
           {PM_Config->MEM_Map[i]=SMEM_RAM;}           // Update the Memory MAP
      PM_Config->MEM_Map[(BV_TdyRAM+PC_RamSize_Bloc)]=SMEM_VID;  // Last 16Kb is Video

#endif     
      }
     for (int i=0;i<40;i++) PM_INFO("%d ",PM_Config->MEM_Map[i]);
         
     PM_Status=STAT_READY;
    break;

   case CMD_Test_RAM :       // Test Shared RAM (Check if working or cache active)
//   PM_INFO("Test RAM ");   // Used by the BIOS
     uint8_t temp;

     temp=BV_Test;
     BV_Test=PM_CmdDataL;
     PM_CmdDataL=temp;

     PM_Status=STAT_READY;
     break;


   case CMD_TESTIO :       // Test Data transfer via IO
     PM_INFO("Test IO ");

     test_pmio_transfer();

     PM_Status=STAT_READY;
     break;
    


   case CMD_SaveCfg :       // CMD: Save the Configuration to the uSD

     PM_INFO("Save Config to CF\n");
 
     if (PM_EnableSD())
      { 
        pm_audio_pause();     // Need to pause audio during uSD Access         
        uint8_t SC_return=PM_SaveConfig();
        pm_audio_resume();         
        if (SC_return!=0)
           {
            PM_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation     
            PM_ERROR("Save Config Error %d\n",SC_return);
     
            PM_CmdDataL=SC_return;   // Return an error code          
            PM_Status=STAT_CMDERROR;         
            break;
           }
      }
     PM_EnablePSRAM();      // Re enable the PSRAM for the Memory emulation        
     PM_Status=STAT_READY;
    break;

   // Initialize the devices back to their Default state (Command sent by the BIOS just before the Boot)
   case CMD_DEV_Init :

     PM_INFO("CMD_DEV_Init: Disable Mouse\n");
     PM_INFO("-Disable Mouse\n");
     MOUSE_Enabled=false;  // Mouse is disabled by default (Enabled by PMMOUSE)
// add SD and PSRAM Speed config here

     PM_INFO("-Set SD: %d PSRAM %d (MHz)\n",PM_Config->SD_Speed,PM_Config->RAM_Speed);

#if USE_USBHOST
     PM_Joystick_OnOff(PM_Config->EnableJOY);
#endif

#if PM_PICO_W
#if USE_NE2000 
     DelPortType(DEV_NE2000);                            // Remove the ne2000 Ports
     if ((PM_Config->ne2000Port!=0)&&(PM_Config->ne2000Port<0x3E0))
        {         
         SetPortType(PM_Config->ne2000Port,DEV_NE2000,4); // Add the ne2000 Ports
        }
        else PM_Config->ne2000Port=0;  // Force 0 if > 3E0

     PM_INFO("-NE2000: P %x I %d\n",PM_Config->ne2000Port,PM_Config->WifiIRQ);
#endif //USE_NE2000
#endif

#if USE_AUDIO
      PM_Audio_OnOff(PM_Config->AudioOut);
#endif

      PM_Status=STAT_READY;
      break;

   case CMD_GetPMString : 

    PM_Status=STAT_READY;
    break;
    
   case CMD_SetMEM :        // CMD: 22 SET 64K of memory to the value
 //    printf("SETMEM %x ",PM_SetRAMVal);
     //setmem()
     for (uint32_t i=0 ; i<65536 ; ++i)
         {
          PM_Memory[i]=PM_CmdDataL;
          //printf("%x: %x,",i,PM_Memory[i]);
         }
     PM_Status=STAT_READY;
     break; 
   case CMD_TestMEM :      // 23
     PM_TestRAMVal=PM_CmdDataL;
     for (uint32_t i=0 ; i<65536 ; ++i)
         {
          if ((PM_Memory[i])!=PM_TestRAMVal) 
             {
              PM_Status=STAT_CMDERROR;
              break;
             }
         ;}
     PM_Status=STAT_READY;
     break; 

   case CMD_StartUSBUART :  // CMD: Do stdio init

     PM_Status=STAT_READY;
    break;

   case CMD_SendIRQ :      // CMD: Enable the IRQ

     PM_INFO("PIN_IRQ: 1\n");

#if UART_PIN0
#else
     gpio_put(PIN_IRQ,0);  // IRQ Down   
#endif     
     PM_Status=STAT_READY;
     break;

   case CMD_IRQAck :       // CMD: Acknowledge IRQ
     
     PM_INFO("PIN_IRQ: 0\n");
#if UART_PIN0
#else 
     gpio_put(PIN_IRQ,1);  // IRQ Up    
#endif     
     PM_Status=STAT_READY;
     break;     

#define CMD_S_GotoXY      0x46  // Change the cursor location (Should be in 80x25)
#define CMD_S_GetXY       0x47  // 
#define CMD_S_SetAtt      0x48
#define CMD_S_SetColor    0x49
   case CMD_S_Init :       // Serial Output Init (Resolution) and clear Screen
 
	   PM_Status=STAT_READY;
     break;
   case CMD_S_PrintStr :   // Print a Null terminated string from xxx
 
	   PM_Status=STAT_READY;
     break;
   case CMD_S_PrintChar :  // Print a char (From RegL)
     if (SERIAL_Enabled) printf("%c",PM_CmdDataL);
	   PM_Status=STAT_READY;
     break;
   case CMD_S_PrintHexB :  // Print a Byte in Hexa
     if (SERIAL_Enabled) printf("%x",PM_CmdDataL);
	   PM_Status=STAT_READY;
     break;
   case CMD_S_PrintHexW :  // Print a Word in Hexa
     if (SERIAL_Enabled) printf("%x",PM_CmdDataH*265+PM_CmdDataL);
	   PM_Status=STAT_READY;
     break;
   case CMD_S_PrintDec :  // Print a Word in Decimal
     if (SERIAL_Enabled) printf("%d",PM_CmdDataH*265+PM_CmdDataL);
	   PM_Status=STAT_READY;
     break;

// *** USB / HID Commands (Human Interface Devices) ***

   case CMD_USB_Enable :    // CMD: 0x50

      PM_INFO("Enable USB\n");
   
      PM_CmdDataL=PM_StartUSB();    
      PM_Status=STAT_READY;
     break;

   case CMD_USB_Disable :     // CMD: 0x51

      PM_INFO("Disable USB\n");
   
      PM_StopUSB();  // Does not readdy Stop the USB, but Block the USB Devices IRQ : To Correct (If possible)
      PM_CmdDataL=0;
      PM_Status=STAT_READY;
     break;

   case CMD_Mouse_Enable :    // CMD: 0x52

      PM_INFO("Enable Mouse\n");
   
      MOUSE_Enabled=true;
      PM_CmdDataL=0;
      PM_Status=STAT_READY;
     break;

   case CMD_Mouse_Disable :   // CMD: 0x53

      PM_INFO("Disable Mouse\n");
      MOUSE_Enabled=false;
      PM_CmdDataL=0;
      PM_Status=STAT_READY;
     break;

   case CMD_Keyb_OnOff :     // CMD: 0x54

      PM_INFO("Keyboard : ");
      if (PM_CmdDataL) 
         {
          KEYB_Enabled=true;
          printf("On\n");
         }
         else
         {
          KEYB_Enabled=false;
          printf("Off\n");
         }
      PM_CmdDataL=0;
      PM_Status=STAT_READY;
     break;  

   case CMD_Joy_OnOff:        // USB Joystick : 0: Off 1: on
      uint16_t arg;
      arg=((uint16_t)PM_CmdDataH<<8)+(uint16_t)PM_CmdDataL;
      printf("%X",arg);
      PM_CmdDataL=PM_Joystick_OnOff(arg);  
      PM_CmdDataH=0;
      PM_Status=STAT_READY;      
     break;
	 
   case CMD_Wifi_Infos	:   // CMD: 0x60  Get the Wifi Status, retry to connect if not connected
#if PM_PICO_W
#if USE_NE2000   
      
      if (BV_WifiInit==0)
       {
        PM_Wifi_GetStatus();
        if ((PM_Wifi.Status==1)&&(PM_Wifi.rssi==0))
          {
           PM_INFO("Retry Wifi connection\n");
           PM_RetryWifi();
          }
       }

      memcpy((void *)&PCCR_Param,&PM_Wifi,sizeof(PM_Wifi));    

      PM_INFO("Wifi Info: Port %x IRQ %d",PM_Config->ne2000Port,PM_Config->WifiIRQ);
      PM_INFO(" SSID %s \n",PM_Wifi.WIFI_SSID);

#endif //USE_NE2000
#endif //PM_PICO_W
      PM_Status=STAT_READY;
     break;

   case CMD_USB_Status	:   // CMD: 0x61  Get the USB Status string
      uint8_t usbnb;
      char *str_tmp;
      str_tmp=(char *) &PCCR_Param;

      usb_print_status();
      if ((PM_Config->EnableUSB)==1)
        {
         usbnb=usb_get_devnb();
    
         str_tmp[0]=usbnb+1;  // Nb of Lines to display
         str_tmp++;
         sprintf(str_tmp,"   USB  : %d device%c", usbnb, usbnb == 1 ? ' ' : 's');
         str_tmp[1]=0xFE;
         PM_INFO("%s",str_tmp);
         usbnb=strlen(str_tmp);
         str_tmp+=usbnb+1;        // Move pointer at the end of the current string
         for (uint8_t i = 0; i < CFG_TUH_DEVICE_MAX; i++)
             if (tuh_mounted(i + 1))
              {
               //str_tmp[0]=0;
               sprintf(str_tmp,"    %s",dev_message[i]);
               printf("%s\n",str_tmp);
               str_tmp+=strlen(str_tmp)+1;   // Move pointer at the end of the current string
              }
        }
        else 
        {
         str_tmp[0]=0;  // Nothing to Display (No USB)
        }
      PM_Status=STAT_READY;
     break;

   case CMD_DISK_Status	:   // CMD: 0x64 Get the Disk Status strings
//      char *str_tmp;
      char *str_disklist;
      str_tmp=(char *) &PCCR_Param;
      str_disklist=str_tmp;
      str_tmp[0]=0;  // Clean the Nb of lines to display
      str_tmp++;

      for (int i=0;i<6;i++)
         {
          //printf("i %d",i);
          if (i<2)
           {  // Floppy
           //printf("Attr%x",PM_Config->FDD_Attribute[i]);
           if ((PM_Config->FDD_Attribute[i]>=0x80))
            {
             // Generate the FDD info string              
             sprintf(str_tmp,"   FDD%d : %-13s  [%4dKB]",i,PM_Config->FDD[i].name,PM_Config->FDD[i].size);
             PM_INFO("%s\n",str_tmp);             
             str_tmp[1]=0xFE;         // Add the special character
             usbnb=strlen(str_tmp);
             str_tmp+=usbnb+1;        // Move pointer at the end of the current string
             str_disklist[0]++;
            }
           }
           else
           {  // HDD
           //printf("Attr%x",PM_Config->HDD_Attribute[i-2]);
           if ((PM_Config->HDD_Attribute[i-2]>=0x80))
            {
             // Generate the HDD info string
             sprintf(str_tmp,"   HDD%d : %-13s  [%4dMB] - C:%d H:%d S:%d",i-2,PM_Config->HDD[i-2].name,PM_Config->HDD[i-2].size,
             imageDiskList[i]->cylinders,imageDiskList[i]->heads,imageDiskList[i]->sectors);
             PM_INFO("%s\n",str_tmp);
             str_tmp[1]=0xFE;          // Add the special character
             usbnb=strlen(str_tmp);
             str_tmp+=usbnb+1;        // Move pointer at the end of the current string
             str_disklist[0]++;
            }
           }
          }
      PM_Status=STAT_READY;
     break;

// *** Audio Commands  0x8x ***

   case CMD_AudioOnOff:     // Full audio rendering 0: Off 1:On
      PM_Audio_OnOff(PM_CmdDataL);
      PM_Status=STAT_READY;
     break;

   case CMD_AdlibOnOff:      // Adlib Audio : 0 : Off 1: On default
      arg=((uint16_t)PM_CmdDataH<<8)+(uint16_t)PM_CmdDataL;
////      printf("%X %X %X",PM_CmdDataH,PM_CmdDataL,arg);     
      PM_Adlib_OnOff(arg);
      quiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
      pm_audio_start();                // Start Audio if not on
      PM_Status=STAT_READY;
     break;

   case CMD_TDYOnOff:        // Tandy Audio : 0 : Off 1: On default or port, return install status
      arg=((uint16_t)PM_CmdDataH<<8)+(uint16_t)PM_CmdDataL;
//      printf("%X %X %X",PM_CmdDataH,PM_CmdDataL,arg);
      PM_CmdDataL=PM_TDY_OnOff(arg);
      quiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
      pm_audio_start();                // Start Audio if not on    
      PM_CmdDataH=0;
      PM_Status=STAT_READY;      
     break;

   case CMD_CMSOnOff:        // CMS Audio   : 0 : Off 1: On default or port, return install status
      arg=(uint16_t)PM_CmdDataH<<8+(uint16_t)PM_CmdDataL;
//      printf("%X %X %X",PM_CmdDataH,PM_CmdDataL,arg);
      PM_CmdDataL=PM_CMS_OnOff(((uint16_t)PM_CmdDataH<<8)+(uint16_t)PM_CmdDataL);
      quiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
      pm_audio_start();                // Start Audio if not on     
      PM_CmdDataH=0;
      PM_Status=STAT_READY;
     break;

   case CMD_SetSBIRQ:        // GUS Audio   : 0 : Off 1: On default or port
      
      PM_Status=STAT_READY;
     break;

   case CMD_SBOnOff:        // Sound Blaster Audio : 0 : Off 1: On default or port  !! Need to add IRQ
      arg=(uint16_t)PM_CmdDataH<<8+(uint16_t)PM_CmdDataL;
//      printf("%X %X %X",PM_CmdDataH,PM_CmdDataL,arg);
      PM_CmdDataL=PM_sbdsp_OnOff(((uint16_t)PM_CmdDataH<<8)+(uint16_t)PM_CmdDataL);
      quiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
      pm_audio_start();                // Start Audio if not on     
      PM_CmdDataH=0;
      PM_Status=STAT_READY;      
      PM_Status=STAT_READY;
     break;


   case CMD_GUSOnOff:        // GUS Audio   : 0 : Off 1: On default or port
      
      PM_Status=STAT_READY;
     break;

// *** Disk Commands  0x8x ***
   case CMD_HDD_Getlist :  // CMD: 
     pm_audio_pause();     // Need to pause audio during uSD Access 
//PC_DiskNB=2;    // Test / Debug
      uint8_t nb;
      PM_CmdDataL=ListImages(1,&nb);
      PM_EnablePSRAM();    // Re enable the PSRAM for the Memory emulation      
      PM_Status=STAT_READY;
     pm_audio_resume();      
     break;

   case CMD_FDD_Getlist :  // CMD: 
     pm_audio_pause();     // Need to pause audio during uSD Access   
      PM_CmdDataL=ListImages(0,&nb);
      PM_EnablePSRAM();    // Re enable the PSRAM for the Memory emulation      
      PM_Status=STAT_READY;
     pm_audio_resume();        
     break;

   case CMD_HDD_Getinfos :  // CMD: Write the hdd infos in the Disk Memory buffer
     
     // memcpy(PM_DP_RAM,defaultdiskname,strlen(defaultdiskname)+1);
     //memcpy(,&PCCR_Param,sizeof());
     PM_Status=STAT_READY;
     break;
     
    case CMD_HDD_Mount :  // CMD: Mount the HDD images
   pm_audio_pause();      // Need to pause audio during uSD Access    
     if (PM_EnableSD())
      {
       char imgpath[24];
    
       PM_INFO("HDD Mount\n");

       for (int i=0;i<4;i++)
         {
           f_close(&HDDfile[i]);  // Close the previous HDD file
           PM_Config->HDD_Attribute[i]=i;
           if (imageDiskList[i+2]!=NULL)
           { 
            delete imageDiskList[i+2];   // Delete the previous HDD Object
            imageDiskList[i+2]=NULL;
           }
        //Mount the disk image
           if ((PM_Config->HDD[i].size>>8)!=0xFE)
            {        
            if (strcmp(PM_Config->HDD[i].name,""))  // Skip if disk name is empty
               { // Create the disk image Path string
                //sprintf(imgpath, "1:/HDD/%s.img", PM_Config->HDD[i].name);
                sprintf(imgpath, "0:/HDD/%s.img", PM_Config->HDD[i].name);
                PM_INFO("HDD%d: %s\n",i,imgpath);
              
                int8_t mountres=pm_mounthddimage(imgpath,i+1);    // Mount the disk image
                if (!mountres) PM_Config->HDD_Attribute[i]=0x80;  // res=0 > PicoMEM Disk
               }
            } else  // Not an image, but a Physical disk redirection
            {
             PM_Config->HDD_Attribute[i]=((uint8_t) PM_Config->HDD[i].size)&0x03;
             PM_INFO("PC Disk nb: %x %x\n",(uint8_t) PM_Config->HDD[i].size,PM_Config->HDD_Attribute[i]);
            }
         }
      }
     
     New_DiskNB=0;  // count the total number of HDD (To update the BIOS)
     for (int i=0;i<4;i++)
      {
        if (PM_Config->HDD_Attribute[i]>=0x80) New_DiskNB++;      // Image Mounted
        if (PM_Config->HDD_Attribute[i]<PC_DiskNB) New_DiskNB++;  // Read existing Floppy        
        PM_INFO(" - %s Attr:%x\n",PM_Config->HDD[i].name,PM_Config->HDD_Attribute[i]);
      }
     PM_INFO("Total HDD : %d / %d\n",New_DiskNB,PC_DiskNB);

     PM_INFO2("HDD Mount End\n");
     PM_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation   
     PM_Status=STAT_READY;
     pm_audio_resume();
     break;

    case CMD_FDD_Mount :  // CMD: Mount the floppy images
   pm_audio_pause();   // Need to pause audio during uSD Access        

     PM_INFO("FDD Mount\n");

     if (PM_EnableSD())
      {
       char imgpath[24];

       for (int i=0;i<2;i++) f_close(&FDDfile[i]); // Close all the HDD files
       for (int i=0;i<2;i++)
         {
           PM_Config->FDD_Attribute[i]=i;
             if (imageDiskList[i]!=NULL) 
              {
               delete imageDiskList[i];
               imageDiskList[i]=NULL;
              }
           if ((PM_Config->FDD[i].size>>8)!=0xFE)
           {
            if (strcmp(PM_Config->FDD[i].name,""))
               {
                sprintf(imgpath, "0:/FLOPPY/%s.img", PM_Config->FDD[i].name);
                 
                PM_INFO("FDD%d: %s\n",i,imgpath);
               
                int8_t mountres=pm_mountfddimage(imgpath,i);       // Mount the floppy image
                if (!mountres) PM_Config->FDD_Attribute[i]=0x80;   // res=0 > PicoMEM Disk
               }
           } else  // Not an image, but a Physical disk redirection
           {
             PM_Config->FDD_Attribute[i]=((uint8_t) PM_Config->FDD[i].size)&0x03;
             PM_INFO("PC Floppy nb: %x %x",(uint8_t) PM_Config->FDD[i].size,PM_Config->FDD_Attribute[i]);
           }
         }
      }

     PM_INFO("Floppy Mount End\n");

     if (PC_FloppyNB<2)   // To not reduce the Nb of floppy if >=2
       {
        New_FloppyNB=0;
         for (int i=0;i<2;i++)
          {
           if (PM_Config->FDD_Attribute[i]>=0x80) New_FloppyNB++;         // Image Mounted
           if (PM_Config->FDD_Attribute[i]<PC_FloppyNB) New_FloppyNB++;  // Read existing Floppy
           PM_INFO(" - %s Attr:%x\n",PM_Config->FDD[i].name,PM_Config->FDD_Attribute[i]);
         }
       } else New_FloppyNB=PC_FloppyNB;
     PM_INFO("Total FDD : %d / %d\n",New_FloppyNB,PC_FloppyNB);

     PM_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation   
     PM_Status=STAT_READY;
     pm_audio_resume();
     break;  

   case CMD_HDD_NewImage  :   // CMD: 
   pm_audio_pause();   // Need to pause audio during uSD Access    
     PM_INFO("CMD_HDD_NewImage\n");
     if (PM_EnableSD()) 
      {
        PM_INFO("NewHDDImage();\n");
        uint8_t NI_return=NewHDDImage();   
        if (NI_return!=0)
           {
            PM_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation     

            PM_INFO("Error:%d\n",NI_return);
       
            PM_CmdDataL=NI_return;   // Return an error code          
            PM_Status=STAT_CMDERROR;         
            break;
           }
      }
      PM_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation      
      PM_Status=STAT_READY;
    pm_audio_resume();        
     break;
       
    case CMD_Int13h :  // Execute Int 13h, registers are passed via the BIOS Memory
     pm_audio_pause();   // Need to pause audio during uSD Access     
     pm_led(1);
     if (PM_EnableSD()) 
      {
        INT13_DiskHandler();        
      }
       else
      {
       reg_ah=4;     // Return error 4
       reg_al=0xFF;
       reg_flagl=1;  // Set Carry (Bit 0)
      }

     PM_EnablePSRAM();   // Re enable the PSRAM for the Memory emulation
     pm_led(0);
    
     PM_Status=STAT_READY;
	   Send_PCC_End();     // Tell the PC Command loop to end
     pm_audio_resume();     
     break;

    case CMD_EthDFS_Send :  // EtherDFS packet exchange
     if (PM_CmdDataL==0)
       {
       pm_audio_pause();      // Need to pause audio during uSD Access     
       pm_led(1);

       //pm_EthDFS_Packet();

       pm_led(0);
    
       PM_Status=STAT_READY;
       pm_audio_resume();  
       } else
       {
        PM_CmdDataL==0xAA;    // Return 0xAA if PM_CmdDataL is <> 0
        PM_Status=STAT_READY;
       }
     break;

    case CMD_DMA_TEST :
     PM_INFO("DMA Test :\n");
     e_dma_start(1, 1, 1, 16);

     PM_INFO("End\n");
     PM_Status=STAT_READY;
     break;

    default:
     PM_ERROR("Not found ! \n");
     PM_Status=STAT_CMDNOTFOUND;  // Command not supported
     break;
  }
}

//#include "pm_debug.h"

#ifdef PIMORONI_PICO_PLUS2_RP2350

 #include "hardware/pll.h"
 #include "hardware/clocks.h"
 #include "hardware/structs/pll.h"
 #include "hardware/structs/clocks.h"

void measure_freqs(void) {
     uint f_pll_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
     uint f_pll_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY);
     uint f_rosc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC);
     uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
     uint f_clk_peri = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI);
     uint f_clk_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB);
     uint f_clk_adc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC);
 #ifdef CLOCKS_FC0_SRC_VALUE_CLK_RTC
     uint f_clk_rtc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_RTC);
 #endif
 
    printf("pll_sys  = %dkHz\n", f_pll_sys);
     printf("pll_usb  = %dkHz\n", f_pll_usb);
     printf("rosc     = %dkHz\n", f_rosc);
     printf("clk_sys  = %dkHz\n", f_clk_sys);
     printf("clk_peri = %dkHz\n", f_clk_peri);
     printf("clk_usb  = %dkHz\n", f_clk_usb);
     printf("clk_adc  = %dkHz\n", f_clk_adc);
 #ifdef CLOCKS_FC0_SRC_VALUE_CLK_RTC
     printf("clk_rtc  = %dkHz\n", f_clk_rtc);
 #endif
 
    // Can't measure clk_ref / xosc as it is the ref
}




void p2_psram_test()
{

uint8_t *test8;
uint8_t *test16;
uint8_t *test32;

//measure_freqs();

 test8=(uint8_t *) __psram_start__;

 printf(">Write Data (8MB)\n");
//    puts("Writing PSRAM...");
 for (uint32_t addr = 0; addr < 8*(1024 * 1024); ++addr) {
     test8[addr]= (uint8_t) (addr & 0xFF);
    }

 printf(">Read Data (8/16/32)\n");
 uint32_t ErrNb=0;
//    puts("Reading PSRAM...");
 uint32_t psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < 8*(1024 * 1024); ++addr) {
     uint8_t result = test8[addr];
     if (static_cast<uint8_t>((addr & 0xFF)) != result) 
         {
         printf("PSRAM failure at address %x (%x != %x)\n", addr, addr & 0xFF, result);
         if (ErrNb++==10) break;
         }
 }
 uint32_t psram_elapsed = time_us_32() - psram_begin;
 float psram_speed = 1000000.0 * 8* 1024.0 * 1024 / psram_elapsed;
 printf("8 bit : PSRAM read 1MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);

/*
printf("ps_total_space: %d\n", ps_total_space());
printf("ps_total_used: %d\n", ps_total_used());
printf("ps_largest_free_block %d\n",ps_largest_free_block());
*/

/*
test8=(uint8_t*) ps_malloc(1024);
if (test8!=NULL)
{
  printf("Allocated : Pointer to a 1024 byte table: %p\n",test8);
  printf("Write/Read 256 bytes:\n",test8);
  for (int i=0;i<256;i++)
  {
    test8[i]=(uint8_t) i;
    printf("%d,",test8[i]);
  }
} else printf("! Pointer not allocated\n");

printf("PSRAM Size :%d \n",ps_size);
printf("ps_total_space: %d\n", ps_total_space());
printf("ps_total_used: %d\n", ps_total_used());
printf("ps_largest_free_block %d\n",ps_largest_free_block());

test8=(uint8_t *) __psram_start__;
for (uint8_t i=0;i<255;i++)
  {
    test8[i*16*1024]=i;
    test8[i*16*1024+1]=i;
    test8[i*16*1024+2]=i;
    test8[i*16*1024+3]=i;
  }
*/
}

#endif

//*******************************************************************************
//*                          2nd Core main code                                 *
//*******************************************************************************
void main_core0 ()
{

// ************  PSRAM Init and Test CODE ******************
pm_timefromlast();
#ifdef PIMORONI_PICO_PLUS2_RP2350

PM_INFO("Init Pimoroni PSRAM:");
setup_psram(false,0);    // With heap start from 0

if (ps_size!=0) 
  { 
    PM_INFO("%dKB\n",ps_size/1024);
    BV_PSRAMInit=8;
  }
  else 
  {
    PM_INFO("Fail\n");
    BV_PSRAMInit=0xFD;  // Failed
  }

p2_psram_test();

#else

#if USE_PSRAM
BV_PSRAMInit=PM_StartPSRAM();
PM_INFO("BV_PSRAMInit: %x\n",BV_PSRAMInit);
#if TEST_PSRAM
PSRAM_Test();     // PSRAM test fail is started before SD init !!!
#endif //TEST_PSRAM
#endif //PSRAM

#endif //PIMORONI_PICO_PLUS2_RP2350

// ************  SDCARD Init and Test CODE ******************
// !!! If uSD Init is done before the PSRAM, Wifi init fail....
#if USE_SDCARD
pm_timefromlast();
PM_INFO("Start uSD\n");

if (PM_StartSD())
   {
    if (PM_LoadConfig()==0) BV_CFGInit=0;  // Success
       else BV_CFGInit=0xFD;  // Failed
    BIOS_SetupDisks();        // Clean the Disk Objects
   } else BV_CFGInit=0xFF;    // Disabled (No Config to load)


if ((BV_CFGInit!=0)||(PM_BIOSADDRESS=0))
   {
    PM_Config->EnableUSB=1;
   }

// Values sent by the code to the BIOS Initialisation, Just after the Config Load
PM_Config->B_MAX_PMRAM=MAX_PMRAM;   // Value defined in the makefile.ini
#if USE_PSRAM
#else
PM_Config->PSRAM_Ext=0; // Force PSRAM emulated RAM Off
#endif

#if TEST_SDCARD
PM_TestSD();
#endif //TEST_SDCARD
#endif //USE_SDCARD

// ************  Qwiic Init and test code ******************

pm_timefromlast();
PM_INFO("Start Qwiic\n");

//#if USE_AUDIO
//quiic_4charLCD_enabled=false;
//#else
pm_qwiic_init(100*1000);
pm_qwiic_scan();

if (quiic_4charLCD_enabled)
 {
  qwiic_display_4char("LED-");

  #if DEV_POST_Enable==1
  dev_post_install(0x80);
  #endif
//pm_qwiic_scan_print();
//ht16k33_demo();
 }
//#endif

// ************  USB Init and Test CODE ******************

pm_timefromlast();
PM_INFO("Start USB\n");

#if USE_USBHOST
  if ((PM_Config->EnableUSB)==1)
   {
    PM_StartUSB();   // Start USB and update BV_USBInit  
   }
  else
   {
    BV_USBInit=0xFF;   // Disabled
    PM_INFO("USB Host Disabled in BIOS\r\n");    
   } 
#else
  BV_USBInit=0xFF;   // Disabled
  PM_INFO("USB Host Disabled in MakeFile\r\n");  
#endif

#if PM_PICO_W
#if USE_NE2000
PM_INFO("Start Wifi\n");

// Be carefull, need to be in uSD Mode to read the wifi.txt file !
PM_EnableSD();
uint8_t WifiRes=PM_ConnectWifi();

if (WifiRes!=0) 
   {
    BV_WifiInit=INIT_SKIPPED;  // 0xFF : Disabled
    PM_Config->ne2000Port=0;
    PM_ERROR("\nWifi Init Err : %d\n",WifiRes);
   }
   else BV_WifiInit=0; // Wifi initialization Ok

#endif //
#endif // 

// The PicoMEM Should start in PSRAM Mode
PM_EnablePSRAM();

// Initialize what is asked in no BIOS Mode
if (PM_BIOSADDRESS==0)
  {

  }

PM_INFO("Init completed\n");

#if TEST_PSRAM  // ! PSRAM Test fail if not after SD Init !
PSRAM_Test();  
#endif //TEST_PSRAM

// After the Config Load, Force some default values (Hardcode, must not remain !)
PM_Config->EnableUSB=1; // Force USB Host for the moment

// display errors to the Qwiic id available
if (BV_PSRAMInit==0xFD) qwiic_display_4char("EPSR");
if (BV_SDInit==0xFD)    qwiic_display_4char("EuSD");

// PicoMEM Init Completed
if (BV_PSRAMInit==INIT_INPROGRESS) BV_PSRAMInit=INIT_SKIPPED;  // Security : Init Skipped
if (BV_SDInit==INIT_INPROGRESS) 
   { 
    BV_SDInit=INIT_SKIPPED;        // Security : Init Skipped
   }
PM_Status=STAT_READY;  // PicoMEM Initialisation completed (The BIOS Wait for its end)
PM_Init_Completed=true;

#if DISP32    // For debug
int TodisplayCnt=0;
#endif

for (;;)  // Core 1 Main Loop (Commands)
{ 

#if DISP32    // For debug
//if (TodisplayCnt++==100)
//   {
     if (To_Display32!=0)
     {
      To_Display32_mask=To_Display32_mask|To_Display32_mask;
      To_Display32_2_mask=To_Display32_2_mask|To_Display32_2_mask;
      PM_INFO("%X:%X ",To_Display32,To_Display32_2);
      To_Display32=0;
     }
    TodisplayCnt=0;
//   }
#endif

#if USE_USBHOST
if (USB_Host_Enabled)
 {
  tuh_task();
  if (pm_mouse.event&&MOUSE_Enabled)
   {
    PM_INFO2("IRQ");
    pm_mouse.event=false; // Reset the mouse event variable
//    pm_mouse.xl=0;
//    pm_mouse.yl=0;
    PM_DP_RAM[OFFS_IRQPARAM]=pm_mouse.buttons;
    PM_DP_RAM[OFFS_IRQPARAM+1]=pm_mouse.x;
    PM_DP_RAM[OFFS_IRQPARAM+2]=pm_mouse.y; 
    PM_IRQ_Raise(IRQ_USBMouse,0);
    busy_wait_us(10);
    PM_IRQ_Lower();
    }
 }
#endif
#if DEV_POST_Enable==1
  dev_post_update();
#endif

if (absolute_time_diff_us(get_absolute_time(), pm_cmd_1sdelay) < 0)
     {  // This code is executed every second
      //printf(".");
      //printf("CMSD: %d, %d \n",dev_cms_delay,dev_cms_playing);
      if (dev_adlib_delay++==4)
         {  // 4 second since last adlib I/O
          dev_adlib_playing=false;  // disable the adlib mixing
         }
      if (dev_cms_delay++==4)
         {  // 4 second since last cms I/O
          dev_cms_playing=false;  // disable the cms mixing
          //printf("CMS Stop\n");
         }
      if (dev_tdy_delay++==4)
         {  // 4 second since last tandy I/O
          dev_tdy_playing=false;  // disable the cms mixing
         }
      pm_cmd_1sdelay = make_timeout_time_us(1000000);  //1000000 is 1 second
     }

#if PM_PICO_W
if (BV_WifiInit==0)
  if (absolute_time_diff_us(get_absolute_time(), wifi_getstatus_delay) < 0)
     {
      PM_Wifi_GetStatus();
      if (PM_Wifi.Status<=0) PM_RetryWifi();      // Retry to connect if connection error/Failed
/*
      PM_INFO("PM_Wifi.Status %d - ",PM_Wifi.Status);
      PM_INFO("PM_Wifi.rssi %d - ",PM_Wifi.rssi);
      PM_INFO("PM_Wifi.rate %d\n",PM_Wifi.rate);     
*/ 
      wifi_getstatus_delay = make_timeout_time_us(WIFI_STATUS_DELAY);
     }
#endif

if (multicore_fifo_rvalid())
  {
  switch(multicore_fifo_pop_blocking())
   {
    case 0 :
            PM_ExecCMD();
            break;
    case 1 : PM_INFO("IR ");
            break;
#ifdef RASPBERRYPI_PICO_W
#if USE_NE2000
    case FIFO_NE2K_SEND:
             ne2000_initiate_send();
             break;
#endif
#endif
   }
  }             
}

}
