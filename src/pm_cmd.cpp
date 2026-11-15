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
#include "bios_wrappers/bios_int13h.h"
#include "bios_wrappers/bios_int21h.h"
#include "bios_wrappers/bios_int2Fh.h"
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
#include "bios_wrappers/bios_int13h.h"
#include "bios_wrappers/bios_int21h.h"

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
#include "pm_libs/pm_bios_menu.h"
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

#if BOARD_WM10
#include "mca_devices/dev_mca_pos.h"
#include "mca_devices/dev_mca_tst.h"
#include "dev_mca_post.h"           // dev_mca_post_update() -- screenpad POST code
#endif



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
#include "gus/gus_dma_dbg.h"   // gus_dma_dbg (DMA-chain instrumentation)
#endif

#include "dev_audiomix.h"
#include "pm_audio.h"

//#include "audio_devices/sbdsp/sbdsp.h"
//#include "audio_devices/audio_fifo.h"

extern void sbdsp_updatebuffer(int16_t* buff,uint32_t samples);

#else
void dev_audiomix_pause() {}
void dev_audiomix_resume() {}
#endif

#if USE_CDROM
#include "dev_cdrom_mke.h"

// POST/reboot quiesce (called from CMD_DEV_Init): C-linkage externs.
extern "C" void pm_keyboard_reset(void);      // pm_libs/inputs/pm_keyboard.c
#if USE_SERMOUSE
extern "C" void dev_sermouse_quiesce(void);   // isa_devices/serial/dev_sermouse.cpp
#endif
#endif

#if PM_WIFI  // PicoMEM Wifi
#if USE_NE2000
extern "C" {
#include "ne2000/dev_ne2000.h"
extern bool dev_ne2000_install();
extern wifi_infos_t PM_Wifi;
}
#endif
#endif

#if BOARD_WM10
extern "C" {
// WonderMCA-side I2C OLED + button keypad (J9 header). Optional --
// modules may not be populated; init silently no-ops if absent.
#include "screenpad.h"
#include "screenpad_menu.h"
// MCA slave DMA helper (Sound Blaster / GUS via system CDMA).
#include "wm_dma.h"
}
#include "pm_libs/pm_screenpad_host.h"
#endif

#if BOARD_WM10 && USE_NE2000_W5500
extern "C" {
#include "ne2000/dev_ne2000.h"
#include "ne2000/ne2000_backend.h"
#include "w5500_backend.h"
#if BOARD_WM10 && USE_NE2000_CDCECM
#include "usbeth.h"     // USB Ethernet (CDC-ECM) NE2000 backend (NET_USB)
#endif
extern wifi_infos_t PM_Wifi;
extern bool dev_ne2000_install();
// WiFi (RM2/CYW43) station connect -- defined in picomem.cpp (BOARD_WM10).
// Async: start at CMD_DEV_Init, poll the link from the Core 0 main loop.
extern uint8_t wm_wifi_connect_start(void);
extern void    wm_wifi_poll(void);
// WM10 USB boot chain test (file scope -- extern "C" cannot be block-local)
extern "C" bool usb_msc_volume_ready(void);
extern "C" void pm_usb_chain_test(void);
// NE2000 TX over CYW43 (defined in picomem.cpp) -- installed into
// ne2000_backend.send when NetType=WiFi routes the NE2000 over WiFi.
extern void    wm_cyw43_send(const uint8_t *frame, uint16_t len);
// NE2000 trace counters (defined in ne2000.c).

}
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
volatile uint8_t PM_Status = STAT_INIT;  // Read from Port 0. Initialized to STAT_INIT
                                  // so PMBIOS POST_WaitInitLoop holds (no-timeout spin)
                                  // until Core 0 finishes WM10 init -- otherwise early
                                  // PMBIOS commands like CMD_Test_RAM hit IPROGRESS
                                  // before Core 0 reaches the FIFO loop, then time out.
volatile uint8_t PM_CmdDataH;     // R/W Register
volatile uint8_t PM_CmdDataL;     // R/W Register
volatile bool PM_CmdReset=false;  // Is set to true when trying to reset a "locked" command

volatile uint8_t PM_TestWrite;
         uint8_t PM_SetRAMVal;
         uint8_t PM_TestRAMVal;

// SB end-of-block IRQ pulse width: assert held this long before auto-lower so
// the chassis 8259 latches the falling edge. 200us matches the proven mux-IRQ
// pulse; fires once per DMA block (~1 s) so cost is negligible. Tune if the
// 8557 still misses the edge (raise) or audio stutters (lower).
#define SB_EOT_IRQ_HOLD_US   200
uint8_t PM_Int13h_LEDCount=0;     // Count the number of Int13h calls to blink the LED during Disk access
uint32_t PM_DiskLED_LastUs=0;     // time_us_32() of the last Int13h -- used to force the
                                  // disk-activity LED off when the access burst stops
                                  // mid-blink (count 3..5), otherwise it would stay lit.

#include "hardware/watchdog.h"
// Core 0 hang breadcrumb (DIAGNOSTIC). g_c0_stage is set before each main-loop
// section and lives in .uninitialized_data so it survives a watchdog reset. If
// Core 0 hangs (a blocking call never returns), the watchdog -- enabled in
// main_core0 -- resets the chip and main() prints the last stage, pinpointing
// which section hung. Remove once the hang is located/fixed.
#define C0_BC_MAGIC 0xC0DE5742u
volatile uint32_t g_c0_bc_magic __attribute__((section(".uninitialized_data")));
volatile uint8_t  g_c0_stage    __attribute__((section(".uninitialized_data")));
volatile uint8_t  g_c0_cmd      __attribute__((section(".uninitialized_data")));  // PM_Command when stage==1
#define C0_STAGE(n) (g_c0_stage = (uint8_t)(n))
        
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

// ============================================================================
// DISK-CORRUPTION BISECTION (temporary -- branch NewDMA).
// Each flag gates one Core-0 main-loop pump. 0 = that pump is DISABLED.
// Goal: find which background activity corrupts SD-backed disk reads.
//   Step 1: build with ALL 0 -> disk path only. Rerun disktest; expect CLEAN.
//   Step 2: flip exactly ONE flag to 1, rebuild+flash, rerun disktest.
//           The flag whose re-enable brings the mismatches back = the culprit.
// Restore ALL to 1 for normal operation when the hunt is done.
// ============================================================================
#define ISO_AUDIO      1   // pm_audio_domix()       (I2S audio mixer)
#define ISO_SCREENPAD  1   // screenpad_menu_poll()  (I2C: OLED + keypad) + LED
#define ISO_WIFI       1   // wm_wifi_poll()         (cyw43 link maintenance)
#define ISO_NET        1   // ne2000/W5500 RX poll + NE2000 TX FIFO
#define ISO_DBGDUMP    1   // [wm_dma]/[gus_dma] console prints (printf on Core 0)

// Set true when the W5500 is opened at Pico boot (main_core0), so the
// CMD_DEV_Init handler doesn't re-open it. Boot-time init runs BEFORE the guest
// BIOS POST -> the Network banner / menu MAC are populated in time.
static bool g_w5500_boot_inited = false;

// Same idea for NET_USB: set when the CDC-ECM adapter enumerated during the
// boot-time wait (after PM_StartUSB) and the NE2000 was installed there.
static bool g_usbeth_boot_inited = false;

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
#endif //USE_AUDIO

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
#if BOARD_WM10 && (USE_SBDSP || USE_GUS)
      // WonderMCA sound card: SB DSP and GUS are MUTUALLY EXCLUSIVE -- they share
      // the 0x220 base + the same MCA POS geometry, and the bus handler routes
      // the shared block to whichever is on via the dev_gus.active flag. Pick
      // per the config; POS[3] must carry a valid geometry (0 until the chassis
      // configures the card). POS[3] (@DEAD.ADF): bits2:0=I/O base(010=0x220..
      // 110=0x260), bits4:3=IRQ(00=2,01=3,10=5,11=7), bits6:5=DMA(00=0,01=1,10=3).
      {
       uint8_t  p3     = pos_regs[POS_REG_OPTION3];
       uint8_t  iobits = p3 & 0x07;
       uint8_t  dmasel = (p3 >> 5) & 0x03;
       bool     pos_valid = (iobits >= 2 && iobits <= 6) && (dmasel != 3);
       uint16_t sndport = 0x200 + ((uint16_t)iobits << 4);   // 0x220..0x260
       static const uint8_t irqmap[4] = {2,3,5,7};
       uint8_t  sndirq  = irqmap[(p3 >> 3) & 0x03];
       uint8_t  snddma  = (dmasel == 2) ? 3 : dmasel;        // 0,1,3
       // Only IRQ 3/5/6/7 are wired to an MCA IRQ pin (mca_irq.c mca_irq_pin).
       // POS[3] bits4:3=00 decode to IRQ 2 (MCA cascade) which CANNOT be
       // delivered -> the card raises nothing and the DOS driver hangs waiting
       // for its interrupt. Warn loudly so it's obvious in the console.
       if (sndirq != 3 && sndirq != 5 && sndirq != 7)
           PM_ERROR("   - net/audio: POS IRQ %d is NOT MCA-deliverable (use 3/5/7 in @DEAD.ADF)\n", sndirq);
#if USE_GUS
       if (PM_Config->GUSEnable && pos_valid)
         {
          PM_INFO(">GUS On from POS[3]=%02x port=%x irq=%d dma=%d (SB off)\n",
                  p3, sndport, sndirq, snddma);
          PM_sbdsp_OnOff(0);                      // SB off (exclusive)
          if (!dev_gus_install(sndport))          // sets dev_gus.active, claims the shared block
              dev_gus_set_irq_dma(sndirq, snddma);
         }
       else
#endif
#if USE_SBDSP
       if (PM_Config->SBEnable && pos_valid)
         {
          PM_INFO(">SB On from POS[3]=%02x port=%x irq=%d dma=%d (GUS off)\n",
                  p3, sndport, sndirq, snddma);
#if USE_GUS
          dev_gus_remove();                       // GUS off (exclusive) -> dev_gus.active=false
#endif
          if (!PM_sbdsp_OnOff(sndport))
              dev_sbdsp_set_irq_dma(sndirq, snddma);
         }
       else
#endif
         {
          PM_INFO(">SB/GUS off (GUSEn=%d SBEn=%d POS[3]=%02x valid=%d)\n",
                  (int)PM_Config->GUSEnable, (int)PM_Config->SBEnable, p3, (int)pos_valid);
#if USE_GUS
          dev_gus_remove();
#endif
#if USE_SBDSP
          PM_sbdsp_OnOff(0);
#endif
         }
      }
#else  // non-WM10: original defaults
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

#if BOARD_PMJR
// The prebuilt option ROM's MEM_GetMaxConv stops counting at the first non-RAM block,
// which on a PCjr is video page 7 (112-128KB). PM_FinalConfig therefore writes 112 to
// 0:0413 and DOS 5 aborts / jrconfig refuses to load. The card cannot write 0x413 itself
// (it deliberately does not decode 00000-1FFFF), so ask the 8088 to do it.
// Called from CMD_Int13h: POST is over by then, so PM_FinalConfig cannot overwrite it.
// Cleared on every CMD_MEM_Init: the Pico keeps running across a host warm boot, so the
// re-stamping has to start over each time the host POSTs.
static uint16_t PM_JR_MemSizeCalls=0;

// Re-applied on every CMD_Int13h during boot, not just once: something between the boot
// sector read and CONFIG.SYS processing puts the POST value back, and jrconfig reads
// 0:0415 for its 256KB minimum check right as it loads.
static void PM_JR_FixMemSize()
{
  if (PM_JR_MemSizeCalls>400) return;   // DOS is up well before this; stop paying the round-trip cost
  PM_JR_MemSizeCalls++;
  uint16_t calls=PM_JR_MemSizeCalls;

  if (PM_JR_MemSizeCalls==1)
   {
    display_memmap();
    display_memindex();
   }
  
  uint16_t kb=128;   // motherboard RAM, always present and never decoded by the card
  for (int i=8;i<46;i++)   // stop at block 46 (B8000h), the CGA window
     {
      uint8_t t=PM_Config->MEM_Map[i];
      if ((t!=MEM_RAM)&&(t!=MEM_PSRAM)) break;
      kb+=16;
     }
  if (kb>640) kb=640;   // above 640K is jrconfig's job (-l), same rule as PM_FinalConfig

  uint16_t a13=PC_MR16(0x413);
  uint16_t a15=PC_MR16(0x415);
  bool wrote=false;

  // jrconfig reboots via INT 19h whenever 0:0413 <= 255, and reads 0:0415 for its 256KB
  // gate. Keep both correct for the whole boot or it loops: 80 column, restart, 40 column.
  if (kb>a13) { PC_MW16(0x413,kb); wrote=true; }
  if (kb>a15) { PC_MW16(0x415,kb); wrote=true; }

  if (wrote || calls==1)
     PM_INFO("\nPCjr #%d: 0:0413=%dKB 0:0415=%dKB -> %dKB%s (now %d/%d)\n",
             calls,a13,a15,kb,wrote?" REWRITTEN":"",PC_MR16(0x413),PC_MR16(0x415));
}
#endif



// *************************************************************
// *****      PicoMEM Commands Run from the 2nd core       *****
// *************************************************************
void PM_ExecCMD()
{
//PM_INFO("CMD %X:%X %X",PM_Command,PM_CmdDataH,PM_CmdDataL);
//PM_INFO("C%X\n",PM_Command);
 g_c0_cmd = PM_Command;
 
 switch(PM_Command)
  {
   case CMD_Reset :         // CMD 0: Clean the Status or Enable/Disable the commands
    PM_INFO(">Reset\n");
    PM_CmdReset=false;      // Disable the Reset (It is completed)
    PM_Status=STAT_READY;
    break;
    
   case CMD_SetBasePort :   // CMD 2: Set the board base port !! To do !!
    PM_INFO(">SetBasePort\n");
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
#if BOARD_WM10
        PM_INFO("   - A:%x T:%x %x \n",PM_CmdDataL,PM_CmdDataH,Addr);
// WonderMCA's CPLD enforces a hard address decode at D0000-D5FFF
        // (blocks 0x34, 0x35).  Aliasing other blocks as PicoMEM RAM
        // makes Core 1 try to drive the bus for addresses the CPLD
        // doesn't claim -- which causes contention with whatever real
        // device sits there (video BIOS, MCA option ROMs, ...) AND
        // adds bus-cycle traffic the MEMW path can't keep up with at
        // 280-320 MHz.  Result: PMBIOS's MEM_Test_ROM scan freezes
        // randomly at whichever block the timing race first bites.
        //
        // Guard: ignore any CMD 6 for blocks outside our decoded range.
        // PMBIOS still gets its STAT_READY ack and continues the scan;
        // the host-side Test16KFast just sees the real bus state for
        // those addresses (no Pico aliasing), so it correctly classifies
        // them as ROM / RAM / empty without us perturbing the test.
        if (PM_CmdDataL != 0x34 && PM_CmdDataL != 0x35) {
            PM_Status = STAT_READY;
            break;
        }
#endif
        uint8_t MemType=GetMEMType(Addr);
        if (MemType==MEM_NULL || MemType==MEM_RAM)
         {
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
         } else {PM_INFO("MEM Type already set %x\n",GetMEMType(Addr));}
       }
    PM_Status=STAT_READY;
    break;

    case CMD_MEM_Init :       // CMD 7: Initialize the memory emulation based on the Config
// Started after the Setup / before the Boot
// RAM initialisation can't be done if the BIOS Does not work / is not enabled.
     PM_INFO(">MEM Init\n");

     dev_memory_install_all();

     PM_Status=STAT_READY;
    break;

   case CMD_TDY_Init :  // Initialize the Tandy conventionnal RAM emulation
    PM_INFO(">Tandy RAM : %dx16Kb\n",BV_TdyRAM);
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

    PM_INFO("\nMem Index: ");
    for (int i=0;i<MEM_T_Size;i++)
        { if (i==10*8) PM_INFO("- "); 
          PM_INFO("%d ",GetMEMIndex(i*8*1024));
        }
    PM_INFO("\n");
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

   case CMD_LoadCfg :        // 0x14: BIOS-triggered SD config reload.
     // PMBIOS sends this after CMD_Test_RAM, BEFORE its early-POST
     // checks (Tandy gate, IRQ detect, etc.) so the BV_* variables
     // that pm_LoadConfig populates from config.txt are correct by
     // the time those checks run. Avoids the race where chassis POST
     // runs faster than Core 0's main-loop SD init.
     PM_INFO(">CMD_LoadCfg: BIOS requested CFG reload\n");
     {
       uint8_t lc_rc = 0xFE;  // default: SD not available
#if USE_SDCARD
       if (PM_StartSD())
         {
           lc_rc = pm_LoadConfig();    // 0 = success
           BV_CFGInit = (lc_rc == 0) ? 0 : 0xFD;
           BIOS_SetupDisks();
         }
       else
         {
           BV_CFGInit = 0xFF;
           lc_rc = 0xFF;
         }
#else
       BV_CFGInit = INIT_DISABLED;
       lc_rc = INIT_DISABLED;
#endif
       PM_CmdDataL = lc_rc;            // BIOS reads this to learn status (0 = OK)
     }
     PM_Status = STAT_READY;
     break;

   case CMD_PCCDebug :       // 0xFD: read PCCR_PCSTATE & PMSTATE from DPRAM,
                             //   return in DataL & DataH.  Lets a DOS test
                             //   verify Pico Core 0 sees what the host just
                             //   wrote (cross-core SRAM coherence check).
     PM_CmdDataL = PCCR_PCSTATE;
     PM_CmdDataH = PCCR_PMSTATE;
     PM_Status   = STAT_READY;
     break;

   case CMD_TESTIO :       // Test Data transfer via IO
     PM_INFO("Test IO Transfer\n");

     test_pmio_transfer();

     PM_Status=STAT_READY;
     break;

   case CMD_SaveCfg :       // CMD: Save the Configuration to the uSD
     PM_CmdDataL=0;
     PM_INFO(">save Config to uSD\n");
 
     if (PM_EnableSD())
      { 
        dev_audiomix_pause();     // Need to pause audio during uSD Access         
        uint8_t SC_return=pm_SaveConfig();
        dev_audiomix_resume();         
        if (SC_return!=0)
           {
            PM_PICO_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation     
            PM_ERROR("   - Save Config Error %d\n",SC_return);
     
            PM_CmdDataL=SC_return;   // Return an error code          
            PM_Status=STAT_CMDERROR;         
            break;
           }
      }
     PM_PICO_EnablePSRAM();      // Re enable the PSRAM for the Memory emulation
     PM_Status=STAT_READY;
    break;

   case CMD_DEV_PreInit :       // CMD: Initialize the devices at the first init phase (RTC)
     PM_INFO(">CMD_DEV_PreInit\n");
     // Add code to initialize devices at the first init phase

#if USE_RTC
     PM_RTC_OnOff(PM_Config->UseRTC);
#endif
     PC_End();     // Tell the PC Command loop to end
     PM_Status = STAT_READY;
     break;

   // Initialize the devices back to their Default state (Command sent by the BIOS just before the Boot)
   case CMD_DEV_Init :

    PM_INFO(">CMD_DEV_Init\n");

    // PMBIOS sends this at every boot (warm included): quiesce state left
    // over from the previous OS session. Stale scancodes in the DPRAM ring
    // would abort the boot countdown; stale sermouse RX/IER would hold IRQ3.
#if BOARD_WM10
    pm_keyboard_reset();            // typematic + pending scancode ring
#endif    
#if USE_SERMOUSE
    dev_sermouse_quiesce();         // mouse silent until a driver re-opens DTR
#endif
#if USE_CDROM
    // A CD floppy-emulation boot overlay must not survive into the next
    // session: give drive 00h back before the disk re-init rewrites the
    // attribute table.
    INT13_CDFDD_Disarm();
#endif
#if BOARD_WM10 && USE_NE2000
    // A started DP8390 + live backend RX would re-raise its IRQ on every
    // arriving frame all through POST (hot-reboot banner hang once USB
    // Ethernet RX actually flowed). Stop the card until DOS reconfigures.
    dev_ne2000_quiesce();
#if USE_NE2000_ASIX
    // The dongle keeps its state across PC reboots (never re-enumerated);
    // a desynced TX parser inherited from the last session silently eats
    // every transmitted frame. Reset it to a known-good state each boot.
    usbeth_asix_reinit();
#endif
#endif

#if USE_DEV_IRQ  // This device display the Port 21h out values for debug
     dev_irq_install();    // For DEBUG ! Display p21h
#endif

     bus_irq_init(); // Init the PM IRQ pin variable
     
#if !BOARD_WM10
#if !USE_HWDMA
     // ISA 8237 DMA controller probe/capture. MCA cards have no ISA DMA
     // controller at port 0x00, so this probe hangs in PC_Wait_CMD_End.
     // Skipped when USE_HWDMA=0 (e.g. BOARD_WM10).

     dev_dma_install();
     bool dma_ok;
     dma_ok=true;
     // Add code to test if DMA can be interrupted
     PM_INFO("DMA register capture test: ");
     PC_SetDMARegs(1,0xFFFF,0xFFFF,0);
     if ((dmachregs[1].baseaddr!=0xFFFF)|(dmachregs[1].baseaddr!=0xFFFF)) dma_ok=false;
     PC_SetDMARegs(1,0,0,0);
     if ((dmachregs[1].baseaddr!=0)|(dmachregs[1].baseaddr!=0)) dma_ok=false;

     if (!dma_ok)
        {
         PM_ERROR("Ko > Disable !\n");
         dev_dma_remove();
        } else PM_INFO("Ok\n");

#endif // !USE_HWDMA
     isa_dma_init();       // Was installed at the Pico Startup
#endif // !BOARD_WM10

     PM_INFO("> disable Mouse\n");
     MOUSE_Enabled=false;  // Mouse is disabled by default (Enabled by PMMOUSE)

// add SD and PSRAM Speed config here (Still not used by the SD and PSRAM driver, but can be used in the future)
     PM_INFO("> set SD: %d PSRAM %d (MHz)\n",PM_Config->SD_Speed,PM_Config->RAM_Speed);

#if USE_USBHOST
     PM_Joystick_OnOff(PM_Config->EnableJOY);
#endif

#if PM_WIFI
#if USE_NE2000 
      DelPortType(DEV_NE2000);                            // Remove the ne2000 Ports
      if ((PM_Config->ne2000Port!=0)&&(PM_Config->ne2000Port<0x3E0))
        {
         dev_ne2000_install();
         PM_INFO("[NE2K] Configuring NE2000 at Port %x IRQ %d\n", PM_Config->ne2000Port, PM_Config->ne2000IRQ);
         dev_ne2000_cfg(PM_Config->ne2000Port,PM_Config->ne2000IRQ); // Set the ne2000 Port and IRQ
         PM_INFO("[NE2K] SetPortType at Port %x\n", PM_Config->ne2000Port);
         SetPortType(PM_Config->ne2000Port,DEV_NE2000,4); // Add the ne2000 Ports
        }
        else PM_Config->ne2000Port=0;  // Force 0 if > 3E0

       PM_INFO(">NE2000 Config: P %x I %d\n",PM_Config->ne2000Port,PM_Config->ne2000IRQ);
#endif //USE_NE2000
#endif

#if BOARD_WM10 && USE_SERMOUSE
      // USB Mouse switch (BIOS "Other" page): the COM2 serial-mouse UART
      // decode follows PMCFG on every guest boot. OFF -> 0x2F8 undecoded
      // (open bus: guests find no UART; mouse.drv's scratch test fails and
      // it moves on). The BIOS side independently gates the equipment-bit-2
      // clear on the same flag, so OFF also re-exposes a real PS/2 mouse
      // to Windows. The device stays initialized either way -- only the
      // port decode toggles.
      if (PM_Config->EnableUSBMouse) {
          SetPortType(0x2F8, DEV_SERMOUSE, 1);
          PM_INFO("[SERMOUSE] enabled (COM2 0x2F8)\n");
      } else {
          DelPortType(DEV_SERMOUSE);
          PM_INFO("[SERMOUSE] disabled in BIOS config\n");
      }
#endif

#if BOARD_WM10 && USE_NE2000
      // Network type from PMCFG (EnableWifi repurposed as NetType):
      //   NET_NONE -> no network
      //   NET_ETH  -> wired Ethernet via W5500 (NE2000). If the W5500 is not
      //               detected, leave the network down -- the BIOS "Network"
      //               line then shows "None".
      //   NET_WIFI -> WiFi via RM2/CYW43 (connect with wifi.txt creds).
      // BV_WifiInit reports the result to the BIOS:
      //   INIT_DONE = link up, INIT_SKIPPED = selected-but-failed (Ethernet
      //   not detected / WiFi not connected), INIT_DISABLED = None.
      {
        uint8_t nettype = PM_Config->EnableWifi;
        DelPortType(DEV_NE2000);                            // clean slate

        if (nettype == NET_ETH) {
#if USE_NE2000_W5500
            if (g_w5500_boot_inited) {
                // Already opened at Pico boot (before the BIOS banner/menu were
                // drawn). Don't re-init the W5500 -- but the "clean slate"
                // DelPortType above JUST WIPED the IO mapping, so re-register
                // it or port 0x300 floats FF (packet driver sees MAC FF:FF...).
                SetPortType(PM_Config->ne2000Port, DEV_NE2000, 4);
                BV_WifiInit = INIT_DONE;
                PM_INFO("[NET][W5500] Already init at boot\n");
            } else {
            // Power the RM2 WiFi module DOWN when using wired Ethernet. The
            // W5500 and RM2/CYW43 SHARE GPIO 11 as chip-select (and GPIO 13);
            // if the WiFi is live it pulses GPIO 11 and spuriously selects the
            // W5500 onto the shared SD/W5500 SPI0 bus -> SD corruption. Holding
            // WL_REG_ON (GPIO 13) low keeps the module off the bus, so you don't
            // have to physically unpower it. (cyw43_arch_init owns GPIO 13 in
            // the NET_WIFI path.) They are mutually exclusive by pin design.
            gpio_init(CYW43_DEFAULT_PIN_WL_REG_ON);
            gpio_set_dir(CYW43_DEFAULT_PIN_WL_REG_ON, GPIO_OUT);
            gpio_put(CYW43_DEFAULT_PIN_WL_REG_ON, 0);   // RM2 WiFi power OFF
            // W5500 backend deliberately does NOT call spi_set_baudrate, so
            // SD's existing SPI0 baud is preserved across W5500 reset/init.
            uint8_t w5500_mac[6];
            if (w5500_backend_open(w5500_mac)) {
                memcpy(PM_Wifi.cyw43_mac, w5500_mac, 6);
                PM_Config->ne2000Port = 0x300;             // classic NE2000 base / CPLD blk_300
                if (PM_Config->ne2000IRQ == 0) PM_Config->ne2000IRQ = 3;
                dev_ne2000_install();
                dev_ne2000_cfg(PM_Config->ne2000Port, PM_Config->ne2000IRQ);
                SetPortType(PM_Config->ne2000Port, DEV_NE2000, 4);
                BV_WifiInit = INIT_DONE;
                PM_INFO("[NET][W5500] Port:%x, Irq:%d, MAC:%02X:%02X:%02X:%02X:%02X:%02X\n",
                        PM_Config->ne2000Port, PM_Config->ne2000IRQ,
                        w5500_mac[0],w5500_mac[1],w5500_mac[2],
                        w5500_mac[3],w5500_mac[4],w5500_mac[5]);
            } else {
                BV_WifiInit = INIT_SKIPPED;                 // W5500 absent -> BIOS shows "None"
                PM_ERROR("[NET][W5500] Module not detected -> None\n");
            }
            }  // end else (not boot-inited)
#else
            BV_WifiInit = INIT_SKIPPED;
#endif
        } else if (nettype == NET_WIFI) {
#if USE_NE2000_CYW43
            // RM2/CYW43 WiFi: bring up the radio + read the STA MAC, then kick
            // off an async connect using wifi.txt (non-blocking; wm_wifi_poll()
            // reports the link result later via BV_WifiInit / PM_Wifi).
            // wres==1 only when cyw43_arch_init() itself failed (no MAC, no
            // radio). Any other result means the radio is up and PM_Wifi.cyw43_mac
            // is valid, so we install the NE2000 emulator and route its transport
            // over CYW43 (TX = wm_cyw43_send; RX = push via
            // cyw43_cb_process_ethernet -> ne2000_deliver_rx). The NE2000 then
            // presents a real MAC to DOS (pm2000 / packet driver) even before
            // association completes; frames only flow once the link is up.
            uint8_t wres = wm_wifi_connect_start();
            if (wres != 1) {
                ne2000_backend.send = wm_cyw43_send;   // NE2000 -> WiFi
                ne2000_backend.poll = NULL;            // RX is push (cb_process_ethernet)
                ne2000_backend.name = "cyw43";
                PM_Config->ne2000Port = 0x300;
                if (PM_Config->ne2000IRQ == 0) PM_Config->ne2000IRQ = 3;
                dev_ne2000_install();                  // copies PM_Wifi.cyw43_mac -> physaddr
                dev_ne2000_cfg(PM_Config->ne2000Port, PM_Config->ne2000IRQ);
                SetPortType(PM_Config->ne2000Port, DEV_NE2000, 4);
                BV_WifiInit = (wres == 0) ? INIT_INPROGRESS : INIT_SKIPPED;
                PM_INFO("[NET][WiFi] NE2000 over CYW43 Port:%x, Irq:%d, MAC:%02X:%02X:%02X:%02X:%02X:%02X (%s)\n",
                        PM_Config->ne2000Port, PM_Config->ne2000IRQ,
                        PM_Wifi.cyw43_mac[0],PM_Wifi.cyw43_mac[1],PM_Wifi.cyw43_mac[2],
                        PM_Wifi.cyw43_mac[3],PM_Wifi.cyw43_mac[4],PM_Wifi.cyw43_mac[5],
                        wres == 0 ? "connecting" : "no AP yet");
            } else {
                BV_WifiInit = INIT_SKIPPED;            // cyw43 init failed -> no radio
                PM_ERROR("[NET][WiFi] init failed (cyw43_arch_init)\n");
            }
#else
            BV_WifiInit = INIT_SKIPPED;                     // WiFi (CYW43) transport not compiled in
#endif
        } else if (nettype == NET_USB) {
#if USE_NE2000_CDCECM
            // USB Ethernet (CDC-ECM) dongle as the NE2000 transport. The
            // boot-time block (after PM_StartUSB) already waited for the
            // adapter and installed everything; this re-entry path also
            // covers a dongle plugged after boot (config re-apply).
            if (g_usbeth_boot_inited) {
                // Boot-time block already installed everything, but the
                // "clean slate" DelPortType above JUST WIPED the IO mapping
                // -- re-register it or port 0x300 floats FF (this was the
                // mTCP "MAC FF:FF:FF:FF:FF:FF" with a correct banner MAC).
                SetPortType(PM_Config->ne2000Port, DEV_NE2000, 4);
                BV_WifiInit = INIT_DONE;
                PM_INFO("[NET][USBETH] Already init at boot\n");
            } else if (usbeth_present()) {
                usbeth_get_mac(PM_Wifi.cyw43_mac);     // BEFORE install: ne2000_init copies it into the PROM
                usbeth_backend_attach();               // vtable -> usbeth (ends the smoke test)
                PM_Config->ne2000Port = 0x300;         // classic NE2000 base / CPLD blk_300
                if (PM_Config->ne2000IRQ == 0) PM_Config->ne2000IRQ = 3;
                dev_ne2000_install();
                dev_ne2000_cfg(PM_Config->ne2000Port, PM_Config->ne2000IRQ);
                SetPortType(PM_Config->ne2000Port, DEV_NE2000, 4);
                BV_WifiInit = INIT_DONE;
                snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr, "USB Ethernet");
                PM_INFO("[NET][USBETH] Port:%x, Irq:%d, MAC:%02X:%02X:%02X:%02X:%02X:%02X\n",
                        PM_Config->ne2000Port, PM_Config->ne2000IRQ,
                        PM_Wifi.cyw43_mac[0],PM_Wifi.cyw43_mac[1],PM_Wifi.cyw43_mac[2],
                        PM_Wifi.cyw43_mac[3],PM_Wifi.cyw43_mac[4],PM_Wifi.cyw43_mac[5]);
            } else {
                BV_WifiInit = INIT_SKIPPED;            // no adapter -> BIOS shows "None"
                snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr, "No USB adapter");
                PM_ERROR("[NET][USBETH] No CDC-ECM adapter detected -> None\n");
            }
#else
            BV_WifiInit = INIT_SKIPPED;                     // ECM driver not compiled in
#endif
        } else {
            BV_WifiInit = INIT_DISABLED;                    // NET_NONE
            PM_INFO("[NET] None\n");
        }
      }
#endif

#if USE_AUDIO
       PM_Audio_OnOff(PM_Config->AudioOut);
#endif

#if USE_RTC
       PM_RTC_OnOff(PM_Config->UseRTC);
#endif

#if USE_CDROM
       dev_cdr_install(PM_Config->MKE_Port);
       dev_cdr_mount_from_config();   // mount the configured image (async in dev_cdr_update)
#endif
      PM_INFO("[SYS] BV_IRQ : %d, BV_HWIRQ %X >",BV_IRQ,BV_HWIRQ);
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
     PM_INFO("[SYS] CMD_SendIRQ\n");
     /*
     #if USE_HWIRQ
     PM_INFO("IRQ Pin DIR 3:%d 5:%d 6:%d 7:%d \n", gpio_get_dir(PIN_IRQ3), gpio_get_dir(PIN_IRQ5), gpio_get_dir(PIN_IRQ6), gpio_get_dir(PIN_IRQ7));
     #else
     PM_INFO("PIN_PM_IRQ %d DIR %d\n", gpio_get(PIN_PM_IRQ), gpio_get_dir(PIN_PM_IRQ));
     #endif
     */
//     BV_IRQ=0;
     BV_HWIRQ=0;
#if BOARD_WM10
     // WM10 has no PIN_PM_IRQ wired to the bus -- the multiplexed
     // PicoMEM IRQ rides on a real MCA IRQ line. Pulse each available
     // MCA IRQ and STOP as soon as the host's PM_TestIntXX ISR writes
     // back to BV_IRQ (first responder wins).
     //
     // SERIAL MOUSE: IRQ3 (COM2) must be OWNED EXCLUSIVELY by the serial
     // mouse -- Windows' mouse driver replaces the IRQ3 vector, so the
     // PicoMEM sharing IRQ3 crashes Windows on mouse movement. We therefore
     // SKIP IRQ3 and probe 7 first (5=SBDSP, 6=floppy are also taken), then
     // 6/5 as fallback. IRQ7's LPT1 spurious-IRQ quirk is tolerable for the
     // PicoMEM's occasional async signalling. (2026-07-14)
     // NOTE: NE2000 must also be moved off IRQ3 (config ne2000IRQ != 3).
     // Pulses use mca_irq_raise/lower so the inverted-polarity output
     // stage is handled correctly.
     mca_irq_lower(7); busy_wait_us(5);
     mca_irq_raise(7, false);
     busy_wait_ms(2);
     mca_irq_lower(7);
     busy_wait_ms(2);

     if (BV_IRQ == 0) {
         mca_irq_lower(6); busy_wait_us(5);
         mca_irq_raise(6, false);
         busy_wait_ms(2);
         mca_irq_lower(6);
         busy_wait_ms(2);
     }

     if (BV_IRQ == 0) {
         mca_irq_lower(5); busy_wait_us(5);
         mca_irq_raise(5, false);
         busy_wait_ms(2);
         mca_irq_lower(5);
     }
#elif USE_HWIRQ
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
     busy_wait_ms(2);        // Wait a bit before the next IRQ   
#else
#if !UART_PIN0
     gpio_put(PIN_PM_IRQ,1);   // IRQ Down (Force if the IRQ was not acknowledge (IRQ masked))
     busy_wait_us(5);
     gpio_put(PIN_PM_IRQ,0);   // Move the needed IRQ Up
     busy_wait_ms(2);          // Keep the IRQ high for 2ms
     gpio_put(PIN_PM_IRQ,1);   // Put back the IRQ Down
#endif
#endif
     busy_wait_ms(20);       // Wait a little more at the end
     uint8_t Last_IRQ;
     Last_IRQ = 0;
     PM_INFO("[SYS][IRQ] HWIRQ list (%x): ", BV_HWIRQ);     
     for (int i = 0; i <= 7; i++) {
        if (BV_HWIRQ & (1 << i)) {
            PM_INFO("%d ", i);
            Last_IRQ = i;
           }
       }
     if (BV_IRQ==0) {
          PM_INFO("[SYS][IRQ] BV_IRQ=0, use Last_IRQ=%d\n", Last_IRQ);
          BV_IRQ = Last_IRQ;
         }
     if ((BV_HWIRQ & (1 << BV_IRQ))==0) {
          PM_WARNING("[SYS][IRQ] BV_IRQ=%d not active in BV_HWIRQ, use Last_IRQ=%d\n", BV_IRQ, Last_IRQ);
          BV_IRQ = Last_IRQ;  // Fallback to the last active IRQ
         }
     PM_INFO("\n");
     PM_INFO("[SYS][IRQ] PicoMEM IRQ: %d \n",BV_IRQ);
     PM_Status=STAT_READY;
     break;

   case CMD_IRQAck :       // CMD: Acknowledge IRQ
     PM_INFO("[SYS][IRQ] CMD_IRQAck\n");
     SVAR_IRQ->PM_IRR=0;
     PM_Status=STAT_READY;
     break;

// *** USB / HID Commands (Human Interface Devices) ***

   case CMD_USB_Enable :    // CMD: 0x50

      PM_INFO("[USB] Enable USB\n");
   
      PM_CmdDataL=PM_StartUSB();    
      PM_Status=STAT_READY;
     break;

   case CMD_USB_Disable :     // CMD: 0x51

      PM_INFO("[USB] Disable USB\n");
   
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
           PM_INFO("[WIFI] Retry connection\n");
           PM_RetryWifi();
          }
       }

      memcpy((void *)&PCCR_Param,&PM_Wifi,sizeof(PM_Wifi));    
      if (PM_Config->ne2000Port==0) PM_INFO("[NE2K] Port %x IRQ %d",PM_Config->ne2000Port,PM_Config->ne2000IRQ);
        else PM_INFO("[NE2K] Port %x IRQ %d",PM_Config->ne2000Port,PM_Config->ne2000IRQ);
      PM_INFO("[WIFI] SSID %s \n",PM_Wifi.WIFI_SSID);

#endif //USE_NE2000
#endif //PM_WIFI
#if BOARD_WM10 && USE_NE2000_CDCECM
      // NET_USB: refresh the live ECM state (before the PCCR_Param copy
      // below) so the network page's Refresh shows plug/unplug + link
      // changes without a reboot.
      if (PM_Config->EnableWifi == NET_USB) {
          if (!usbeth_present())
              snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr, "No USB adapter");
          else
              snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr, "USB Ethernet link %s",
                       usbeth_link_up() ? "up" : "down");
      }
#endif
#if BOARD_WM10 && USE_NE2000_W5500
      // WM10: PM_WIFI=0 (RM2/CYW43 wifi runs in picomem.cpp, not the NE2000
      // stack), but PM_Wifi IS populated -- SSID at connect, MAC + DHCP lease by
      // the snoop. Copy it to PCCR_Param so the BIOS network page shows the live
      // SSID/MAC/signal; without this the BIOS reads stale PCCR_Param (the USB
      // device-count string -> "B: 0 devices" in the SSID field).
      memcpy((void *)&PCCR_Param,&PM_Wifi,sizeof(PM_Wifi));
#endif
      PM_Status=STAT_READY;
     break;

   case CMD_RTC_Get	:   // CMD: 0xA0  Read PCF85263 -> 7 bytes at DPRAM[RTC_BUF_OFFS]
#if USE_RTC
      if (rtc_i2c_read_buf((uint8_t *)&PM_DP_RAM[RTC_BUF_OFFS]))
         PM_Status=STAT_READY;
      else
         PM_Status=STAT_CMDERROR;
#else
      PM_Status=STAT_CMDERROR;
#endif
      break;

   case CMD_RTC_Set	:   // CMD: 0xA1  Write 7 bytes from DPRAM[RTC_BUF_OFFS] -> PCF85263
#if USE_RTC
      if (rtc_i2c_write_buf((const uint8_t *)&PM_DP_RAM[RTC_BUF_OFFS]))
         PM_Status=STAT_READY;
      else
         PM_Status=STAT_CMDERROR;
#else
      PM_Status=STAT_CMDERROR;
#endif
      break;

   case CMD_Audio_Status :  // CMD: 0x7D  Fill the audio status buffer (37 bytes)
     {
#if BOARD_WM10      
      uint8_t *ab = (uint8_t *)&PM_DP_RAM[AUDIO_STATUS_OFFS];
      // SB/GUS share the MCA POS geometry (same decode as PM_Audio_OnOff).
      uint8_t  p3     = pos_regs[POS_REG_OPTION3];
      uint8_t  iobits = p3 & 0x07;
      static const uint8_t irqmap[4] = {2,3,5,7};
      uint8_t  snd_irq = irqmap[(p3 >> 3) & 0x03];
      uint8_t  dmasel  = (p3 >> 5) & 0x03;
      uint8_t  snd_dma = (dmasel == 2) ? 3 : dmasel;    // 0,1,3 (3=invalid)
      uint8_t *e;

      ab[0] = PM_Config->AudioOut;                      // global master on/off
      ab[1] = (uint8_t)dev_audiomix.dev_active;         // live mix bitmask
      e = &ab[2];
      #define AST_PUT(inst,port,irq,dma) do { \
          e[0]=(uint8_t)(inst); e[1]=(uint8_t)((port)&0xFF); e[2]=(uint8_t)((port)>>8); \
          e[3]=(uint8_t)(irq); e[4]=(uint8_t)(dma); e+=5; } while (0)
      AST_PUT(dev_adlib.active,      0x388,               0xFF,    0xFF);      // Adlib
      AST_PUT(dev_sbdsp_active,      dev_sbdsp_baseport,  sbdsp.irq, sbdsp.dma8); // SoundBlaster
#if USE_GUS
      AST_PUT(dev_gus.active,        dev_gus.baseport,    snd_irq, snd_dma);   // GUS
#else
      AST_PUT(0,                     0,                   0xFF,    0xFF);
#endif
      AST_PUT(dev_cms.active,        dev_cms.baseport,    0xFF,    0xFF);      // CMS
      AST_PUT(dev_tdy.active,        dev_tdy.baseport,    0xFF,    0xFF);      // Tandy
      AST_PUT(dev_cvx.active,        dev_cvx.baseport,    0xFF,    0xFF);      // Covox
      AST_PUT(dev_mmb.active,        dev_mmb.baseport,    0xFF,    0xFF);      // Mindscape
      #undef AST_PUT
      (void)iobits;
#endif      
      PM_Status = STAT_READY;
     }
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
             PM_INFO("[DSK][FDD] %s\n",str_tmp);             
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
             PM_INFO("[DSK][HDD] %s\n",str_tmp);
             str_tmp[1]=0xFE;          // Add the special character
             usbnb=strlen(str_tmp);
             str_tmp+=usbnb+1;         // Move pointer at the end of the current string
             str_disklist[0]++;
            }
           }
          }
#if USE_CDROM
      if (dev_cdr_installed() && PM_Config->CDR.name[0])
         {
          // Generate the CD-ROM info string (config disc; loaded async by dev_cdr_update)
          sprintf(str_tmp,"   CD   : %-13s  [%4dMB]",PM_Config->CDR.name,PM_Config->CDR.size);
          PM_INFO("[DSK][CDR] %s\n",str_tmp);
          str_tmp[1]=0xFE;          // Add the special character
          usbnb=strlen(str_tmp);
          str_tmp+=usbnb+1;         // Move pointer at the end of the current string
          str_disklist[0]++;
         }
#endif
      PM_Status=STAT_READY;
     break;

// *** Audio Commands  0x8x ***

   case CMD_AudioOnOff:     // Full audio rendering 0: Off 1:On
      PM_Audio_OnOff(PM_CmdDataL);
      PM_Status=STAT_READY;      
     break;

   case CMD_AdlibOnOff:      // Adlib Audio : 0 : Off 1: On default
      arg=(uint16_t)PM_CmdDataL;   
      PM_INFO("[SND] Adlib OnOff: %x\n",arg);
      if (arg==0xFF)  // Arg=0xFF means return the install status
         {
          PM_CmdDataL=dev_adlib_installed();  // Return the install status
          PM_CmdDataH=0;
          PM_Status=STAT_READY;
          break;
         }
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
      PM_INFO("[SND] TDY OnOff: %x\n",arg);
      if (arg==0xFF)  // Arg=0xFF means return the install status
         {
          PM_CmdDataL=dev_tdy_installed();  // Return the install status
          PM_CmdDataH=0;
          PM_Status=STAT_READY;
          break;
         }      
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
      PM_INFO("[SND] CMS OnOff: %x\n",arg);     
      if (arg==0xFF)  // Arg=0xFF means return the install status
         {
          PM_CmdDataL=dev_cms_installed();  // Return the install status
          PM_CmdDataH=0;
          PM_Status=STAT_READY;
          break;
         }        
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
      PM_INFO("[SND] MMB OnOff: %x\n",arg);
      if (arg==0xFF)  // Arg=0xFF means return the install status
         {
          PM_CmdDataL=dev_mmb_installed();  // Return the install status
          PM_CmdDataH=0;
          PM_Status=STAT_READY;
          break;
         }       
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

   case CMD_SetSBIRQDMA:        // SB: Set SB IRQ and DMA number
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
      PM_INFO("[SND] SB DSP OnOff: %x (%x %X)\n",arg,PM_CmdDataH,PM_CmdDataL);
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
      PM_CmdDataL=CMDERR_DEV_NA;  // Error, SB not supported
      PM_CmdDataH=0;
    #endif
      PM_Status=STAT_READY;
     break;

   case CMD_SetGUSIRQDMA:     // Gravis Audio   : 0 : Off 1: On default or port
#if USE_GUS
      PM_INFO("[SND] Set GUS IRQ DMA: %x %x\n",PM_CmdDataH,PM_CmdDataL);
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
      PM_INFO("[SND] GUS OnOff: %x (%x %X)\n",arg,PM_CmdDataH,PM_CmdDataL);
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
      PM_CmdDataL=CMDERR_DEV_NA;  // Error, GUS not supported
      PM_CmdDataH=0;
#endif      
      PM_Status=STAT_READY;
      break;  

   case CMD_CVXOnOff:        // MMB Audio   : 0 : Off 1: On default or port, return install status
      arg=((uint16_t)(PM_CmdDataH&0x03)<<8)+(uint16_t)PM_CmdDataL;
      PM_INFO("[SND] CVX OnOff: %d %x\n",PM_CmdDataH>>2,arg);
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

   // MPU/General MIDI : 0 : Off 1: On default or port, return install status
   // Cmd_DataH=0xFF : Return the install status
   // Cmd_DataH=0xFE : Set the MPU Type 
   case CMD_MPU_CMD:
#if USE_MPU
      PM_INFO("MPU Command: %x %x\n",PM_CmdDataH,PM_CmdDataL);
      arg=((uint16_t)(PM_CmdDataH&0x03)<<8)+(uint16_t)PM_CmdDataL;
      if (PM_CmdDataH==0xFF)
          {
            PM_CmdDataL=dev_mpu_installed();  // Return the install status
            PM_CmdDataH=0;
            PM_Status=STAT_READY;
            break;
          }
      if (PM_CmdDataH==0xFE)  // Set MPU Type
          {
           PM_CmdDataL=dev_mpu_define_renderer(PM_CmdDataL);
           PM_CmdDataH=0;
           PM_Status=STAT_READY;
           break;
          }
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
#else
      PM_CmdDataL=CMDERR_DEV_NA;  // Error, MPU not supported
      PM_CmdDataH=0;
      PM_Status=STAT_READY;
      break;
#endif

// *** Disk Commands  0x8x ***
   case CMD_HDD_Getlist :  // CMD: Get the list of HDD images
#if CORE0_WATCHDOG
watchdog_disable();
#endif
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
#if CORE0_WATCHDOG
watchdog_enable(8000, true);
#endif
      break;

   case CMD_FDD_Getlist :  // CMD: Get the list of FDD images
#if CORE0_WATCHDOG
watchdog_disable();
#endif   
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
#if CORE0_WATCHDOG
watchdog_enable(8000, true);
#endif
     break;

   case CMD_CD_Getlist :  // CMD: list the CD images (0:/CDROM, *.iso/*.cue, folder browse)
#if CORE0_WATCHDOG
watchdog_disable();
#endif
      if (PM_CmdDataL==0)
         { // Get the list from the CDROM root
          pm_cd_InitSearchPath();
         }
        else
         { // Get the list, folder change
           PM_INFO("Load CD, Change DIR %d\n",PM_CmdDataH);
           pm_cd_UpdateSearchPath(PM_CmdDataH);
         }
      PM_CmdDataL=ListImages(3,&nb,(FSizeName_t *) &PCCR_Param);
      PM_PICO_EnablePSRAM();    // Re enable the PSRAM for the Memory emulation
      PM_Status=STAT_READY;
#if CORE0_WATCHDOG
      watchdog_enable(8000, true);
#endif         
     break;

   case CMD_CD_Mount :  // CMD: mount/eject the image in PM_Config->CDR (hot swap)
#if USE_CDROM
      dev_cdr_mount_from_config();
#endif
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
       PM_INFO("[DSK][HDD] Mount\n");

       for (int i=0;i<4;i++)
         {
           f_close(&HDDfile[i]);  // Close the previous HDD file
           // Preserve bit 6 (USB volume marker) across the reset.
           // SelectImage() set bit 6 when the user mounted from [USB];
           // wiping it here would lose USB-vs-SD info on the next save.
           uint8_t vol_bit = PM_Config->HDD_Attribute[i] & 0x40;
           PM_Config->HDD_Attribute[i] = i | vol_bit;
           if (imageDiskList[i+2]!=NULL)
           {
            delete imageDiskList[i+2];   // Delete the previous HDD Object
            imageDiskList[i+2]=NULL;
           }
        //Mount the disk image
           if ((PM_Config->HDD[i].size>>8)!=0xFE)
            {
            if (strcmp(PM_Config->HDD[i].name,""))  // Skip if disk name is empty
               { // Pick the volume prefix from the saved attribute
                const char *vol_prefix = vol_bit ? "1" : "0";
                sprintf(imgpath, "%s:/HDD/%s%s.img", vol_prefix, PM_Config->HDDPath[i], PM_Config->HDD[i].name);
                PM_INFO("[DSK][HDD] HDD%d: %s\n",i,imgpath);

                int8_t mountres=pm_mounthddimage(imgpath,i+1);    // Mount the disk image
                if (!mountres) PM_Config->HDD_Attribute[i] = 0x80 | vol_bit;  // mounted, keep vol bit
               }
            } else  // Not an image, but a Physical disk redirection
            {
             PM_Config->HDD_Attribute[i]=((uint8_t) PM_Config->HDD[i].size)&0x03;
             PM_INFO("[DSK][HDD] PC Disk nb: %x %x\n",(uint8_t) PM_Config->HDD[i].size,PM_Config->HDD_Attribute[i]);
            }
         }
      }
     
     New_DiskNB=0;  // count the total number of HDD (To update the BIOS)
     for (int i=0;i<4;i++)
      {
        if (PM_Config->HDD_Attribute[i]>=0x80) New_DiskNB++;      // Image Mounted
        if (PM_Config->HDD_Attribute[i]<PC_DiskNB) New_DiskNB++;  // Read existing Floppy        
        PM_INFO("[DSK][HDD] HDD:%s Attr:%x\n",PM_Config->HDD[i].name,PM_Config->HDD_Attribute[i]);
      }
     PM_INFO("[DSK][HDD] Total HDD:%d/%d\n",New_DiskNB,PC_DiskNB);

     PM_INFO2("[DSK][HDD] Mount End\n");
     PM_PICO_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation   
     PM_Status=STAT_READY;
     break;

    case CMD_FDD_Mount :  // CMD: Mount the floppy images    
     PM_INFO("FDD Mount\n");    

     if (PM_EnableSD())
      {
       char imgpath[64+30];
 
       for (int i=0;i<2;i++)
         {
           f_close(&FDDfile[i]);
           uint8_t vol_bit = PM_Config->FDD_Attribute[i] & 0x40;
           PM_Config->FDD_Attribute[i] = i | vol_bit;          
             if (imageDiskList[i]!=NULL) 
              {
               delete imageDiskList[i];
               imageDiskList[i]=NULL;
              }
           // Sanitize the stored image name: corrupt SD config can leave
           // non-printable / non-ASCII bytes in there which then leak into
           // every UI that displays the slot (BIOS menu, screenpad) AND
           // get spliced into the imgpath we try to open. Truncate at the
           // first byte that isn't a valid FAT short-name character.
           for (int k = 0; k < (int)sizeof PM_Config->FDD[i].name; k++) {
               unsigned char ch = (unsigned char) PM_Config->FDD[i].name[k];
               if (ch == 0) break;
               bool ok = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')
                      || (ch >= '0' && ch <= '9') || ch == '.' || ch == '_'
                      || ch == '-' || ch == '~' || ch == '!' || ch == '#'
                      || ch == '$' || ch == '%' || ch == '&' || ch == '('
                      || ch == ')' || ch == '@' || ch == '^' || ch == ' ';
               if (!ok) {
                   PM_INFO("[DSK][FDD] FDD%d: name has non-ASCII (0x%02X at %d) -- clearing\n", i, ch, k);
                   PM_Config->FDD[i].name[0] = '\0';
                   break;
               }
           }
           if ((PM_Config->FDD[i].size>>8)!=0xFE)
           {
            if (strcmp(PM_Config->FDD[i].name,""))
               {
                // Pick the volume prefix from the saved attribute
                const char *vol_prefix = vol_bit ? "1" : "0";
                sprintf(imgpath, "%s:/FLOPPY/%s%s.img", vol_prefix, PM_Config->FDDPath[i], PM_Config->FDD[i].name);
                PM_INFO("[DSK][FDD] FDD%d: %s\n",i,imgpath);
               
                int8_t mountres=pm_mountfddimage(imgpath,i);       // Mount the floppy image
                if (!mountres) {
                    PM_Config->FDD_Attribute[i] = 0x80 | vol_bit;  // mounted, keep vol bit
                } else {
                    // Mount failed (file missing, wrong size, corrupt header,
                    // etc.). Clear the slot so the BIOS menu / screenpad
                    // report it as empty and the user can pick a fresh image
                    // -- otherwise a stale bad name persists across reboots
                    // and re-fails every boot.
                    PM_INFO("[DSK][FDD] FDD%d: mount failed (rc=%d) -- clearing slot\n", i, mountres);
                    PM_Config->FDD[i].name[0] = '\0';
                    PM_Config->FDDPath[i][0] = '\0';
                    PM_Config->FDD[i].size = 0;
                    PM_Config->FDD_Attribute[i] = i | vol_bit;
                }
               } else PM_INFO("[DSK][FDD] FDD%d: None\n",i);
           } else  // Not an image, but a Physical disk redirection
           {
             PM_Config->FDD_Attribute[i]=((uint8_t) PM_Config->FDD[i].size)&0x03;
             PM_INFO("[DSK][FDD] PC Floppy nb: %x %x",(uint8_t) PM_Config->FDD[i].size,PM_Config->FDD_Attribute[i]);
           }
         }
      }

     PM_INFO("Floppy Mount End\n");

     if (PC_FloppyNB<2)   // To not reduce the Nb of floppy if >=2
       {
        New_FloppyNB=0;
         for (int i=0;i<2;i++)
          {
           if (PM_Config->FDD_Attribute[i]>=0x80) New_FloppyNB++;        // Image Mounted
           if (PM_Config->FDD_Attribute[i]<PC_FloppyNB) New_FloppyNB++;  // Read existing Floppy
          // PM_INFO(" - %s Attr:%x\n",PM_Config->FDD[i].name,PM_Config->FDD_Attribute[i]);
         }
       } else New_FloppyNB=PC_FloppyNB;
     PM_INFO("[DSK][FDD] Total FDD:%d/%d\n",New_FloppyNB,PC_FloppyNB);

     PM_PICO_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation
     PM_Status=STAT_READY;
     break;  

     case CMD_Image_Select :  // CMD: Select an image in the list (PM_CmdDataL: Image NB, PM_CmdDataH : 0-1 FDSD0-1 : 2-6 HDD0-3)
      PM_INFO("[DSK] CMD_Image_Select: %d %d\n",PM_CmdDataH,PM_CmdDataL);
      SelectImage(PM_CmdDataH, PM_CmdDataL, (FSizeName_t *) &PCCR_Param);
      PM_Status=STAT_READY;
     break;

   case CMD_HDD_NewImage  :   // CMD:  
     PM_INFO("[DSK] CMD_HDD_NewImage\n");
     if (PM_EnableSD()) 
      {
        uint8_t NI_return=NewHDDImage();   
        if (NI_return!=0)
           {
            PM_PICO_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation
            PM_INFO("[DSK] Error:%d\n",NI_return);

            PM_CmdDataL=NI_return;   // Return an error code          
            PM_Status=STAT_CMDERROR;         
            break;
           }
      }
      PM_PICO_EnablePSRAM();  // Re enable the PSRAM for the Memory emulation      
      PM_Status=STAT_READY;    
     break;
       
        case CMD_Int13h :  // Execute Int 13h, registers are passed via the BIOS Memory

#if BOARD_PMJR
          PM_JR_FixMemSize();
#endif        

          bios_int13h_execute();
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

     case CMD_BIOS_MENU:  // Legacy BIOS menu/submenu entry point
#if CORE0_WATCHDOG
watchdog_disable();
#endif      
        PM_INFO ("BIOS Menu %d/%d\n",PM_CmdDataH,PM_CmdDataL);

        pm_apps_biosmenu(PM_CmdDataH, PM_CmdDataL);

        PC_End();     // Tell the PC Command loop to end
#if CORE0_WATCHDOG
watchdog_enable(8000, true);
#endif        
        PM_Status=STAT_READY;
      break;

    case CMD_BIOS_DRAW_MENU:  // Draw a BIOS menu/submenu page by index
        // PM_CmdDataL page index, PM_CmdDataH reserved for future use
        PM_INFO ("BIOS Menu page %d\n",PM_CmdDataL);

        DrawPageById(PM_CmdDataL);

        PC_End();     // Tell the PC Command loop to end
        PM_Status=STAT_READY;
      break;

    case CMD_Int21h :         // Interrupt 21h "Spy"
      bios_int21h_execute();
      break;

    case CMD_Int21End :
        bios_int21h_end();
        break;

    case CMD_Int2Fh :         // Interrupt 2Fh "Spy"
       bios_int2Fh_execute();
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
     // Unknown CMD. Reverted to STAT_CMDNOTFOUND while bisecting the
     // 8556 late-boot hang -- earlier we had STAT_READY here to silence
     // host-side IO probes, but that may have masked a real CMD that
     // DOS expected a meaningful reply to. If the 8556 boots further
     // with this revert, the hang was caused by a probe getting a
     // false-success ack.
     PM_INFO("[CMD] Unknown COMMAND ! \n");
     PM_Status = STAT_CMDNOTFOUND;
     break;
  }

 // Legacy `PM_INFO("E ")` trailer removed (2026-06-05). Was providing
 // ~50us of inadvertent Core 0 pacing that the chassis polling loop
 // historically depended on. If boot regresses, instrument with
 // tstpcc Test 7 (TC_NOP vs TC_PRINTF runs) to confirm before
 // re-adding any pacing primitive.
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
// USB keyboard typematic repeat pump (pm_keyboard.c, C linkage).
extern "C" void pm_keyboard_tick(void);

// PS/2 aux mouse packet builder (dev_mouse.cpp, C++ linkage). Forward-declared
// here rather than including dev_mouse.h -- that header's pm_mouse_t struct
// collides with the old USB_Host_driver hid_dev.h definition already in scope.
void pm_mouse_ps2_accumulate(int dx, int dy, uint8_t jp_buttons);
void pm_mouse_ps2_tick(void);

// Serial-mouse UART worker + report pacing (dev_sermouse.cpp, C linkage).
extern "C" void dev_sermouse_task(void);

void main_core0 ()
{

#if BOARD_WM10
// WonderMCA: minimal Core 0 — just process commands immediately
PM_INFO("[BOOT][Core0] init\n");


wm_drain_fifo();

// SD card init and config loading
#if USE_SDCARD
PM_INFO("[DSK] Start uSD\n");
wm_drain_fifo();          // drain before slow SD mount
if (PM_StartSD())
   {
    wm_drain_fifo();      // drain after SD mount, before reading config
    if (pm_LoadConfig()==0) BV_CFGInit=0;
       else BV_CFGInit=0xFD;
    wm_drain_fifo();      // drain after config load (~644-byte SD read)
    BIOS_SetupDisks();
    wm_drain_fifo();      // drain after BIOS_SetupDisks (allocates imageDisks)
   } else BV_CFGInit=0xFF;

if ((BV_CFGInit!=0)||(PM_BIOSADDRESS==0))
   {
    PM_Config->EnableUSB=1;
   }
#else
BV_SDInit=INIT_DISABLED;
BV_CFGInit=INIT_DISABLED;
#endif

wm_drain_fifo();          // final drain before main loop entry

#if BOARD_WM10
// WM10: force PicoMEM bootstrap (no legacy Int19h for virtual disks)
PM_Config->PMBOOT=1;

// WM10: BV_Int13hCLI=1 keeps PMBIOS doing CLI during Int13h so the
// 2-instruction critical window between PM_SendCMD's `OUT 0x2A1,AX` (data
// word) and `OUT 0x2A0,AL` (CMD trigger) cannot be preempted by a music-
// engine timer IRQ. Without this, AdLib-heavy DOS programs intermittently
// see `PC_MemCopyW_512b: Timeout 20ms/100ms pcs=00 pms=00 cmd=00 src=1`
// because an IRQ fires mid-PM_SendCMD, the CMD byte never gets written,
// the Pico-side handler runs with stale data, and PCCMD round-trip fails.
//
// This was temporarily set to 0 during 8550Z bring-up debugging, but
// that chassis is now stable so the IRQ-block is the right default
// again. Tradeoff: brief audio glitch during sector loads -- acceptable.
BV_Int13hCLI=1;
#endif

// IMPORTANT: do NOT flip PM_Status to STAT_READY here. The screenpad
// init below contains a sleep_ms(800), and wm_dma_install can also
// take time -- if PMBIOS sees STAT_READY before Core 0 is parked in
// the FIFO loop, it will start sending CMD_Test_RAM, Core 1 will set
// STAT_CMDINPROGRESS, but Core 0 won't drain the FIFO for hundreds of
// ms, and PMBIOS's 65-ms WaitCMDEnd_Timeout will fire -> hang.
// PM_Status -> STAT_READY happens just before the for(;;) loop entry,
// after the screenpad / wm_dma init is done. PMBIOS spins harmlessly
// in POST_WaitInitLoop (no timeout) while STAT_INIT is held.
PM_Init_Completed = true;

// *** W5500 boot-time init (re-enabled 2026-07-13) ***
// Runs at Pico boot (after SD/PSRAM settle) but BEFORE the guest BIOS POST, so
// the W5500 MAC + BV_WifiInit are populated before PMBIOS draws the Network
// banner / boot menu. When this ran inside CMD_DEV_Init instead, the slow
// w5500_backend_open (PLL lock + reset + link check) exceeded PMBIOS's ~65 ms
// WaitCMDEnd timeout, so the BIOS rendered "Network: None" / MAC 00:00:00:00
// before the init finished. Its old disable reasons -- SD SPI0 contention
// (8556 wedge) and audio underrun -- are addressed by the WiFi power-down
// (frees the shared GPIO 11) + the SD signal-integrity fixes.
#if BOARD_WM10 && USE_NE2000_W5500
if (PM_Config->EnableWifi == NET_ETH) {           // only for wired Ethernet
    // RM2 WiFi shares GPIO 11 (CS) with the W5500 -- hold it powered down so it
    // can't spuriously select the W5500 during the probe.
    
    PM_INFO("[NET][W5500] Init\n");
    
    PM_INFO("[NET][W5500] Deactivate CYW43, WL_ON Low\n");
    gpio_init(CYW43_DEFAULT_PIN_WL_REG_ON);
    gpio_set_dir(CYW43_DEFAULT_PIN_WL_REG_ON, GPIO_OUT);
    gpio_put(CYW43_DEFAULT_PIN_WL_REG_ON, 0);
    
    uint8_t w5500_mac[6];
    if (w5500_backend_open(w5500_mac)) {
        memcpy(PM_Wifi.cyw43_mac, w5500_mac, 6);
        PM_Config->ne2000Port = 0x300;
        if (PM_Config->ne2000IRQ == 0) PM_Config->ne2000IRQ = 3;  // Classic NE2000 default
        DelPortType(DEV_NE2000);
        dev_ne2000_install();
        dev_ne2000_cfg(PM_Config->ne2000Port, PM_Config->ne2000IRQ);
        SetPortType(PM_Config->ne2000Port, DEV_NE2000, 4);
        BV_WifiInit = INIT_DONE;                   // BIOS banner -> "Ethernet"
        g_w5500_boot_inited = true;                // CMD_DEV_Init skips re-open
        PM_INFO("[NET][W5500] Port=%x, Irq=%d MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
                PM_Config->ne2000Port, PM_Config->ne2000IRQ,
                w5500_mac[0],w5500_mac[1],w5500_mac[2],
                w5500_mac[3],w5500_mac[4],w5500_mac[5]);
    } else {
        BV_WifiInit = INIT_SKIPPED;
        PM_ERROR("[NET][W5500] Module not detected, NE2000 disabled\n");
    }
}
#endif

// WiFi (RM2/CYW43) boot-time init -- mirror of the W5500 block so the STA MAC
// is known BEFORE the BIOS banner draws. cyw43_wifi_get_mac() is valid right
// after arch_init (before association), and wm_wifi_connect_start() also kicks
// off the async connect. It's idempotent (wm_wifi_up guard), so CMD_DEV_Init's
// later NET_WIFI call is a no-op re-entry that still installs the NE2000 shim.
#if BOARD_WM10 && USE_NE2000_CYW43
if (PM_Config->EnableWifi == NET_WIFI) {
    PM_INFO("[NET][WiFi] Init\n");
    uint8_t wres = wm_wifi_connect_start();     // arch_init + STA MAC + async connect
    if (wres != 1) {
        BV_WifiInit = INIT_INPROGRESS;          // radio up + MAC valid; associating
        PM_INFO("[NET][WiFi] Radio up, MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
                PM_Wifi.cyw43_mac[0],PM_Wifi.cyw43_mac[1],PM_Wifi.cyw43_mac[2],
                PM_Wifi.cyw43_mac[3],PM_Wifi.cyw43_mac[4],PM_Wifi.cyw43_mac[5]);
    } else {
        BV_WifiInit = INIT_SKIPPED;             // cyw43_arch_init failed
        PM_ERROR("[NET][WiFi] Init failed\n");
    }
}
#endif


// Screenpad bring-up: I2C1 init, OLED logo, host-callback wiring, then
// menu render. Re-enabled 2026-05-10 from clean baseline (commit
// 95caee0). The 800 ms logo splash delays Core 0; PMBIOS POST_WaitInitLoop
// tolerates it via the no-timeout STAT_INIT spin that runs until we
// flip to STAT_READY a few lines below.
#if BOARD_WM10
{
    PM_INFO("[I2C] Init i2c1\n");
    i2c_init(i2c1, 400 * 1000);                 // 400 kHz on i2c1 (fast mode -- SSD1306 OK)
    gpio_set_function(PIN_I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C1_SDA);
    gpio_pull_up(PIN_I2C1_SCL);
    screenpad_init();
    // Probe the on-card PCF85263 RTC once at boot and print its date/time. Runs
    // before the OLED logo DMA starts so there's no i2c1 bus contention. Buffer:
    // [0]sec [1]min [2]hour [3]day [4]wday [5]month (BCD), [6]=year-1980.
    if (rtc_i2c_detect()) {
        uint8_t rb[7];
        if (rtc_i2c_read_buf(rb)) {
            #define RTC_BCD(x) (((x) >> 4) * 10 + ((x) & 0x0F))
            PM_INFO("[I2C][RTC] PCF85263 (0x51): %04u-%02u-%02u %02u:%02u:%02u\n",
                    1980u + rb[6],
                    (unsigned)RTC_BCD(rb[5]),   // month
                    (unsigned)RTC_BCD(rb[3]),   // day
                    (unsigned)RTC_BCD(rb[2]),   // hour
                    (unsigned)RTC_BCD(rb[1]),   // minute
                    (unsigned)RTC_BCD(rb[0]));  // second
            #undef RTC_BCD
        } else {
            PM_ERROR("[I2C][RTC] PCF85263 found on 0x51 but read failed\n");
        }
    } else {
        PM_ERROR("[I2C][RTC] PCF85263 NOT found on i2c1 (0x51)\n");
    }
    screenpad_show_logo();
    pm_screenpad_host_init();   // wire the floppy mount/list callbacks
    sleep_ms(800);              // brief logo splash before the menu takes over
    screenpad_menu_init();      // paint the IDLE A:/B: status screen
}
#endif

// BISECT step A: wm_dma_install + set_capbuf re-enabled, but the
// wm_dma_consume_byte hook in handle_iow stays commented out. If POST
// 165 returns, the install/set_capbuf is the cause; if it stays gone,
// the hook is the cause.
PM_INFO("[DMA][SBDSP] Install\n");
wm_dma_install();
wm_dma_set_capbuf(&PM_DP_RAM[WMDMA_CAPBUF_OFFS], WMDMA_CAPBUF_SIZE);

// Uncomment to re-enable the 1 Hz heartbeat log for bring-up debugging.
//#define WM10_HB_LOG 1

// Adlib trace -- prints last reads/writes once per second when Adlib is active.
// Comment out to disable (default off after bring-up).
//#define WM10_ADLIB_TRACE 1

// NE2000 trace -- prints IOR/IOW deltas and W5500 backend state once
// per second whenever counters change. Useful for bringing up new
// drivers; noisy at runtime. Off by default.
//#define WM10_NE2K_TRACE 1

#ifdef WM10_HB_LOG
absolute_time_t wm_hb_next = make_timeout_time_ms(1000);
uint32_t wm_hb_count = 0;
uint32_t wm_cmd_count = 0;
#endif

#if defined(WM10_ADLIB_TRACE) && USE_AUDIO
absolute_time_t wm_adlib_next = make_timeout_time_ms(1000);
#endif

#if defined(WM10_NE2K_TRACE) && BOARD_WM10 && USE_NE2000_W5500
// NE2000 trace state
absolute_time_t wm_ne2k_next = make_timeout_time_ms(1000);
uint32_t last_ne2k_raise = 0, last_ne2k_lower = 0;
uint32_t last_ne2k_isr   = 0, last_ne2k_imr   = 0;
uint32_t last_ne2k_iow   = 0, last_ne2k_ior   = 0;
#endif

// USB Host start (WM10) -- mirrors the non-WM10 init at line ~2659.
// Without this call, tusb_init() / hid_init() never run, USB_Host_Enabled
// stays false, the tuh_task() poll in the main loop is skipped, and no
// USB drives ever appear in the BIOS image-list page.
PM_INFO("[USB] Init\n");
#if USE_USBHOST
// The "ep was already available" USB-host panic on WM10 was root-caused to
// core over-voltage (1.30 V at 280 MHz): under combined Core0+Core1 load the
// supply transients disturbed the USB PHY. Fixed by dropping VREG to 1.15 V
// (+ safe flash timing clkdiv=4/rxdelay=3) in wm_board_init(). USB host now
// follows the BIOS/PICOMEM.CFG EnableUSB setting normally.
if ((PM_Config->EnableUSB) == 1) {
    PM_StartUSB();
} else {
    BV_USBInit = INIT_DISABLED;
    PM_INFO("[USB] USB Host disabled in BIOS\r\n");
}
#else
BV_USBInit = INIT_DISABLED;
PM_INFO("[USB] USB Host disabled in MakeFile\r\n");
#endif

// NET_USB boot-time init -- mirror of the W5500/WiFi boot blocks (~line 2400)
// but it must live HERE, after PM_StartUSB: enumeration only advances while
// tuh_task() is pumped, so we pump it ourselves with a bounded wait. On
// success the NE2000 is installed with the dongle MAC before the BIOS banner
// draws (PMBIOS is still spinning on STAT_INIT, no timeout). The wait is only
// paid when NET_USB is selected; no-dongle worst case = the full timeout.
#if BOARD_WM10 && USE_NE2000_CDCECM
if (PM_Config->EnableWifi == NET_USB) {
    // Neither the W5500 nor the RM2 radio is used -- hold the RM2 powered
    // down (WL_REG_ON low) so it can't glitch shared GPIO 11/SPI0 (same
    // hygiene as the NET_ETH path).
    gpio_init(CYW43_DEFAULT_PIN_WL_REG_ON);
    gpio_set_dir(CYW43_DEFAULT_PIN_WL_REG_ON, GPIO_OUT);
    gpio_put(CYW43_DEFAULT_PIN_WL_REG_ON, 0);

    if (BV_USBInit == INIT_DISABLED) {
        BV_WifiInit = INIT_SKIPPED;
        snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr, "USB host disabled");
        PM_ERROR("[NET][USBETH] NET_USB selected but USB host is off\n");
    } else {
        PM_INFO("[NET][USBETH] Waiting for USB Ethernet adapter...\n");
        // 5 s: ASIX enumeration + its 250 ms reset chain can brush past 3 s;
        // an expired wait = NE2000 never installs = port 0x300 floats FF
        // (seen as packet-driver MAC FF:FF:FF:FF:FF:FF).
        absolute_time_t deadline = make_timeout_time_ms(5000);
        while (!usbeth_present() && !time_reached(deadline)) tuh_task();

        if (usbeth_present()) {
            usbeth_get_mac(PM_Wifi.cyw43_mac);     // BEFORE install: ne2000_init copies it into the PROM
            usbeth_backend_attach();               // vtable -> usbeth (ends the smoke test)
            PM_Config->ne2000Port = 0x300;
            if (PM_Config->ne2000IRQ == 0) PM_Config->ne2000IRQ = 3;
            DelPortType(DEV_NE2000);
            dev_ne2000_install();
            dev_ne2000_cfg(PM_Config->ne2000Port, PM_Config->ne2000IRQ);
            SetPortType(PM_Config->ne2000Port, DEV_NE2000, 4);
            BV_WifiInit = INIT_DONE;               // BIOS banner -> "USB Eth" + MAC
            g_usbeth_boot_inited = true;           // CMD_DEV_Init skips re-init
            snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr, "USB Ethernet");
            PM_INFO("[NET][USBETH] Port=%x, Irq=%d MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
                    PM_Config->ne2000Port, PM_Config->ne2000IRQ,
                    PM_Wifi.cyw43_mac[0],PM_Wifi.cyw43_mac[1],PM_Wifi.cyw43_mac[2],
                    PM_Wifi.cyw43_mac[3],PM_Wifi.cyw43_mac[4],PM_Wifi.cyw43_mac[5]);
        } else {
            BV_WifiInit = INIT_SKIPPED;            // BIOS banner -> "None"
            snprintf(PM_Wifi.StatusStr, sizeof PM_Wifi.StatusStr, "No USB adapter");
            PM_ERROR("[NET][USBETH] No CDC-ECM adapter after 3 s -> None\n");
        }
    }
}
#endif

// NOTE: the boot-time USB chain test moved EARLY and standalone to
// wm_usb_early_selftest() in picomem.cpp (called ~line 879, before Core 1 /
// MCA / SD). pm_usb_chain_test() (pm_disk.cpp) is the shared test body.

// All Core 0 init done — flip PM_Status to STAT_READY so PMBIOS can
// start sending CMDs. PMBIOS POST_WaitInitLoop (which has been spinning
// on STAT_INIT, no timeout) will see READY on its next read and proceed.
// FIFO drain is now happening every iteration of the for(;;) below, so
// any CMD that comes in here will be processed within microseconds.
PM_INFO("[CMD][Core0] Init done\n");

#if BOARD_WM10
// End-of-init CHRDY release: pulse RP_REL HIGH->LOW once to clear any CPLD
// CHRDY self-feedback latch that may have stuck during the long Core 0 init
// (SD / config / USB), so the chassis bus is not left held. Mirrors the
// startup latch reset in wm_mca_rp2350_v10.h.
gpio_put(PIN_R_REL, 1);
sleep_us(1);
gpio_put(PIN_R_REL, 0);
#endif

PM_Status = STAT_READY;

#if BOARD_WM10
// DIAGNOSTIC: arm the Core 0 hang watchdog. 8 s is far above any normal
// main-loop section, so it only fires on a real hang. g_c0_bc_magic marks the
// breadcrumb as valid so a cold boot isn't misreported.
g_c0_bc_magic = C0_BC_MAGIC;
watchdog_enable(8000, true);
#endif

for (;;) {
    C0_STAGE(0); watchdog_update();   // loop top + feed the hang watchdog
    if (multicore_fifo_rvalid()) {
        switch(multicore_fifo_pop_blocking()) {
            case 0:
                C0_STAGE(1);          // CMD / disk (SD)
                g_c0_cmd = PM_Command; // record which command, for the hang report
                BV_CmdRunning = PM_Command;
                PM_ExecCMD();
                BV_CmdRunning = 0;

                break;
            case 1: PM_INFO("IR ");
                break;
#if BOARD_WM10 && USE_NE2000 && ISO_NET
            case FIFO_NE2K_SEND:
                // DP8390 emulator on Core 1 pushed a TX-go token when
                // the DOS packet driver wrote the start-tx command.
                // ne2000_initiate_send() forwards the framed bytes to
                // ne2000_backend.send() (-> w5500_send for WM10) and
                // increments dbg_w5500_tx_calls. Without this case the
                // FIFO token was silently dropped and tx never fired.
                ne2000_initiate_send();
                break;
#endif

        }
    }

    // Audio mix path: must run on Core 0 every loop iteration so the I2S
    // playback FIFO stays fed. Without this call the OPL2 / SBDSP samples
    // produced on the bus never reach the PCM5102 (silent output even
    // though IOWs to 0x388 are visible on the bus). Gated by .enabled so
    // it's a no-op until pm_audio_init() flips it on (called from
    // dev_audiomix_start, triggered by CMD_DEV_Init -> PM_Audio_OnOff(1)).
#if USE_AUDIO && ISO_AUDIO
    C0_STAGE(2);                      // audio mix
    if (dev_audiomix.enabled) pm_audio_domix();
    if (dev_audiomix.enabled) dev_audiomix_diag();
#if USE_SBDSP
    sbdsp_trace_drain();              // DOOM SFX diag: print SB cmd/event trace (Core 0)
#endif
#endif

    // Screenpad keypad poll. Drains the PCF8574 every ~5 ms (rate-
    // limited internally), decodes rotary + buttons into
    // screenpad_event_t enum values, and dispatches each event to the
    // active page's on_event handler via the menu bridge.
#if BOARD_WM10
#if ISO_SCREENPAD
    C0_STAGE(3);                      // screenpad I2C
    screenpad_menu_poll();
#endif
#if USE_NE2000 && ISO_WIFI
    C0_STAGE(4);                      // wifi (cyw43)
    wm_wifi_poll();          // drive the async WiFi connect / maintain the link
#endif
#endif

    // CD-ROM deferred work: cdrom_tasks() performs BOTH the async image
    // open (queued by dev_cdr_mount_from_config) and the sector reads
    // that feed the MKE data FIFO. All SD access, so it must run here on
    // Core 0 with the rest of the disk IO; MKE_READ/WRITE on the bus core
    // only touch registers/FIFOs. Without this poll the drive never has
    // media and never returns data (DOS CDR101 "not ready").
#if USE_CDROM
    C0_STAGE(6);                      // CD-ROM image tasks (SD)
    dev_cdr_update();
#endif

#if BOARD_WM10 && USE_NE2000
    // TEMP DIAGNOSTIC (USB Ethernet DHCP bring-up): 5 s heartbeat with the
    // usbeth transport counters + DP8390 IO dispatch counters. Localizes a
    // dead direction (tx=0 -> guest/backend, rx=0 -> wire/driver, both
    // moving -> delivery/filtering). Remove after mTCP DHCP validates.
    {
        static uint32_t net_last_us = 0;
        uint32_t net_now = time_us_32();
        if ((uint32_t)(net_now - net_last_us) >= 5000000u) {
            net_last_us = net_now;
            if (usbeth_present()) {
                extern volatile uint32_t dbg_ne2k_ior_count, dbg_ne2k_iow_count;
                extern volatile uint32_t dbg_ne2k_irq_raise_count, dbg_ne2k_irq_lower_count;
                extern volatile uint32_t dbg_ne2k_rx_deliver, dbg_ne2k_rx_drop_stop,
                                         dbg_ne2k_rx_drop_full, dbg_ne2k_rx_drop_bcast,
                                         dbg_ne2k_rx_drop_mcast, dbg_ne2k_rx_drop_unic,
                                         dbg_ne2k_rx_drop_runt;
                char net_s[100];
                usbeth_status(net_s, sizeof net_s);
                PM_INFO("[NETDIAG] %s | ne2k io r=%lu w=%lu irq%u raise=%lu lower=%lu\n", net_s,
                        (unsigned long)dbg_ne2k_ior_count,
                        (unsigned long)dbg_ne2k_iow_count,
                        (unsigned)PM_Config->ne2000IRQ,
                        (unsigned long)dbg_ne2k_irq_raise_count,
                        (unsigned long)dbg_ne2k_irq_lower_count);
                PM_INFO("[NETDIAG] rx8390 ok=%lu stop=%lu full=%lu bc=%lu mc=%lu uni=%lu runt=%lu\n",
                        (unsigned long)dbg_ne2k_rx_deliver,
                        (unsigned long)dbg_ne2k_rx_drop_stop,
                        (unsigned long)dbg_ne2k_rx_drop_full,
                        (unsigned long)dbg_ne2k_rx_drop_bcast,
                        (unsigned long)dbg_ne2k_rx_drop_mcast,
                        (unsigned long)dbg_ne2k_rx_drop_unic,
                        (unsigned long)dbg_ne2k_rx_drop_runt);
#if USE_NE2000_CDCECM
                usbeth_ecm_diag(net_s, sizeof net_s);
                PM_INFO("[NETDIAG] ecm pipes %s\n", net_s);
#endif
#if USE_NE2000_ASIX
                extern volatile uint32_t tuh_rp2040_salvaged_nak, tuh_rp2040_salvaged_timeout;
                extern volatile uint32_t tuh_rp2040_to_aborts, tuh_rp2040_seq_errors,
                                         tuh_rp2040_seq_accepts, tuh_rp2040_reclaims;
                usbeth_asix_diag(net_s, sizeof net_s);
                PM_INFO("[NETDIAG] asix pipes %s | salv nak=%lu timo=%lu | drop to=%lu seq=%lu/%lu rec=%lu\n",
                        net_s,
                        (unsigned long)tuh_rp2040_salvaged_nak,
                        (unsigned long)tuh_rp2040_salvaged_timeout,
                        (unsigned long)tuh_rp2040_to_aborts,
                        (unsigned long)tuh_rp2040_seq_errors,
                        (unsigned long)tuh_rp2040_seq_accepts,
                        (unsigned long)tuh_rp2040_reclaims);
#endif
            }
        }
    }
#endif

    // Disk-activity LED idle-off. The blink turns the LED on at
    // PM_Int13h_LEDCount==3 and off at ==6. If the access burst stops while
    // the count is 3..5, ==6 is never reached and the LED would stay lit.
    // Force it off (and reset the counter) once ~300 ms pass with no Int13h.
    if (PM_Int13h_LEDCount != 0 &&
        (uint32_t)(time_us_32() - PM_DiskLED_LastUs) > 300000u) {
        pm_led(0);
#if BOARD_WM10 && ISO_SCREENPAD
        screenpad_set_led(false);
#endif
        PM_Int13h_LEDCount = 0;
    }

    // NE2000 RX drain. The W5500 backend is pull-model: it doesn't
    // push received frames asynchronously, so Core 0 must poll once
    // per loop iteration to drain the RX queue and feed the DP8390
    // emulator. The function pointer is NULL for push-model backends
    // (e.g. CYW43), so the test guards both flavours. The backend's
    // SPI accesses are mutex-protected against concurrent SD I/O via
    // the SD library's spi_t->mutex (see w5500_backend.c spi_bus_lock).
#if BOARD_WM10 && USE_NE2000_W5500 && ISO_NET
    C0_STAGE(5);                      // NE2000 / W5500 SPI poll
    if (ne2000_backend.poll) ne2000_backend.poll();
#endif

    // wm_dma live byte-counter dump (rate-limited to 500 ms, only emits
    // when bytes_received changed -- silent at idle, prints once per
    // state change while DMA cycles are flowing). Shows running count,
    // last byte captured, /TC flag, and PIN_R_DMA_REQ / PIN_R_DMA_GRANT
    // GPIO states.
#if BOARD_WM10 && ISO_DBGDUMP
    {
        static absolute_time_t wm_dma_last_dump   = nil_time;
        static uint32_t        wm_dma_last_bytes  = 0xFFFFFFFFu;
        absolute_time_t now = get_absolute_time();

        if (absolute_time_diff_us(wm_dma_last_dump, now) > 500000) {
            // Skip when GUS owns the shared wm_dma engine -- otherwise this
            // SBDSP dump prints GUS-driven shared counters (bytes/tc) with SB's
            // own fields zeroed (en=0 -> SB DMA never ran), which reads like a
            // phantom SB transfer. GUS has its own [DMA][GUS] line.
            if (!dev_gus.active && wm_dma.bytes_received != wm_dma_last_bytes) {  // print only on change -> goes quiet when DMA is idle
                PM_INFO("[DMA][SBDSP] bytes=%lu/%lu tc=%d ai=%d irq=%lu ack=%lu en=%lu spin_to=%lu req=%d grant=%d\n",
                        (unsigned long)wm_dma.bytes_received,
                        (unsigned long)sbdsp.dma_sample_count,   // EOT target (samples the PC asked for)
                        wm_dma.last_tc ? 1 : 0,
                        sbdsp.autoinit ? 1 : 0,
                        (unsigned long)wm_dma.eot_irq_fired,
                        (unsigned long)wm_dma.irq_ack_count,
                        (unsigned long)wm_dma.dma_enable_count,
                        (unsigned long)wm_dma.cmd_spin_timeouts,
                        gpio_get(PIN_R_DMA_REQ) ? 1 : 0,
                        gpio_get(PIN_R_DMA_GRANT) ? 0 : 1);  // GPIO 37 LOW = granted (74LVC04 inverter)
#if SB_CMD_TRACE
                // audio diag: what actually reaches the mix. in=range of raw
                // FIFO bytes consumed (all 0x80 -> guest sent silence; wide ->
                // real PCM); outpeak=|SB sample| produced (0 -> silent mix);
                // drained=input bytes consumed since last dump.
                PM_INFO("[DMA][SBDSP] in=%02X..%02X outpeak=%d drained=%lu\n",
                        (unsigned)sb_dbg_in_min, (unsigned)sb_dbg_in_max,
                        (int)sb_dbg_out_peak, (unsigned long)sb_dbg_drained);
                sb_dbg_in_min = 0xFF; sb_dbg_in_max = 0x00;
                sb_dbg_out_peak = 0;  sb_dbg_drained = 0;
#endif
                wm_dma_last_bytes = wm_dma.bytes_received;
            }
            wm_dma_last_dump = now;
        }
    }
#endif

#if BOARD_WM10 && USE_GUS && ISO_DBGDUMP
    // GUS DMA-chain dump (rate-limited 500 ms, prints on ANY stage change --
    // including when bytes never flow, which the [wm_dma] dump above misses).
    // Read left-to-right to localize a patch-upload stall (see gus-x.h):
    //   ctrl   = last GF1 reg 0x41 DMA-control byte the guest programmed
    //   starts = StartDMA count   ev = DMA events (REQ)   isr = bytes captured
    //   by     = bytes into DRAM   tc/irq = terminal count + IRQ raised
    //   wmdma req/rx/tc_l = live REQ pin, wm_dma delivered count, /TC latch
    {
        static absolute_time_t gus_last_dump = nil_time;
        static uint32_t        gus_last_sig  = 0xFFFFFFFFu;
        absolute_time_t now = get_absolute_time();
        // NOTE: irqstat_rd (2X6 polls) is deliberately NOT in the signature --
        // the driver polls 2X6 continuously during normal playback, which would
        // spam the dump. Fire only on actual DMA / IRQ-ack activity.
        uint32_t sig = gus_dma_dbg.starts + gus_dma_dbg.events +
                       gus_dma_dbg.isr_calls + gus_dma_dbg.bytes +
                       gus_dma_dbg.tc + gus_dma_dbg.irq + gus_dma_dbg.reg41_rd;
        if (absolute_time_diff_us(gus_last_dump, now) > 500000) {
            if (sig != gus_last_sig) {
                PM_INFO("[DMA][GUS] ctrl=%02x starts=%lu ev=%lu isr=%lu by=%lu tc=%lu irq=%lu(n%d) 2x6rd=%lu eff=%02x rawis=%02x r41rd=%lu | req=%d tc_l=%d\n",
                        gus_dma_dbg.last_ctrl,
                        (unsigned long)gus_dma_dbg.starts,
                        (unsigned long)gus_dma_dbg.events,
                        (unsigned long)gus_dma_dbg.isr_calls,
                        (unsigned long)gus_dma_dbg.bytes,
                        (unsigned long)gus_dma_dbg.tc,
                        (unsigned long)gus_dma_dbg.irq,
                        (int)gus_dma_dbg.irq_num,
                        (unsigned long)gus_dma_dbg.irqstat_rd,
                        gus_dma_dbg.eff_irq,
                        gus_dma_dbg.raw_irqstat,
                        (unsigned long)gus_dma_dbg.reg41_rd,
                        gpio_get(PIN_R_DMA_REQ) ? 1 : 0,
                        wm_dma.tc_latched ? 1 : 0);
                gus_last_sig = sig;
            }
            gus_last_dump = now;
        }
        // (gus_reg_trace_drain removed with the session's reg-trace instrumentation)
        // One-time hex dump of the first captured DMA bytes (raw from CDMA vs
        // stored after MSB-invert). For 16-bit signed samples the raw stream
        // should look like plausible little-endian audio; a scrambled pattern
        // means the 16-bit DMA byte pairing/invert is wrong on the CDMA path.
        // One-time capture hex-dump DISABLED: ~8 PM_INFO lines block Core 0
        // for ~40 ms (stdio ring), stalling the PC-command loop + wm_dma
        // watchdog at a moment when the guest is mid GUS setup -> contributed
        // to a chassis reset. Data integrity already validated (raw stream is
        // plausible little-endian audio). Set GUS_DMA_CAP_DUMP 1 to re-enable
        // for one-off byte-pairing/invert debugging.
        #define GUS_DMA_CAP_DUMP 0
#if GUS_DMA_CAP_DUMP
        static bool gus_cap_dumped = false;
        if (!gus_cap_dumped && gus_dma_dbg.cap_n >= GUS_DMA_CAP_N) {
            gus_cap_dumped = true;
            PM_INFO("[gus_dma] capture ctrl=%02x start@%06lx (raw = from CDMA, dram = stored):\n",
                    gus_dma_dbg.cap_ctrl, (unsigned long)gus_dma_dbg.cap_addr0);
            for (uint32_t r = 0; r < GUS_DMA_CAP_N; r += 16) {
                char raw[64], drm[64]; int p = 0, q = 0;
                for (uint32_t c = 0; c < 16 && r + c < GUS_DMA_CAP_N; ++c) {
                    p += snprintf(raw + p, sizeof(raw) - p, "%02x ", gus_dma_dbg.cap_raw[r + c]);
                    q += snprintf(drm + q, sizeof(drm) - q, "%02x ", gus_dma_dbg.cap[r + c]);
                }
                PM_INFO("  +%02lx raw %s\n       dram %s\n", (unsigned long)r, raw, drm);
            }
        }
#endif
    }
#endif

    // PMIO trace drain -- no-op unless built with -DPMIO_TRACE=1.
    // When enabled, prints rate-limited PM IO access timeline (port +
    // data + PM_Status + PM_Command + us-timestamp). See the trace ring
    // in dev_picomem_io.cpp for details. Used during the rev 57 CPLD
    // CHRDY-for-DMA investigation.
    extern void pmio_trace_drain(void);
    pmio_trace_drain();

    // Core 0 heartbeat: every 5 s, print "Core0 alive" so a hang on
    // the host side is distinguishable from a hang on the Pico side.
   /*
    {
        static absolute_time_t pm_hb5_next = nil_time;
        if (absolute_time_diff_us(get_absolute_time(), pm_hb5_next) < 0) {
            PM_INFO("[Core0 alive]\n");
            pm_hb5_next = make_timeout_time_ms(5000);
        }
    }
*/

    // PS/2 POST code drain -- pumps HostIO.Port80h_Updated set by
    // Core 1's dev_mca_post_iow into the history ring + UART log.
    // Cheap (flag check + early return when no new code), safe to
    // call every loop iteration.
    dev_mca_post_update();

    // PCCmd protocol REPLICA dispatch -- handles port 0x328..0x32F
    // test CMDs queued by Core 1's dev_mca_tst_iow. No-op unless
    // tst_status==1 (BUSY). Used by the DOS tstpcc.com tool to probe
    // the Core 1 -> Core 0 race window without the PMBIOS / INT 13h /
    // SBDSP stack underneath. See WonderMCA/src/mca_devices/dev_mca_tst.h
    extern void dev_mca_tst_dispatch(void);
    dev_mca_tst_dispatch();

#if USE_SBDSP
    // SBDSP mainloop_action drain -- serializes all SB-IRQ assertions
    // through Core 0 main-loop context. The /TC EOT hook (single-mode)
    // and the rx>count branch (autoinit) both queue DSP_MLC_SENDIRQ (=4)
    // here instead of firing bus_irq_raise from PIC timer context.
    // Firing from PIC context races with DOS's in-flight IRQ handler --
    // nested handler frames pile up -> "Internal stack overflow" on
    // SBDIAG loop stop / rapid back-to-back transfers.
    //
    // force=false: the previous transfer's IRQ was already acked by the
    // DOS handler reading SB status port (which calls sbdsp_read
    // DSP_READ_STATUS -> bus_irq_lower). Line is inactive before this
    // fire, so we don't need the deassert-then-assert pulse and its
    // 20us busy_wait_us that starves Core 0 (which was the chassis-
    // stops-at-22-bytes regression).
    if (sbdsp.mainloop_action == 4 /* DSP_MLC_SENDIRQ */) {
        // PULSE the SB end-of-block IRQ: assert (clean falling edge via the
        // force=true 20us inactive lead-in) -> bounded hold -> auto-lower.
        // The 8557's 8259 latches the falling edge, so the line must NOT be
        // held until ack: the prior hold-until-ack left IRQ5 stuck when DOS
        // never acked -> only one TADA on the 8557 (8570/8550 latched fine)
        // AND a stuck IRQ5 into POST = reboot hang. Same discipline as the
        // mux IRQ (isa_irq.h pulse). Fires once per block (~1 s), so the
        // hold is negligible -- not the per-byte mux-IRQ starvation case.
        bus_irq_raise(IRQ_R_AUDIO, sbdsp.irq, true);   // 20us inactive lead-in, then ASSERT
        busy_wait_us(SB_EOT_IRQ_HOLD_US);              // hold so the 8259 latches the edge
        bus_irq_lower(IRQ_R_AUDIO, sbdsp.irq);         // auto-lower -- line never sticks
        wm_dma.eot_irq_fired++;   // INSTRUMENTATION
        sbdsp.mainloop_action = 0 /* DSP_MLC_NONE */;
    }
#endif

#if USE_USBHOST
    // USB Host polling -- re-enabled 2026-06-02. Surgical guard: skip
    // tuh_task() whenever Core 1 has a pending FIFO message for Core 0,
    // so PCCMD round-trip latency (PC_MemCopyW_512b etc.) is not
    // starved by TinyUSB scheduling on bursty IO. Mouse-event mirror
    // pulls pm_mouse.* into SVAR_IRQ for the host-side INT 33h shim.
    // bus_pm_irq_raise drives BV_IRQ via mca_irq_raise (no critical
    // section on WM10 -- see isa_irq.h BOARD_WM10 branches; deadlock
    // fix 2026-06-07).
    //
    // 2026-06-07 stack-overflow fix: don't raise the next mouse IRQ
    // until the chassis has FULLY drained the previous one. PMBIOS
    // PM_Int clears PM_IRR early (line 1525) but only sets PM_ISR=0
    // when its tail-end CMD_PMIRQ_ACK is processed by Core 1. So
    // PM_ISR is the authoritative "host ISR is done" signal.
    //
    // Without this guard the chassis re-entered PM_Int on every mouse
    // event while INT 33h still had STI'd inside the previous call --
    // stack frames piled up until DOS printed
    //   "Internal stack overflow, system halted"
    // (the keyboard freeze + system halt the user reported).
    //
    // Bonus floor of MOUSE_IRQ_MIN_GAP_US guarantees forward progress
    // even if PM_ISR somehow gets stuck at 1 (e.g. ACK lost): we'll
    // still attempt one re-raise per gap window.
    // Same bug class as SBDSP IRQ nesting (commit 9d89d8f).
    // 100ms = 10Hz mouse refresh -- match DOS timer tick (18.2Hz)
    // approximate cadence. Both DOS and Windows freeze if we go faster
    // (rapid clicks AND fast movement reproduce the chassis-side
    // re-entry hazard in PMBIOS PM_Int -> INT 33h AX=0060h chain).
    // 100ms is the largest gap that still feels responsive in DOS
    // apps and Windows. If even this freezes, the fix has to move to
    // chassis side: don't call INT 33h from the IRQ ISR, defer to an
    // INT 8 timer tick hook.
    // Pull model (2026-06-07): accumulate USB-mouse deltas into DPRAM.
    // PMMOUSE polls mouse_x/y/b directly during its INT 33h handlers
    // (status_03, mickeys_0B, ...) and zeroes mouse_x/y after consuming.
    // No more MCA IRQ / INT 33h push -> no DOSX fault under WIN /S.
    if (USB_Host_Enabled) {
        if (!multicore_fifo_rvalid()) tuh_task();
        pm_keyboard_tick();     // USB keyboard typematic repeat (no-op when idle)
#if USE_NE2000_CDCECM
        usbeth_ecm_tick();      // revive a dead CDC-ECM bulk-IN pipe (RX watchdog)
#endif
#if USE_NE2000_ASIX
        usbeth_asix_tick();     // revive dead ASIX IN pipes (arm-failure watchdog)
        // TX-dead watchdog: the AX88772 TX parser has no resync -- once
        // desynced it ACKs every USB transfer but puts nothing on the wire.
        // Signature: guest keeps transmitting, DP8390 never gets a reply
        // delivered. 3+ unanswered frames over 12 s -> full dongle re-init
        // (60 s cooldown so a genuinely silent peer can't thrash it).
        {
            extern volatile uint32_t dbg_ne2k_rx_deliver;
            static uint32_t wd_tx0, wd_del0, wd_t0, wd_cool;
            static bool wd_armed, wd_cooled;
            uint32_t wd_now = time_us_32();
            uint32_t wd_tx  = usbeth_asix_tx_frames();
            if (dbg_ne2k_rx_deliver != wd_del0) {          // RX progress: reset
                wd_del0 = dbg_ne2k_rx_deliver;
                wd_tx0  = wd_tx;
                wd_armed = false;
            } else if (!wd_armed) {
                if (wd_tx != wd_tx0) { wd_armed = true; wd_t0 = wd_now; }
            } else if ((wd_tx - wd_tx0) >= 3 &&
                       (uint32_t)(wd_now - wd_t0) > 12000000u &&
                       (!wd_cooled || (uint32_t)(wd_now - wd_cool) > 60000000u)) {
                PM_ERROR("[USBETH] TX-dead heuristic: %lu frames sent, no RX delivered\n",
                         (unsigned long)(wd_tx - wd_tx0));
                wd_cool = wd_now; wd_cooled = true;
                wd_tx0 = wd_tx; wd_armed = false;
                usbeth_asix_reinit();
            }
        }
#endif
#if USE_SERMOUSE
        dev_sermouse_task();    // serial-mouse UART worker + paced reports
#endif
        if (pm_mouse.event) {
            pm_mouse.event = false;
            int8_t  mdx  = (int8_t)pm_mouse.x;
            int8_t  mdy  = (int8_t)pm_mouse.y;
            uint8_t mbtn = pm_mouse.buttons;
            if (MOUSE_Enabled) {
                // INT 33h (PMMOUSE) pull model: accumulate into shared DPRAM.
                int16_t nx = (int16_t)(int8_t)SVAR_IRQ->mouse_x + mdx;
                int16_t ny = (int16_t)(int8_t)SVAR_IRQ->mouse_y + mdy;
                if (nx >  127) nx =  127;
                if (nx < -128) nx = -128;
                if (ny >  127) ny =  127;
                if (ny < -128) ny = -128;
                SVAR_IRQ->mouse_x = (uint8_t)(int8_t)nx;
                SVAR_IRQ->mouse_y = (uint8_t)(int8_t)ny;
                SVAR_IRQ->mouse_b = mbtn;
            }
            // PS/2 aux path (Windows / 8042): accumulate regardless of
            // MOUSE_Enabled -- Windows loads its own PS/2 driver, not PMMOUSE.
            //
            // DIAGNOSTIC GATE (2026-07-16, Win3.11 dead-cursor hunt): the
            // [REG] fingerprint shows mouse.drv serial-bound and consuming
            // COM2 packets while the cursor stays dead -- with the aux
            // inject (minj) streaming unsolicited IRQ12 data into the real
            // 8042 during mouse.drv's own init. Suspect: that stream
            // corrupts mouse.drv's device-detection. 0 = aux inject OFF
            // (mailbox never publishes, PMBIOS injects nothing); the
            // serial path is unaffected. Restore to 1 after the test.
#define SERMOUSE_WIN_PS2_INJECT 0
#if SERMOUSE_WIN_PS2_INJECT
            pm_mouse_ps2_accumulate(mdx, mdy, mbtn);
#endif
        }
#if SERMOUSE_WIN_PS2_INJECT
        pm_mouse_ps2_tick();   // publish a coalesced PS/2 packet to the mailbox
#endif
    }
#endif

    tight_loop_contents();
}
PM_INFO("WM10: Core0 Never see this\n");
#endif // BOARD_WM10

// **************************************************************************
// ******************* PicoMEM Initialization Start *************************
// **************************************************************************
#if PM_PRINTF
pm_timefromlast();
#endif

// ************  Init the PicoMEM SPI PSRAM  ******************
#ifdef PIMORONI_PICO_PLUS2_RP2350
#else

#if USE_SPI_PSRAM   // !! PicoMEM 1 PSRAM must be configured before the SDCard, other wise it crash 
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
PM_INFO("[DSK] >Start uSD\n");
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

// ******* We are now after the configs load *******

bus_irq_init();
isa_dma_install();

#if BOARD_PM15 || BOARD_PM20   // Put on the PM1.5 Only for th emoment (SB / DMA)
DBG_PIN_INIT();
#endif

// Values sent by the code to the BIOS Initialisation, Just after the Config Load
PM_Config->B_MAX_PMRAM=MAX_PMRAM;   // Value defined in the makefile.ini
#if !USE_SPI_PSRAM
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

#if BOARD_PMJR
  dev_memory_install(MEM_RAM,0);         
  dev_memory_install(MEM_PSRAM,0); 
  PM_INFO(">PC Junior firmware : Enable RAM emulation earlier\n");  
#endif

// **************  PicoMEM RTC init  ******************

#if !BOARD_WM10
// Set a default date
pico_rtc_init();
#else
// WM10: skip RTC init while bisecting 8556 boot hang. PCF85263 is on
// i2c1; if not present (or slow to respond) the i2c reads stall Core 0
// for ms-to-seconds while PMBIOS is sending POST CMDs.
PM_INFO(">Skip RTC init (WM10 boot bisect)\n");
#endif

// ************  Qwiic Init and test code ******************

#if !BOARD_WM10
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
#else
// WM10: skip Qwiic init/scan while bisecting 8556 boot hang. The scan
// probes all 128 i2c1 addresses with timeouts on every non-ACK, which
// can stall Core 0 for ~1 second -- long enough for PMBIOS POST to
// time out a CMD.
PM_INFO(">Skip Qwiic init/scan (WM10 boot bisect)\n");
#endif

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
PM_INFO("[USB] >Start USB\n");

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

/*
#if USE_CDROM
       dev_cdr_install();
       dev_cdr_mount_from_config();   // mount the configured image (async in dev_cdr_update)
#endif
*/

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
#if BOARD_WM10
// WM10: keep BV_Int13hCLI=1 set earlier in WM10 init. This block runs
// AFTER the WM10-specific path that sets BV_Int13hCLI=1 (line ~2129),
// so resetting to 0 here silently undid that fix and let
// AdLib/music-engine timer IRQs preempt PM_SendCMD's 2-instruction
// window between data-port and CMD-port OUTs -- producing the
// `PC_MemCopyW_512b: Timeout 20/100ms pcs=00 pms=00 cmd=00 src=1`
// pattern seen on 8555 too (NOT just 8570). Don't clear it here.
BV_Int13hCLI=1;
#else
BV_Int13hCLI=0;
#endif
BV_BoardID=BOARD_ID;    // Set the Board ID
PM_Status=STAT_READY;   // PicoMEM Initialisation completed (The BIOS Wait for its end)
PM_Init_Completed=true;

wifi_delay_counter=0;
pm_cmd_100msdelay = make_timeout_time_us(100000);
pm_cmd_1sdelay    = make_timeout_time_us(1000000);

PM_INFO("Init completed\n");

#if BOARD_WM10
for (;;)  // Core 1 Main Loop (Commands)
{
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
}
#endif

#if CORE0_WATCHDOG
// DIAGNOSTIC: arm the Core 0 hang watchdog. 8 s is far above any normal
// main-loop section, so it only fires on a real hang. g_c0_bc_magic marks the
// breadcrumb as valid so a cold boot isn't misreported.
g_c0_bc_magic = C0_BC_MAGIC;
watchdog_enable(8000, true);
#endif

for (;;) {
#if CORE0_WATCHDOG  
 C0_STAGE(0); watchdog_update();   // loop top + feed the hang watchdog
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
  C0_STAGE(1);  // tuh_task() stage
  // Surgical FIFO-pending guard: skip tuh_task() whenever Core 1 has
  // queued a message for Core 0, so PCCMD round-trip latency is not
  // starved by TinyUSB scheduling on bursty IO.
  if (!multicore_fifo_rvalid()) tuh_task();

  C0_STAGE(2);  // USB keyboard handling
  // USB keyboard typematic repeat: HID reports only on change, so we
  // synthesize the held-key auto-repeat here. Cheap no-op when idle.
  pm_keyboard_tick();

  if (pm_mouse.event&&MOUSE_Enabled)
   {
    pm_mouse.event=false;
    SVAR_IRQ->mouse_x=pm_mouse.x;
    SVAR_IRQ->mouse_y=pm_mouse.y;
    SVAR_IRQ->mouse_b=pm_mouse.buttons;
    PM_INFO("Mouse Event : x:%d y:%d b:%d\n",pm_mouse.x,pm_mouse.y,pm_mouse.buttons);
    bus_pm_irq_raise(IRQ_R_MOUSE,1,false);
   }
 }
#endif

#if USE_MPU
  C0_STAGE(3);  // MPU update stage
  dev_mpu_update();
#endif // USE_MPU

#if USE_CDROM
  C0_STAGE(4);  // CD-ROM update stage
  dev_cdr_update();
#endif // USE_CDROM

#if USE_SBDSP
  C0_STAGE(5);  // SBDSP update stage
  dev_sbdsp_update();  
#endif // USE_SBDSP

#if USE_AUDIO
  C0_STAGE(6);  // Audio mix stage
  if (dev_audiomix.enabled)
#if BOARD_WM10||BOARD_PM20||BOARD_PM15
  pm_audio_domix(true);   // Insert Audio Mix during the wait time (Can use SD)
#else
  pm_audio_domix(false);   // Insert Audio Mix during the wait time (Can't use SD)
#endif // BOARD_WM10||BOARD_PM20||BOARD_PM15
#endif // USE_AUDIO

absolute_time_t pm_delay_delta=absolute_time_diff_us(get_absolute_time(), pm_cmd_100msdelay);
// Portion of the code executed every 100ms for fast update code (Display)
if (absolute_time_diff_us(get_absolute_time(), pm_cmd_100msdelay) < 0)
 {
#if DEV_POST_Enable  // POST Code display
#if BOARD_WM10
   // WM10: dev_post_iow fills HostIO from the passive_snoop branch of
   // handle_iow; pump it here to the history ring + (optional) UART.
   dev_mca_post_update();
#else
   if (!dev_audiomix.dev_active) dev_post_update();  // Update POST Display, Only when there is no Audio
#endif
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
//      PM_INFO("*");  // Show that the system is alive (and not stuck in a loop for example)

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
          
         }
#if USE_CD_AUDIO
         if (dev_cdrom_mke.delay++==4) {   // 4 seconds since last CD-ROM Audio read
          dev_cdr_disable_mix();    // Disable the mixing
         }
#endif
#if USE_MPU
         if (dev_mpu_delay++==8) {  // 8 seconds since last mpu Output
          dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_MPU;    //Disable the mixing
         } 
#endif                  
        if (dev_sbdsp_delay++==8) {  // 8 second since last sbdsp I/O or sound output
          dev_sbdsp_disable_mix();  //Disable the Sound Blaster mixing
        }
#if USE_GUS
        if (dev_gus.delay++==10) {  // 10 second since last sbdsp I/O or sound output
          dev_gus_disable_mix();  //Disable the Sound Blaster mixing
        }
#endif
      }
       else
      {  // 1s based actions when Audio is Off
#if PM_WIFI
      if (wifi_delay_counter++==WIFI_DELAY) {  // Check Wifi connection Status / reconnect every 10s
          if (BV_WifiInit==INIT_DONE) {
               PM_INFO("[WIFI] Get Status\n");
               PM_Wifi_GetStatus();
               if (PM_Wifi.Status<=0)
                 {
                   if (dev_audiomix.enabled) 
#if BOARD_WM10||BOARD_WM20||BOARD_PM15
			               	pm_audio_domix(true);   // Insert Audio Mix during the wait time
#else
				              pm_audio_domix(false);   // Insert Audio Mix during the wait time
#endif 
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