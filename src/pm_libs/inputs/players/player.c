// Simple implementation of the Joypad-os player code for the PicoMEM

#include "feedback.h"
#include "manager.h"
#if USE_USBHOST
#include "usbh.h"
#endif

#include "../../pm_debug.h"
#include "dev_joystick.h"

// LED patterns for PS3/Switch controllers
const uint8_t PLAYER_LEDS[] = {
  0x00, // OFF
  0x01, // LED1  0001
  0x02, // LED2  0010
  0x04, // LED3  0100
  0x08, // LED4  1000
  0x09, // LED5  1001
  0x0A, // LED6  1010
  0x0C, // LED7  1100
  0x0D, // LED8  1101
  0x0E, // LED9  1110
  0x0F, // LED10 1111
};

// Get feedback state for a player (0-based index)
feedback_state_t* feedback_get_state(uint8_t player_index)
{
  return NULL;
}

void remove_player(uint8_t number)
{
  PM_INFO("Remove Player %d\n",number);
  if (number==0)
   { // If first player removed, move the 2nd player infos
     joystate_struct.pl_dev[0]=joystate_struct.pl_dev[1];
     joystate_struct.pl_inst[0]=joystate_struct.pl_inst[1];
   }
 joystate_struct.pl_dev[1]=255;  // Set 2nd player dev to 255
 joystate_struct.pl_total--;
 // Reset the Joystick values to neutral
 joystate_struct.joy1_x=127;
 joystate_struct.joy1_y=127;
 joystate_struct.joy2_x=127;
 joystate_struct.joy2_y=127; 
 joystate_struct.button_mask=0;
}

void remove_players_by_address(int dev_addr, int instance)
{
 PM_INFO("remove_players(%d, %d)\n",dev_addr,instance);  
 PM_INFO("%d players P0 : %d,%d P1 : %d,%d \n",joystate_struct.pl_total, joystate_struct.pl_dev[0],joystate_struct.pl_inst[0],joystate_struct.pl_dev[1],joystate_struct.pl_inst[1]);
 if (instance==-1)
    {
      if (joystate_struct.pl_dev[1]==dev_addr) remove_player(1);
      if (joystate_struct.pl_dev[0]==dev_addr) remove_player(0);
    }
    else 
    {
      if ((joystate_struct.pl_dev[1]==dev_addr)&&(joystate_struct.pl_inst[1]==instance)) remove_player(1);
      if ((joystate_struct.pl_dev[0]==dev_addr)&&(joystate_struct.pl_inst[0]==instance)) remove_player(0);
    }
 #if USE_USBHOST    
 usbh_task(); // To Update the LED
 #endif
}

// Add player to array
// SHIFT mode: Adds to end (playersCount++)
// FIXED mode: Finds first empty slot (dev_addr == -1)
// Returns player index (0-based), or -1 if full
int add_player(int dev_addr, int instance)
{
  PM_INFO("add_player(%d,%d)\n",dev_addr,instance);
  if (joystate_struct.pl_total==2) return -1;
  if (joystate_struct.pl_total==0) 
     {
      joystate_struct.pl_dev[0]=dev_addr;
      joystate_struct.pl_inst[0]=instance;
      joystate_struct.pl_total=1;
      PM_INFO("Add Player 0 (1)\n");
      PM_INFO("%d players P0 : %d,%d P1 : %d,%d \n",joystate_struct.pl_total, joystate_struct.pl_dev[0],joystate_struct.pl_inst[0],joystate_struct.pl_dev[1],joystate_struct.pl_inst[1]);      
      #if USE_USBHOST
      usbh_task(); // To Update the LED
      #endif
      return 0;
     }
     else
     {
      joystate_struct.pl_dev[1]=dev_addr;
      joystate_struct.pl_inst[1]=instance;
      joystate_struct.pl_total=2;
      PM_INFO("Add Player 1 (2)\n");
      PM_INFO("%d players P0 : %d,%d P1 : %d,%d \n",joystate_struct.pl_total, joystate_struct.pl_dev[0],joystate_struct.pl_inst[0],joystate_struct.pl_dev[1],joystate_struct.pl_inst[1]);      
      #if USE_USBHOST
      usbh_task(); // To Update the LED      
      #endif
      return 1;
     }
}

// Find player by dev_addr and instance
// Returns player index (0-based), or -1 if not found
int find_player_index(int dev_addr, int instance)
{
  if ((joystate_struct.pl_dev[0]==dev_addr)&&(joystate_struct.pl_inst[0]==instance)) return 0;
  if ((joystate_struct.pl_dev[1]==dev_addr)&&(joystate_struct.pl_inst[1]==instance)) return 1;
  return -1;
}