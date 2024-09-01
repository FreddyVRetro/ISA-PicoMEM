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
#include "pm_debug.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/irq.h"
#include "hardware/vreg.h"
#include "hardware/regs/vreg_and_chip_reset.h"

#include "psram_spi.h"
#include "pico/cyw43_arch.h"
#include "cyw43.h"
#include "cyw43_stats.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/regs/busctrl.h"              /* Bus Priority defines */

// For Clock Display (Test)
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

//SDCARD and File system definitions
#include "hw_config.h"  /* The SDCARD and SPI definitions */	
#include "f_util.h"
#include "ff.h"
#include "sd_card.h"

//DOSBOX Code
#include "dosbox\bios_disk.h"

//Other PicoMEM Code
#include "isa_iomem.pio.h"  // PIN Defines
#include "pm_gvars.h"
#include "picomem.h"
#include "pm_gpiodef.h"     // Various PicoMEM GPIO Definitions
#include "pm_defines.h"     // Shared MEMORY variables defines (Registers and PC Commands)
#include "pm_libs.h"
#include "pm_disk\pm_disk.h"
#include "pm_pccmd.h"
#include "qwiic.h"

// PicoMEM emulated devices include
#include "dev_memory.h"
#include "dev_picomem_io.h"
#include "dev_post.h"
#include "dev_joystick.h"

#if USE_AUDIO
#include "audio_devices.h"
#include "dev_adlib.h"
#include "dev_pmaudio.h"
#include "pm_audio.h"
#else
void pm_audio_pause() {}
void pm_audio_resume() {}
#endif

#if USE_NE2000
extern "C" {
#include "dev_ne2000.h"
extern wifi_infos_t PM_Wifi;
}
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

//extern void cdc_task(void);
//extern void hid_app_task(void);
//extern void cdc_app_task(void);

CFG_TUSB_MEM_SECTION static char serial_in_buffer[64] = { 0 };

#endif // TinyUSB Host definitions END

//#ifndef USE_ALARM
//#include "pico_pic.h"
//#endif

// ** State Machines ** Order should not be changed, "Hardcoded" (For ISA)
// pio0 / sm0 : isa_bus_sm    : Wait for ALE falling edge, fetch the Address and Data
//
// pio1 / sm0              : SD or PSRAM
// pio1 / sm1              : SD or PSRAM

//***********************************************
//*    PicoMEM PIO State Machine variables      *
//***********************************************

#define LED_PIN 25

//***********************************************
//*              PicoMEM Variables              *
//***********************************************
							
#define DEV_POST_Enable 1

bool PM_Init_Completed=false;

PMCFG_t *PM_Config;             // Pointer to the PicoMEM Configuration in the BIOS Shared Memory

volatile uint8_t PM_Command;    // Write at port 0  > Command to send to the Pico (Can be changed only if status is 0 Success)
volatile uint8_t PM_Status;     // Read from Port 0 > Command and Pico Status ( ! When a SD Command is in progress, sometimes read FFh)
volatile uint8_t PM_CmdDataH;   // R/W Register
volatile uint8_t PM_CmdDataL;   // R/W Register

//volatile uint8_t PM_Register;   // Selected register number

volatile uint8_t PM_TestWrite;
         uint8_t PM_SetRAMVal;
         uint8_t PM_TestRAMVal;

// USB Host variables

bool USB_Host_Enabled=false;	// True if USB Host mode is enabled
bool MOUSE_Enabled=false;
bool KEYB_Enabled=false;

// Serial Output variables

bool SERIAL_Enabled=false;	// True is Serial output is enabled (Can do printf)
bool SERIAL_USB=false;   	// True is Serial is oved USB

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
if (BV_IRQStatus!=IRQ_Stat_COMPLETED) 
    {
     printf("IRQ CONFL %d",BV_IRQStatus);
    }
    BV_IRQArg=Arg;
    BV_IRQStatus=IRQ_Stat_REQUESTED;  
#if UART_PIN0
#else
    if (IRQ_raised)
       {
        printf("! IRQ still Up");
        gpio_put(PIN_IRQ,1);  // IRQ Down
        busy_wait_us(100);
       }
    gpio_put(PIN_IRQ,0);  // IRQ Up
    IRQ_raised=true;
#endif    
    return 0;
}

void PM_IRQ_Lower()
{
    PM_INFO("l");
#if UART_PIN0
#else    
    gpio_put(PIN_IRQ,1);  // IRQ Down
    IRQ_raised=false;    
#endif    
}

//********************************************************
//*          PSRAM Initialisation and test Code          *
//********************************************************

// PM_StartPSRAM: Initialise the External PSRAM return true if 8Mb detected
// return the memory size in MB or Init error/Disabled code
uint8_t PM_StartPSRAM()
{
#if USE_PSRAM

 PM_INFO("PSRAM Init: ");

 psram_spi = psram_init();
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

      qwiic_display_4char("EPSR");
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
     gpio_set_function(PM_SPI_SCK, GPIO_FUNC_PIO1);
     gpio_set_function(PM_SPI_MOSI, GPIO_FUNC_PIO1);
     gpio_set_function(PM_SPI_MISO, GPIO_FUNC_PIO1);
// printf(">Enable PSRAM sm %d \n",psram_spi.sm);
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

//********************************************************
//*          SDCARD Initialisation and test Code         *
//********************************************************
bool PM_StartSD()
{
 sd_card_t *pSD = sd_get_by_num(0);
 FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
 if (FR_OK != fr) { 
     PM_ERROR("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
     qwiic_display_4char("EuSD");
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
       gpio_set_function(PM_SPI_SCK, GPIO_FUNC_SPI);
       gpio_set_function(PM_SPI_MOSI, GPIO_FUNC_SPI);
       gpio_set_function(PM_SPI_MISO, GPIO_FUNC_SPI);  
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

// ****************************************************
// ***** Pico MEMP Commands Run from the 2nd core *****
// ****************************************************

void PM_ExecCMD()
{
//char spcc[]="Now the PicoMEM Can execute commands to the PC\r\n";
//char spcc2[]="Like Printing this :)\r\n ";
//char spcc3[]="Printf3 ";

//printf("CMD %x Data %x > ",PM_Command,PM_CmdDataL);
if (PM_Command!=0) PM_INFO("CMD %x, > ",PM_Command,PM_CmdDataL);


 switch(PM_Command) 
  {
   case CMD_Reset :         // CMD 0: Clean the Status or Enable/Disable the commands

    PM_INFO("Reset\n");
    PM_Status=STAT_READY;
    break;
    
   case CMD_SetBasePort :   // CMD 2: Set the board base port !! To do !!

    PM_Status=STAT_CMDNOTFOUND;
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
//        MEM_Index_T[PM_CmdDataL^(0xF0000>>14)]=PM_CmdDataH;
       }
    PM_Status=STAT_READY;
    break;
   case CMD_MEM_Init :       // CMD 7: Initialize the memory emulation based on the Config
// Started after the Setup / before the Boot
     PM_INFO("MEM Init\n");    

     PM_INFO("PC RAM: %dKb\n",PC_MEM+256*PC_MEM_H);

    PM_INFO("Mem Type: ");
    for (int i=0;i<MEM_T_Size;i++)
        { if (i==10*8) PM_INFO("- "); 
          PM_INFO("%d ",MEM_Type_T[i]);
        }

    // 1- Remove all emulated RAM
     dev_memory_remove();
 
    PM_INFO("Clean > Mem Type: ");
    for (int i=0;i<MEM_T_Size;i++)
        { if (i==10*8) PM_INFO("- "); 
          PM_INFO("%d ",MEM_Type_T[i]);
        }

    // 2- Initialize the EMS Port and RAM
     for (uint8_t i=1;i<EMS_PORT_NB;i++) 
       {
        PORT_Table[EMS_Port_List[i]]=DEV_NULL;
       }
#if USE_PSRAM
     if (PM_Config->EMS_Port!=0)
        { 

         PM_INFO("EMS Port:%d %x ",PM_Config->EMS_Port,EMS_Port_List[PM_Config->EMS_Port]<<3);
         
//       SetPortType(EMS_Port_List[PM_Config->EMS_Port],DEV_LTEMS,1);
         PORT_Table[EMS_Port_List[PM_Config->EMS_Port]]=DEV_LTEMS; // Set the EMS IO Port

         // Set the EMS @ decoding
         uint32_t EMS_Base=(EMS_Addr_List[PM_Config->EMS_Addr]<<14);
         SetMEMType(EMS_Base,MEM_EMS,MEM_S_16k*4);     //64Kb (126Kx4)
         SetMEMIndex(EMS_Base,MEM_EMS,MEM_S_16k*4);    //64Kb (126Kx4)         

         PM_INFO(" @:%x\n",EMS_Base);

        }
#endif        

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

   case CMD_TDY_Init :
    PM_INFO("Init Tandy RAM : %dx16Kb\n",BV_TdyRAM);
     
     BV_TdyRAM=8;  // Test
     
     if (BV_TdyRAM!=0)
      {

      if (MAX_PMRAM!=0)
         {
          int AddBlock_NB;
          AddBlock_NB=BV_TdyRAM;
          if (BV_TdyRAM>MAX_PMRAM) AddBlock_NB=MAX_PMRAM;
          PM_INFO("Add %dx16K PMRAM \n",AddBlock_NB);
          RAM_Offset_Table[MEM_RAM]=&PM_Memory[0]; // Place the RAM at the begining
          SetMEMType(0,MEM_RAM,MEM_S_16k*AddBlock_NB);    //MEMType is "for info"
          SetMEMIndex(0,MEM_RAM,MEM_S_16k*AddBlock_NB);   //MEMIndex is used for decoding
         }

#if USE_PSRAM
     if (BV_TdyRAM>MAX_PMRAM)
        {  // Add Tandy RAM as PSRAM From the Address MAX_PMRAM*+16KB, size (BV_TdyRAM-MAX_PMRAM)*16KB  
          PM_INFO("Add %dx16K PSRAM \n",BV_TdyRAM-MAX_PMRAM);     
          uint32_t MEM_Addr=MAX_PMRAM<<14;  // Shift 14 for 16Kb Block
          SetMEMType(MEM_Addr,MEM_PSRAM,MEM_S_16k*(BV_TdyRAM-MAX_PMRAM));
          SetMEMIndex(MEM_Addr,MEM_PSRAM,MEM_S_16k*(BV_TdyRAM-MAX_PMRAM));
        }
#endif     
      }
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
     if ((PM_Config->EnableJOY)&&(USB_Host_Enabled)) 
       {
        PM_INFO("-Enable Joystick\n");        
        dev_joystick_install();
       }
       else dev_joystick_remove();
#endif        

#if PM_PICO_W
#if USE_NE2000 
     DelPortType(DEV_NE2000);                           // Remove the ne2000 Ports
     if ((PM_Config->ne2000Port!=0)&&(PM_Config->ne2000Port<0x3E0))
        {         
         SetPortType(PM_Config->ne2000Port,DEV_NE2000,4); // Add the ne2000 Ports
        }
        else PM_Config->ne2000Port=0;  // Force 0 if > 3E0

     PM_INFO("-NE2000: P %x I %d\n",PM_Config->ne2000Port,PM_Config->WifiIRQ);

/* for (int i=0;i<128;i++) 
   { 
    if (i==(PM_Config->ne2000Port>>3)) printf("*");
    printf ("%d,",PORT_Table[i]); 
   } */
#endif //USE_NE2000
#endif

#if USE_AUDIO
   // WARNING Need to unallocate the RAM Taken by audio every time otherwise Crash !
     if (PM_Config->Adlib)
        {
         PM_INFO("-Start Audio\n");
         if (!dev_adlib_installed())
            {
             quiic_4charLCD_enabled=false;  // Stop Qwiic when audio is active
             dev_adlib_install();           // Will not reinstall if already installed
             pm_audio_start();
             //play_adlib();
            }
        } else  // Remove Adlib
        {
 //        pm_audio_stop();
 //        dev_adlib_remove();
        }

     if (PM_Config->TDYPort)
        {
        } else
        {

        }
        
     if (PM_Config->CMSPort)
        {
        } else
        {

        }        

//for (;;) {};
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

   case CMD_Keyb_Enable :     // CMD: 0x54

      PM_INFO("Enable Kb\n");
  
      KEYB_Enabled=true;
      PM_CmdDataL=0;
      PM_Status=STAT_READY;
     break;  

   case CMD_Keyb_Disable :    // CMD: 0x55

      PM_INFO("Disable Kb\n");
   
      KEYB_Enabled=false;
      PM_CmdDataL=0;
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
           printf("Retry Wifi connection\n");
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

// *** Disk Commands  0x8x ***
   case CMD_HDD_Getlist :   // CMD: 
    pm_audio_pause();     // Need to pause audio during uSD Access 
//PC_DiskNB=2;    // Test / Debug
      uint8_t nb;
      PM_CmdDataL=ListImages(1,&nb);
      PM_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation      
      PM_Status=STAT_READY;
     pm_audio_resume();      
     break;

   case CMD_FDD_Getlist :   // CMD: 
    pm_audio_pause();     // Need to pause audio during uSD Access   
      PM_CmdDataL=ListImages(0,&nb);
      PM_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation      
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
                sprintf(imgpath, "HDD/%s.img", PM_Config->HDD[i].name);
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
     
     New_DiskNB=0;
     for (int i=0;i<4;i++)
      {
        if (PM_Config->HDD_Attribute[i]>=0x80) New_DiskNB++;       // Image Mounted
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
                sprintf(imgpath, "FLOPPY/%s.img", PM_Config->FDD[i].name);
                 
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

     New_FloppyNB=0;
     for (int i=0;i<2;i++)
      {
        if (PM_Config->FDD_Attribute[i]>=0x80) New_FloppyNB++;         // Image Mounted
        if (PM_Config->FDD_Attribute[i]<PC_FloppyNB) New_FloppyNB++;  // Read existing Floppy
        PM_INFO(" - %s Attr:%x\n",PM_Config->FDD[i].name,PM_Config->FDD_Attribute[i]);
      }
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
    default:
     PM_Status=STAT_CMDNOTFOUND;  // Command not supported
     break;
  }
}

//#include "pm_debug.h"

//*******************************************************************************
//*                          2nd Core main code                                 *
//*******************************************************************************
void main_core0 ()
{

pm_timefromlast();

//#if USE_AUDIO
//quiic_4charLCD_enabled=false;
//#else
pm_qwiic_init(100*1000);
pm_qwiic_scan();

if (quiic_4charLCD_enabled)
 {
  qwiic_display_4char("LCD-");

  #if DEV_POST_Enable==1
  dev_post_install(0x80);
  #endif
//pm_qwiic_scan_print();
//ht16k33_demo();
 }
//#endif

// ************  PSRAM Init and Test CODE ******************
#if USE_PSRAM
BV_PSRAMInit=PM_StartPSRAM();
PM_INFO("BV_PSRAMInit: %x\n",BV_PSRAMInit);
/*if (BV_PSRAMInit<=16)
   {
    PM_INFO("Define EMS Port 0x260");
    SetPortType(0x260,DEV_LTEMS,1);   // PSRAM Enabled : We can define the EMS Ports
   }; */
#if TEST_PSRAM
PSRAM_Test();     // PSRAM test fail is started before SD init !!!
#endif //TEST_PSRAM
#endif //PSRAM

pm_timefromlast();

// ************  SDCARD Init and Test CODE ******************
// !!! If uSD Init is done before the PSRAM, Wifi init fail....
#if USE_SDCARD
PM_INFO("Start uSD\n");

if (PM_StartSD())
   {
    if (PM_LoadConfig()==0) BV_CFGInit=0;  // Success
       else BV_CFGInit=0xFD;  // Failed
    BIOS_SetupDisks();        // Clean the Disk Objects
   } else BV_CFGInit=0xFF;    // Disabled (No Config to load)

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

pm_timefromlast();

PM_INFO("Start USB\n");

// ************  USB Init and Test CODE ******************

#if USE_USBHOST
  if (PM_Config->EnableUSB=1)
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

// Be carefull, need to be in uSD Mode to read the config file !
PM_EnableSD();
uint8_t WifiRes=PM_EnableWifi();

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
      printf("Id%X ",To_Display32);
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
#if PM_PRINTF
  printf("IRQ");
#endif     
    PM_IRQ_Raise(IRQ_USBMouse,0);
    busy_wait_us(100);
    PM_IRQ_Lower();
    }
 }
#endif
#if DEV_POST_Enable==1
  dev_post_update();
#endif

#if PM_PICO_W
if (BV_WifiInit==0)
  if (absolute_time_diff_us(get_absolute_time(), wifi_getstatus_delay) < 0)
     {
      PM_Wifi_GetStatus();
      if (PM_Wifi.Status<=0) PM_RetryWifi();      // Retry to connect if connection error/Failed
      PM_INFO("PM_Wifi.Status %d - ",PM_Wifi.Status);
      PM_INFO("PM_Wifi.rssi %d - ",PM_Wifi.rssi);
      PM_INFO("PM_Wifi.rate %d\n",PM_Wifi.rate);     
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
#if USE_NE2000
    case FIFO_NE2K_SEND:
             ne2000_initiate_send();
             break;
#endif
   }
  }             
}

}