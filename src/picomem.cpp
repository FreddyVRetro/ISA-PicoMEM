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

//extern void sbdsp_test();

#define ISA_DBG_IO 0

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/vreg.h"

#if !(defined(RASPBERRYPI_PICO2) || defined(PIMORONI_PICO_PLUS2_RP2350))
#include "hardware/regs/vreg_and_chip_reset.h"
#endif

#include "hardware/structs/bus_ctrl.h"
#include "hardware/regs/busctrl.h"              /* Bus Priority defines */

#include "pm_debug.h"

//#include "ff.h"    // Needed by pm_disk.h

//Other PicoMEM Code
#include "pm_gpiodef.h"     // Various PicoMEM GPIO Definitions
#include "pm_defines.h"     // Shared MEMORY variables defines (Registers and PC Commands)
#include "pm_cmd.h"
#include "pm_libs/pm_libs.h"
#include "ff.h"
#include "pm_libs/pm_disk.h"
#include "pm_pccmd.h"
#include "qwiic.h"

// PicoMEM emulated devices include
//#include "isa_devices.h"
#include "dev_post.h"
#include "dev_irq.h"
#include "dev_joystick.h"
#if USE_RTC
#include "dev_rtc.h"
#endif
#if USE_AUDIO
#include "dev_adlib.h"
#include "dev_cms.h"
#include "dev_tandy.h"
#include "dev_mindscape.h"
#include "dev_sbdsp.h"
#include "dev_dma.h"
#endif

extern "C" const uint8_t _binary_pmbios_bin_start[];
extern "C" const uint8_t _binary_pmbios_bin_size[];
uint8_t  __in_flash() *PMBIOS=(uint8_t *)_binary_pmbios_bin_start;
// PicoMEM BIOS is stored in the RAM (It is what I need) but I don't understand why :(


// Memory and IO Tables in the core stack RAM (Must be before PM_defines)

volatile uint8_t MEM_Type_T[MEM_T_Size];     // Memory type list

// !! On the Pico2, when the BIOS @ is modified and the code is short in scratch X, and variable in scratch : Crash

// Return a memory Index based on the PC Address >>13 (8Kb)
__scratch_x("MemDevTable") volatile uint8_t MEM_Index_T[MEM_T_Size]; // Memory @ Table for fast decoding

// Pointer to the "Memory index" table in the RP2040 Address Space
// = RP2040 Memory Address - PC Memory Base Address
__scratch_x("MemBaseTable") volatile uint8_t *RAM_Offset_Table[21];  // 16 + 4 for EMS (RP2350)

//Devices emulation tables
// Port table for fast decoding (IO Port >>3 then 8 Bytes precision)
__scratch_x("IODevTable") volatile uint8_t PORT_Table[256];  // Now size=256, set the upper 128 to 0 (DMA Detection)

// MEM / IO fonctions defines
#include "dev_memory.h"
#include "dev_picomem_io.h"

#ifdef RASPBERRYPI_PICO_W
#if USE_NE2000
extern "C" {
#include "ne2000/dev_ne2000.h"
extern wifi_infos_t PM_Wifi;
}
#endif
#endif

//PSRAM Definitions
#if USE_PSRAM
#if USE_PSRAM_DMA
#include "psram_spi.h"
psram_spi_inst_t psram_spi;
psram_spi_inst_t* async_spi_inst;
#else
#include "psram_spi2.h"
pio_spi_inst_t psram_spi;
#endif
#endif

#if BOARD_PM15
#include "isa_iomem_m2_v15.pio.h"
#else
#if ISA_NOAEN
#include "isa_iomem_noaen.pio.h"
#else
#if MUX_V2
//#include "isa_iomem_test_addr_v2.pio.h"
#include "isa_iomem_m2.pio.h"
#else
#include "isa_iomem_m1.pio.h"
#endif
#endif
#endif

// Define the UART values for debug (On the Free GPIO 28)
#if ASYNC_UART
#include "stdio_async_uart.h"
#endif 

#if BOARD_PM15

#define UART_TX_PIN 36
#define UART_RX_PIN 37
#define UART_ID     uart1

#else
#if UART_PIN0
#define UART_TX_PIN 0
#else
#define UART_TX_PIN 28
#endif
#define UART_RX_PIN (-1)
#define UART_ID     uart0
#endif

#define BAUD_RATE   115200

#if BOARD_PM15
#define LED_PIN 11
#else
#define LED_PIN 25
#endif

// ** State Machines ** Order should not be changed, "Hardcoded" (For ISA)
//
// ISA    : 27  (5)
// PSRAM  : 11  
// I2S    : 8   (32-8-11:13)
// Cyw43  :

// SD : No PIO
// pio0 / sm0 : isa_bus_sm : ISA bus communication
//
// pio1 / sm0              : PSRAM, Wifi or Audio
// pio1 / sm1              : PSRAM, Wifi or Audio
// pio1 / sm2              : PSRAM, Wifi or Audio
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

#define CTRL_MR    0x01  // 0001b
#define CTRL_MW    0x02  // 0010b
#define CTRL_IOR   0x04  // 0100b
#define CTRL_IOW   0x08  // 1000b

#define CTRL_MR_   0x0E  // 0001b
#define CTRL_MW_   0x0D  // 0010b
#define CTRL_IOR_  0x0B  // 0100b
#define CTRL_IOW_  0x07  // 1000b

#define PM_IOBASEPORT 0x2A0

#define DEV_POST_Enable 1

uint32_t PM_Version_Detect;

uint8_t PM_BasePort;  // Real port >>3

bool     ISA_WasRead;

// Lotech like EMS Regs
uint8_t  EMS_Bank[4]={0,1,2,3};
uint32_t EMS_Base[4]={0,1*16*1024,2*16*1024,3*16*1024};

// RAM/ROM Emulation tables

//uint8_t PM_Memory[128*1024];           // Table used to emulate RAM from the Pico SRAM
uint8_t PM_Memory[16*1024*MAX_PMRAM];    // Table used to emulate RAM from the Pico SRAM
volatile uint8_t PM_DP_RAM[8*1024];    // Disk I/O Buffer, to map in the ROM RAM.

//uint8_t PM_BIOS12k[12*1024];

volatile uint32_t RAM_InitialAddress;  // This is the Base Address of the RAM Emulated by the Pi Pico Memory  

// ** Virtual Disks Variables **

volatile uint8_t *PM_PTR_DISKBUFFER;


// ** ISA Debug Infos
#if DISP32
volatile uint32_t To_Display32;
volatile uint32_t To_Display32_2;

volatile uint32_t To_Display32_mask;
volatile uint32_t To_Display32_2_mask;

#endif
volatile uint32_t PSRC_Hit;   // PSRAM Cache Hit nb
volatile uint32_t PSRC_Miss;  // PSRAM Cache Miss nb



void Init_ISAIOVars()
{

//  Initialize the ISA PIO Code
gpio_set_slew_rate(PIN_AD, GPIO_SLEW_RATE_FAST);
gpio_set_slew_rate(PIN_AS, GPIO_SLEW_RATE_FAST);
gpio_set_slew_rate(PIN_IORDY, GPIO_SLEW_RATE_FAST);

//PM_INFO("init gpio %d to %d\n",PIN_AD0,PIN_AD0+11);
for (int i=PIN_AD0; i<(PIN_AD0 + 12); ++i) {
  gpio_init(i);
  gpio_disable_pulls(i);
  gpio_set_dir(i, GPIO_IN);
}

// Start the ISA PIO State Machine
#if BOARD_PM15
pio_set_gpio_base(isa_pio,16);          // This pio will start from the gpio 16
#endif
uint isa_bus_offset = pio_add_program(isa_pio, &isa_bus_program);

PM_INFO("ISA SM Offset: %d SM: %d\n",isa_bus_offset,isa_bus_sm); 
if (pio_claim_unused_sm(isa_pio, true)!=isa_bus_sm) PM_ERROR("PIO SM For ISA Error");
isa_bus_program_init(isa_pio, isa_bus_sm, isa_bus_offset);

// Invert the A16_MR to A11_IW pin, for signal detect speed up !! If placed before the SM init, does not work
#if MUX_V2
gpio_set_inover (PIN_A8_MR,GPIO_OVERRIDE_INVERT);
gpio_set_inover (PIN_A9_MW,GPIO_OVERRIDE_INVERT);
gpio_set_inover (PIN_A10_IR,GPIO_OVERRIDE_INVERT);
gpio_set_inover (PIN_A11_IW,GPIO_OVERRIDE_INVERT);
#else
gpio_set_inover (PIN_A16_MR,GPIO_OVERRIDE_INVERT);
gpio_set_inover (PIN_A17_MW,GPIO_OVERRIDE_INVERT);
gpio_set_inover (PIN_A18_IR,GPIO_OVERRIDE_INVERT);
gpio_set_inover (PIN_A19_IW,GPIO_OVERRIDE_INVERT);
#endif

PM_BasePort=PM_IOBASEPORT>>3;
SetPortType(PM_IOBASEPORT,DEV_PM,1);

// Default MEMORY Blocks configuration

dev_memory_init(0);

#ifdef ROM_BIOS
PM_BIOSADDRESS=(ROM_BIOS>>4);
//PM_INFO("ROM_BIOS %x PM_BIOSADDRESS %x\n",ROM_BIOS,PM_BIOSADDRESS);
PC_DB_Start=ROM_BIOS+0x4000+PM_DB_Offset;
//dev_memory_init(ROM_BIOS);

SetMEMType(ROM_BIOS,MEM_BIOS,MEM_S_16k);
SetMEMIndex(ROM_BIOS,MEM_BIOS,MEM_S_16k);
RAM_Offset_Table[MEM_BIOS]=&PMBIOS[-ROM_BIOS];

SetMEMType(ROM_BIOS+0x4000,MEM_DISK,1);  // 8kb only
SetMEMIndex(ROM_BIOS+0x4000,MEM_DISK,1);
RAM_Offset_Table[MEM_DISK]=&PM_DP_RAM[-(ROM_BIOS+0x4000)];
#endif

PM_DP_RAM[0] = 0x12; //ID of The PicoMEM BIOS RAM  (Magic ID)

} //Init_ISAIOVars

//#define DO_MEMR 0x3010u  // PIO instruction for (wait 0 gpio PIN_A16_MR) 
//#define DO_IOR  0x3012u  // PIO instruction for (wait 0 gpio PIN_A18_IR) 

#define DO_MEMR 2 
#define DO_IOR  1 


__force_inline void pm_do_ior(void)
{
 ISA_WasRead=true;
 #if BOARD_PM15
// 0x380e, // 16: wait   0 gpio, 14      side 2
 pio_sm_put(isa_pio, isa_bus_sm, 0x380e); // Do IOR (wait 0 gpio PIN_A18_IR)
 #else
 pio_sm_put(isa_pio, isa_bus_sm, DO_IOR); // Do IOR (wait 0 gpio PIN_A18_IR)
 #endif
} // Data is sent after in the core1

// Start of a Memory Read with Wait States added
__force_inline void pm_do_memr(uint32_t ISA_Data)
{

#if BOARD_PM15
 //0x380c, // 18: wait   0 gpio, 12      side 2
pio_sm_put(isa_pio, isa_bus_sm, 0x380c); // 2nd Write : Do MEMR (wait 0 gpio PIN_A16_MR)
#else
 pio_sm_put(isa_pio, isa_bus_sm, DO_MEMR);                  // 2nd Write : Do MEMR (wait 0 gpio PIN_A16_MR)
#endif
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

// Various version of the ISA I/O and Memory code

#if ISA_DBG_IO
#include "pm_isa_test.h"
#else
#if defined(RASPBERRYPI_PICO2) || defined(PIMORONI_PICO_PLUS2_RP2350)

#if BOARD_PM15
//#include "pm_isa_debug.h"
#include "pm_isa_rp2350_m2_v15.h"
#else
#if MUX_V2
#include "pm_isa_rp2350_v2_dev.h"
#else
#include "pm_isa_rp2350.h"
#endif
#endif

#else
#if MUX_V2
//#include "pm_isa_rp2040_m2.h"
#include "pm_isa_rp2040_m2_dev.h"
#else
#include "pm_isa_rp2040.h"
#endif
#endif
#endif

void Do_PIOTest()
{
    uint32_t sm_data;
     
    gpio_init_put(LED_PIN,1);

    gpio_init_put(PIN_IORDY,0);
    gpio_init_put(PIN_AD,1);
    gpio_init_put(PIN_AS,1);
    
    // Start the ISA PIO State Machine
    pio_set_gpio_base(isa_pio,16);          // This pio will start from the gpio 16
    uint isa_bus_offset = pio_add_program(isa_pio, &isa_bus_program);
    PM_INFO("ISA SM Offset: %d SM: %d\n",isa_bus_offset,isa_bus_sm); 
    if (isa_bus_offset<0) PM_ERROR("pio_add_program Error");    

    if (pio_claim_unused_sm(isa_pio, true)!=isa_bus_sm) PM_ERROR("PIO SM For ISA Error");
    isa_bus_program_init(isa_pio, isa_bus_sm, isa_bus_offset);

    PM_INFO("Write/Read in loop to/from the ISA SM\n",isa_bus_offset,isa_bus_sm);     
for (;;) {
        PM_INFO("1");
        pio_sm_put(isa_pio, isa_bus_sm, 0);  // Start the cycle
//        printf("");
        sm_data = pio_sm_get_blocking(isa_pio, isa_bus_sm);
//        printf("3 Addr:%x",sm_data);
        pio_sm_put(isa_pio, isa_bus_sm, 1);  // 1st Write : No Wait State
//        printf("4");
        pio_sm_put(isa_pio, isa_bus_sm, 0);  // 2nd Write : Directly Loop (Write Cycle or nothing)
}
/*
    for (;;) 
    {
        uint32_t readsm;
        readsm=pio_sm_get_blocking(isa_pio, isa_bus_sm);
        printf("Read SM: %x\n",readsm);
        busy_wait_ms(500);
    }
*/

}

//***********************************************************************************************//
//*         CORE 0 Main : PicoMEM Code Start                                                    *//
//***********************************************************************************************//

int main(void) 
{

 #if PM_ENABLE_STDIO
 stdio_init_all();
 #endif

// 130000 133000 160000 170000 180000 190000 200000 210000 220000 232000 240000  250000 260000 270000 280000
// Use hacked set_sys_clock_khz to keep SPI clock high - see clock_pll.h for details
// Voltage : 280 VREG_VOLTAGE_1_20
//           300 VREG_VOLTAGE_1_25

// Pico2 Overclocking : https://forums.raspberrypi.com/viewtopic.php?t=375975


// Overclock!
#ifdef PIMORONI_PICO_PLUS2_RP2350
vreg_set_voltage(VREG_VOLTAGE_1_20);
set_sys_clock_khz(280*1000, true); 
//set_sys_clock_khz(PM_SYS_CLK*1000, true); 
#else
vreg_set_voltage(VREG_VOLTAGE_1_20);
set_sys_clock_khz(PM_SYS_CLK*1000, true);
#endif


 //Do_PIOTest();

// TinyUSB Test
// https://forums.raspberrypi.com/viewtopic.php?t=346802
// https://github.com/raspberrypi/tinyusb/blob/pico/docs/boards.md

#ifdef USB_TEST
  stdio_async_uart_init_full(UART_ID, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);

  board_init();
  PM_INFO("TinyUSB Host CDC MSC HID Example\r\n");
  PM_INFO("CFG USE_USBHOST: %d\n",USE_USBHOST);  
  
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

gpio_init(1);
gpio_init(PIN_IRQ);
PM_Version_Detect=gpio_get_all();

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
#if BOARD_PM13
gpio_put(PIN_AS, SEL_CTRL);        // Low part of the @ and I/O Mem Signals
#else
gpio_put(PIN_AS, SEL_ADL);        // Low part of the @ and I/O Mem Signals
#endif

// * Init the SDCard CS to 1 to not disturb the PSRAM init

//#define PIN_SD_CS   3

gpio_init(SD_SPI_CS);
gpio_set_dir(SD_SPI_CS, GPIO_OUT);
gpio_put(SD_SPI_CS, 1);

// Same with PSRAM CS
#if USE_PSRAM
#define PSRAM_PIN_CS   5 // Chip Select (SPI0 CS )
gpio_init(PSRAM_PIN_CS);
gpio_set_dir(PSRAM_PIN_CS, GPIO_OUT);
gpio_put(PSRAM_PIN_CS, 1);
#else
#define PSRAM_PIN_CS   5 // Chip Select (SPI0 CS )
#endif

// * Init other Output pin 
#if UART_PIN0
#else
gpio_init(PIN_IRQ);
gpio_set_dir(PIN_IRQ, GPIO_OUT);
gpio_put(PIN_IRQ, 1);  // Set IRQ Up (There is an inverter) for no IRQ
#endif

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

// ************************************* INIT ISA SM and Loop code *************************************

Init_ISAIOVars(); // Setup the ISA bus code variables and PIO SM

// Set the priority of PROC0 to high
bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC1_BITS; // 

#if ISA_DBG_IO
multicore_launch_core1(test_IOW_core1);
#else
// ** Start the ISA Cycles management code
multicore_launch_core1(main_core1);
#endif

// ********************************************** INIT STDIO *********************************************

#if PM_ENABLE_STDIO

#if ASYNC_UART
    stdio_async_uart_init_full(UART_ID, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);
    PM_Status=STAT_INIT;  // Status Register set to Initialisation in progress  
    SERIAL_Enabled=true;
    PM_INFO("PicoMEM is alive (async uart)\n");
#else
// Serial using USB or default serial
    stdio_init_all();   
    SERIAL_Enabled=true;
    PM_INFO("PicoMEM is alive (stdio)\n");
#endif

#if USB_UART
    SERIAL_USB=true;
#else
    SERIAL_USB=false;
#endif   

pm_timefrominit();

/*
PM_INFO("CFG BIOS Addr: %x\n",ROM_BIOS);
PM_INFO("CFG SYS Clk: %d\n",PM_SYS_CLK);
PM_INFO("CFG USE_USBHOST: %d\n",USE_USBHOST);
*/

// 1.3a   : Pull Up on GPIO 0,1,2        PM_Version_Detect 100007
// 1.14   : Pull Up on GPIO 0, 1,2 Input PM_Version_Detect 100003
// LP 1.0 : Pull Up and Down on GPIO0 ! Error
PM_INFO("PM_Version_Detect %x\n",PM_Version_Detect);

/*
PM_INFO("Mem Index: ");
for (int i=0;i<MEM_T_Size;i++)
    { if (i==10*8) PM_INFO("- "); 
          PM_INFO("%d ",GetMEMIndex(i*8*1024));
    }
PM_INFO("\n");
*/

/*
PM_INFO("PM BIOS Start  : %x %x\n",(uint32_t) _binary_pmbios_bin_start,&PMBIOS[0]);
PM_INFO("BIOS Signature : %x %x\n",PMBIOS[0],PMBIOS[1]);
PM_INFO("PM_Memory Start : %x\n",&PM_Memory);
PM_INFO("PM_DP_RAM Start : %x\n",&PM_DP_RAM);
*/

//sbdsp_test();

#endif

// Various TEST Code not present in standard build

/*
ifdef //PM_PICO_INFOS
// Debug / to learn
    measure_freqs();
    pico_infos();
#endif ////PM_PICO_INFOS
*/

PM_INFO("PIN_AD0: %d \n",PIN_AD0);

#if BOARD_PM15
#if !PICO_PIO_USE_GPIO_BASE
#error PICO_PIO_USE_GPIO_BASE must be 1 to use >32 pins
#endif
#endif

PM_Status=STAT_INIT;  // Status Register set to Initialisation in progress

PM_Config=(PMCFG_t *) &PM_DP_RAM[PMRAM_CFG_Offset]; // Define the Config register
PM_PTR_DISKBUFFER=(uint8_t *) &PM_DP_RAM[PM_DB_Offset];

// Start the Init and commands functions code
main_core0();

}  // main
