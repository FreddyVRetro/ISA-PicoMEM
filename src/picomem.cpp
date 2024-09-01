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
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/vreg.h"
#include "hardware/regs/vreg_and_chip_reset.h"

#include "hardware/structs/bus_ctrl.h"
#include "hardware/regs/busctrl.h"              /* Bus Priority defines */

#include "ff.h"    // Needed by pm_disk.h

//Other PicoMEM Code
#include "pm_defines.h"     // Shared MEMORY variables defines (Registers and PC Commands)
#include "pm_cmd.h"
#include "pm_libs.h"
#include "pm_gpiodef.h"     // Various PicoMEM GPIO Definitions
#include "pm_disk\pm_disk.h"
#include "pm_pccmd.h"
#include "qwiic.h"

//SDCARD and File system definitions
#include "hw_config.h"  /* The SDCARD and SPI definitions */	
#include "f_util.h"
#include "ff.h"
#include "sd_card.h"

// PicoMEM emulated devices include
#include "dev_post.h"
#include "dev_joystick.h"

#if USE_AUDIO
#include "audio_devices.h"
#include "dev_adlib.h"
#endif

// Memory and IO Tables in the core stack RAM (Must be before PM_defines)

volatile uint8_t MEM_Type_T[MEM_T_Size];     // Memory type list
// Return a memory Index based on the PC Address >>13 (8Kb)
__scratch_x("MemDevTable") volatile uint8_t MEM_Index_T[MEM_T_Size]; // Memory @ Table for fast decoding
// Pointer to the "Memory index" table in the RP2040 Address Space
// = RP2040 Memory Address - PC Memory Base Address
__scratch_x("MemBaseTable") volatile uint8_t *RAM_Offset_Table[16];

//Devices emulation tables
// Port table for fast decoding (IO Port >>3 then 8 Bytes precision)
__scratch_x("IODevTable") volatile uint8_t PORT_Table[256];  // Now size=256, set the upper 128 to 0 (DMA Detection)

// MEM / IO fonctions defines
#include "dev_memory.h"
#include "dev_picomem_io.h"

#if USE_NE2000

extern "C" {
#include "dev_ne2000.h"
extern wifi_infos_t PM_Wifi;
}
 
#endif

//PSRAM Definitions
#if USE_PSRAM
#include "psram_spi.h"
pio_spi_inst_t psram_spi;
#endif

#include "isa_iomem.pio.h"

// Define the UART values for debug (On the Free GPIO 28)
#ifdef ASYNC_UART
#include "stdio_async_uart.h"
#endif 
#if UART_PIN0
#define UART_TX_PIN 0
#else
#define UART_TX_PIN 28
#endif
#define UART_RX_PIN (-1)
#define UART_ID     uart0
#define BAUD_RATE   115200

#define LED_PIN 25

// ** State Machines ** Order should not be changed, "Hardcoded" (For ISA)
// pio0 / sm0 : isa_bus_sm : ISA bus communication
//
// pio1 / sm0              : SD or PSRAM
// pio1 / sm1              : SD or PSRAM
//
// DMA Channels :
// SD  : DMA0 & 1 (If claimed first)
// USB : ?
// DMA IRQ : 0 for Audio, 1 for SD

//***********************************************
//*    PicoMEM PIO State Machine variables      *
//***********************************************

PIO isa_pio = pio0;
#define isa_bus_sm 0            // Warning : HardCoded ISA State Machine number for speed
//static uint isa_bus_sm;       // State machine number for ISA BUS
static uint sd_spi_sm;          // State machine number for SD

//***********************************************
//*              PicoMEM Variables              *
//***********************************************

#define CTRL_MR   0x01  // 0001b
#define CTRL_MW   0x02  // 0010b
#define CTRL_IOR  0x04  // 0100b
#define CTRL_IOW  0x08  // 1000b
							
#define PM_IOBASEPORT 0x2A0 

#define DEV_POST_Enable 1

uint8_t PM_BasePort;  // Real port >>3

bool     ISA_WasRead;

// Lotech like EMS Regs
uint8_t  EMS_Bank[4]={0,1,2,3};
uint32_t EMS_Base[4]={0,1*16*1024,2*16*1024,3*16*1024};

// RAM/ROM Emulation tables
extern "C" const uint8_t _binary_pmbios_bin_start[];
extern "C" const uint8_t _binary_pmbios_bin_size[];

uint8_t  __in_flash() *PMBIOS=(uint8_t *)_binary_pmbios_bin_start;

//uint8_t PM_Memory[128*1024];           // Table used to emulate RAM from the Pico SRAM
uint8_t PM_Memory[16*1024*MAX_PMRAM];    // Table used to emulate RAM from the Pico SRAM
volatile uint8_t PM_DP_RAM[8*1024];    // Disk I/O Buffer, to map in the ROM RAM.

//uint8_t PM_BIOS12k[12*1024];

volatile uint32_t RAM_InitialAddress;  // This is the Base Address of the RAM Emulated by the Pi Pico Memory  

// ** Virtual Disks Variables **

volatile uint8_t *PM_PTR_DISKBUFFER;

volatile uint32_t To_Display32;

void Init_ISAIOVars()
{

//  Initialize the ISA PIO Code
gpio_set_slew_rate(PIN_AD, GPIO_SLEW_RATE_FAST);
gpio_set_slew_rate(PIN_AS, GPIO_SLEW_RATE_FAST);
gpio_set_slew_rate(PIN_IORDY, GPIO_SLEW_RATE_FAST);

// Start the ISA PIO State Machine

uint isa_bus_offset = pio_add_program(isa_pio, &isa_bus_program);
//printf("ISA_Offset: %d SM: %d\n",isa_bus_offset,isa_bus_sm); 
if (pio_claim_unused_sm(isa_pio, true)!=isa_bus_sm) printf("PIO SM For ISA Error");
isa_bus_program_init(isa_pio, isa_bus_sm, isa_bus_offset);

// Invert the A16_MR to A11_IW pin, for signal detect speed up !! If placed before the SM init, does not work
gpio_set_inover (PIN_A16_MR,GPIO_OVERRIDE_INVERT);
gpio_set_inover (PIN_A17_MW,GPIO_OVERRIDE_INVERT);
gpio_set_inover (PIN_A18_IR,GPIO_OVERRIDE_INVERT);
gpio_set_inover (PIN_A19_IW,GPIO_OVERRIDE_INVERT);

PM_BasePort=PM_IOBASEPORT>>3;
SetPortType(PM_IOBASEPORT,DEV_PM,1);

// Default MEMORY Blocks configuration

#ifdef ROM_BIOS
PM_BIOSADDRESS=ROM_BIOS>>4;
SetMEMType(ROM_BIOS,MEM_BIOS,MEM_S_16k);
SetMEMIndex(ROM_BIOS,MEM_BIOS_I,MEM_S_16k);
RAM_Offset_Table[MEM_BIOS_I]=&PMBIOS[-ROM_BIOS];

SetMEMType(ROM_DISK,MEM_DISK,1);  // 8kb only
SetMEMIndex(ROM_DISK,MEM_DISK_I,1);
RAM_Offset_Table[MEM_DISK_I]=&PM_DP_RAM[-ROM_DISK];

PC_DB_Start=ROM_DISK+PM_DB_Offset;
#endif

PM_DP_RAM[0] = 0x12; //ID of The PicoMEM BIOS RAM  (Magic ID)

} //Init_ISAIOVars

#define DO_MEMR 0x3010u  // PIO instruction for (wait 0 gpio PIN_A16_MR) 
#define DO_IOR  0x3012u  // PIO instruction for (wait 0 gpio PIN_A18_IR) 

__force_inline void pm_do_wait(void)
{
}

__force_inline void pm_do_ior(void)
{
 ISA_WasRead=true; 
 pio_sm_put(isa_pio, isa_bus_sm, DO_IOR); // Do IOR (wait 0 gpio PIN_A18_IR)
}

__force_inline void pm_do_iornows(void)
{
 ISA_WasRead=true;
 pio_sm_put(isa_pio, isa_bus_sm, DO_IOR); // Do IOR (wait 0 gpio PIN_A18_IR)
}

// Start of a Memory Read with Wait States added
__force_inline void pm_do_memr(uint32_t ISA_Data)
{
// ISA_WasRead=true;
 pio_sm_put(isa_pio, isa_bus_sm, DO_MEMR);                  // 2nd Write : Do MEMR (wait 0 gpio PIN_A16_MR)  
 pio_sm_put(isa_pio, isa_bus_sm, 0x00ffff00u | ISA_Data);   // 3nd Write : Send the Data to the CPU (Read Cycle)
 asm volatile ("nop");                                      // Force the compiler to not prepare the next instruction in advanced : One Cycle less
 // Added to wait until the PIO Send the Data
 #if TIMING_DEBUG
 //gpio_put(PIN_IRQ, 1);
 #endif
 ISA_Data = pio_sm_get_blocking(isa_pio, isa_bus_sm);       // Wait that the data is sent to wait for the next ALE 
}

//***********************************************************************************************//
//*         CORE 1 Main : Wait for the ISA Bus events, emulate RAM and ROM                      *//
//***********************************************************************************************//
// The code is placed in the Scratch_x (4k Bank used for the Core 1 Stack) to avoid RAM Access conflict.

//void __time_critical_func(main_core1)(void)
void __scratch_x("core1_ISA_code") (main_core1)(void)
{
uint32_t  ISA_Addr;
uint8_t   ISA_Data;

// Send the value to use to return the fake Address (Address where the is a ROM or no IO)
 pio_sm_put(isa_pio, isa_bus_sm, 0x0FFFFF0F0);  // If 0x0FFFFF0FF, Floppy access (DMA) Crash

for (;;) {
#if TIMING_DEBUG   
//  gpio_put(PIN_IRQ,0);  // IRQ Up
#endif

 ISA_WasRead = false;

// 2 Values returned by the Assembly code
  static uint32_t Ctrl_m=0x0F0000;
  uint32_t IO_CTRL_MDIndex;          // Control for IO, Dev Type for MEM, No need for uint8_t

 // ** Wait until a Control signal is detected **
 // ** Then, jump to MEMR / MEMW / IO and decode the Memory Address and Device Index
 asm volatile goto (
     "DetectCtrlEndLoop:            \n\t"   // 1) Wait until there is no more Control signal (We detect an edge)
     "ldr %[DEV_T],[%[IOB],%[IOO]]   \n\t"  // Load the GPIO Values 
     "tst %[DEV_T],%[CM]             \n\t"  // Apply the mask to keep the ISA Control Signals Only
     "bne DetectCtrlEndLoop         \n\t"   // Loop if Control Signal detected (!=0)

     "DetectCtrlLoop:               \n\t"   // 2) Wait until a Control Signal is present
     "ldr %[DEV_T],[%[IOB],%[IOO]]   \n\t"  // Load the GPIO Values
     "tst %[DEV_T],%[CM]             \n\t"  // Apply the mask to keep the ISA Control Signals Only    
     "beq DetectCtrlLoop            \n\t"   // Loop if no Control signal detected
     "str %[DEV_T],[%[PIOB],%[PIOT]] \n\t" // pio->txf[0] = CTRL_AL8; Start the Cycle 
                                           // Then, PIO will returns AL12 or FFFFh if a DMA Cycle is detected   
//     "tst %[DEV_T],2"
//     "beq NoDMA                   \n\t"  // Loop if no Control signal detected
//     "NoDMA:                      \n\t"

     "lsl %[DEV_T],%[DEV_T],#12       \n\t"  // ISA_CTRL=ISA_CTRL<<12   CAAxxxxx
     "lsr %[DEV_T],%[DEV_T],#28       \n\t"  // ISA_CTRL=ISA_CTRL>>28   -> ISA_CTRL is now correct

     "cmp %[DEV_T],#1                \n\t"   // Is it a MEM Read ?
     "beq  ISA_Do_MEMR               \n\t"   // Jump must be <200 bytes

     "cmp %[DEV_T],#2                \n\t"   // Is it a MEM Read ?
     "beq  ISA_Do_MEMW               \n\t"   // Jump must be <200 bytes

//** > ISA IOR / IOW   **

     "b %l[ISA_Do_IO]                \n\t"   // If %[TMP]!=0 Do IO    Jump must be <200 bytes

//** > ISA Memory Write Address read and IOCHRDY **
     "ISA_Do_MEMW:                      \n\t"
//     "nop            \n\t"    // Added to wait until the PIO Send the Address Value  
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value 
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value 
// Directly read the RX pipe : Be carefull about the timing
     "ldr %[ADDR],[%[PIOB],%[PIOR]]      \n\t"   // ISA_Addr<<12 = pio->rxf[sm]; Read ISA_Addr from the PIO (Address with Mask)
     "lsr %[DEV_T], %[ADDR], #25         \n\t"   // ISA_Addr>>20+4 (16kb Block) >>20+5 for 8Kb Block
     "ldrb %[DEV_T], [%[MIT], %[DEV_T]]  \n\t"   // IO_CTRL_MDIndex = MEM_Index_T[(ISAM_AH12>>x)] (TMP=MTA)
     "lsr %[MIT], %[DEV_T], #3           \n\t"   // TMP = IO_CTRL_MDIndex>>3 (0-7 : No Wait State)
     "str %[MIT], [%[PIOB], %[PIOT]]     \n\t"   // pio->txf[sm] = TMP;  > Command the IOCHRDY Move if !=0
// complete the ISA_Addr Address calculation for Memory Read
     "lsr %[ADDR], %[ADDR], #12          \n\t"   // ISA_Addr=ISA_Addr>>12
     "eor %[ADDR], %[CM]                 \n\t"   // ISA_Addr+ISA_Addr^Ctrl_m  (0x0F0000)
     "b %l[ISA_Do_MEMW]                  \n\t"   // 

//** > ISA Memory Read Address read and IOCHRDY **
     "ISA_Do_MEMR:                      \n\t"   // Minimum 4 NOP (20/05)  5 at 300MHz on the PM 1.0
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value  
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value  
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value  
#if DO_FAST_RAM    // Fast RAM : Can tell to not move IOCHRDY Directly
     "mov %[ADDR], #0                     \n\t"   // TMP = 0
     "str %[ADDR], [%[PIOB], %[PIOT]]     \n\t"   // pio->txf[sm] = TMP;  > Command the IOCHRDY Move if !=0
#else
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value 
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value 
#endif

// Directly read the RX pipe : Be carefull about the timing
     "ldr %[ADDR],[%[PIOB],%[PIOR]]      \n\t"   // ISA_Addr<<12 = pio->rxf[sm]; Read ISA_Addr from the PIO (Address with Mask) 
     "lsr %[DEV_T], %[ADDR], #25         \n\t"   // ISA_Addr>>20+4 (16kb Block) >>20+5 for 8Kb Block
     "ldrb %[DEV_T], [%[MIT], %[DEV_T]]  \n\t"   // IO_CTRL_MDIndex = MEM_Index_T[(ISAM_AH12>>x)] (TMP=MTA)
#if DO_FAST_RAM    // Fast RAM : Only 0 or a no PSRAM Memory
#else
     "lsr %[MIT], %[DEV_T], #3           \n\t"   // TMP = IO_CTRL_MDIndex>>3 (3, 0-7 : No Wait State) (3, 0-7)
     "str %[MIT], [%[PIOB], %[PIOT]]     \n\t"   // pio->txf[sm] = TMP;  > Command the IOCHRDY Move if !=0
#endif
// complete the ISA_Addr Address calculation for Memory Read
     "lsr %[ADDR], %[ADDR], #12          \n\t"   // ISA_Addr=ISA_Addr>>12
     "eor %[ADDR], %[CM]                 \n\t"   // ISA_Addr+ISA_Addr^Ctrl_m (0x0F0000)

// Define Outputs
//     :[I_CT]"+l" (ISA_CTRL),
      :[DEV_T]"+l" (IO_CTRL_MDIndex),      // IO_CTRL_MDIndex is for MEM, use it as ISA_CTRL for I/O and used as Temp register
      [ADDR]"+l" (ISA_Addr)     
// Define Inputs
     :[CM]"l" (Ctrl_m),
      [MIT]"l" (MEM_Index_T),          // l for low register     
      [IOB]"l" (SIO_BASE),             // IO Registers Base Address (l for low register)
      [IOO]"i" (SIO_GPIO_IN_OFFSET),   // Integer, GPIO In Offset (#4)
      [PIOB]"l"(isa_pio),               
      [PIOT]"i" (PIO_TXF0_OFFSET),
      [PIOR]"i" (PIO_RXF0_OFFSET)
     :"cc"                              // Tell the compiler that regs are changed
     : ISA_Do_IO,
       ISA_Do_MEMW
      );

//*************************************************
//***         The ISA Cycle is MEM              ***
//*************************************************
// !! This portion of the code is "Hard Coded" to be cycle accurate with the State Machine !!
// If the Memory access does not work anymore, check this part of the code and add "nop".
// ! 3 nop after if only one ASM instruction is inserted.

  //ISA_Addr = pio_sm_get_blocking(isa_pio, isa_bus_sm); // Read A8-A12
  //IO_CTRL_MDIndex = MEM_Index_T[ISA_Addr>>(5+20)];
  //pio_sm_put(isa_pio, isa_bus_sm, IO_CTRL_MDIndex); // First pio put : No Wait State if 0 
  //ISA_Addr = (ISA_Addr>>12^Ctrl_m);

//  printf("Ma:%x %x ",ISA_Addr,IO_CTRL_MDIndex);
//  busy_wait_ms(50);

#if DISP32
//To_Display32=ISA_Addr;
#endif

// ********** ISA_Cycle= Memory Read ********** (1110b)

// ** Read from the Pico Memory or ROM
#if DO_FAST_RAM    // Fast RAM : Only 0 or a no PSRAM Memory
   if (IO_CTRL_MDIndex!=0) 
#else
   if ((IO_CTRL_MDIndex>0)&&(IO_CTRL_MDIndex<16))  
#endif     
        {
        pm_do_memr(RAM_Offset_Table[IO_CTRL_MDIndex][ISA_Addr]);
    //    To_Display32=IO_CTRL_MDIndex;
        continue;  // Go back to the main loop begining
        }
#if DO_FAST_RAM   // Fast RAM : Only 0 or a no PSRAM Memory
        else
       {
        pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
        continue;  // Go back to the main loop begining
       }
#else
   if (IO_CTRL_MDIndex==0) {
        pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
        continue;  // Go back to the main loop begining
       }

// ** Read from the PSRAM  (Emulated RAM or EMS)
   if (IO_CTRL_MDIndex==MEM_EMS) {  // EMS
        ISA_Addr=EMS_Base[(ISA_Addr>>14)&0x03]+(ISA_Addr & 0x3FFF);  // Compute the base PSRAM Address              
       }
//#if PM_PRINTF
//          if (!ePSRAM_Enabled) printf("!Fatal EMS Read %x ",ISA_Addr);             
//#endif 
       pm_do_memr(psram_read8(&psram_spi, ISA_Addr));                            
       continue;  // Go back to the main loop begining
#endif

ISA_Do_MEMW:

// ********** ISA_Cycle=Memory Write (1101b) **********
   uint32_t ISAMW_Data;
   ISAMW_Data=((uint32_t)gpio_get_all()>>PIN_AD0);   

// ** Write to the PicoMEM Internal RAM
#if DO_FAST_RAM    // Fast RAM : Only 0 or a no PSRAM Memory
   if (IO_CTRL_MDIndex!=0) 
#else
   if ((IO_CTRL_MDIndex>0)&&(IO_CTRL_MDIndex<MI_RW_NOWS_END))  // Memory in the Pico Memory or ROM
#endif
             {
              RAM_Offset_Table[IO_CTRL_MDIndex][ISA_Addr]=(uint8_t) ISAMW_Data;
              pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
      //        To_Display32=IO_CTRL_MDIndex+1<<8;
              continue;  // Go back to the main loop begining              
             }
#if DO_FAST_RAM   // Fast RAM : Only 0 or a no PSRAM Memory
        else
       {
        pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
        continue;  // Go back to the main loop begining
       }
#else
   if (IO_CTRL_MDIndex==0) {
          pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
          continue;  // Go back to the main loop begining
         }

// ** Write to the PSRAM (Emulated RAM or EMS)
   if (IO_CTRL_MDIndex==MEM_EMS) {  // EMS
         ISA_Addr=EMS_Base[(ISA_Addr>>14)&0x03u]+(ISA_Addr & 0x3FFFu);  // Compute the base PSRAM Address           
        }
   psram_write8(&psram_spi, ISA_Addr, (uint8_t) ISAMW_Data);

   pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
   continue;  // Go back to the main loop begining
#endif
 //***    End of Memory Cycle         ***

//************************************************
//***         The ISA Cycle is IO              ***
//************************************************
ISA_Do_IO:  // Goto, from the assembly code

  uint32_t ISA_IO_Addr;
  uint32_t IO_Device;

  ISA_Addr = pio_sm_get_blocking(isa_pio, isa_bus_sm); // Read (A19-A0)<<12
// Initial : 11
//  ISA_IO_Addr = (ISA_Addr>>12) & 0x3FF;
//  IO_Device = PORT_Table[ISA_IO_Addr>>3];

  IO_Device = PORT_Table[(ISA_Addr<<9)>>24];  // <<9 to keep the Bit 13 as well, for DMA Detection
  pio_sm_put(isa_pio, isa_bus_sm, IO_Device); // First pio put : No Wait State if 0

  asm volatile ("nop");

  ISA_IO_Addr = (ISA_Addr<<10)>>22;           // Same as (ISA_Addr>>12) & 0x3FF

/*  ISA_Addr=(ISA_Addr^0xF0000000)>>12;
  if (ISA_Addr>=0x3FF) 
     {
      //To_Display32=ISA_Addr;
      IO_Device=0;
     }
*/

//if (IO_Device) To_Display32=ISA_Addr;
//if (IO_Device!=4) IO_Device=0;
//IO_Device=0;
//To_Display32=IO_Device;

  switch(IO_CTRL_MDIndex)
      {
// ********** PicoMEM IOW **********        
       case CTRL_IOW : // ISA_Cycle=IOW  (0111b)
       uint32_t ISAIOW_Data;
       ISAIOW_Data=((uint32_t)gpio_get_all()>>PIN_AD0)&0xFF;
       switch(IO_Device)
        {
        case DEV_PM :
           dev_pmio_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
        case DEV_LTEMS :  // LTEMS : Write to the Bank Registers
           EMS_Bank[(uint8_t) ISA_IO_Addr & 0x03]=ISAIOW_Data;   // Write the Bank Register
	         EMS_Base[(uint8_t) ISA_IO_Addr & 0x03]=((uint32_t) ISAIOW_Data*16*1024)+1024*1024; // EMS Bank base Address : 1Mb + Bank number *16Kb (in SPI RAM)
	      break;  //DEV_LTEMS
#if USE_NE2000
        case DEV_NE2000:
           PM_NE2000_Write((ISA_IO_Addr-PM_Config->ne2000Port) & 0x1F,ISAIOW_Data);
          break;
#endif
#if DEV_POST_Enable==1
	      case DEV_POST :
           dev_post_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
#endif
#if USE_USBHOST
        case DEV_JOY :
           dev_joystick_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
#endif          
#if USE_AUDIO        
        case DEV_ADLIB :
           dev_adlib_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
#endif

// *** Add Other device IOW here ***         

        }          // switch(IO_Device) (Write)
        break;     // End ISA_Cycle=IOW

// ********** PicoMEM IOR ********** 
       case CTRL_IOR : // ISA_Cycle=IOR (1011b)
       switch(IO_Device)
        {
        case DEV_PM : 
           dev_pmio_ior(ISA_IO_Addr,&ISA_Data);
           pm_do_ior();
          break; //DEV_PM    
        case DEV_LTEMS :
           ISA_Data=EMS_Bank[(uint8_t) ISA_IO_Addr & 0x03];  // Read the Bank Register
           pm_do_ior();
          break;  //DEV_LTEMS  
#if PM_PICO_W
#if USE_NE2000             
        case DEV_NE2000:        
           ISA_Data = PM_NE2000_Read((ISA_IO_Addr-PM_Config->ne2000Port) & 0x1F);
           pm_do_ior();
          break;          
#endif          
#endif
#if USE_USBHOST
        case DEV_JOY:
           dev_joystick_ior(ISA_IO_Addr,&ISA_Data);
           pm_do_ior();           
          break;
#endif
#if USE_AUDIO
        case DEV_ADLIB:
           dev_adlib_ior(ISA_IO_Addr,&ISA_Data);
           pm_do_ior();           
          break;
#endif          


   // *** Add Other device IOR here ***
        }         // switch(IO_Device) (Read)
        break;    // End ISA_Cycle=IOR
// *** Place no more than 4 Case  ***
       default: break;
     } // End switch(ISA_Cycle) ** In the IO Block

  //***    End of I/O Cycle     ***

// ** Now Need to send back the value Read, or stop the IOCHRDY **
if (ISA_WasRead)
   {
    pio_sm_put(isa_pio, isa_bus_sm, 0x00ffff00u | ISA_Data);  // 3nd Write : Send the Data to the CPU (Read Cycle)
    ISA_Data = pio_sm_get_blocking(isa_pio, isa_bus_sm);      // Wait that the data is sent to wait for the next ALE  
   }
   else
   {
    pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
   }

} // End For()
// Never go there, soooooo sad :(

}    // main_core1 End

int main(void) 
{

// TinyUSB Test
// https://forums.raspberrypi.com/viewtopic.php?t=346802
// https://github.com/raspberrypi/tinyusb/blob/pico/docs/boards.md

#ifdef USB_TEST
  stdio_async_uart_init_full(UART_ID, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);

  board_init();
  printf("TinyUSB Host CDC MSC HID Example\r\n");
  printf("CFG USE_USBHOST: %d\n",USE_USBHOST);  
  
  // init host stack on configured roothub port
  tuh_init(BOARD_TUH_RHPORT);

  while (1)
  {
    tuh_task();

    led_blinking_task();
  //  cdc_app_task();
  //  hid_app_task();
  }
#endif

// 130000 133000 160000 170000 180000 190000 200000 210000 220000 232000 240000  250000 260000 270000 280000
// Use hacked set_sys_clock_khz to keep SPI clock high - see clock_pll.h for details
// Voltage : 280 VREG_VOLTAGE_1_20
//           300 VREG_VOLTAGE_1_25

// Overclock!
vreg_set_voltage(VREG_VOLTAGE_1_20);
set_sys_clock_khz(PM_SYS_CLK*1000, true); 

// ************ Pico I/O Pin Initialisation ****************
// * Put the I/O in a correct state to avoid PC Crash      *
gpio_init(PIN_IORDY);
gpio_set_dir(PIN_IORDY, GPIO_OUT);
gpio_put(PIN_IORDY, 0);          // 0 > Signal at One as there is an inverter.

// * Init Multiplexers control Pin (There is a Pull up on this Pin)
gpio_init(PIN_AD);
gpio_set_dir(PIN_AD, GPIO_OUT);
gpio_put(PIN_AD, SEL_ADR);       // Address Multiplexer active, Data buffer 3 States

gpio_init(PIN_AS);
gpio_set_dir(PIN_AS, GPIO_OUT);
gpio_put(PIN_AS, SEL_ADL);        // Low part of the @ and I/O Mem Signals

// * Init the SDCard CS to 1 to not disturb the PSRAM init

#define PIN_SD_CS   3

gpio_init(PIN_SD_CS);
gpio_set_dir(PIN_SD_CS, GPIO_OUT);
gpio_put(PIN_SD_CS, 1);

// Same with PSRAM CS
#if USE_PSRAM
#else
#define PIN_CS   5 // Chip Select (SPI0 CS )
#endif
gpio_init(PIN_CS);
gpio_set_dir(PIN_CS, GPIO_OUT);
gpio_put(PIN_CS, 1);

// * Set input pins direction (Not needed normally, GPIO In are always available to the CPU)
for (int i=PIN_AEN; i<(PIN_AEN + 13); ++i) {
        gpio_disable_pulls(i);
        gpio_set_dir(i, GPIO_IN);
    }

// * Init other Output pin 
#if UART_PIN0
#else
gpio_init(PIN_IRQ);
gpio_set_dir(PIN_IRQ, GPIO_OUT);
gpio_put(PIN_IRQ, 1);  // Set IRQ Up (There is an inverter) for no IRQ
#endif

// GPIO 26, 27, 28 to Output (TEST)
gpio_init(PIN_GP26);
gpio_init(PIN_GP27);
gpio_init(PIN_GP28);  
gpio_set_dir(PIN_GP26, GPIO_OUT);
gpio_set_dir(PIN_GP27, GPIO_OUT);
gpio_set_dir(PIN_GP28, GPIO_OUT);        
gpio_put(PIN_GP26, 1);
gpio_put(PIN_GP27, 1);
gpio_put(PIN_GP28, 1);

// * Init and Light the LED
#if PM_PICO_W
#else
gpio_init(LED_PIN);
gpio_set_dir(LED_PIN, GPIO_OUT);
gpio_put(LED_PIN, 1);
#endif

// Define the First Status value
#if PM_ENABLE_STDIO
PM_Status=STAT_WAITCOM;  // Status Register set to Wait for COM to be connected
#else
PM_Status=STAT_INIT;     // Status Register set to Initialisation in progress
#endif
// !!  Must be done Before core0 and 1 Start !!
#if USE_PSRAM
BV_PSRAMInit=INIT_INPROGRESS;   // Init in progress
#else
BV_PSRAMInit=0xFF;   // Disabled
#endif

#if USE_SDCARD
BV_SDInit=INIT_INPROGRESS;     // Init in progress
BV_CFGInit=INIT_INPROGRESS;    // Init in progress
#else
BV_SDInit =0xFF;    // Disabled
BV_CFGInit=0xFF;    // Disabled
#endif //USE_SDCARD

#if USE_USBHOST
BV_USBInit=INIT_INPROGRESS;    // Init in progress
#else
BV_USBInit=0xFF;    // Disabled
#endif

#if USE_NE2000
BV_WifiInit=INIT_INPROGRESS;    // Init in progress
#else
BV_WifiInit=0xFF;    // Disabled
#endif

Init_ISAIOVars();

// Set the priority of PROC0 to high
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC1_BITS; // 

//PM_INFO("ISA Start");

// ** Start the ISA Cycles management code
multicore_launch_core1(main_core1);

// ********************************************** INIT STDIO *********************************************

#if PM_ENABLE_STDIO
#ifdef ASYNC_UART
    stdio_async_uart_init_full(UART_ID, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);
    PM_Status=STAT_INIT;  // Status Register set to Initialisation in progress  
    SERIAL_Enabled=true;
#else
// Serial using USB
    stdio_init_all();
    PM_Status=STAT_INIT;  // Status Register set to Initialisation in progress    
    SERIAL_Enabled=true;
    SERIAL_USB=true;
#endif

printf("PicoMEM is alive\n");

pm_timefrominit();

printf("CFG BIOS Addr: %x\n",ROM_BIOS);
printf("CFG SYS Clk: %d\n",PM_SYS_CLK);
//printf("CFG USE_USBHOST: %d\n",USE_USBHOST);

printf("PM BIOS Start  : %x \n",(uint32_t) _binary_pmbios_bin_start);
printf("BIOS Signature : %x %x \n",PMBIOS[0],PMBIOS[1]);
printf("PM_Memory Start  : %x \n",&PM_Memory);
printf("PM_DP_RAM Start  : %x \n",&PM_DP_RAM);

#endif

/*
printf("Port List:");
for (int i=0;i<256;i++) 
  { 
   printf ("%d,",PORT_Table[i]); 
  } 
*/

// Various TEST Code not present in standard build

/*
ifdef //PM_PICO_INFOS
// Debug / to learn
    measure_freqs();
    pico_infos();
#endif ////PM_PICO_INFOS
*/

PM_Config=(PMCFG_t *) &PM_DP_RAM[PMRAM_CFG_Offset]; // Define the Config register
PM_PTR_DISKBUFFER=(uint8_t *) &PM_DP_RAM[PM_DB_Offset];

// Start the Init and commands functions code
main_core0();

}  // main