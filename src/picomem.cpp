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
#include "hardware/watchdog.h"   // watchdog_caused_reboot() -- Core0 hang diagnostic
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

#if BOARD_WM10
#include "mca_iomem.pio.h"
extern "C" {
#include "mca_irq.h"        // WonderMCA-side direct MCA IRQ delivery
}
#include "dev_mca_pos.h"    // dev_mca_pos_install() -- called pre-launch
#include "dev_mca_tst.h"    // dev_mca_tst_install() -- called pre-launch
#include "dev_mca_post.h"   // dev_mca_post_install() -- PS/2 POST 0x80 capture
#include "dev_sermouse.h"   // serial mouse (COM2 0x2F8/IRQ3) -- Win+DOS mouse
#endif

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
#include "isa_devices.h"
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
#include "opl.h"          // OPL_Pico_stereo16 (ADLIB_SELFTEST)
#include "pm_audio.h"     // PM_SAMPLES_PER_BUFFER  (ADLIB_SELFTEST)
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

#if BOARD_WM10
// PIO instance for WonderMCA bus handler.
// WM10 builds have no ISA bus code, so pio0 is free. PIO1 is used by I2S
// audio (PICO_AUDIO_I2S_PIO=1). MCA needs pio_set_gpio_base(pio,16) so it
// can drive GPIO 16-47 (data + addr mux + control); putting MCA on pio0
// lets PIO1 keep its default base=0 and reach the I2S pins (GPIO 8/9/10).
extern "C" {
    PIO mca_pio = pio0;
}
#endif


// Return a memory Index based on the PC Address >>13 (8Kb)
// On WM10 the full main_core1() function is pinned into SCRATCH_X via
// __scratch_x("core1_MCA_code") and the Core 1 stack also lives in
// SCRATCH_X (.stack1_dummy). Pulling these tables out of scratch_x
// frees ~212 bytes (128 + 84) -- enough room for main_core1 to grow
// (e.g. the wm_dma_consume_byte hook in handle_iow) without overflowing
// the 4 KB region. The tables stay in regular SRAM (BSS), still fast
// for Core 1 access; performance impact is negligible on RP2350 where
// the cores share single-cycle SRAM.
// ISA path keeps them in scratch_x because that build doesn't pin
// main_core1 into scratch_x (different __scratch_x section name) and
// has more room to spare.
#if BOARD_WM10
volatile uint8_t MEM_Index_T[MEM_T_Size];
volatile uint8_t *RAM_Offset_Table[21];
#else
__scratch_x("MemDevTable")  volatile uint8_t MEM_Index_T[MEM_T_Size];
__scratch_x("MemBaseTable") volatile uint8_t *RAM_Offset_Table[21];
#endif

//Devices emulation tables
// Port table for fast decoding (IO Port >>3 then 8 Bytes precision)
#if BOARD_WM10
// WM10: ASM in main_core1 extracts addr_lo as 12 bits, so `addr_lo >> 3`
// reaches 0x1FF (= 511). The table MUST be sized [512] to avoid OOB reads
// on DMA cycles where CDMA drives 0xFFFC as the default IO address.
// Earlier [256] sizing caused intermittent SBDSP DMA failures: the OOB
// read at IO_Index_T[0x1FF] returned non-zero garbage, the ASM dispatched
// to the switch instead of the DMA fast-path, and wm_dma_consume_byte was
// never called -- bytes_received stayed at 0 despite CDMA running cycles.
// All entries 0x100..0x1FF are zero-initialized BSS, so the fast-path
// (DEV_NULL → GRANT check → consume_byte) fires correctly for any DMA
// cycle regardless of the address the CDMA happens to drive.
volatile uint8_t IO_Index_T[512];
#else
__scratch_x("IODevTable") volatile uint8_t IO_Index_T[256];
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

// - Define the BIOS default base address
// Don't use C000 Segment (0xC0000u) if the PC is configured to use an external video BIOS
// To Configure the ROM, Be carefull that the XTIDE ROM is 32Kb, even if only 16Kb is used

#if BOARD_PMJR
#define  ROM_BIOS 0x0C0000u      // @ of the PM BIOS           12Kb
#else
#define  ROM_BIOS 0x0D0000u      // @ of the PM BIOS           12Kb
#endif

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
#endif  // PM_SYS_CLK>280
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

// * Init the DMA GPIO Pins
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

// * Init the IRQ Pint to Output
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
gpio_init(PIN_IRQ7);
gpio_set_dir(PIN_IRQ7, GPIO_OUT);
gpio_put(PIN_IRQ7, 1);  // Set IRQ Up (There is an inverter) for no IRQ
#else
gpio_init(PIN_PM_IRQ);
gpio_set_dir(PIN_PM_IRQ, GPIO_OUT);
gpio_put(PIN_PM_IRQ, 1);  // Set IRQ Up (There is an inverter) for no IRQ
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

// WonderMCA I2S DAC startup self-test (1 kHz tone, ~2 s) -- defined in
// WonderMCA/src/utils.c (C linkage; utils.h has no extern "C" guard).
extern "C" void pcm5102_test(void);

#ifdef BOARD_WM10
#include "pico_psram.h"      // QMI/QSPI PSRAM: setup_psram(), __psram_start__ (0x1D000000), ps_size
#include "pico/cyw43_arch.h" // WiFi RM2 / CYW43439 driver over gSPI (pins from CYW43_DEFAULT_PIN_WL_*)
#include "hardware/sync.h"   // __dmb() for QMI reconfig ordering

// Auto-calibrate the QMI PSRAM read-sample delay (RXDELAY, m[1].timing bits[10:8],
// 0..7 in half-clock units). Writes a bit-toggling pattern to PSRAM, sweeps
// RXDELAY, and selects the CENTER of the widest passing window = maximum timing
// margin. Sets m[1].timing to the chosen value; returns it, or -1 if the read
// eye is fully closed. Call after setup_psram() at the FINAL sys clock, before
// PSRAM is used for anything else. Only touches m[1] (PSRAM); code runs from
// flash (m[0]), so even a failing RXDELAY can't crash the scan (printf works).
static inline uint32_t wm_rxcal_pat(uint32_t i) { return (i*0x9E3779B1u) ^ 0xA5A5A5A5u ^ (i<<5); }

static int __no_inline_not_in_flash_func(wm_psram_calibrate_rxdelay)(void)
{
    if (ps_size == 0) { printf("[rxcal] no PSRAM\n"); return -1; }

    volatile uint32_t *ps = (volatile uint32_t *)__psram_start__;   // no-cache window
    const uint32_t words = (ps_size < 64u*1024u ? (uint32_t)ps_size : 64u*1024u) / 4u;
    const uint32_t saved = qmi_hw->m[1].timing;                     // restore on total failure

    for (uint32_t i = 0; i < words; i++) ps[i] = wm_rxcal_pat(i);   // writes don't use RXDELAY
    __dmb();

    bool pass[8];
    for (uint32_t rd = 0; rd < 8; rd++) {
        hw_write_masked(&qmi_hw->m[1].timing,
                        rd << QMI_M1_TIMING_RXDELAY_LSB, QMI_M1_TIMING_RXDELAY_BITS);
        __dmb();
        uint32_t errs = 0;
        for (uint32_t i = 0; i < words; i++)
            if (ps[i] != wm_rxcal_pat(i) && ++errs > 4) break;
        pass[rd] = (errs == 0);
        printf("[rxcal] rxdelay=%u : %s\n", (unsigned)rd, pass[rd] ? "OK" : "FAIL");
    }

    // Widest contiguous passing window -> its center = max margin.
    int bstart = -1, blen = 0, cstart = -1, clen = 0;
    for (int rd = 0; rd < 8; rd++) {
        if (pass[rd]) { if (cstart < 0) { cstart = rd; clen = 0; }
                        if (++clen > blen) { blen = clen; bstart = cstart; } }
        else          { cstart = -1; clen = 0; }
    }
    if (bstart < 0) { qmi_hw->m[1].timing = saved; __dmb();
                      printf("[rxcal] NO passing rxdelay -- read eye closed!\n"); return -1; }

    int best = bstart + blen/2;
    hw_write_masked(&qmi_hw->m[1].timing,
                    (uint32_t)best << QMI_M1_TIMING_RXDELAY_LSB, QMI_M1_TIMING_RXDELAY_BITS);
    __dmb();
    printf("[rxcal] rxdelay=%d chosen (eye %d..%d, width=%d) @ %d MHz\n",
           best, bstart, bstart+blen-1, blen, PM_SYS_CLK);
    return best;
}

// 2-D QMI PSRAM timing eye: sweeps CLKDIV x RXDELAY, prints the pass/fail grid,
// and picks the FASTEST clkdiv (lowest divider = highest PSRAM clock) that still
// has a RXDELAY window of at least WM_RXCAL_MINWIDTH, then the CENTER of that
// window. Sets m[1].timing to the winner; returns (clkdiv<<8)|rxdelay, or -1.
// Same safety as the 1-D scan: only m[1] touched; a too-fast clkdiv just reads
// garbage (whole row 'X'), it doesn't hang. clkdiv=1 (full sys clock) skipped.
#define WM_CLKDIV_MIN     2
#define WM_CLKDIV_MAX     8
#define WM_RXCAL_MINWIDTH 2      // need >=2 good rxdelays to call a clkdiv usable

static int __no_inline_not_in_flash_func(wm_psram_calibrate_2d)(void)
{
    if (ps_size == 0) { printf("[2dcal] no PSRAM\n"); return -1; }

    volatile uint32_t *ps = (volatile uint32_t *)__psram_start__;
    const uint32_t words  = (ps_size < 64u*1024u ? (uint32_t)ps_size : 64u*1024u) / 4u;
    const uint32_t saved  = qmi_hw->m[1].timing;
    const uint32_t sysmhz = PM_SYS_CLK;

    printf("[2dcal] CLKDIV x RXDELAY eye @ %u MHz sys  ('.'=OK 'X'=fail)\n", (unsigned)sysmhz);
    printf("[2dcal]                     rxdelay: 0 1 2 3 4 5 6 7\n");

    static bool map[WM_CLKDIV_MAX+1][8];
    for (uint32_t cd = WM_CLKDIV_MIN; cd <= WM_CLKDIV_MAX; cd++) {
        // Set clkdiv for this row, then write the pattern with that write-clock.
        hw_write_masked(&qmi_hw->m[1].timing,
                        (cd << QMI_M1_TIMING_CLKDIV_LSB) & QMI_M1_TIMING_CLKDIV_BITS,
                        QMI_M1_TIMING_CLKDIV_BITS);
        __dmb();
        for (uint32_t i = 0; i < words; i++) ps[i] = wm_rxcal_pat(i);
        __dmb();

        char row[17]; int p = 0;
        for (uint32_t rd = 0; rd < 8; rd++) {
            hw_write_masked(&qmi_hw->m[1].timing,
                            rd << QMI_M1_TIMING_RXDELAY_LSB, QMI_M1_TIMING_RXDELAY_BITS);
            __dmb();
            uint32_t errs = 0;
            for (uint32_t i = 0; i < words; i++)
                if (ps[i] != wm_rxcal_pat(i) && ++errs > 4) break;
            map[cd][rd] = (errs == 0);
            row[p++] = map[cd][rd] ? '.' : 'X'; row[p++] = ' ';
        }
        row[p] = 0;
        printf("[2dcal] clkdiv=%u (%3u MHz): %s\n", (unsigned)cd, (unsigned)(sysmhz/cd), row);
    }

    // Fastest clkdiv with a usable window -> center of that window = max margin.
    int best_cd = -1, best_rd = -1;
    for (uint32_t cd = WM_CLKDIV_MIN; cd <= WM_CLKDIV_MAX && best_cd < 0; cd++) {
        int bs = -1, bl = 0, cs = -1, cl = 0;
        for (int rd = 0; rd < 8; rd++) {
            if (map[cd][rd]) { if (cs < 0) { cs = rd; cl = 0; } if (++cl > bl) { bl = cl; bs = cs; } }
            else             { cs = -1; cl = 0; }
        }
        if (bl >= WM_RXCAL_MINWIDTH) { best_cd = (int)cd; best_rd = bs + bl/2; }
    }
    if (best_cd < 0) {
        qmi_hw->m[1].timing = saved; __dmb();
        printf("[2dcal] no clkdiv has a usable rxdelay window -- kept previous timing\n");
        return -1;
    }
    hw_write_masked(&qmi_hw->m[1].timing,
                    (((uint32_t)best_cd << QMI_M1_TIMING_CLKDIV_LSB) & QMI_M1_TIMING_CLKDIV_BITS) |
                    ((uint32_t)best_rd << QMI_M1_TIMING_RXDELAY_LSB),
                    QMI_M1_TIMING_CLKDIV_BITS | QMI_M1_TIMING_RXDELAY_BITS);
    __dmb();
    printf("[2dcal] CHOSE clkdiv=%d (%u MHz PSRAM), rxdelay=%d\n",
           best_cd, (unsigned)(sysmhz/best_cd), best_rd);
    return (best_cd << 8) | best_rd;
}

// Bring up the QMI memory-mapped QSPI PSRAM (APS6404L, 8 MB) and self-test it.
// This is the first time QMI is configured on WM10 -- if CS/timing were wrong,
// setup_psram() would hang polling the QMI, so a clean return here is itself
// a positive signal. Tests run via plain pointer access at __psram_start__.
static bool wm_qspi_psram_test(void)
{
    printf("[PSRAM] Bringing up QMI/QSPI PSRAM...\n");
    setup_psram(false, 0);   // configure QMI, map at 0x1D000000, no PSRAM heap
    printf("[PSRAM] Detected size: %u bytes (%u MB)\n",
           (unsigned)ps_size, (unsigned)(ps_size >> 20));

    if (ps_size == 0) {
        printf("[PSRAM] FAIL: no PSRAM detected on QMI\n");
        return false;
    }

    // 2-D CLKDIV x RXDELAY eye scan: pick the fastest stable divider + best
    // sample point for this clock/board (replaces the hard-coded set_psram_timing
    // values). The 1-D wm_psram_calibrate_rxdelay() above is kept for reference.
    (void)&wm_psram_calibrate_rxdelay;   // silence unused (1-D kept as reference)
    //wm_psram_calibrate_2d();

    volatile uint8_t *p8 = (volatile uint8_t *)__psram_start__;
    const uint32_t SZ = (uint32_t)ps_size;
    bool pass = true;

    // Test 1: byte pattern at base
    for (int i = 0; i < 256; i++) p8[i] = (uint8_t)i;
    for (int i = 0; i < 256; i++) if (p8[i] != (uint8_t)i) { pass = false; break; }
    printf("[PSRAM] %s: pattern @0x000000\n", pass ? "PASS" : "FAIL");

    // Test 2: byte pattern at top of memory
    {
        bool ok = true;
        for (int i = 0; i < 256; i++) p8[SZ - 256 + i] = (uint8_t)(0xFF - i);
        for (int i = 0; i < 256; i++) if (p8[SZ - 256 + i] != (uint8_t)(0xFF - i)) { ok = false; break; }
        printf("[PSRAM] %s: pattern @0x%06X\n", ok ? "PASS" : "FAIL", (unsigned)(SZ - 256));
        pass = pass && ok;
    }

    // Test 3: address aliasing (base vs 4 MB offset)
    if (SZ > 4u * 1024 * 1024) {
        p8[0] = 0xAA;
        p8[4u * 1024 * 1024] = 0x55;
        if (p8[0] != 0xAA || p8[4u * 1024 * 1024] != 0x55) {
            pass = false;
            printf("[PSRAM] FAIL: aliasing (0=0x%02X, 4MB=0x%02X)\n",
                   p8[0], p8[4u * 1024 * 1024]);
        } else {
            printf("[PSRAM] PASS: no aliasing (0 vs 4 MB)\n");
        }
    }

    printf("[PSRAM] QSPI test: %s\n", pass ? "ALL PASSED" : "FAILED");
    return pass;
}

// WiFi RM2 (CYW43439) connection test via the real cyw43 driver. cyw43_arch_init()
// powers WL_ON, runs the gSPI handshake, and uploads the chip firmware -- a clean
// return means the module is present and talking on the pins set by the WM10
// CYW43_DEFAULT_PIN_WL_* defines. We read the MAC as extra proof, then deinit so
// the driver's PIO/DMA/IRQ are released before Core 1 brings up the MCA bus (PIO0).
// This is a one-shot presence test, not a persistent WiFi bring-up.
static bool wm_wifi_rm2_test(void)
{
    printf("[WiFi] cyw43_arch_init() -- RM2/CYW43439 (ON=%d DATA=%d CLK=%d CS=%d)...\n",
           PIN_WIFI_ON, PIN_WIFI_DATA, PIN_WIFI_CLK, PIN_WIFI_CS);

    // NOTE: with pico_cyw43_arch_none, cyw43_arch_init() only sets up the
    // driver structs -- it returns 0 WITHOUT bringing the radio up. The real
    // gSPI handshake (cyw43_ll_bus_init, which reads the 0xFEEDBEAD test
    // pattern) happens lazily in cyw43_arch_enable_sta_mode() below. So a 0
    // here is NOT proof the module is present -- we must validate the MAC.
    int err = cyw43_arch_init();
    if (err) {
        printf("[WiFi] FAIL: cyw43_arch_init() returned %d\n", err);
        return false;
    }

    // This is where the chip is actually probed over gSPI. If the test
    // pattern read fails ("Failed to read test pattern" / "Failed to start
    // CYW43" from the driver), the radio never comes up and the MAC stays 0.
    cyw43_arch_enable_sta_mode();

    uint8_t mac[6] = {0};
    int mres = cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac);
    bool mac_valid = (mres == 0) &&
                     (mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]);

    if (mac_valid) {
        printf("[WiFi] OK: RM2 responding, MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        printf("[WiFi] FAIL: gSPI handshake failed -- CYW43439 not responding "
               "(MAC=00:..:00, get_mac=%d). Check 3V3/GND power to the RM2, "
               "GPIO15/W_CLK trace (must go ONLY to RM2, not the MADE24 gate), "
               "and continuity on ON=13/DATA=14/CLK=15/CS=11.\n", mres);
    }

    // Release PIO/DMA/IRQ so they don't collide with the MCA bus handler.
    cyw43_arch_deinit();
    return mac_valid;
}

// ---------------------------------------------------------------------------
// cyw43 raw-ethernet callbacks (no-lwIP / arch_none).
// The SDK's weak defaults for these call no_lwip_fail() -> panic("cyw43 has no
// ethernet interface") the moment the chip reports a link event or hands up a
// received frame (which happens even around a failed join). We use the radio
// without lwIP (raw frames for NE2000), so provide strong overrides: link
// up/down are no-ops, and received frames are discarded for now. This is also
// the hook where NE2000-over-WiFi will consume RX frames later.
// ---------------------------------------------------------------------------
extern "C" void cyw43_cb_tcpip_set_link_up(cyw43_t *self, int itf)   { (void)self; (void)itf; }
extern "C" void cyw43_cb_tcpip_set_link_down(cyw43_t *self, int itf) { (void)self; (void)itf; }
extern "C" void ne2000_deliver_rx(const uint8_t *frame, uint16_t len);

// Linker stub: cyw43_ll_send_ethernet references pbuf_copy_partial (lwIP) even
// in the non-pbuf path we use (is_pbuf=false), and we build arch_none (no lwIP).
// Never actually called with our is_pbuf=false sends; present only to link.
extern "C" uint16_t pbuf_copy_partial(void *buf, void *dataptr, uint16_t len, uint16_t offset)
{
    (void)buf; (void)dataptr; (void)len; (void)offset;
    return 0;
}

// RX: WiFi -> NE2000. The cyw43 driver calls this for every received Ethernet
// frame. When NetType=WiFi has routed ne2000_backend over CYW43, hand the frame
// to the NE2000 emulator (which filters by dest MAC / promisc). Harmless if the
// NE2000 isn't installed (deliver_rx just has nowhere to queue).
// DHCP snoop: the Pico has no IP of its own (cyw43 arch_none, no lwIP) -- DOS
// (mTCP) owns the lease. Every RX frame passes through here, so we watch DHCP
// ACK packets (UDP 67->68, msg-type 5) go by and capture yiaddr + the gateway
// (opt 3) and DNS (opt 6) into PM_Wifi so the screenpad can show the live IP.
// Read-only; never modifies the frame. Big-endian-in-uint32 (octet1 high).
#if USE_NE2000   // networking helpers (cyw43/NE2000) -- gated out when USE_NE2000=0
static void wm_dhcp_snoop(const uint8_t *f, size_t len)
{
    if (len < 14 + 20 + 8 + 240) return;            // too short to hold DHCP
    if (f[12] != 0x08 || f[13] != 0x00) return;     // not IPv4
    uint8_t ihl = (uint8_t)((f[14] & 0x0F) * 4);
    if (ihl < 20 || f[14 + 9] != 0x11) return;      // not UDP
    const uint8_t *udp = f + 14 + ihl;
    if ((size_t)(udp + 8 - f) > len) return;
    if (((udp[0] << 8) | udp[1]) != 67 || ((udp[2] << 8) | udp[3]) != 68) return; // DHCP srv->cli
    const uint8_t *dh = udp + 8;
    if ((size_t)(dh + 240 - f) > len) return;
    if (dh[236] != 0x63 || dh[237] != 0x82 || dh[238] != 0x53 || dh[239] != 0x63) return; // magic
    bool is_ack = false; uint32_t mask = 0, gw = 0, dns = 0;
    const uint8_t *opt = dh + 240, *end = f + len;
    while (opt < end && *opt != 0xFF) {
        if (*opt == 0) { opt++; continue; }         // pad
        if (opt + 2 > end) break;
        uint8_t code = opt[0], l = opt[1]; const uint8_t *v = opt + 2;
        if (v + l > end) break;
        if      (code == 53 && l >= 1) is_ack = (v[0] == 5);   // DHCPACK
        else if (code == 1  && l >= 4) mask = ((uint32_t)v[0]<<24)|((uint32_t)v[1]<<16)|((uint32_t)v[2]<<8)|v[3]; // subnet
        else if (code == 3  && l >= 4) gw  = ((uint32_t)v[0]<<24)|((uint32_t)v[1]<<16)|((uint32_t)v[2]<<8)|v[3];
        else if (code == 6  && l >= 4) dns = ((uint32_t)v[0]<<24)|((uint32_t)v[1]<<16)|((uint32_t)v[2]<<8)|v[3];
        opt += 2 + l;
    }
    if (!is_ack) return;
    uint32_t yi = ((uint32_t)dh[16]<<24)|((uint32_t)dh[17]<<16)|((uint32_t)dh[18]<<8)|dh[19]; // yiaddr
    if (!yi) return;
    PM_Wifi.dhcp_ip = yi;
    if (mask) PM_Wifi.dhcp_mask = mask;
    if (gw)   PM_Wifi.dhcp_gw   = gw;
    if (dns)  PM_Wifi.dhcp_dns  = dns;
    printf("[NET][DHCP] DOS leased %lu.%lu.%lu.%lu\n",
           (unsigned long)((yi>>24)&0xFF),(unsigned long)((yi>>16)&0xFF),
           (unsigned long)((yi>>8)&0xFF),(unsigned long)(yi&0xFF));
}

extern "C" void cyw43_cb_process_ethernet(void *cb_data, int itf, size_t len, const uint8_t *buf)
{
    (void)cb_data; (void)itf;
    if (len) { wm_dhcp_snoop(buf, len); ne2000_deliver_rx(buf, (uint16_t)len); }
}

// TX: NE2000 -> WiFi. Installed into ne2000_backend.send for NetType=WiFi so the
// emulator's outbound frames go out the CYW43 station interface.
extern "C" void wm_cyw43_send(const uint8_t *frame, uint16_t len)
{
    cyw43_send_ethernet(&cyw43_state, CYW43_ITF_STA, len, frame, false);
}

// ---------------------------------------------------------------------------
// WiFi station connect (NetType = WiFi). Async: started from CMD_DEV_Init and
// polled from the Core 0 main loop, so it never blocks POST. Credentials come
// from wifi.txt on the SD (line 1 = SSID, line 2 = password, WPA2).
// ---------------------------------------------------------------------------
static bool     wm_wifi_up       = false;  // cyw43 initialised + connect issued
static bool     wm_wifi_joining  = false;  // association still in progress
static uint32_t wm_wifi_deadline = 0;      // time_us_32() connect deadline

extern "C" uint8_t wm_wifi_connect_start(void)
{
    // Guard against re-init on a PC OS warm-reboot. The x86 side re-runs its
    // option-ROM init, which re-sends CMD_DEV_Init -> here, but the RP2350 (and
    // thus the CYW43 radio) never lost power. Calling cyw43_arch_init() a second
    // time on an already-live CYW43 corrupts the gSPI bring-up: the next status
    // read returns garbage (bytes_pending 0x7FF) -> "SPI invalid bytes pending
    // 2047" -> hard assert. The radio, STA MAC and association all persist across
    // an x86 reset, so keep them and just let the caller re-install NE2000 with
    // the MAC already in PM_Wifi.cyw43_mac.
    if (wm_wifi_up) {
        printf("[NET][WiFi] already up -- skipping re-init on OS reboot\n");
        return 0;
    }
    if (cyw43_arch_init() != 0) {
        printf("[NET][WiFi] arch_init failed\n");
        snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr, "Init failed");
        return 1;
    }
    cyw43_arch_enable_sta_mode();

    // Read the CYW43 station MAC now (available right after enable_sta_mode,
    // before association) into PM_Wifi.cyw43_mac -- the NE2000 emulator copies
    // it into its physaddr at install time, so this must be set before the
    // dev_ne2000_install() in CMD_DEV_Init's NET_WIFI branch.
    cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, (uint8_t *)PM_Wifi.cyw43_mac);
    printf("[NET][WiFi] STA MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
           PM_Wifi.cyw43_mac[0], PM_Wifi.cyw43_mac[1], PM_Wifi.cyw43_mac[2],
           PM_Wifi.cyw43_mac[3], PM_Wifi.cyw43_mac[4], PM_Wifi.cyw43_mac[5]);

    FIL fcfg;
    if (f_open(&fcfg, "wifi.txt", FA_OPEN_EXISTING | FA_READ) != FR_OK) {
        printf("[NET][WiFi] wifi.txt not found on SD\n");
        snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr, "No wifi.txt");
        return 2;
    }
    char ssid[40] = {0}, key[72] = {0};
    bool got = f_gets(ssid, sizeof ssid, &fcfg) && f_gets(key, sizeof key, &fcfg);
    f_close(&fcfg);
    if (!got) {
        printf("[NET][WiFi] wifi.txt malformed (need SSID line + password line)\n");
        snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr, "Bad wifi.txt");
        return 3;
    }
    // Strip trailing CR/LF/space from both lines.
    for (int i = (int)strlen(ssid) - 1; i >= 0 && (ssid[i]=='\r'||ssid[i]=='\n'||ssid[i]==' '); --i) ssid[i] = 0;
    for (int i = (int)strlen(key)  - 1; i >= 0 && (key[i] =='\r'||key[i] =='\n'||key[i] ==' '); --i) key[i]  = 0;
    if (!ssid[0] || !key[0]) {
        printf("[NET][WiFi] empty SSID or password in wifi.txt\n");
        snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr, "Empty SSID/key");
        return 4;
    }

    strncpy(PM_Wifi.WIFI_SSID, ssid, sizeof PM_Wifi.WIFI_SSID - 1);
    strncpy(PM_Wifi.WIFI_KEY,  key,  sizeof PM_Wifi.WIFI_KEY  - 1);
    printf("[NET][WiFi] connecting to '%s'...\n", ssid);
    snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr, "Connecting...");

    cyw43_arch_wifi_connect_async(ssid, key, CYW43_AUTH_WPA2_MIXED_PSK);
    wm_wifi_up       = true;
    wm_wifi_joining  = true;
    wm_wifi_deadline = time_us_32() + 30000000u;   // 30 s
    return 0;
}

extern "C" void wm_wifi_poll(void)
{
    if (!wm_wifi_up) return;
    cyw43_arch_poll();                 // no-op in IRQ/background mode
    if (!wm_wifi_joining) {
        // Connected: refresh the WiFi signal strength every ~2 s.
        if (PM_Wifi.Status == 1) {
            static uint32_t rssi_next = 0;
            if ((int32_t)(time_us_32() - rssi_next) >= 0) {
                rssi_next = time_us_32() + 2000000u;
                int32_t r = 0;
                if (cyw43_wifi_get_rssi(&cyw43_state, &r) == 0) PM_Wifi.rssi = r;
            }
        }
        return;
    }

    int st = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    // cyw43_wifi_link_status is the WiFi (radio) link, not the IP link. With no
    // lwIP it associates and stays at CYW43_LINK_JOIN (1); the higher NOIP(2)/
    // UP(3) states only happen with an IP stack. So JOIN means "associated".
    if (st >= CYW43_LINK_JOIN) {                          // associated
        BV_WifiInit    = INIT_DONE;
        PM_Wifi.Status = 1;
        snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr, "Connected");
        printf("[NET][WiFi] connected to SSID:%s\n", PM_Wifi.WIFI_SSID);
        wm_wifi_joining = false;
    } else if (st < 0 || (int32_t)(time_us_32() - wm_wifi_deadline) >= 0) {
        BV_WifiInit    = INIT_SKIPPED;
        PM_Wifi.Status = 0;
        snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr,
                 st < 0 ? "Connect failed" : "Connect timeout");
        printf("[NET][WiFi] connect failed (link status %d)\n", st);
        wm_wifi_joining = false;
    }
}
#endif // USE_NE2000 (networking helpers)
#endif // BOARD_WM10

#if defined(BOARD_WM10) && USE_USBHOST
#include "tusb.h"                                // tuh_task() (inline -> tuh_task_ext)
// ---------------------------------------------------------------------------
// Standalone early USB self-test (diagnostic). Brings up the USB host stack on
// Core 0 and runs the pure USB -> MSC -> FatFs read stress (pm_usb_chain_test)
// BEFORE Core 1 launch, the MCA bus, SD/config, audio, WiFi and the PM_CMD
// loop -- so the USB chain is exercised in complete isolation. Plug a USB stick
// in before power-on. The board intentionally HALTS in a tuh_task() loop after
// the test; comment out the call in main() (line ~879) to resume normal boot.
// ---------------------------------------------------------------------------
extern bool      PM_StartUSB();                  // pmdev.h
extern "C" bool  usb_msc_volume_ready(void);     // fatfs_usb.c
extern "C" void  pm_usb_chain_test(void);        // pm_disk.cpp

static void wm_usb_early_selftest(void)
{
    printf("\n[USBTEST] === early standalone USB self-test (Core0, pre-Core1) ===\n");
    PM_StartUSB();                               // tusb_init(host) + hid_init()
    printf("[USBTEST] waiting for USB volume (plug stick now, 15s)...\n");
    uint32_t dl = time_us_32() + 15000000u;
    while (!usb_msc_volume_ready() && (int32_t)(time_us_32() - dl) < 0)
        tuh_task();
    if (usb_msc_volume_ready()) pm_usb_chain_test();
    else                        printf("[USBTEST] no USB volume after 15s\n");
    printf("[USBTEST] === complete -- halting in tuh_task() loop (reset to exit) ===\n");
    for (;;) tuh_task();                         // pure isolation; nothing else runs
}

#endif // BOARD_WM10 && USE_USBHOST

void wm_display_core0hang()
{
    // DIAGNOSTIC: report a Core 0 hang caught by the watchdog (set in main_core0).
// g_c0_stage (in .uninitialized_data) survives the reset and names the section
// that hung. Only trust it on a watchdog reboot with a valid breadcrumb magic.

    extern volatile uint32_t g_c0_bc_magic;
    extern volatile uint8_t  g_c0_stage;
    extern volatile uint8_t  g_c0_cmd;
    if (watchdog_caused_reboot() && g_c0_bc_magic == 0xC0DE5742u) {
        printf("[SYS] *** Core0 WATCHDOG RESET -- hung at stage %u (cmd 0x%02X) ***\n",
               g_c0_stage, g_c0_cmd);
        printf("[SYS] stages: 0=loop-top 1=CMD/disk(SD) 2=audio 3=screenpad-I2C "
               "4=wifi(cyw43) 5=NE2000/W5500\n");
        g_c0_bc_magic = 0;   // consume so a later clean reboot isn't misreported
    }
}

void pm_display_core0hang()
{
// DIAGNOSTIC: report a Core 0 hang caught by the watchdog (set in main_core0).
// g_c0_stage (in .uninitialized_data) survives the reset and names the section
// that hung. Only trust it on a watchdog reboot with a valid breadcrumb magic.

    extern volatile uint32_t g_c0_bc_magic;
    extern volatile uint8_t  g_c0_stage;
    extern volatile uint8_t  g_c0_cmd;
    if (watchdog_caused_reboot() && g_c0_bc_magic == 0xC0DE5742u) {
        printf("[SYS] *** Core0 WATCHDOG RESET -- hung at stage %u (cmd 0x%02X) ***\n",
               g_c0_stage, g_c0_cmd);
        printf("[SYS] stages: 0=loop-top 1=CMD/disk(SD) 2=audio 3=screenpad-I2C "
               "4=wifi(cyw43) 5=NE2000/W5500\n");
        g_c0_bc_magic = 0;   // consume so a later clean reboot isn't misreported
    }
}

int main(void)
{


#ifdef BOARD_WM10
wm_board_init();
stdio_init_all();
printf("\n\n=== WonderMCA START ===\n");

wm_display_core0hang();

// POS-LATENCY: the LED heartbeat (~1 s) and the QSPI PSRAM bring-up/test
// were moved to AFTER multicore_launch_core1() (below) so the card answers
// POS register reads within a few ms of boot. Running them here scanned the
// card before pos_regs/Core 1 were ready -> PS/2 POST 165.

// WiFi RM2 (CYW43439) presence test over gSPI.
// === WM10 USB TEST (2026-06-22, revert after) ==========================
// Skip the cyw43 boot probe: it hard-asserts on finicky RM2 gSPI bring-up
// (cyw43_kso_set fail) and claims/leaves DMA+IRQ+PIO state that confounds
// the USB-host hang we are isolating. WiFi is forced off for this test anyway.
// wm_wifi_rm2_test();
// =======================================================================
#else
BOARD_Init();      // Init the Board (Pico OverClock)
BOARD_Init_GPIO(); // Init the GPIOs for PicoMEM
#endif



// Initialise the BIOS Variables/Devices Init Status
#if BOARD_WM10

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

// ************************************ INIT ISA SM and Loop code *************************************
#if BOARD_WM10
// WM10: IO_Index_T and MEM_Index_T are in SCRATCH_X — initialized by Core 1 only
PM_BIOSADDRESS = (ROM_BIOS >> 4);
PC_DB_Start = WM_DPRAM_BASE_ADDR + PM_DB_Offset;  // disk buffer = DPRAM base + 4KB (0xD7000); was ROM_BIOS+0x4000+... which broke when the ROM grew to 24KB
PM_BasePort = PM_IOBASEPORT >> 3;

gpio_set_slew_rate(PIN_D_OE, GPIO_SLEW_RATE_FAST);
gpio_set_slew_rate(PIN_A_AB, GPIO_SLEW_RATE_FAST);
gpio_set_slew_rate(PIN_R_REL, GPIO_SLEW_RATE_FAST);   // GPIO 34 (was PIN_R_CHRDY)

mca_irq_init();

dev_mca_pos_install();                            // 0x100/0x108 + pos_regs
dev_mca_tst_install();                            // 0x320 MCA test port

SetPortType(PM_IOBASEPORT, DEV_PM, 1);            // 0x2A0 PicoMEM cmd port
#if USE_AUDIO
SetPortType(0x388, DEV_ADLIB, 1);                 // 0x388 Adlib / OPL2
#endif
#if USE_SERMOUSE
dev_sermouse_init(0x2F8, 3);                      // COM2 serial mouse UART, IRQ3
SetPortType(0x2F8, DEV_SERMOUSE, 1);             // 0x2F8-0x2FF (one IO_Index entry)
#endif

// Port 0x80 PS/2 POST capture. Paired with the passive_snoop flag
// in wm_mca_rp2350_v10.h handle_iow / loop_continue. Known to break
// boot on 50Z/P70 in this configuration -- left enabled per request.
dev_mca_post_install();                           // 0x80 PS/2 POST codes
PM_DP_RAM[0] = 0x12;        // BIOS magic byte (BV_VARID) at DPRAM[0]
#else
PM_ISA_IO_Init(); // Setup the ISA bus code variables and PIO SM
PM_BASE_DEV_Init();
#endif

// Core 1 (MCA bus handler) gets high bus priority so it meets the tight
// MCA MEMR/MEMW timing window.
bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC1_BITS;

#if ISA_DBG_IO
multicore_launch_core1(test_IOW_core1);
#else
// ** Start the ISA Cycles management code
multicore_launch_core1(main_core1);
#endif

#if defined(BOARD_WM10) && USE_USBHOST
//wm_usb_early_selftest();
#endif

// ---- POS-deferred boot work -------------------------------------------
// Core 1 is now serving the MCA bus (incl. POS register reads via pos_regs),
// so the slow, POS-irrelevant startup work runs HERE instead of before the
// launch -- the card is POS-responsive within a few ms of power-on.
#ifdef BOARD_WM10
// Startup heartbeat: blink the board LED (GPIO 1 on WM10) so it's visibly
// clear the firmware booted. Also gives USB CDC a moment to enumerate.
gpio_init(LED_PIN);
gpio_set_dir(LED_PIN, GPIO_OUT);
for (int i = 0; i < 3; i++) {
    gpio_put(LED_PIN, 1); sleep_ms(50);
    gpio_put(LED_PIN, 0); sleep_ms(50);
}
// QSPI PSRAM bring-up + self-test (QMI memory-mapped, APS6404L 8 MB).
wm_qspi_psram_test();   // ENABLED: brings up QMI PSRAM + eye-scans RXDELAY ([rxcal])
#endif

#if defined(BOARD_WM10) && USE_USBHOST
//wm_usb_early_selftest();
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
#if !BOARD_WM10
    stdio_init_all();
#else
    // WM10: stdio was already brought up at boot (line ~869, before the
    // Core1 launch). Re-running stdio_init_all() HERE -- after Core1 has
    // reclaimed GPIO45 for /CD-SETUP (gpio_set_function SIO + pull_down) --
    // re-applies GPIO_FUNC_UART to GPIO45 (PICO_DEFAULT_UART_RX_PIN=45) and
    // wipes the pull-down, so the POS gate gpio_get(PIN_CD_SETUP) reads a
    // UART pin and the card stops answering POS (not detected). So DO NOT
    // re-init stdio here on WM10.
#endif
    SERIAL_Enabled=true;
#ifdef BOARD_PMJR
printf("PicoMEM Junior is alive (stdio)\n");
#else    
    PM_INFO("PicoMEM is alive (stdio)\n");
#endif    
#if CORE0_WATCHDOG
    pm_display_core0hang();
#endif
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


PM_INFO("[BOOT] System clock: %d\n",PM_SYS_CLK);
PM_INFO("[BOOT] Voltage: %d\n",vreg_get_voltage());

// Build with -DADLIB_SELFTEST=1 (or define here) to play a 440 Hz Adlib
// tone at boot, before the BIOS command loop. Useful to confirm the
// OPL2 -> mixer -> I2S -> PCM5102 chain end-to-end without a PS/2.
// Adds ~1 s to boot. Off by default.
//#define ADLIB_SELFTEST 1

#if defined(ADLIB_SELFTEST) && BOARD_WM10 && USE_AUDIO
// ============================================================
// Adlib self-test (Option B): runs once at boot, before BIOS
// command loop. Initializes OPL2 + I2S directly and plays a
// short tone via dev_adlib_iow(). Lets us isolate "is the
// OPL->I2S audio chain working" from any MCA bus stalls.
// Watch the serial log for the "ADLIB SELFTEST" markers --
// if execution stalls between markers, we have located the
// hang.
// ============================================================
{
    PM_INFO("\n=== ADLIB SELFTEST: begin ===\n");

    PM_INFO("ADLIB SELFTEST: install (OPL_Pico_Init + SetPortType)\n");
    dev_adlib_install();
    PM_INFO("ADLIB SELFTEST: install done\n");

    PM_INFO("ADLIB SELFTEST: start audiomix (pm_audio_init_i2s)\n");
    dev_audiomix_start();
    PM_INFO("ADLIB SELFTEST: audiomix started, enabled=%d\n",
            (int)dev_audiomix.enabled);

    // OPL2 channel 0 voice setup -- additive, modulator op1, carrier op2.
    // Register layout:
    //   0x20 op1 = AM/VIB/EG/KSR/MULT
    //   0x40 op1 = KSL/Total Level (volume; 0=loud, 0x3F=silent)
    //   0x60 op1 = Attack/Decay
    //   0x80 op1 = Sustain/Release
    //   +3 offsets (0x23,0x43,0x63,0x83) = same fields for op2
    //   0xC0 ch0 = feedback/algorithm (bit0=1 => additive)
    //   0xA0 ch0 = F-Number low
    //   0xB0 ch0 = key-on / octave / F-Number high
    PM_INFO("ADLIB SELFTEST: program OPL2 voice 0\n");
    dev_adlib_iow(0, 0x20); dev_adlib_iow(1, 0x01);  // op1 mult=1
    dev_adlib_iow(0, 0x40); dev_adlib_iow(1, 0x10);  // op1 TL=16
    dev_adlib_iow(0, 0x60); dev_adlib_iow(1, 0xF0);  // op1 fast attack
    dev_adlib_iow(0, 0x80); dev_adlib_iow(1, 0x77);  // op1 sus/rel
    dev_adlib_iow(0, 0x23); dev_adlib_iow(1, 0x01);  // op2 mult=1
    dev_adlib_iow(0, 0x43); dev_adlib_iow(1, 0x00);  // op2 TL=0 (loud)
    dev_adlib_iow(0, 0x63); dev_adlib_iow(1, 0xF0);  // op2 fast attack
    dev_adlib_iow(0, 0x83); dev_adlib_iow(1, 0x77);  // op2 sus/rel
    dev_adlib_iow(0, 0xC0); dev_adlib_iow(1, 0x01);  // ch0 algo additive
    // 440Hz at octave 4 -> F-Number ~0x199 (depends on OPL clock)
    dev_adlib_iow(0, 0xA0); dev_adlib_iow(1, 0x99);  // F-num low
    dev_adlib_iow(0, 0xB0); dev_adlib_iow(1, 0x32);  // key-on, oct=4, F-hi=1

    // Mark dev_active so pm_audio_domix actually mixes Adlib output.
    dev_audiomix.dev_active |= AD_ADLIB;

    PM_INFO("ADLIB SELFTEST: tone ON for 800ms (listen for beep)\n");

    // Direct sanity check: render OPL into a local buffer and report
    // peak amplitude. If peak is 0, the OPL emulator isn't producing
    // audio (software/config issue). If peak > 0, samples ARE being
    // generated and silence means hardware (DAC/speaker/wiring).
    {
        int16_t probe[PM_SAMPLES_PER_BUFFER * 2] = {0};
        OPL_Pico_stereo16(probe, PM_SAMPLES_PER_BUFFER);
        int16_t peak = 0;
        for (int i = 0; i < PM_SAMPLES_PER_BUFFER * 2; i++) {
            int16_t v = probe[i] < 0 ? -probe[i] : probe[i];
            if (v > peak) peak = v;
        }
        PM_INFO("ADLIB SELFTEST: OPL render peak=%d (0=silent emulator, "
                ">0=samples OK -> check DAC/speaker)\n", (int)peak);
    }

    // Pump pm_audio_domix() ourselves -- the main BIOS command loop
    // (which normally calls it) hasn't started yet at this point.
    {
        absolute_time_t deadline = make_timeout_time_ms(800);
        while (absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
            pm_audio_domix(false);
        }
    }

    dev_adlib_iow(0, 0xB0); dev_adlib_iow(1, 0x12);  // key-off
    // Let the release tail render briefly so the note fades cleanly.
    {
        absolute_time_t deadline = make_timeout_time_ms(100);
        while (absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
            pm_audio_domix(false);
        }
    }
    PM_INFO("ADLIB SELFTEST: tone OFF\n");

    PM_INFO("=== ADLIB SELFTEST: end ===\n\n");
}
#endif // ADLIB_SELFTEST && BOARD_WM10 && USE_AUDIO

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