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

 #include <sys/time.h>
 #include <time.h>
 #include <ctime>

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

#if USE_SPI_PSRAM
#if USE_SPI_PSRAM_DMA
#include "psram_spi.h"
#else
#include "psram_spi2.h"
#endif
#endif

#ifdef PIMORONI_PICO_PLUS2_RP2350  //Select the right PSRAM code
#include "pico_psram.h"    // Add QSPI PSRAM Code
#else
#if USE_SPI_PSRAM              // Add other PSRAM Code if not Pimoroni
#if USE_SPI_PSRAM_DMA
#include "psram_spi.h"
#else
#include "psram_spi2.h"
#endif  //USE_SPI_PSRAM_DMA
#endif  //USE_SPI_PSRAM
#endif  //PIMORONI_PICO_PLUS2_RP2350

#if PM_WIFI
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
//#include "isa_iomem.pio.h"  // PIN Defines
#include "pm_gvars.h"
#include "picomem.h"
#include "pm_board.h"       // PicoMEM Board Definitions
#include "pm_defines.h"     // Shared MEMORY variables defines (Registers and PC Commands)
#include "pm_libs/pm_disk.h"
#include "pm_libs/pico_rtc.h"
#include "pm_libs/pm_libs.h"
#include "pm_libs/bus_irq.h"
#include "pm_libs/pm_apps.h"
#include "pm_libs/isa_dma.h"
#include "pm_pccmd.h"

#include "hardware/i2c.h"
#include "pm_i2c.h"
#include "i2c_pfc85263.h"  // Real time Clock

// Libs present on 1.5+ Boards only
#if USE_OLED
#include "dev_oled.h"
#include "pm_display.h"
#include "inputs/pm_inputs.h"
#endif

#include "pmdev.h" // PicoMEM ressources (PSDAM, SD, USB) code

// PicoMEM emulated devices include
#include "dev_memory.h"
#include "dev_picomem_io.h"
#include "dev_ltemm.h"
#include "dev_post.h"
#include "dev_irq.h"
#include "dev_joystick.h"
#include "ethdfs.h"

#if USE_USBHOST
#include "dev_joystick.h"
#include "usbh.h"
#endif

#if USE_RTC
#include "dev_rtc.h"
#endif

#if USE_AUDIO
#include "dev_adlib.h"
#include "dev_cms.h"
#include "dev_tandy.h"
#include "dev_mindscape.h"
#include "dev_covox.h"
#include "covox/covox_dac.h"
#include "dev_dma.h"
#include "dev_sbdsp.h"
#include "dev_mpu.h"
#if USE_GUS
#include "dev_gus.h"
#endif

#include "dev_audiomix.h"
#include "pm_audio.h"

#include "audio_devices/sbdsp/sbdsp.h"
#include "audio_devices/audio_fifo.h"

extern void sbdsp_updatebuffer(int16_t* buff,uint32_t samples);

#else
void dev_audiomix_pause() {}
void dev_audiomix_resume() {}
#endif

#if USE_CDROM
#include "dev_cdrom_mke.h"
#endif

#if PM_WIFI
#if USE_NE2000
extern "C" {
#include "ne2000/dev_ne2000.h"
extern wifi_infos_t PM_Wifi;
}
#endif
#endif

#if USE_USBHOST
// TinyUSB Host
#include "tusb.h"
extern "C" {
#include "usb.h"
}
#endif

#if USE_USBHOST

#include "hid_dev.h"  // USB HID Devices variables : Mouse, KB, Joystick



// TinyUSB Host
void led_blinking_task(void);

CFG_TUSB_MEM_SECTION static char serial_in_buffer[64] = { 0 };

#endif // TinyUSB Host definitions END

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

PMCFG_t   *PM_Config=(PMCFG_t *) &PM_DP_RAM[CONF_OFFS]; // Pointer to the PicoMEM Configuration in the BIOS Shared Memory
BIOSVAR_t *PM_BIOS;               // Pointer to the PicoMEM BIOS Variables

volatile uint8_t PM_Command;      // Write at port 0  > Command to send to the Pico (Can be changed only if status is 0 Success)
volatile uint8_t PM_Status;       // Read from Port 0 > Command and Pico Status ( ! When a SD Command is in progress, sometimes read FFh)
volatile uint8_t PM_CmdDataH;     // R/W Register
volatile uint8_t PM_CmdDataL;     // R/W Register
volatile bool PM_CmdReset=false;  // Is set to true when trying to reset a "locked" command

volatile uint8_t PM_TestWrite;
         uint8_t PM_SetRAMVal;
         uint8_t PM_TestRAMVal;

uint8_t PM_Int13h_LEDCount=0;     // Count the number of Int13h calls to blink the LED during Disk access
        
// USB Host variables

bool USB_Host_Enabled=false;	// True if USB Host mode is enabled
bool MOUSE_Enabled=false;
bool KEYB_Enabled=false;

// Serial Output variables

bool SERIAL_Enabled=false;	// True is Serial output is enabled (Can do printf)
uint8_t PM_OLED_Type=1;     // OLED Screen Type

// PSRAM Memory variables / Registers


// Delay values during the command
#define WIFI_DELAY 10 // Delay in us between 2 Wifi get status call (10s)

uint64_t pm_cmd_100msdelay;
uint64_t pm_cmd_1sdelay;
uint8_t wifi_delay_counter;

// Add in registers ?

/*
uint16_t PICO_Freq=280;  //PM_SYS_CLK;
uint8_t  PSRAM_Freq=135; //PM_SYS_CLK/2;
uint8_t  SD_Freq=20;
*/


// *******************************
// ***** Audio On / On Code  *****
// *******************************
#if USE_AUDIO
void PM_Adlib_OnOff(uint16_t state)
{
 if (state)
     {
      PM_INFO(">Adlib On\n");      
      dev_adlib_install();           // Will not reinstall if already installed
     }
     else
     {
      PM_INFO(">Adlib Off\n");
      dev_adlib_remove();
     }

} //PM_Adlib_OnOff

// Return 0 When Ok
uint8_t PM_CMS_OnOff(uint16_t state)
{

 if (state)
     {
       if (state==1) state=0x220;    // Default CMS Port
       PM_INFO(">CMS On %x\n",state);
       return dev_cms_install(state);
     }
     else
     {
       PM_INFO(">CMS Off\n");      
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
      PM_INFO(">Tandy On %x\n",state);      
      return dev_tdy_install(state);
     }
     else
     {
      dev_tdy_remove();
      PM_INFO(">Tandy Off\n");        
      return 0;
     }
} //PM_TDY_OnOff

// Return 0 When Ok
uint8_t PM_MMB_OnOff(uint16_t state)
{
 if (state)
     {
      if (state==1) state=0x300;    // Default mindscape Port
      PM_INFO(">Mindscape On %x\n",state);      
      return dev_mmb_install(state);
     }
     else
     {
      dev_mmb_remove();
      PM_INFO(">Mindscape Off\n");
      return 0;
     }
} //PM_MMS_OnOff

// Return 0 When Ok
uint8_t PM_CVX_OnOff(uint8_t type, uint16_t state)
{

 if (state)
     {
       if (state==1) state=0x378;    // LPT1 port
       if (state==2) state=0x278;    // LPT2 port
       PM_INFO(">COVOX On %x\n",state);
       return dev_cvx_install(CVX_TYPE_DAC,state);
     }
     else
     {
       PM_INFO(">COVOX Off\n");
       dev_cvx_remove();
       return 0;
     }
} //PM_CVX_OnOff

// Return 0 When Ok
uint8_t PM_DSS_OnOff(uint16_t state)
{

 if (state)
     {
       if (state==1) state=0x220;    // Default CMS Port
       PM_INFO(">Disney SS On %x\n",state);
       return dev_cvx_install(CVX_TYPE_DISNEY,state);
     }
     else
     {
       PM_INFO(">Disney SS Off\n");
       dev_cvx_remove();
       return 0;
     }
} //PM_DSS_OnOff

#if USE_SBDSP  
// Return 0 When Ok
uint8_t PM_sbdsp_OnOff(uint16_t state)
{
 uint8_t res;
 //PM_INFO ("-PM_sbdsp_OnOff %x",state);
 if (state)
     {
      if (state==1) state=0x220;    // Default SB DSP port
      PM_INFO(">SB DSP On %x\n",state);
      res=dev_sbdsp_install(state);
      if (!res) dev_adlib_install();  // If SB DSP is installed, Add Adlib
      return res;
     }
     else
     {
      dev_sbdsp_remove();
      PM_INFO(">SB DSP Off\n");
      return 0;
     }
} //PM_sbdsp_OnOff
#endif //USE_SBDSP

// Return 0 When Ok
uint8_t PM_gus_OnOff(uint16_t state)
{
#if USE_GUS  
 uint8_t res;
 //PM_INFO ("-PM_sbdsp_OnOff %x",state);
 if (state)
     {
      if (state==1) state=0x220;    // Default SB DSP port
      PM_INFO(">GUS On %x\n",state);
      res=dev_gus_install(state);
      return res;
     }
     else
     {
      dev_gus_remove();
      PM_INFO(">GUS Off\n");
      return 0;
     }
#else
  PM_INFO(">GUS Not Supported in this build\n");
  return 1;
#endif  
} //PM_gus_OnOff

// Return 0 When Ok
uint8_t PM_MPU_OnOff(uint8_t type, uint16_t state)
{
#if USE_MPU
 if (state)
     {
       if (state==1) state=0x330;    // mpu default port
       PM_INFO(">MPU On %x\n",state);
       return dev_mpu_install(state);
     }
     else
     {
       PM_INFO(">MPU Off\n");
       dev_mpu_remove();
       return 0;
     }
#else
  PM_INFO(">MPU Not Supported in this build\n");
  return 1; 
#endif     
} //PM_MPU_OnOff


// PM_Audio_OnOff : Enable/Disable the Audio device / Sound cards
// Started by the BIOS just before the boot
// state : 0: Off / 1: On
void PM_Audio_OnOff(uint8_t state)
{
  if (state)
    {
     // PM_Config->MMBPort=0x300;
      PM_Adlib_OnOff(PM_Config->Adlib);
      PM_CMS_OnOff(PM_Config->CMSPort);
      PM_TDY_OnOff(PM_Config->TDYPort);
      PM_MMB_OnOff(PM_Config->MMBPort);
//      PM_CVX_OnOff(0);  //Off by default (Configured by PMInit)
#if USE_SBDSP      
      PM_sbdsp_OnOff(0);          // Set SB Off by default
#endif
#if USE_GUS
      PM_gus_OnOff(0);            // Set GUS Off by default
#endif      
#if USE_MPU // Don't stop MPU at reboot for the moment
    //  dev_mpu_install(0x330);
#endif
#if I2S_QWIIC_SHARES // When I2S and QwiiC are shared, Stop all QwiiC access when Audio is ON
#else
      qwiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
#endif
      dev_audiomix_start();
    }
    else
    {
      PM_Adlib_OnOff(0);
      PM_CMS_OnOff(0);
      PM_TDY_OnOff(0);
      PM_MMB_OnOff(0);
#if USE_SBDSP    
      PM_sbdsp_OnOff(0);
#endif
      PM_gus_OnOff(0);
     // dev_audiomix_stop();  // Does not work, can't reenable after :(
    }
}    
#endif

// PM_Joystick_OnOff : Enable/Disable the Joystick
// state : 0: Off / 1: On
// return false it it can't enable it
#if USE_USBHOST
bool PM_Joystick_OnOff(uint8_t state)
{
 if (state)
    {
     if (USB_Host_Enabled)
      {
       PM_INFO(">Enable Joystick\n");        
       dev_joystick_install();
       return true;
      }
      else return false;
    }
    else 
    {
      PM_INFO(">Disable Joystick\n");
      dev_joystick_remove();
      return true;      
     }
}
#endif

#if USE_RTC
bool PM_RTC_OnOff(uint8_t state)
{
 if (state)
    {
       PM_INFO(">Enable RTC\n");
       dev_rtc_install(1);
       return true;
    }
    else
    {
      PM_INFO(">Disable RTC\n");
      dev_rtc_remove();
      return true;      
     }
}
#endif

// *************************************************************
// *****      PicoMEM Commands Run from the 2nd core       *****
// *************************************************************
void PM_ExecCMD()
{

 PM_INFO("CMD %X:%X %X > ",PM_Command,PM_CmdDataH,PM_CmdDataL);
//printf("CMD %X:%X %X > ",PM_Command,PM_CmdDataH,PM_CmdDataL);

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

     dev_memory_install_all();

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

#if USE_SPI_PSRAM
     if (BV_TdyRAM>MAX_PMRAM) // Then PSRAM
        {  // Add Tandy RAM as PSRAM From the Address MAX_PMRAM*+16KB, size (BV_TdyRAM-MAX_PMRAM)*16KB  
          PM_INFO("Add %dx16K PSRAM, Addr %x \n",BV_TdyRAM-MAX_PMRAM,MAX_PMRAM<<14);     
          uint32_t MEM_Addr=MAX_PMRAM<<14;  // Shift 14 for 16Kb Block
          SetMEMType(MEM_Addr,MEM_PSRAM,MEM_S_16k*(BV_TdyRAM-MAX_PMRAM));
          SetMEMIndex(MEM_Addr,MEM_PSRAM,MEM_S_16k*(BV_TdyRAM-MAX_PMRAM));
          BV_SPIPSRAM=1;                    // Tell that SPI PSRAM is used
          BV_Int13hCLI=1;                   // Block interrupt during Int13h
          for (int i=MAX_PMRAM;i<BV_TdyRAM;i++)
           {PM_Config->MEM_Map[i]=MEM_PSRAM;}           // Update the Memory MAP
        }
#else

#endif
// Move the Sysem RAM to its new location (Just after the added RAM)
     for (int i=BV_TdyRAM;i<(BV_TdyRAM+PC_RamSize_Bloc);i++)
           {PM_Config->MEM_Map[i]=SMEM_RAM;}                     // Update the Memory MAP
      PM_Config->MEM_Map[(BV_TdyRAM+PC_RamSize_Bloc)]=SMEM_VID;  // Last 16Kb is Video
      }

#if PM_PRINTF
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
#endif
     PM_Status=STAT_READY;
    break;

   case CMD_Test_RAM :       // Test Shared RAM (Used by the BIOS to check if working or cache active)
//   PM_INFO("Test RAM\n");   // Used by the BIOS
     uint8_t temp;

     temp=BV_Test;
     BV_Test=PM_CmdDataL;
     PM_CmdDataL=temp;

     PM_Status=STAT_READY;
     break;

   case CMD_TESTIO :       // Test Data transfer via IO
     PM_INFO("Test IO Transfer\n");

     test_pmio_transfer();

     PM_Status=STAT_READY;
     break;

   case CMD_SaveCfg :       // CMD: Save the Configuration to the uSD
     PM_CmdDataL=0;
     PM_INFO("Save Config to CF\n");
 
     if (PM_EnableSD())
      { 
        dev_audiomix_pause();     // Need to pause audio during uSD Access         
        uint8_t SC_return=pm_SaveConfig();
        dev_audiomix_resume();         
        if (SC_return!=0)
           {
            PM_PICO_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation     
            PM_ERROR("Save Config Error %d\n",SC_return);
     
            PM_CmdDataL=SC_return;   // Return an error code          
            PM_Status=STAT_CMDERROR;         
            break;
           }
      }
     PM_PICO_EnablePSRAM();      // Re enable the PSRAM for the Memory emulation
     PM_Status=STAT_READY;
    break;

   // Initialize the devices back to their Default state (Command sent by the BIOS just before the Boot)
   case CMD_DEV_Init :

    PM_INFO("CMD_DEV_Init:\n");

#if USE_DEV_IRQ  // This device display the Port 21h out values for debug
     dev_irq_install();    // For DEBUG ! Display p21h
#endif
//     isa_irq_init();

// DMA registers capture/Emulation installation
#if !USE_HWDMA
     dev_dma_install();     
     bool dma_ok;
     dma_ok=true;     
     // Add code to test if DMA can be interrupted
     PM_INFO("DMA register capture test: ");
     PC_SetDMARegs(1,0xFFFF,0xFFFF,0);
  //   PM_INFO("Virtual DMA: Offset: %x Size: %x\n",dmachregs[1].baseaddr, dmachregs[1].basecnt);
     if ((dmachregs[1].baseaddr!=0xFFFF)|(dmachregs[1].baseaddr!=0xFFFF)) dma_ok=false;
     PC_SetDMARegs(1,0,0,0);
  //   PM_INFO("Virtual DMA: Offset: %x Size: %x\n",dmachregs[1].baseaddr, dmachregs[1].basecnt);
     if ((dmachregs[1].baseaddr!=0)|(dmachregs[1].baseaddr!=0)) dma_ok=false;
    
     if (!dma_ok)
        {
         PM_ERROR("Ko > Disable !\n");
         dev_dma_remove();
        } else PM_INFO("Ok\n");
#endif

     isa_dma_init();       // Was installed at the Pico Startup

     PM_INFO("-Disable Mouse\n");
     MOUSE_Enabled=false;  // Mouse is disabled by default (Enabled by PMMOUSE)
// add SD and PSRAM Speed config here

     PM_INFO("-Set SD: %d PSRAM %d (MHz)\n",PM_Config->SD_Speed,PM_Config->RAM_Speed);

#if USE_USBHOST
     PM_Joystick_OnOff(PM_Config->EnableJOY);
#endif

#if PM_WIFI
#if USE_NE2000 
      DelPortType(DEV_NE2000);                            // Remove the ne2000 Ports
      if ((PM_Config->ne2000Port!=0)&&(PM_Config->ne2000Port<0x3E0))
        {
         dev_ne2000_cfg(PM_Config->ne2000Port,PM_Config->ne2000IRQ); // Set the ne2000 Port and IRQ
         SetPortType(PM_Config->ne2000Port,DEV_NE2000,4); // Add the ne2000 Ports
        }
        else PM_Config->ne2000Port=0;  // Force 0 if > 3E0

       PM_INFO("-NE2000: P %x I %d\n",PM_Config->ne2000Port,PM_Config->ne2000IRQ);
#endif //USE_NE2000
#endif

#if USE_AUDIO
       PM_Audio_OnOff(PM_Config->AudioOut);
#endif

#if USE_RTC
       PM_RTC_OnOff(PM_Config->UseRTC);
#endif

#if USE_CDROM
       dev_cdr_install();
#endif
      PM_INFO("BV_IRQ : %d, BV_HWIRQ %X >",BV_IRQ,BV_HWIRQ);
      for (int shft=0;shft<8;shft++)
          { if ((BV_HWIRQ>>shft) & 1) PM_INFO(" %d",shft); }
      PM_INFO("\n");
      PC_End();     // Tell the PC Command loop to end      
      PM_Status=STAT_READY;
      break;

   case CMD_GetPMString : 

    PM_Status=STAT_READY;
    break;
    
   case CMD_SetMEM :        // CMD: 22 SET 64K of memory to the value
 //    PM_INFO("SETMEM %x ",PM_SetRAMVal);
     //setmem()
     for (uint32_t i=0 ; i<65536 ; ++i)
         {
          PM_Memory[i]=PM_CmdDataL;
          //PM_INFO("%x: %x,",i,PM_Memory[i]);
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

   case CMD_SendIRQ :      // Will Fire all the Interrupts one after the other for IRQ Detection
     PM_INFO("CMD_SendIRQ\n");
     BV_IRQ=0;
     BV_HWIRQ=0;
   #if BOARD_PM15 || BOARD_PM20
     gpio_put(PIN_IRQ3,1);   // IRQ Down (Force if the IRQ was not acknowledge (IRQ masked))
     busy_wait_us(5);
     gpio_put(PIN_IRQ3,0);   // Move the needed IRQ Up
     busy_wait_ms(2);        // Keep the IRQ high for 2ms
     gpio_put(PIN_IRQ3,1);   // Put back the IRQ Down
     busy_wait_ms(2);        // Wait a bit before the next IRQ

     gpio_put(PIN_IRQ5,1);   // IRQ Down (Force if the IRQ was not acknowledge (IRQ masked))
     busy_wait_us(5);
     gpio_put(PIN_IRQ5,0);   // Move the needed IRQ Up
     busy_wait_ms(2);        // Keep the IRQ high for 10ms
     gpio_put(PIN_IRQ5,1);   // Put back the IRQ Down
     busy_wait_ms(2);        // Wait a bit before the next IRQ
#ifdef PIN_IRQ6              // PicoMEM 2 has IRQ6 available
     gpio_put(PIN_IRQ6,1);   // IRQ Down (Force if the IRQ was not acknowledge (IRQ masked))
     busy_wait_us(5);
     gpio_put(PIN_IRQ6,0);   // Move the needed IRQ Up
     busy_wait_ms(2);        // Keep the IRQ high for 2ms
     gpio_put(PIN_IRQ6,1);   // Put back the IRQ Down
     busy_wait_ms(2);        // Wait a bit before the next IRQ       
#endif
     gpio_put(PIN_IRQ7,1);   // IRQ Down (Force if the IRQ was not acknowledge (IRQ masked))
     busy_wait_us(5);
     gpio_put(PIN_IRQ7,0);   // Move the needed IRQ Up
     busy_wait_ms(2);        // Keep the IRQ high for 2ms
     gpio_put(PIN_IRQ7,1);   // Put back the IRQ Down
#else
#if UART_PIN0
#else
     gpio_put(PIN_PM_IRQ,1);   // IRQ Down (Force if the IRQ was not acknowledge (IRQ masked))
     busy_wait_us(5);
     gpio_put(PIN_PM_IRQ,0);   // Move the needed IRQ Up
     busy_wait_ms(2);          // Keep the IRQ high for 2ms
     gpio_put(PIN_PM_IRQ,1);   // Put back the IRQ Down
#endif 
#endif
     busy_wait_ms(20);       // Wait a little more at the end
     PM_INFO("IRQ Detected: %d Mask %X\n",BV_IRQ,BV_HWIRQ);
     PM_Status=STAT_READY;
     break;

   case CMD_IRQAck :       // CMD: Acknowledge IRQ
     PM_INFO("CMD_IRQAck\n");
     SVAR_IRQ->PM_IRR=0;
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
          PM_INFO("On\n");
         }
         else
         {
          KEYB_Enabled=false;
          PM_INFO("Off\n");
         }
      PM_CmdDataL=0;
      PM_Status=STAT_READY;
     break;  

   case CMD_Joy_OnOff:        // USB Joystick : 0: Off 1: on
      uint16_t arg;   
#if USE_USBHOST
      arg=((uint16_t)PM_CmdDataH<<8)+(uint16_t)PM_CmdDataL;
      PM_INFO("%X",arg);
      PM_CmdDataL=PM_Joystick_OnOff(arg);  
      PM_CmdDataH=0;
#else
      PM_INFO("Joystick Not Supported in this build\n");
      PM_CmdDataL=1;  // Return an error
      PM_CmdDataH=0;
#endif      
      PM_Status=STAT_READY;      
     break;
	 
   case CMD_Wifi_Infos	:   // CMD: 0x60  Get the Wifi Status, retry to connect if not connected
#if PM_WIFI
#if USE_NE2000
      if (BV_WifiInit==INIT_DONE)
       {
        PM_Wifi_GetStatus();
        if ((PM_Wifi.Status==1)&&(PM_Wifi.rssi==0))
          {
           PM_INFO("Retry Wifi connection\n");
           PM_RetryWifi();
          }
       }

      memcpy((void *)&PCCR_Param,&PM_Wifi,sizeof(PM_Wifi));    

      PM_INFO("Wifi Info: Port %x IRQ %d",PM_Config->ne2000Port,PM_Config->ne2000IRQ);
      PM_INFO(" SSID %s \n",PM_Wifi.WIFI_SSID);

#endif //USE_NE2000
#endif //PM_WIFI
      PM_Status=STAT_READY;
     break;

   case CMD_USB_Status	:   // CMD: 0x61  Get the USB Status string
      char *str_tmp;
      str_tmp=(char *) &PCCR_Param;
      uint8_t usbnb;

#if USE_USBHOST   
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
               PM_INFO("%s\n",str_tmp);
               str_tmp+=strlen(str_tmp)+1;   // Move pointer at the end of the current string
              }
        }
        else 
        {
         str_tmp[0]=0;
        }
#else 
      str_tmp[0]=0;
#endif      
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
          //PM_INFO("i %d",i);
          if (i<2)
           {  // Floppy
           //PM_INFO("Attr%x",PM_Config->FDD_Attribute[i]);
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
           //PM_INFO("Attr%x",PM_Config->HDD_Attribute[i-2]);
           if ((PM_Config->HDD_Attribute[i-2]>=0x80))
            {
             // Generate the HDD info string
             sprintf(str_tmp,"   HDD%d : %-13s  [%4dMB] - C:%d H:%d S:%d",i-2,PM_Config->HDD[i-2].name,PM_Config->HDD[i-2].size,
             imageDiskList[i]->cylinders,imageDiskList[i]->heads,imageDiskList[i]->sectors);
             PM_INFO("%s\n",str_tmp);
             str_tmp[1]=0xFE;          // Add the special character
             usbnb=strlen(str_tmp);
             str_tmp+=usbnb+1;         // Move pointer at the end of the current string
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
      arg=(uint16_t)PM_CmdDataL;   
      PM_INFO("Adlib OnOff: %x\n",arg);  
      if (arg)
       { // If enable device, start audio mix
#if I2S_QWIIC_SHARES // When I2S and QwiiC are shared, Stop all QwiiC access when Audio is ON
#else
        qwiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
#endif
        dev_audiomix_start();            // Start Audio if not on
       }   
      PM_Adlib_OnOff(arg);
      PM_CmdDataL=0;
      PM_CmdDataH=0;
      PM_Status=STAT_READY;
     break;

   case CMD_TDYOnOff:        // Tandy Audio : 0 : Off 1: On default or port, return install status
      arg=((uint16_t)PM_CmdDataH<<8)+(uint16_t)PM_CmdDataL;
      PM_INFO("TDY OnOff: %x\n",arg);
      if (arg)
       { // If enable device, start audio mix
#if I2S_QWIIC_SHARES // When I2S and QwiiC are shared, Stop all QwiiC access when Audio is ON
#else
        qwiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
#endif
        dev_audiomix_start();            // Start Audio if not on
       }
      PM_CmdDataL=PM_TDY_OnOff(arg);     
      PM_CmdDataH=0;
      PM_Status=STAT_READY;      
     break;

   case CMD_CMSOnOff:        // CMS Audio   : 0 : Off 1: On default or port, return install status
      arg=((uint16_t)PM_CmdDataH<<8)+(uint16_t)PM_CmdDataL;
      PM_INFO("CMS OnOff: %x\n",arg);      
      if (arg)
       { // If enable device, start audio mix
#if I2S_QWIIC_SHARES // When I2S and QwiiC are shared, Stop all QwiiC access when Audio is ON
#else
        qwiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
#endif
        dev_audiomix_start();            // Start Audio if not on
       }
      PM_CmdDataL=PM_CMS_OnOff(arg);
      PM_CmdDataH=0;
      PM_Status=STAT_READY;      
     break;

   case CMD_MMBOnOff:        // MMB Audio   : 0 : Off 1: On default or port, return install status
      arg=((uint16_t)PM_CmdDataH<<8)+(uint16_t)PM_CmdDataL;
      PM_INFO("MMB OnOff: %x\n",arg);
      if (arg)
       { // If enable device, start audio mix
#if I2S_QWIIC_SHARES // When I2S and QwiiC are shared, Stop all QwiiC access when Audio is ON
#else
        qwiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
#endif
        dev_audiomix_start();            // Start Audio if not on
       }
      PM_CmdDataL=PM_MMB_OnOff(arg);
      PM_CmdDataH=0;
      PM_Status=STAT_READY;
     break;

   case CMD_SetSBIRQDMA:        // GUS Audio   : 0 : Off 1: On default or port
      PM_CmdDataL=dev_sbdsp_set_irq_dma((uint16_t)PM_CmdDataL,(uint16_t)PM_CmdDataH);
      PM_CmdDataH=0;
      PM_Status=STAT_READY;      
     break;

   case CMD_SBOnOff:        // Sound Blaster Audio : 0 : Off 1: On default or port  !! Need to add IRQ
   #if USE_SBDSP
      if (BV_IRQ==0)
        {
          PM_ERROR("Can't start Sound Blaster, no IRQ detected !\n");
          PM_CmdDataL=CMDERR_NOIRQ;  // Error, no IRQ detected
          PM_CmdDataH=0;
          PM_Status=STAT_READY;      
          break;
        }
      arg=((uint16_t)PM_CmdDataH<<8)+(uint16_t)PM_CmdDataL;
      PM_INFO("SB DSP OnOff: %x (%x %X)\n",arg,PM_CmdDataH,PM_CmdDataL);
      if (arg)
       { // If enable device, start audio mix
#if I2S_QWIIC_SHARES // When I2S and QwiiC are shared, Stop all QwiiC access when Audio is ON
#else
        qwiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
#endif
        dev_audiomix_start();            // Start Audio if not on
       }
      PM_CmdDataL=PM_sbdsp_OnOff(arg);
      PM_CmdDataH=0;
    #else
      PM_CmdDataL=1;  // Error, SB not supported
      PM_CmdDataH=0;
    #endif
      PM_Status=STAT_READY;
     break;

   case CMD_SetGUSIRQDMA:     // Gravis Audio   : 0 : Off 1: On default or port
#if USE_GUS
      PM_INFO("Set GUS IRQ DMA: %x %x\n",PM_CmdDataH,PM_CmdDataL);
      PM_CmdDataL=dev_gus_set_irq_dma((uint16_t)PM_CmdDataL,(uint16_t)PM_CmdDataH);
      PM_CmdDataH=0;
#else
      PM_INFO("Set GUS IRQ DMA (Ignored)\n");
      PM_CmdDataL=1;  // Error, GUS not supported
      PM_CmdDataH=0;
#endif
      PM_Status=STAT_READY;
      break;

   case CMD_GUSOnOff:        // Gravis Audio : 0 : Off 1: On default or port  !! Need to add IRQ
#if USE_GUS   
      arg=((uint16_t)PM_CmdDataH<<8)+(uint16_t)PM_CmdDataL;
      PM_INFO("GUS OnOff: %x (%x %X)\n",arg,PM_CmdDataH,PM_CmdDataL);
      if (arg)
       { // If enable device, start audio mix
#if I2S_QWIIC_SHARES // When I2S and QwiiC are shared, Stop all QwiiC access when Audio is ON
#else
        qwiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
#endif
        dev_audiomix_start();            // Start Audio if not on
       }
      PM_CmdDataL=PM_gus_OnOff(arg);
      PM_CmdDataH=0;
#else
      PM_CmdDataL=1;  // Error, GUS not supported
      PM_CmdDataH=0;
#endif      
      PM_Status=STAT_READY;
      break;  

   case CMD_CVXOnOff:        // MMB Audio   : 0 : Off 1: On default or port, return install status
      arg=((uint16_t)(PM_CmdDataH&0x03)<<8)+(uint16_t)PM_CmdDataL;
      PM_INFO("CVX OnOff: %d %x\n",PM_CmdDataH>>2,arg);
      if (arg)
       { // If enable device, start audio mix
#if I2S_QWIIC_SHARES // When I2S and QwiiC are shared, Stop all QwiiC access when Audio is ON
#else
        qwiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
#endif
        dev_audiomix_start();            // Start Audio if not on
       }
      PM_CmdDataL=PM_CVX_OnOff(PM_CmdDataH>>2,arg);
      PM_Status=STAT_READY;
     break;

   case CMD_MPUOnOff:        // MPU/General MIDI : 0 : Off 1: On default or port, return install status
      arg=((uint16_t)(PM_CmdDataH&0x03)<<8)+(uint16_t)PM_CmdDataL;
      PM_INFO("MPU OnOff: %d %x\n",PM_CmdDataH>>2,arg);
      if (arg)
       { // If enable device, start audio mix
#if I2S_QWIIC_SHARES // When I2S and QwiiC are shared, Stop all QwiiC access when Audio is ON
#else
        qwiic_4charLCD_enabled=false;    // Stop Qwiic when audio is active
#endif
        dev_audiomix_start();            // Start Audio if not on
       }
      PM_CmdDataL=PM_MPU_OnOff(PM_CmdDataH>>2,arg);
      PM_Status=STAT_READY;
     break;    

// *** Disk Commands  0x8x ***
   case CMD_HDD_Getlist :  // CMD: Get the list of HDD images
      uint8_t nb;
      //PM_INFO("GetDirList: %d %d\n",PM_CmdDataL, PM_CmdDataH);
      if (PM_CmdDataL==0) 
         { // Get the list from Home folder
          pm_disk_InitSearchPath();
         }
        else
         { // Get the list, folder change
           PM_INFO("Load HDD, Change DIR %d\n",PM_CmdDataH);
           pm_disk_UpdateSearchPath(PM_CmdDataH, (FSizeName_t *) &PCCR_Param);
         }
      PM_CmdDataL=ListImages(1,&nb,(FSizeName_t *) &PCCR_Param);
      PM_PICO_EnablePSRAM();    // Re enable the PSRAM for the Memory emulation
      PM_Status=STAT_READY;    
     break;

   case CMD_FDD_Getlist :  // CMD: Get the list of FDD images
      if (PM_CmdDataL==0) 
         { // Get the list from Home folder
          pm_disk_InitSearchPath();
         }
        else
         { // Get the list, folder change
           PM_INFO("Load FDD, Change DIR %d\n",PM_CmdDataH);
           pm_disk_UpdateSearchPath(PM_CmdDataH, (FSizeName_t *) &PCCR_Param);
         }   
      PM_CmdDataL=ListImages(0,&nb,(FSizeName_t *) &PCCR_Param);
      PM_PICO_EnablePSRAM();    // Re enable the PSRAM for the Memory emulation      
      PM_Status=STAT_READY;   
     break;

   case CMD_HDD_Getinfos :  // CMD: Write the hdd infos in the Disk Memory buffer
     
     // memcpy(PM_DP_RAM,defaultdiskname,strlen(defaultdiskname)+1);
     //memcpy(,&PCCR_Param,sizeof());
     PM_Status=STAT_READY;
     break;
     
    case CMD_HDD_Mount :  // CMD: Mount the HDD images  
     if (PM_EnableSD())
      {
       char imgpath[64+26];
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
                sprintf(imgpath, "0:/HDD/%s%s", PM_Config->HDDPath[i], PM_Config->HDD[i].name);
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
     PM_PICO_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation   
     PM_Status=STAT_READY;
     break;

    case CMD_FDD_Mount :  // CMD: Mount the floppy images    
     PM_INFO("FDD Mount\n");    

     if (PM_EnableSD())
      {
       pm_disk_mount_fdd(0);    // Mount the FDD0
       pm_disk_mount_fdd(1);    // Mount the FDD1
       pm_disk_update_fdd_nb();
      }

     PM_PICO_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation
     PM_Status=STAT_READY;
     break;  

     case CMD_Image_Select :  // CMD: Select an image in the list (PM_CmdDataL: Image NB, PM_CmdDataH : 0-1 FDSD0-1 : 2-6 HDD0-3)
      PM_INFO("CMD_Image_Select: %d %d\n",PM_CmdDataH,PM_CmdDataL);
      SelectImage(PM_CmdDataH, PM_CmdDataL, (FSizeName_t *)&PCCR_Param);  // Select the image and return the image name in the Disk Memory buffer
      PM_Status=STAT_READY;
     break;

   case CMD_HDD_NewImage  :   // CMD:  
     PM_INFO("CMD_HDD_NewImage\n");
     if (PM_EnableSD()) 
      {
        uint8_t NI_return=NewHDDImage();   
        if (NI_return!=0)
           {
            PM_PICO_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation
            PM_INFO("Error:%d\n",NI_return);

            PM_CmdDataL=NI_return;   // Return an error code          
            PM_Status=STAT_CMDERROR;         
            break;
           }
      }
      PM_PICO_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation      
      PM_Status=STAT_READY;    
     break;
       
    case CMD_Int13h :  // Execute Int 13h, registers are passed via the BIOS Memory
     if (BV_PCCMD_SOURCE!=0)  // If the PC Command is running
        {
          PM_ERROR("Int13h: PC Command already running ! (%d)\n",BV_PCCMD_SOURCE);
          reg_ah=4;     // Return error 4
          reg_al=0xFF;
          reg_flagl=1;  // Set Carry (Bit 0)
          PM_Status=STAT_READY;
          break;
         }    

     BV_PCCMD_SOURCE=1;

     //printf("i%d",BV_Int13hCLI);

     if (PM_EnableSD()) 
      {
        if (PM_Int13h_LEDCount++==3) pm_led(1);  // Don't blink the LED all the time
        INT13_DiskHandler();        
      }
       else
      {
       reg_ah=4;     // Return error 4
       reg_al=0xFF;
       reg_flagl=1;  // Set Carry (Bit 0)
      }

     PM_PICO_EnablePSRAM();   // Re enable the PSRAM for the Memory emulation
	   PC_End();                // Tell the PC Command loop to end
  
     if (PM_Int13h_LEDCount==6) {
        pm_led(0);
        PM_Int13h_LEDCount=0;
       }

     BV_PCCMD_SOURCE=0;

     if (PM_Status==STAT_CMDINPROGRESS) PM_Status=STAT_READY;
     break;

    case CMD_EthDFS_Send :  // EtherDFS packet exchange
       pm_led(1);

       ethdfs_docmd((uint8_t*)&PM_ETHDFS_BUFF);

       PM_PICO_EnablePSRAM();   // Re enable the PSRAM for the Memory emulation (In case it is disabled)
       pm_led(0);
       PM_Status=STAT_READY;
     break;

     case CMD_PCINFO :  // Debug code for the moment (Press d or D under BIOS menu)

       uint8_t X,Y;
       uint16_t Data;
       uint8_t Page;
       uint16_t Offset,Size;

       PM_INFO ("PCINFO Command\n");
       
       PC_Getpos(&X,&Y);  
       PC_printf("\r\nBIOS D : %d %d\r\n",X,Y);
       
       PC_OUT8(0x80,0x12);

       PM_INFO("PicoMEM Test Port : ");
       for (int i=0;i<10;i++)
       {
        PM_INFO("%x,",PC_IN8(0x2A3));
       }
       PM_INFO("\n");

       PC_OUT8(0x0C,0);      // Clear DMA FlipFlop
       PC_OUT8(0x02,0xAA);   // Address L
       PC_OUT8(0x02,0x55);   // Address H

       PC_OUT8(0x03,0x00);   // Size L
       PC_OUT8(0x03,0x10);   // Size H

       PC_GetDMARegs(1,&Page,&Offset,&Size);
       PM_INFO("PC_GetDMARegs: Page:%X Offset: %X Size: %X\n",Page,Offset,Size);
       PM_INFO("Virtual DMA: Page:%X Offset: %X Size: %X\n",dmachregs[1].page, dmachregs[1].baseaddr, dmachregs[1].basecnt);
       
       PM_INFO("PC_SetDMARegs: Offset: 0x1234 Size: 0x1000\n");
       PC_SetDMARegs(1,0x1234,0x1000,0);

       PC_GetDMARegs(1,&Page,&Offset,&Size);
       PM_INFO("PC_GetDMARegs: Page:%x Offset: %x Size: %x\n",Page,Offset,Size);
       PM_INFO("Virtual DMA: Page:%x Offset: %x Size: %x\n",dmachregs[1].page, dmachregs[1].baseaddr, dmachregs[1].basecnt);

	     PC_End();            // Tell the PC Command loop to end      
       PM_Status=STAT_READY;
     break;

     case CMD_BIOS_MENU :  // Take the control in a BIOS menu
        // PM_CmdDataL Menu, PM_CmdDataH SubMenu
        PM_INFO ("BIOS Menu %d Sub Menu %d\n",PM_CmdDataH,PM_CmdDataL);
        
        pm_apps_biosmenu(PM_CmdDataH,PM_CmdDataL);
        
        PC_End();     // Tell the PC Command loop to end
        PM_Status=STAT_READY;
      break;

    case CMD_Int21h :         // Interrupt 21h "Spy"   

      if ((reg_ah==0x40) & (reg_bx==1))  // Skip write to screen
        {
          PM_Status=STAT_READY;
          break;
        }

       PM_INFO(".DD........... Int21h DOS ............DD.\n");

       switch (reg_ah)
       {
       case 0x0D: PM_INFO(". DISK RESET (AH:%x)\n",reg_ah);
                  break; 
       case 0x0E: PM_INFO(". SELECT DEFAULT DRIVE (AH:%x)\n",reg_ah);
                  break;  
       case 0x0F: PM_INFO(". OPEN FILE USING FCB (AH:%x)\n",reg_ah);
                  break;        
       case 0x10: PM_INFO(". CLOSE FILE USING FCB (AH:%x)\n",reg_ah);
                  break;
       case 0x11: PM_INFO(". FIND FIRST MATCHING FILE USING FCB (AH:%x)\n",reg_ah);
                  break;          
       case 0x12: PM_INFO(". FIND NEXT MATCHING FILE USING FCB (AH:%x)\n",reg_ah);
                  break;         
       case 0x13: PM_INFO(". DELETE FILE USING FCB (AH:%x)\n",reg_ah);
                  break;        
       case 0x14: PM_INFO(". SEQUENTIAL READ FROM FCB FILE (AH:%x)\n",reg_ah);
                  break;
       case 0x15: PM_INFO(". SEQUENTIAL WRITE TO FCB FILE (AH:%X)\n",reg_ah);
                  break;
       case 0x16: PM_INFO(". CREATE OR TRUNCATE FILE (AH:%X)\n",reg_ah);
                  break;
       case 0x17: PM_INFO(". EXTENDED RENAME FILE (AH:%x)\n",reg_ah);
                  break; 
       case 0x18: PM_INFO(". EXTENDED RENAME FILE (AH:%x)\n",reg_ah);
                  break;  
       case 0x19: PM_INFO(". GET CURRENT DEFAULT DRIVE (AH:%x)\n",reg_ah);
                  break; 
       case 0x1A: PM_INFO(". SET DISK TRANSFER AREA ADDRESS (AH:%x)\n",reg_ah);
                  break;                          
       case 0x23: PM_INFO(". GET FILE SIZE (AH:%x)\n",reg_ah);
                  break;
       case 0x25: PM_INFO(". SET INTERRUPT VECTOR (AH:%x)\n",reg_ah);
                  break;
       case 0x29: PM_INFO(". PARSE FILENAME INTO FCB (AH:%x)\n",reg_ah);
                  break;                  
       case 0x2A: PM_INFO(". GET SYSTEM DATE (AH:%x)\n",reg_ah);
                  break;
       case 0x30: PM_INFO(". GET DOS VERSION (AH:%x)\n",reg_ah);
                  break;                  
       case 0x2C: PM_INFO(". GET SYSTEM TIME (AH:%x)\n",reg_ah);
                  break;
       case 0x32: PM_INFO(". GET DOS DRIVE PARAMETER BLOCK FOR SPECIFIC DRIVE (AH:%x) DL:%X\n",reg_ah,reg_dl);
                  break;
       case 0x36: PM_INFO(". GET FREE DISK SPACE (AH:%x) Drive:%d\n",reg_ah,reg_dl);
                  break;
       case 0x39: PM_INFO(". MKDIR - CREATE SUBDIRECTORY (AH:%x)\n",reg_ah);
                  break;
       case 0x3A: PM_INFO(". RMDIR - REMOVE SUBDIRECTORY (AH:%x)\n",reg_ah);
                  break;
       case 0x3B: PM_INFO(". CHDIR - SET CURRENT DIRECTORY (AH:%x)\n",reg_ah);
                  break;                  
       case 0x3C: PM_INFO(". CREAT - CREATE OR TRUNCATE FILE (AH:%x)\n",reg_ah);
                  break;     
       case 0x3D: PM_INFO(". OPEN - OPEN EXISTING FILE (AH:%x)\n",reg_ah);
                  break;
       case 0x3E: PM_INFO(". CLOSE - CLOSE FILE (AH:%x) Hannd:%X\n",reg_ah,(uint16_t) reg_bl);
                  break;
       case 0x3F: PM_INFO(". READ - READ FROM FILE OR DEVICE (AH:%x) Handle:%X\n",reg_ah,(uint16_t) reg_bl);
                  break;                  
       case 0x40: PM_INFO(". WRITE - WRITE TO FILE OR DEVICE (AH:%x) Handle:%x\n",reg_ah,(uint16_t) reg_bl);
                  break;
       case 0x41: PM_INFO(". UNLINK - DELETE FILE (AH:%x) Handle:%xn",reg_ah,(uint16_t) reg_bl);
                  break;
       case 0x42: PM_INFO(". LSEEK - SET FILE POINTER (AH:%x) Handle:%x\n",reg_ah,(uint16_t) reg_bl);
                  break;
       case 0x43: PM_INFO(". GET FILE ATTRIBUTES (AH:%x)\n",reg_ah);
                  break;
       case 0x47: PM_INFO(". CWD - GET CURRENT DIRECTORY (AH:%x)\n",reg_ah);
                  break;
       case 0x48: PM_INFO(". ALLOCATE MEMORY (AH:%x) Paragraph:%x\n",reg_ah,(uint16_t) reg_bl);
                  break; 
       case 0x49: PM_INFO(". FREE MEMORY Segment:%x\n",reg_ah,(uint16_t) reg_esl);
                  break;
       case 0x4A: PM_INFO(". RESIZE MEMORY BLOCK (AH:%x)\n",reg_ah);
                  break;
       case 0x4B: PM_INFO(". EXEC - LOAD AND/OR EXECUTE PROGRAM %x\n",reg_ah);
                  break; 
       case 0x4C: PM_INFO(". EXIT - TERMINATE WITH RETURN CODE (AH:%x) AL:%x\n",reg_ah,reg_al);
                  break; 
       case 0x4D: PM_INFO(". GET RETURN CODE (ERRORLEVEL) (AH:%x)\n",reg_ah);
                  break; 
       case 0x4E: PM_INFO(". FINDFIRST - FIND FIRST MATCHING FILE (AH:%x)\n",reg_ah);
                  break;
       case 0x4F: PM_INFO(". FINDNEXT - FIND NEXT MATCHING FILE (AH:%x)\n",reg_ah);
                  break;
       case 0x50: PM_INFO(". SET CURRENT PSP (AH:%x)\n",reg_ah);
                  break;
       case 0x63: PM_INFO(". GET EXTENDED ERROR INFORMATION (AH:%x)\n",reg_ah);
                  break;

       default: PM_INFO(". AH:%x AL:%X BX:%X, CX:%X, DX:%X\n",reg_ah, reg_al, (uint16_t) reg_bl, (uint16_t) reg_cl, (uint16_t) reg_dl);
                break;
       }

       PM_Status=STAT_READY;
       break;
    case CMD_Int21End :
        PM_INFO(".DD........... Int21h END ............DD.\n");
        PM_INFO(". AH:%x AL:%X BX:%X, CX:%X, DX:%X\n",reg_ah, reg_al, (uint16_t) reg_bl, (uint16_t) reg_cl, (uint16_t) reg_dl);
        PM_Status=STAT_READY;
        break;

    case CMD_Int2Fh :         // Interrupt 2Fh "Spy"

       PM_INFO(".NR.......... Int2Fh NW Redir ...........NR.\n");
       if (reg_ah==0x11) {

         switch (reg_al)
         {
          case 0x16: PM_INFO(". NR - OPEN EXISTING REMOTE FILE (AH:%X)\n",reg_ah);
                     break;
          case 0x17: PM_INFO(". NR - CREATE OR TRUNCATE REMOTE FILE (AH:%X)\n",reg_ah);
                     break; 
          case 0x18: PM_INFO(". NR - CREATE/TRUNCATE FILE WITHOUT CDS (AH:%X)\n",reg_ah);
                     break;   
          case 0x19: PM_INFO(". NR - FIND FIRST FILE WITHOUT CDS (AH:%X)\n",reg_ah);
                      break;
          case 0x1A: PM_INFO(". NR - FIND NEXT FILE WITHOUT CDS (AH:%X)\n",reg_ah);
                     break;
          case 0x1B: PM_INFO(". NR - FINDFIRST (AH:%X)\n",reg_ah);
                      break;
          case 0x22: PM_INFO(". NR - PROCESS TERMINATION HOOK (AH:%x)\n",reg_ah);
                     break;
          case 0x23: PM_INFO(". NR - QUALIFY REMOTE FILENAME (AH:%x)\n",reg_ah);
                     break;
                                     
          default  : PM_INFO(". AH:%x AL:%X BX:%X, CX:%X, DX:%X\n",reg_ah, reg_al, (uint16_t) reg_bl, (uint16_t) reg_cl, (uint16_t) reg_dl);
                     break;
         }
      }

       PM_Status=STAT_READY;
     break;

    case CMD_DMA_TEST :
#if  USE_HWDMA 

     PM_INFO("DMA Test (HW DMA)\n");

     uint16_t trlen;

     if (dmachregs[1].changed)  // !! Risk if only one change is captured when arriving here, Nobody should do that
      {
       trlen=dmachregs[1].basecnt+1;  // DMA register is the size -1
       PM_INFO("DMA 1 REG Changed : DMA Addr:%x Size:%d\n",((uint32_t) ((dmachregs[1].page<<16)+dmachregs[1].baseaddr)),dmachregs[1].basecnt);
      } else ("DMA Regs not detected to be changed\n");


      for (int i=0;i<10;i++)
        {
         isa_dma_start_write();
         PM_INFO("%d, ",isa_dma_complete_write_blocking());
        }


#else
     PM_INFO("DMA Test (Virtual DMA)\n");

     uint16_t trlen;

     if (dmachregs[1].changed)  // !! Risk if only one change is captured when arriving here, Nobody should do that
      {
       trlen=dmachregs[1].basecnt+1;  // DMA register is the size -1
       PM_INFO("DMA 1 REG Changed : DMA Addr:%x Size:%d\n",((uint32_t) ((dmachregs[1].page<<16)+dmachregs[1].baseaddr)),dmachregs[1].basecnt);
      } else ("DMA Regs not detected to be changed\n");

     sbdsp.vdma_to_send=65;
     isa_dma_buffer_free();

      PM_INFO("SB DMA 1 Transfer %d samples\n",sbdsp.vdma_to_send);

     sbdsp_dma_worker();  // Do the VDMA transfer

     for (int i=0;i<65;i++)
      {
       int8_t res;
       if (afifo_take_sample8(&sbdsp_fifo, &res)) PM_INFO("%d ",res);
         else PM_INFO("x ",res);
      }

     sbdsp_dma_worker();  // Do the VDMA transfer

     for (int i=0;i<65;i++)
      {
       int8_t res;
       if (afifo_take_sample8(&sbdsp_fifo, &res)) PM_INFO("%d ",res);
         else PM_INFO("x ",res);
      }      
#endif

     PM_Status=STAT_READY;
     break;

    default:
     PM_ERROR("Not found ! \n");
     PM_Status=STAT_CMDNOTFOUND;  // Command not supported
     break;
  }
 PM_INFO ("E ");
 // printf("E %X",PM_Command);
}

//#include "pm_debug.h"
void pm_display_claimed_pio_sm()
{
    if (pio_sm_is_claimed(pio0, 0)) PM_ERROR("pio/sm pio0/0 already claimed\n");
       else PM_INFO("PIO pio0 SM 0 free\n");
    if (pio_sm_is_claimed(pio0, 1)) PM_ERROR("pio/sm pio0/1 already claimed\n");
       else PM_INFO("PIO pio0 SM 1 free\n");
    if (pio_sm_is_claimed(pio0, 2)) PM_ERROR("pio/sm pio0/2 already claimed\n");
       else PM_INFO("PIO pio0 SM 2 free\n");
    if (pio_sm_is_claimed(pio0, 3)) PM_ERROR("pio/sm pio0/3 already claimed\n");
        else PM_INFO("PIO pio0 SM 3 free\n");

    if (pio_sm_is_claimed(pio1, 0)) PM_ERROR("pio/sm pio1/0 already claimed\n");
       else PM_INFO("PIO pio1 SM 0 free\n");
    if (pio_sm_is_claimed(pio1, 1)) PM_ERROR("pio/sm pio1/1 already claimed\n");
       else PM_INFO("PIO pio1 SM 1 free\n");
    if (pio_sm_is_claimed(pio1, 2)) PM_ERROR("pio/sm pio1/2 already claimed\n");
       else PM_INFO("PIO pio1 SM 2 free\n");
    if (pio_sm_is_claimed(pio1, 3)) PM_ERROR("pio/sm pio1/3 already claimed\n");
        else PM_INFO("PIO pio1 SM 3 free\n");              
}

#if defined(RASPBERRYPI_PICO2) || defined(PIMORONI_PICO_PLUS2_RP2350)

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
 
     PM_INFO("pll_sys  = %dkHz\n", f_pll_sys);
     PM_INFO("pll_usb  = %dkHz\n", f_pll_usb);
     PM_INFO("rosc     = %dkHz\n", f_rosc);
     PM_INFO("clk_sys  = %dkHz\n", f_clk_sys);
     PM_INFO("clk_peri = %dkHz\n", f_clk_peri);
     PM_INFO("clk_usb  = %dkHz\n", f_clk_usb);
     PM_INFO("clk_adc  = %dkHz\n", f_clk_adc);
 #ifdef CLOCKS_FC0_SRC_VALUE_CLK_RTC
     PM_INFO("clk_rtc  = %dkHz\n", f_clk_rtc);
 #endif
 
    // Can't measure clk_ref / xosc as it is the ref
}

void p2_psram_test()
{

volatile uint8_t *test;
volatile uint16_t *test16;
volatile uint32_t *test32;

uint16_t result16;
uint32_t result32;

float psram_speed;

//measure_freqs();

 test=(uint8_t *) __psram_start__;
 test16=(uint16_t *) __psram_start__;
 test32=(uint32_t *) __psram_start__;


 PM_INFO("---- 8bit test (4MB):\n>Write Data (4MB) (Low byte of the adress)\n");
 for (uint32_t addr = 0; addr < 4*(1024 * 1024); ++addr) {
     test[addr]= (uint8_t) (addr & 0xFF);
    }

 PM_INFO(">Read Data\n");
 uint32_t ErrNb=0;
 uint32_t psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < 4*(1024 * 1024); ++addr) {
     uint8_t result = test[addr];
     if (static_cast<uint8_t>((addr & 0xFF)) != result) 
         {
          PM_INFO("PSRAM failure at address %x (%x != %x), 2nd read: %x\n", addr, addr & 0xFF, result, test[addr]);
         if (ErrNb++==10) break;
         }
 }
 uint32_t psram_elapsed = time_us_32() - psram_begin;
 psram_speed = 1000000.0 * 4* 1024.0 * 1024 / psram_elapsed;
 PM_INFO("8 bit : PSRAM read 4MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);

 
 PM_INFO("---- 16bit test (4MB):\n>Write Data (Low word of the adress)\n");
 for (uint32_t addr = 0; addr < 2*(1024 * 1024); ++addr) { // 2MB, 16Bit=4MB
     test16[addr]= (uint16_t) (addr & 0xFFFF);
    }

 PM_INFO(">Read Data\n");
 psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < 2*(1024 * 1024); ++addr) {
     result16 = test16[addr];
     if (static_cast<uint16_t>((addr & 0xFFFF)) != result16) 
         {
         PM_INFO("PSRAM failure at address %x (%x != %x), 2nd read: %x\n", addr, addr & 0xFFFF, result16, (uint16_t) test[addr]);
         if (ErrNb++==20) break;
         }
 }
 psram_elapsed = time_us_32() - psram_begin;
 psram_speed = 1000000.0 * 4* 1024.0 * 1024 / psram_elapsed;
 PM_INFO("16 bit : PSRAM read 1MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);

 /*
 PM_INFO("---- 32bit test (4MB):\n>Write Data (0xAAAAAAAA)\n");
 for (uint32_t addr = 0; addr < 1*(1024 * 1024); ++addr) {
     test32[addr]= 0xAAAAAAAA;
    }

 PM_INFO(">Read Data\n");
 uint32_t psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < 1*(1024 * 1024); ++addr) {
     result32 = test32[addr];
     if (result32!=0xAAAAAAAA)
         {
         printf("PSRAM failure at address %x (%x != 0xAAAAAAAA), 2nd read: %x\n", addr, result32, test32[addr]);
         if (ErrNb++==30) break;
         }
 PM_INFO("---- 32bit test End\n");

 PM_INFO("---- 32bit test (4MB):\n>Write Data (0x55555555)\n"); 
  for (uint32_t addr = 0; addr < 4*(1024 * 1024); ++addr) { // 2MB, 16Bit=4MB
       test[addr]= 0x55;
      } 

  PM_INFO(">Read Data\n");
 uint32_t psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < 1*(1024 * 1024); ++addr) {
     result32 = (uint32_t) test[addr];
     if (result32!=0x55555555)
         {
         printf("PSRAM failure at address %x (%x != 0x55555555), 2nd read: %x\n", addr, result32, (uint32_t) test[addr]);
         if (ErrNb++==40) break;
         }
 PM_INFO("---- 32bit test End\n");

 PM_INFO("---- 32bit test (4MB):\n>Write Data (0xFFFFFFFF)\n"); 
  for (uint32_t addr = 0; addr < 4*(1024 * 1024); ++addr) { // 2MB, 16Bit=4MB
       test[addr]= 0xFF;
      } 

 PM_INFO(">Read Data\n");
 uint32_t psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < 1*(1024 * 1024); ++addr) {
     result32 = (uint16_t) test[addr];
     if (result32!=0xFFFFFFFF)
         {
         printf("PSRAM failure at address %x (%x != 0xFFFFFFFF), 2nd read: %x\n", addr, result32, (uint32_t) test[addr]);
         if (ErrNb++==50) break;
         }
 PM_INFO("---- 32bit test End\n");

 PM_INFO("---- 32bit test (4MB):\n>Write Data (0)\n"); 
  for (uint32_t addr = 0; addr < 4*(1024 * 1024); ++addr) { // 2MB, 16Bit=4MB
       test[addr]= 0;
      } 

 PM_INFO(">Read Data\n");
 uint32_t psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < 1*(1024 * 1024); ++addr) {
     result32 = (uint32_t) test[addr];
     if (result32!=0)
         {
         PM_INFO("PSRAM failure at address %x (%x != 0), 2nd read: %x\n", addr, result32, (uint32_t) test[addr]);
         if (ErrNb++==60) break;
         }
 PM_INFO("---- 32bit test End\n");

*/

if (ErrNb==0) {PM_INFO("PSRAM Test OK :) \n");}
   else {PM_INFO("PSRAM Test FAIL :( \n");}

/*
PM_INFO("ps_total_space: %d\n", ps_total_space());
PM_INFO("ps_total_used: %d\n", ps_total_used());
PM_INFO("ps_largest_free_block %d\n",ps_largest_free_block());
*/

/*
test8=(uint8_t*) ps_malloc(1024);
if (test8!=NULL)
{
  PM_INFO("Allocated : Pointer to a 1024 byte table: %p\n",test8);
  PM_INFO("Write/Read 256 bytes:\n",test8);
  for (int i=0;i<256;i++)
  {
    test8[i]=(uint8_t) i;
    PM_INFO("%d,",test8[i]);
  }
} else PM_INFO("! Pointer not allocated\n");

PM_INFO("PSRAM Size :%d \n",ps_size);
PM_INFO("ps_total_space: %d\n", ps_total_space());
PM_INFO("ps_total_used: %d\n", ps_total_used());
PM_INFO("ps_largest_free_block %d\n",ps_largest_free_block());

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

void joy_test()
{
PM_INFO("Test Joystick\n");

dev_joystick_install();
joystate_struct.mode=3;
joystate_struct.pl_total=1;

gpio_set_dir(43,GPIO_OUT);
gpio_set_dir(46,GPIO_OUT);
gpio_set_dir(47,GPIO_OUT);
uint8_t d;
for (;;)
{
//  gpio_put(43,1);  // Test scope pin
  busy_wait_us(10);
//  gpio_put(46,0);  // Test scope pin  
  dev_joystick_ior(0x201,&d);
}  
}

//*******************************************************************************
//*                          2nd Core main code                                 *
#if BOARD_WM10
// WonderMCA: process any pending BIOS commands during init
// Must be called periodically to prevent Core 1 FIFO from blocking
void wm_drain_fifo(void) {
    while (multicore_fifo_rvalid()) {
        switch(multicore_fifo_pop_blocking()) {
            case 0:
                BV_CmdRunning = PM_Command;
                PM_ExecCMD();
                BV_CmdRunning = 0;
                break;
        }
    }
}
#endif

//*******************************************************************************
void main_core0 ()
{

#if BOARD_WM10
// WonderMCA: minimal Core 0 — just process commands immediately
PM_Status = STAT_READY;
PM_Init_Completed = true;
PM_INFO("WM10: Core0 command loop ready\n");

for (;;) {
    if (multicore_fifo_rvalid()) {
        switch(multicore_fifo_pop_blocking()) {
            case 0:
                BV_CmdRunning = PM_Command;
                PM_ExecCMD();
                BV_CmdRunning = 0;
                break;
            case 1: PM_INFO("IR ");
                break;
        }
    }
    tight_loop_contents();
}
#endif // BOARD_WM10

#if PM_PRINTF
pm_timefromlast();
#endif

isa_irq_init();
isa_dma_install();

#if BOARD_PM15 || BOARD_PM20   // Put on the PM1.5 Only for th emoment (SB / DMA)
DBG_PIN_INIT();
#endif

//dev_cvx_test();

// ************  Init the PicoMEM SPI PSRAM  ******************

#ifdef PIMORONI_PICO_PLUS2_RP2350
#else

#if USE_SPI_PSRAM
BV_PSRAMInit=PM_PICO_StartPSRAM();
PM_INFO("BV_PSRAMInit: %x\n",BV_PSRAMInit);
#if TEST_PSRAM
PSRAM_Test();     // PSRAM test fail if started before SD init !!!
#endif //TEST_PSRAM
#endif //PSRAM

#endif //PIMORONI_PICO_PLUS2_RP2350

// ************  Init the SDCARD and Read the config files ******************
// !!! If uSD Init is done before the PSRAM, Wifi init fail....
#if USE_SDCARD
#if PM_PRINTF	
pm_timefromlast();
#endif
PM_INFO(">Start uSD\n");
#if BOARD_WM10
wm_drain_fifo();
#endif

if (PM_StartSD())
   {
    if (pm_LoadConfig()==0) BV_CFGInit=0;  // Success
       else BV_CFGInit=0xFD;  // Failed
    BIOS_SetupDisks();        // Clean the Disk Objects
   } else BV_CFGInit=0xFF;    // Disabled (No Config to load)

#if BOARD_WM10
wm_drain_fifo();
#endif

if ((BV_CFGInit!=0)||(PM_BIOSADDRESS==0))
   {
    PM_Config->EnableUSB=1;
   }

#if TEST_SDCARD
PM_TestSD();
#endif //TEST_SDCARD
#endif //USE_SDCARD

// Values sent by the code to the BIOS Initialisation, Just after the Config Load
PM_Config->B_MAX_PMRAM=MAX_PMRAM;   // Value defined in the makefile.ini
#if USE_SPI_PSRAM
#else
PM_Config->PSRAM_Ext=0;            // Force PSRAM emulated RAM Off
#endif

// ************  Init the Pimoroni / PM1.5 PSRAM  ******************
#ifdef PIMORONI_PICO_PLUS2_RP2350
#if PSRAM_HEAP
PM_INFO(">Init RP2350 PSRAM with Heap at 1MB\n");
setup_psram(true,1024*1024);    // Without heap, Start at 1MB
PM_INFO("PSRAM Heap total space: %d bytes\n",ps_total_space());
#else
PM_INFO(">Init RP2350 PSRAM:");
setup_psram(false,0);    // Without heap
#endif

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

//p2_psram_test();
#endif

// **************  PicoMEM RTC init  ******************

// Set a default date
pico_rtc_init();

// ************  Qwiic Init and test code ******************

#if PM_PRINTF	
pm_timefromlast();
#endif
PM_INFO(">Start Qwiic\n");

pm_qwiic_init(100*1000);
pm_qwiic_scan();

if (qwiic_4charLCD_enabled)
 {
//  PM_INFO("QwiiC LED display found\n");

  qwiic_display_4char("LED-");

  #if DEV_POST_Enable
  dev_post_toinstall=true;
  #endif
//  ht16k33_demo();
 }

 pm_qwiic_scan_print(i2c1);

// **************  OLED Screen init  ******************

#if BOARD_PM15 || BOARD_PM20
//pm_i2c0_init(400*1000);  // Fast I2C for the OLED Screen

if (pm_display_init(PM_OLED_Type)) ;  // I2C init is here
   {
#if DEV_POST_Enable
   dev_post_toinstall=true;
#endif
    pm_input_init();  // Initialize as well the buttons for the display
   }
pm_i2c0_scan_print();

#endif
// **** Install POST device if enabled (Must be done after the I2C Init for the Qwiic) ****
#if DEV_POST_Enable
if (dev_post_toinstall) dev_post_install(); 
#endif

// **************  USB Init  ******************

#if PM_PRINTF	
pm_timefromlast();
#endif
PM_INFO(">Start USB\n");

#if USE_USBHOST
  if ((PM_Config->EnableUSB)==1)
   {
    PM_StartUSB();   // Start USB and update BV_USBInit  
   }
  else
   {
    BV_USBInit=INIT_DISABLED;   // Disabled
    PM_INFO("USB Host Disabled in BIOS\r\n");    
   } 
#else
  BV_USBInit=INIT_DISABLED;   // Disabled
  PM_INFO("USB Host Disabled in MakeFile\r\n");  
#endif

// **************  Wifi Init  ******************

#if PM_WIFI
#if USE_NE2000
PM_INFO(">Start Wifi\n");

// Be carefull, need to be in uSD Mode to read the wifi.txt file !
PM_EnableSD();

uint8_t WifiRes=PM_ConnectWifi();
//uint8_t WifiRes=1;

if (WifiRes!=0) 
   {
    BV_WifiInit=INIT_SKIPPED;  // 0xFC : Skipped/Error
    PM_Config->ne2000Port=0;
    PM_ERROR("\nWifi Init Err : %d\n",WifiRes);
   }
   else BV_WifiInit=INIT_DONE; // Wifi initialization Ok

#endif //
#endif // 

// **************  EtherDFS init  ******************
ethdfs_init();

// The PicoMEM Should start in PSRAM Mode
PM_PICO_EnablePSRAM();

// Initialize what is asked in no BIOS Mode
if (PM_BIOSADDRESS==0)
  {
  PM_INFO("NO BIOS (PM_BIOSADDRESS)=0 ?\n");
  }

#if TEST_DFS
ethdfs_test();
#endif

#if TEST_PSRAM  // ! PSRAM Test fail if not after SD Init !
PSRAM_Test();  
#endif //TEST_PSRAM

//dev_mpu_test();

//gpio_set_dir(43,GPIO_OUT);
//gpio_set_dir(46,GPIO_OUT);
//gpio_set_dir(47,GPIO_OUT);
//joy_test();

// After the Config Load, Force some default values (Hardcode, must not remain !)
PM_Config->EnableUSB=1; // Force USB Host for the moment

// display errors to the Qwiic if available
if (BV_PSRAMInit==0xFD) qwiic_display_4char("EPSR");
if (BV_SDInit==0xFD)    qwiic_display_4char("EuSD");

#if BOARD_WM10
wm_drain_fifo();
#endif
// PicoMEM Init Completed
if (BV_PSRAMInit==INIT_INPROGRESS) BV_PSRAMInit=INIT_SKIPPED;  // Security : Init Skipped
if (BV_SDInit==INIT_INPROGRESS) 
   { 
    BV_SDInit=INIT_SKIPPED;        // Security : Init Skipped
   }

BV_PCCMD_SOURCE=0;      // Reset the PCCMD_FLAG
BV_SPIPSRAM=0;          // Reset the SPI PSRAM Flag
BV_Int13hCLI=0;
BV_BoardID=BOARD_ID;    // Set the Board ID
PM_Status=STAT_READY;   // PicoMEM Initialisation completed (The BIOS Wait for its end)
PM_Init_Completed=true;

wifi_delay_counter=0;
pm_cmd_100msdelay = make_timeout_time_us(100000);
pm_cmd_1sdelay    = make_timeout_time_us(1000000);

PM_INFO("Init completed\n");

for (;;)  // Core 1 Main Loop (Commands)
{
#if BOARD_WM10
// WonderMCA: minimal Core 0 loop — only process commands via FIFO
if (multicore_fifo_rvalid()) {
    switch(multicore_fifo_pop_blocking()) {
        case 0:
            BV_CmdRunning = PM_Command;
            PM_ExecCMD();
            BV_CmdRunning = 0;
            break;
        case 1: PM_INFO("IR ");
            break;
    }
}
tight_loop_contents();
continue;  // Skip all ISA-specific update code below
#endif

 if (BV_PCCMD_SOURCE!=0) PM_INFO("f%d ",BV_PCCMD_SOURCE); // If the PC Command is running

#if DISP32    // For debug
  if (To_Display32!=0)
     {
      To_Display32_mask=To_Display32_mask|To_Display32_mask;
      To_Display32_2_mask=To_Display32_2_mask|To_Display32_2_mask;
      PM_INFO("%X:%X ",To_Display32,To_Display32_2);
      To_Display32=0;
     }
#endif

#if USE_USBHOST
if (USB_Host_Enabled)
 {
  tuh_task();

  if (pm_mouse.event&&MOUSE_Enabled)
   {
   // PM_INFO2("!MI ");
    pm_mouse.event=false; // Reset the mouse event variable
    SVAR_IRQ->mouse_x=pm_mouse.x;   // Send the data to the Shared VAR table
    SVAR_IRQ->mouse_y=pm_mouse.y;
    SVAR_IRQ->mouse_b=pm_mouse.buttons;
    PM_INFO("Mouse Event : x:%d y:%d b:%d\n",pm_mouse.x,pm_mouse.y,pm_mouse.buttons);
    bus_pm_irq_raise(IRQ_R_MOUSE,1,false);
    // No need to lower the IRQ commands, the PM IRQ Acknowledge itself
    }
 }
#endif

#if USE_MPU
  dev_mpu_update();
#endif

#if USE_CDROM
  dev_cdr_update();
#endif

#if USE_SBDSP
  dev_sbdsp_update();  
#endif

#if !BOARD_WM10
if (dev_audiomix.enabled) pm_audio_domix();
#endif

absolute_time_t pm_delay_delta=absolute_time_diff_us(get_absolute_time(), pm_cmd_100msdelay);
// Portion of the code executed every 100ms for fast update code (Display)
if (absolute_time_diff_us(get_absolute_time(), pm_cmd_100msdelay) < 0)
 {
#if DEV_POST_Enable  // POST Code display
   if (!dev_audiomix.dev_active) dev_post_update();  // Update POST Display, Only when there is no Audio
#endif

#if !BOARD_WM10
#if USE_OLED
   if (dev_oled_screen.enabled)
      {
       pm_input_task();
       pm_display_worker(-pm_delay_delta);
      }
#endif
#endif

// Portion of the code executed every second for "Slow" reaction time needed code
   if (absolute_time_diff_us(get_absolute_time(), pm_cmd_1sdelay) < 0)
     {  
      PM_INFO("*");  // Show that the system is alive (and not stuck in a loop for example)

      // 1s based actions done all the time
#if USE_RTC
      if (dev_rtc.do_update) dev_rtc_update();
#endif

#if CORE1_ALIVETEST
      if ( Core1_AliveCnt==Core1_AliveCnt_Last) 
         {
            PM_ERROR("Core1 Stuck ?\n");
         } else
         {
           Core1_AliveCnt_Last=Core1_AliveCnt;
           PM_INFO("*"); // Show that the core is alive
         }
#endif

      if (dev_audiomix.dev_active)
       { // 1s based actions when Audio is On
        if (dev_adlib.delay++==4) {  // 4 seconds since last Adlib I/O
          dev_adlib_disable_mix(); //Disable the mixing
         }
        if (dev_cms.delay++==4) {   // 4 seconds since last CMS I/O
          dev_cms_disable_mix();  // Disable the CMS mixing
         }
        if (dev_tdy.delay--==0) {   // 4 seconds since last Tandy I/O
          dev_tdy_disable_mix();  // Disable the Tandy mixing
         }
        if (dev_mmb.delay++==4) {   // 4 seconds since last Mindscape I/O
          dev_mmb_disable_mix();  // Disable the mixing
         } 
        if (dev_cvx.delay++==1) {   // 1 seconds since last Covox I/O or sound output
          dev_cvx_disable_mix();  // Disable the mixing
          cvx_stop_dac();         // STOP the Covox DAC
         }
#if USE_MPU
         if (dev_mpu_delay++==8) {  // 10 seconds since last mpu Output
          dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_MPU;    //Disable the mixing
         } 
#endif                  
        if (dev_sbdsp_delay++==8) {  // 4 second since last sbdsp I/O or sound output
          dev_sbdsp_disable_mix();  //Disable the Sound Blaster mixing
        }
#if USE_GUS        
        if (dev_gus.delay++==10) {  // 4 second since last sbdsp I/O or sound output
          dev_gus_disable_mix();  //Disable the Sound Blaster mixing
        }  
#endif                
      }
       else
      {  // 1s based actions when Audio is Off
#if PM_WIFI
      if (wifi_delay_counter++==WIFI_DELAY) {  // Check Wifi connection Status / reconnect every 10s
          if (BV_WifiInit==INIT_DONE) {
               PM_INFO("Check Wifi Connexion\n");
               PM_Wifi_GetStatus();
               if (PM_Wifi.Status<=0)
                 {
                   if (dev_audiomix.enabled) pm_audio_domix();  
                   PM_RetryWifi();   // Retry to connect if connection error/Failed
                 }                  
               wifi_delay_counter=0;
              }
            } 
#endif
      } 

      pm_cmd_1sdelay = make_timeout_time_us(1000000);    // Update the 1s delay value (1000000 is 1 second)
     }  // 1s delay end

    pm_cmd_100msdelay = make_timeout_time_us(100000);  // Update the 100ms delay value (100000 is 100ms)
  } // 100ms delay end (For Display update)

if (multicore_fifo_rvalid())
  {
  switch(multicore_fifo_pop_blocking())
   {
    case 0 :
            BV_CmdRunning=PM_Command;  // To allow the BIOS to detect when the command really start
            PM_ExecCMD();
            BV_CmdRunning=0;
            break;
    case 1 : PM_INFO("IR ");
            break;
#if PM_WIFI
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
