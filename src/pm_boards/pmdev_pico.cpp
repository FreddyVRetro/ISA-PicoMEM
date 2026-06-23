
/*  Code specific to the Pi Pico Boards
    PSRAM, USB, MicroSD (fatfs) initialisation
*/

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

#include "pm_board.h"       // PicoMEM Board Definitions (GPIO)

#if USE_SPI_PSRAM
#if USE_SPI_PSRAM_DMA
#include "psram_spi.h"
#else
#include "../psram_spi2.h"
#endif
#endif

#ifdef PIMORONI_PICO_PLUS2_RP2350
#include "pico_psram.h"    // Add QSPI PSRAM Code
#else
#if USE_SPI_PSRAM              // Add other PSRAM Code if not Pimoroni
#if USE_SPI_PSRAM_DMA
#include "psram_spi.h"
#else
#include "../psram_spi2.h"
#endif  //USE_SPI_PSRAM_DMA
#endif  //USE_SPI_PSRAM
#endif  //PIMORONI_PICO_PLUS2_RP2350

#if USE_USBHOST
// TinyUSB Host
#include "tusb.h"
extern "C" {
#include "usb.h"
}
extern "C" void hid_init();   // Initialise the hid code
#endif

#include "../pm_debug.h"

//SDCARD and File system definitions
#include "hw_config.h"  /* The SDCARD and SPI definitions */	
#include "f_util.h"
#include "ff.h"
#include "sd_card.h"

//Other PicoMEM Code

//#include "isa_iomem.pio.h"  // PIN Defines
#include "../pm_gvars.h"
#include "../picomem.h"
#include "pm_defines.h"     // Shared MEMORY variables defines (Registers and PC Commands)

FATFS SDFS;  // FatFS FileSystem Object for the SD

bool EPSRAM_Enabled=false;
extern bool USB_Host_Enabled;	// True if USB Host mode is enabled

static uint EPSRAM_Size;
bool EPSRAM_Available=false;   // True if the External PSRAM is initialized

bool SD_Available=false;
bool SD_Enabled=false;

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
#define PSRAM_DMA_FREQ 220
#define PSRAM_FREQ 200

// PM_PICO_StartPSRAM: Initialise the External PSRAM
// return the memory size in MB or Init error/Disabled code
uint8_t PM_PICO_StartPSRAM()
{
#if USE_SPI_PSRAM
 PM_INFO("SPI PSRAM Init: ");

#if USE_SPI_PSRAM_DMA
 float div = (float)clock_get_hz(clk_sys) / (PSRAM_DMA_FREQ*1000000); //
 PM_INFO("PSRAM (DMA) PIO Freq %d, div (DMA) : %f ",PSRAM_DMA_FREQ,div);
 psram_spi = psram_spi_init_clkdiv(pio1, -1, div, true);
// Modify again the output pad
 gpio_set_drive_strength(PSRAM_PIN_CS, GPIO_DRIVE_STRENGTH_8MA);
 gpio_set_drive_strength(PSRAM_PIN_SCK, GPIO_DRIVE_STRENGTH_8MA);
 gpio_set_drive_strength(PSRAM_PIN_MOSI, GPIO_DRIVE_STRENGTH_8MA);
 /* gpio_set_slew_rate(PSRAM_PIN_CS, GPIO_SLEW_RATE_FAST); */
 gpio_set_slew_rate(PSRAM_PIN_SCK, GPIO_SLEW_RATE_FAST);
 gpio_set_slew_rate(PSRAM_PIN_MOSI, GPIO_SLEW_RATE_FAST);

#else
 float div = (float)clock_get_hz(clk_sys) / (PSRAM_FREQ*1000000); //
 PM_INFO("PSRAM (No DMA) PIO Freq %d, div (DMA) : %f ",PSRAM_FREQ,div);
 psram_spi = psram_init(div);
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
} // PM_PICO_StartPSRAM

// PM_PICO_EnablePSRAM: Re configure the PIO pin direction for External PSRAM
extern "C" bool PM_PICO_EnablePSRAM()
{
#if USE_SPI_PSRAM  
 if (EPSRAM_Available)
    {
     SD_Enabled=false;
#ifndef PIMORONI_PICO_PLUS2_RP2350     
     gpio_set_function(SD_SPI_SCK, GPIO_FUNC_PIO1);
     gpio_set_function(SD_SPI_MOSI, GPIO_FUNC_PIO1);
     gpio_set_function(SD_SPI_MISO, GPIO_FUNC_PIO1);
// PM_INFO(">Enable PSRAM sm %d \n",psram_spi.sm);
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

// PM_EnableSD : Re configure the PIO pin direction for SD
extern "C" bool PM_EnableSD()
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
#if USB_UART
   PM_INFO("Fail : Serial USB is Active\r\n");    
   BV_USBInit=0xFD;  // Failed  
   return false;
#else
  // init host stack on configured roothub port
  tusb_rhport_init_t host_init = {
    .role = TUSB_ROLE_HOST,
    .speed = TUSB_SPEED_AUTO
  };
   tusb_init(BOARD_TUH_RHPORT, &host_init);
   hid_init();   // Initialise the hid code
//   pm_mouse.div=4;
   PM_INFO("Ok\r\n");
   USB_Host_Enabled=true;
   BV_USBInit=0;    // Success 
   return true;
#endif   // if USB_UART
#else    //  if !DO_USBHOST
   PM_INFO2("USE_USBHOST!=1 \n ");
   BV_USBInit=0xFD;  // Failed
return false;
#endif   
}

void PM_StopUSB()
{
 USB_Host_Enabled=false;
 BV_USBInit=INIT_DISABLED;   // Disabled
}