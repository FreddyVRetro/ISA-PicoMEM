#include "bios_int13h.h"

#include "pico/stdlib.h"
#include "pm_debug.h"
#include "pm_defines.h"
#include "pm_gvars.h"
#include "pmdev.h"
#include "pm_pccmd.h"
#include "pm_libs/pm_libs.h"
#include "bios_disk.h"

extern volatile uint8_t PM_Status;
extern uint8_t PM_Int13h_LEDCount;
extern uint32_t PM_DiskLED_LastUs;
/*
// INT 13h disk services receive the BIOS register image through PM_DP_RAM.
// PM_EnableSD() may temporarily disable PSRAM while the image is accessed.
// PM_Int13h_LEDCount and PM_DiskLED_LastUs keep the disk activity indicator
// alive across a burst of BIOS disk calls.
*/

void bios_int13h_execute()
{
  BV_PCCMD_SOURCE = 1;
/*     if (BV_PCCMD_SOURCE!=0)  // If the PC Command is running
        {
          // PM_ERROR("[DSK] Int13h: PC Command already running ! (%d)\n",BV_PCCMD_SOURCE);
          reg_ah=4;     // Return error 4
          reg_al=0xFF;
          reg_flagl=1;  // Set Carry (Bit 0)
          PM_Status=STAT_READY;
          return;
         }
*/
  if (PM_EnableSD()) {

  // Mark disk activity and blink the LED only after several consecutive calls.
  PM_DiskLED_LastUs = time_us_32();   // mark last disk access (for idle-off)
  if (PM_Int13h_LEDCount++ == 3) {
  pm_led(1);  // Don't blink the LED all the time
#if BOARD_WM10
  screenpad_set_led(true);   // mirror disk activity on screenpad P7 LED
#endif
  }

    INT13_DiskHandler();
  } else {
    reg_ah = 4;
    reg_al = 0xFF;
    reg_flagl = 1;
  }

  // Turn the LED off at the end of the access burst.
  if (PM_Int13h_LEDCount == 6) {
  pm_led(0);
#if BOARD_WM10
  screenpad_set_led(false);  // disk activity done -> screenpad P7 LED off
#endif
  PM_Int13h_LEDCount = 0;
  }  

  PM_PICO_EnablePSRAM();
  PC_End();
  BV_PCCMD_SOURCE = 0;

  if (PM_Status == STAT_CMDINPROGRESS) PM_Status = STAT_READY;
}
