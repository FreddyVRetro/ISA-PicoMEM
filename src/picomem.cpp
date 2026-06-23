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
#if BOARD_PM15 || BOARD_PM20   // For Pico2 Overclocking
#include <hardware/structs/qmi.h>
#endif
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/vreg.h"

#if !(defined(RASPBERRYPI_PICO2) || defined(PIMORONI_PICO_PLUS2_RP2350))
#include "hardware/regs/vreg_and_chip_reset.h"
#endif

#include "hardware/structs/bus_ctrl.h"
#include "hardware/regs/busctrl.h"              /* Bus Priority defines */

#include "pm_debug.h"
#include "pm_board.h"     // PicoMEM Board Definitions (GPIO)

#include "pm_defines.h"      // Shared MEMORY variables defines (Registers and PC Commands)
#include "pm_cmd.h"
#include "pm_libs/pm_libs.h"
#include "ff.h"
#include "pm_libs/pm_disk.h"
#include "pm_pccmd.h"

#include "pm_i2c.h"

// Memory and IO Tables in the core stack RAM (Must be before PM_defines)

volatile uint8_t MEM_Type_T[MEM_T_Size];     // Memory type list

// RAM/ROM Emulation tables
uint8_t PM_Memory[16*1024*MAX_PMRAM];  // Table used to emulate RAM from the Pico SRAM
//uint8_t PM_Memory[1024];  // Table used to emulate RAM from the Pico SRAM
volatile uint8_t PM_DP_RAM[8*1024];    // Disk I/O Buffer, to map in the ROM RAM.

// PicoMEM emulated devices include
//#include "isa_devices.h"
#include "dev_ltemm.h"
#include "dev_post.h"
#include "dev_irq.h"
#include "dev_joystick.h"
#if USE_RTC
#include "dev_rtc.h"
#endif // USE_RTC
#if USE_AUDIO
#include "dev_adlib.h"
#include "dev_cms.h"
#include "dev_tandy.h"
#include "dev_mindscape.h"
#include "dev_covox.h"
#include "dev_sbdsp.h"
#include "dev_dma.h"
#if USE_MPU
#include "dev_mpu.h"
#endif
#if USE_GUS
#include "dev_gus.h"
#endif
#endif //USE_AUDIO

#if USE_CDROM
#include "dev_cdrom_mke.h"
#endif // USE_CDROM

#if USE_OLED
#include "dev_oled.h"
#endif

#if BOARD_PM15 || BOARD_PM20
extern "C" const uint8_t _binary_pm2bios_bin_start[];
extern "C" const uint8_t _binary_pm2bios_bin_size[];
uint8_t  __in_flash() *PMBIOS=(uint8_t *)_binary_pm2bios_bin_start;
#else
extern "C" const uint8_t _binary_pmbios_bin_start[];
extern "C" const uint8_t _binary_pmbios_bin_size[];
uint8_t  __in_flash() *PMBIOS=(uint8_t *)_binary_pmbios_bin_start;
#endif
// PicoMEM BIOS is stored in the RAM (It is what I need) but I don't understand why :(

// Return a memory Index based on the PC Address >>13 (8Kb)
__scratch_x("MemDevTable") volatile uint8_t MEM_Index_T[MEM_T_Size]; // Memory @ Table for fast decoding

// Pointer to the "Memory index" table in the RP2040 Address Space
// = RP2040 Memory Address - PC Memory Base Address
__scratch_x("MemBaseTable") volatile uint8_t *RAM_Offset_Table[21];  // 16 + 4 for EMS (RP2350)

//Devices emulation tables
// Port table for fast decoding (IO Port >>3 then 8 Bytes precision)
#if BOARD_WM10
volatile uint8_t IO_Index_T[256];  // WM10: NOT in scratch_x (Core 1 uses scratch_x for stack)
#else
__scratch_x("IODevTable") volatile uint8_t IO_Index_T[256];  // ISA: in scratch_x for fast access
#endif

// MEM / IO fonctions defines
#include "dev_memory.h"
#include "dev_picomem_io.h"

#include "isa_dma.h"

#if PM_WIFI
#if USE_NE2000
extern "C" {
#include "ne2000/dev_ne2000.h"
extern wifi_infos_t PM_Wifi;
}
#endif
#endif

//PSRAM Definitions
#if USE_SPI_PSRAM
#if USE_SPI_PSRAM_DMA
#include "psram_spi.h"
psram_spi_inst_t psram_spi;
psram_spi_inst_t* async_spi_inst;
#else
#include "psram_spi2.h"
pio_spi_inst_t psram_spi;
#endif
#endif

#if BOARD_WM10
//#include "mca_iomem_v10.pio.h"                  // PIO is not used, later use
#else

#if BOARD_PM15 || BOARD_PM20
#include "isa_iomem_m2_v15.pio.h"
#else
#if MUX_V2
#include "isa_iomem_m2.pio.h"
#else
#include "isa_iomem_m1.pio.h"
#endif //MUX_V2
#endif //BOARD_PM15 || BOARD_PM20
#endif //BOARD_WM10


// Define the UART values for stdio async debug (On the Free GPIO 28)
#if ASYNC_UART
#include "stdio_async_uart.h"

#if BOARD_PM15 || BOARD_PM20

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

bool ISA_WasRead;

#if CORE1_ALIVETEST 
uint32_t Core1_AliveCnt=0;
uint32_t Core1_AliveCnt_Last=0;
#endif

volatile uint32_t RAM_InitialAddress;  // This is the Base Address of the RAM Emulated by the Pi Pico Memory  

// ** Virtual Disks Variables **
volatile uint8_t *PM_PTR_DISKBUFFER = (uint8_t *) &PM_DP_RAM[PM_DB_Offset];


// ** ISA Debug Infos
#if DISP32
volatile uint32_t To_Display32;
volatile uint32_t To_Display32_2;

volatile uint32_t To_Display32_mask;
volatile uint32_t To_Display32_2_mask;

#endif

#if !BOARD_WM10
__force_inline void PM_ISA_IO_Init()
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
#if BOARD_PM15 || BOARD_PM20
pio_set_gpio_base(isa_pio,16);  // This pio will start from the gpio 16
#endif

PM_INFO(" > Add ISA R/W SM\n");
if (pio_sm_is_claimed(isa_pio,0)) PM_ERROR("pio/sm pio0/0 already claimed\n");
pio_sm_claim(isa_pio,0);        // Claim the PIO State machine (If another more clean code check)
uint isa_bus_offset = pio_add_program(isa_pio, &isa_bus_program);
isa_bus_program_init(isa_pio, 0, isa_bus_offset);

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

} //PM_ISA_IO_Init
#endif // !BOARD_WM10

#if !BOARD_WM10
__force_inline void PM_BASE_DEV_Init()
{
 PM_BasePort=PM_IOBASEPORT>>3;
 SetPortType(PM_IOBASEPORT,DEV_PM,1);

// Default MEMORY Blocks configuration

 dev_memory_init(0);

 #ifdef ROM_BIOS
 PM_BIOSADDRESS=(ROM_BIOS>>4);
 PC_DB_Start=ROM_BIOS+0x4000+PM_DB_Offset;
 //PM_INFO("ROM_BIOS %x PM_BIOSADDRESS %x\n",ROM_BIOS,PM_BIOSADDRESS);
 //dev_memory_init(ROM_BIOS);

 SetMEMType(ROM_BIOS,MEM_BIOS,MEM_S_16k); // BIOS ROM, 16Kb
 SetMEMIndex(ROM_BIOS,MEM_BIOS,MEM_S_16k);
 RAM_Offset_Table[MEM_BIOS]=&PMBIOS[-ROM_BIOS];

 SetMEMType(ROM_BIOS+0x4000,MEM_DISK,1);  // BIOS RAM and Disk Buffer, 8Kb
 SetMEMIndex(ROM_BIOS+0x4000,MEM_DISK,1);
 RAM_Offset_Table[MEM_DISK]=&PM_DP_RAM[-(ROM_BIOS+0x4000)];
 #endif

PM_DP_RAM[0] = 0x12; //ID of The PicoMEM BIOS RAM  (Magic ID)

} //PM_BASE_DEV_Init
#endif // !BOARD_WM10

//***********************************************************************************************//
//*         CORE 1 Main : Wait for the ISA Bus events, emulate RAM and ROM                      *//
//***********************************************************************************************//

// Various version of the ISA I/O and Memory code

#if ISA_DBG_IO
#include "pm_isa_test.h"
#else
#if defined(RASPBERRYPI_PICO2) || defined(PIMORONI_PICO_PLUS2_RP2350)

#if BOARD_PM15 || BOARD_PM20
//#include "pm_isa_debug.h"
#include "pm_isa_rp2350_m2_v15.h"
#elif BOARD_WM10
#include "wm_mca_rp2350_v10.h"
#else
#if MUX_V2
#include "pm_isa_rp2350_v2_dev.h"
#else
#include "pm_isa_rp2350.h"
#endif
#endif

#else
#if MUX_V2
#include "pm_isa_rp2040_m2_dev.h"
#else
#include "pm_isa_rp2040.h"
#endif
#endif
#endif


__force_inline void BOARD_Init()
{

/*
 #if PM_ENABLE_STDIO
 stdio_init_all();
 PM_INFO("PicoMEM is alive\n");
 #endif
*/

// *** Overclock! ***
//   Freq CLKDIV Voltage
//   280     2   VREG_VOLTAGE_1_10
//   300     3   VREG_VOLTAGE_1_15
//   360     3   VREG_VOLTAGE_1_25
//   380     3   VREG_VOLTAGE_1_30
//   400     4   VREG_VOLTAGE_1_30
// Pico2 Overclocking : https://forums.raspberrypi.com/viewtopic.php?t=375975
#if PM_SYS_CLK>280
// PM_SYS_CLK> 280 MHz : Need to update Flash timing
#if PM_SYS_CLK>380
#define CLK_DIV 4
#define RX_DELAY 4
#define CPU_VOLTAGE VREG_VOLTAGE_1_30
#else
#define CLK_DIV 3
#define RX_DELAY 3
#define CPU_VOLTAGE VREG_VOLTAGE_1_30
#endif
hw_write_masked( &qmi_hw->m[0].timing,
                ((CLK_DIV << QMI_M0_TIMING_CLKDIV_LSB) & QMI_M0_TIMING_CLKDIV_BITS) |
                ((RX_DELAY << QMI_M0_TIMING_RXDELAY_LSB) & QMI_M0_TIMING_RXDELAY_BITS),
                QMI_M0_TIMING_CLKDIV_BITS | QMI_M0_TIMING_RXDELAY_BITS );  // FLASH Timing update for overclocking
vreg_disable_voltage_limit();
vreg_set_voltage(CPU_VOLTAGE);
busy_wait_ms(10);
set_sys_clock_khz(PM_SYS_CLK*1000, true);
#else
vreg_set_voltage(VREG_VOLTAGE_1_15);
set_sys_clock_khz(PM_SYS_CLK*1000, true);
#endif

}

#if !BOARD_WM10
__force_inline void BOARD_Init_GPIO()
{

gpio_init(1);
gpio_init(PIN_PM_IRQ);
PM_Version_Detect=gpio_get_all();

// ************ Pico I/O Pin Initialisation ****************
// * Put the I/O in a correct state to avoid PC Crash      *
gpio_init(PIN_IORDY);
gpio_put(PIN_IORDY, 0);          // 0 > Signal at One as there is an inverter.
gpio_set_dir(PIN_IORDY, GPIO_OUT);

// * Init Multiplexers control Pin (There is a Pull up on this Pin)
gpio_init(PIN_AD);
gpio_put(PIN_AD, SEL_ADR);       // Address Multiplexer active, Data buffer 3 States
gpio_set_dir(PIN_AD, GPIO_OUT);

gpio_init(PIN_AS);
gpio_set_dir(PIN_AS, GPIO_OUT);
#if BOARD_PM13
gpio_put(PIN_AS, SEL_CTRL);        // Low part of the @ and I/O Mem Signals
#else
gpio_put(PIN_AS, SEL_ADL);        // Low part of the @ and I/O Mem Signals
#endif

#ifdef PIN_DRQ
#define PIN_DRQ  38         // Init the DMA Request and Acknowledge Pins
#define PIN_DMAW 39
gpio_init(PIN_DRQ);
gpio_put(PIN_DRQ, 1);       // 1 set signal to 0, no DMA Request
gpio_set_dir(PIN_DRQ, GPIO_OUT);
gpio_init(PIN_DMAW);
#endif
#ifdef PIN_TC
gpio_init(PIN_TC);
#endif

// * Init the SDCard CS to 1 to not disturb the PSRAM init

//#define PIN_SD_CS   3
// Disable SPI CS at startup
#ifndef BOARD_PM15
gpio_init(SD_SPI_CS);
gpio_set_dir(SD_SPI_CS, GPIO_OUT);
gpio_put(SD_SPI_CS, 1);
#endif

// Same with PSRAM CS
#if USE_SPI_PSRAM
gpio_init(PSRAM_PIN_CS);
gpio_set_dir(PSRAM_PIN_CS, GPIO_OUT);
gpio_put(PSRAM_PIN_CS, 1);
#endif

// * Init other Output pin 
#if UART_PIN0
#else
gpio_init(PIN_PM_IRQ);
gpio_set_dir(PIN_PM_IRQ, GPIO_OUT);
gpio_put(PIN_PM_IRQ, 1);  // Set IRQ Up (There is an inverter) for no IRQ
#endif
#if USE_HWIRQ       // For the boards with more than one IRQ PIN
gpio_init(PIN_IRQ3);
gpio_set_dir(PIN_IRQ3, GPIO_OUT);
gpio_put(PIN_IRQ3, 1);  // Set IRQ Up (There is an inverter) for no IRQ
gpio_init(PIN_IRQ5);
gpio_set_dir(PIN_IRQ5, GPIO_OUT);
gpio_put(PIN_IRQ5, 1);  // Set IRQ Up (There is an inverter) for no IRQ
#ifdef PIN_IRQ6
 gpio_init(PIN_IRQ6);
 gpio_set_dir(PIN_IRQ6, GPIO_OUT);
 gpio_put(PIN_IRQ6, 1);  // Set IRQ Up (There is an inverter) for no IRQ
#endif
// PIN_PM_IRQ is the IRQ7 pin
#endif

// * Init and Light the LED
#if PM_WIFI_LED
#else
gpio_init(LED_PIN);
gpio_set_dir(LED_PIN, GPIO_OUT);
gpio_put(LED_PIN, 1);
#endif
}
#endif // !BOARD_WM10

//***********************************************************************************************//
//*         CORE 0 Main : PicoMEM Code Start                                                    *//
//***********************************************************************************************//

int main(void) 
{

#ifdef BOARD_WM10
wm_board_init();
stdio_init_all();
printf("\n\n=== WonderMCA UART OK ===\n");
#else
BOARD_Init();      // Init the Board (Pico OverClock)
BOARD_Init_GPIO(); // Init the GPIOs for PicoMEM
#endif

// Initialise the BIOS Variables/Devices Init Status
#if BOARD_WM10
PM_Status=STAT_READY;    // WonderMCA: BIOS expects READY immediately
#elif PM_ENABLE_STDIO
PM_Status=STAT_WAITCOM;  // Status Register set to Wait for COM to be connected
#else
PM_Status=STAT_INIT;     // Status Register set to Initialisation in progress
#endif
// !!  Must be done Before core0 and 1 Start !!
#if USE_SPI_PSRAM
BV_PSRAMInit=INIT_INPROGRESS;   // Init in progress
#else
BV_PSRAMInit=INIT_DISABLED;   // Disabled
#endif

#if USE_SDCARD
BV_SDInit=INIT_INPROGRESS;     // Init in progress
BV_CFGInit=INIT_INPROGRESS;    // Init in progress
#else
BV_SDInit =INIT_DISABLED;    // Disabled
BV_CFGInit=INIT_DISABLED;    // Disabled
#endif //USE_SDCARD

#if USE_USBHOST
BV_USBInit=INIT_INPROGRESS;    // Init in progress
#else
BV_USBInit=INIT_DISABLED;    // Disabled
#endif

#if USE_NE2000
BV_WifiInit=INIT_INPROGRESS;    // Init in progress
#else
BV_WifiInit=INIT_DISABLED;    // Disabled
#endif

// ************************************* INIT ISA SM and Loop code *************************************
#if BOARD_WM10
// PicoMEM BIOS init — IO_Index_T is now in normal RAM, safe to write before core1 launch
PM_BIOSADDRESS = (ROM_BIOS >> 4);
PC_DB_Start = ROM_BIOS + 0x4000 + PM_DB_Offset;
PM_BasePort = PM_IOBASEPORT >> 3;
SetPortType(PM_IOBASEPORT, DEV_PM, 1);
PM_DP_RAM[0] = 0x12;  // Magic ID
#else
PM_ISA_IO_Init(); // Setup the ISA bus code variables and PIO SM
PM_BASE_DEV_Init();
#endif




// Set the priority of PROC0 to high
bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC1_BITS; // 

#if ISA_DBG_IO
multicore_launch_core1(test_IOW_core1);
#else
// ** Start the ISA Cycles management code
multicore_launch_core1(main_core1);
#endif

// WM10: IO_Index_T setup moved before core1 launch (no longer in SCRATCH_X)

// ********************************************** INIT STDIO *********************************************

#if PM_ENABLE_STDIO

#if ASYNC_UART
    stdio_async_uart_init_full(UART_ID, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);
    SERIAL_Enabled=true;
    PM_INFO("PicoMEM is alive (async uart)\n");
#else
// Serial using USB or default serial

    stdio_init_all();   
    SERIAL_Enabled=true;
    PM_INFO("PicoMEM is alive (stdio)\n");
#endif

pm_timefrominit();

#ifdef RASPBERRYPI_PICO
PM_INFO("Pi Pico RP2040\n");
#endif
#ifdef PIMORONI_PICO_PLUS2_RP2350
PM_INFO("Pimoroni Pico Plus 2 RP2350B\n");
#endif
#ifdef PIMORONI_PICO_PLUS2_W_RP2350
PM_INFO("Pimoroni Pico Plus 2 W RP2350B !!! Not Supported\n");
#endif
#ifdef RASPBERRYPI_PICO2
PM_INFO("Pi Pico 2 RP2350A\n");
#endif
#ifdef RASPBERRYPI_PICO2_W
PM_INFO("Pi Pico 2 W RP2350A\n");
#endif


PM_INFO("SYS Clk: %d\n",PM_SYS_CLK);
PM_INFO("Voltage: %d\n",vreg_get_voltage());
/*
PM_INFO("CFG BIOS Addr: %x\n",ROM_BIOS);
PM_INFO("CFG USE_USBHOST: %d\n",USE_USBHOST);
*/

// 1.3a   : Pull Up on GPIO 0,1,2        PM_Version_Detect 100007
// 1.14   : Pull Up on GPIO 0, 1,2 Input PM_Version_Detect 100003
// LP 1.0 : Pull Up and Down on GPIO0 ! Error
//PM_INFO("PM_Version_Detect %x\n",PM_Version_Detect);

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

// Various TEST Code not present in standard build

/*
ifdef //PM_PICO_INFOS
// Debug / to learn
    measure_freqs();
    pico_infos();
#endif ////PM_PICO_INFOS
*/

#if BOARD_PM15 || BOARD_PM20
PM_INFO("PIN_AD0: %d \n",PIN_AD0);
#if !PICO_PIO_USE_GPIO_BASE
#error PICO_PIO_USE_GPIO_BASE must be 1 to use >32 pins
#endif
#endif

#endif //PM_ENABLE_STDIO

// Start the Init and commands functions code
main_core0();

}  // main