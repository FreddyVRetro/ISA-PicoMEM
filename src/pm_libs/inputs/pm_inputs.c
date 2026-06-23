// Simple implementation of the Joypad-os interfaces for the PicoMEM

#include "inputs/input_event.h"
#include "buttons.h"
#include "players/manager.h"
#include "../../pm_debug.h"
#include "dev_joystick.h"
#include "dev_mouse.h"
#include "inputs/buttons/sp_buttons.h"
#include "pm_menu.h"

/*
#define JP_BUTTON_DU (1 << 12)  // D-Up      D-Up      D-Up       Hat
#define JP_BUTTON_DD (1 << 13)  // D-Down    D-Down    D-Down     Hat
#define JP_BUTTON_DL (1 << 14)  // D-Left    D-Left    D-Left     Hat
#define JP_BUTTON_DR (1 << 15)  // D-Right   D-Right   D-Right    Hat
*/
#define DIR_UP 1
#define DIR_DN 2
#define DIR_LF 4
#define DIR_RT 8

void get_joystick_state(const input_event_t* event, uint8_t *x,uint8_t *y,uint8_t *buttons)
{
    *buttons = ((~event->buttons)&0x03);  // Extract Button 1,2
    switch (event->buttons>>12 & 0x0F) {
    case DIR_UP: // N
        *x = 127;
        *y = 0;
        break;
    case DIR_UP|DIR_RT: // NE
        *x = 255;
        *y = 0;
        break;
    case DIR_RT: // E
        *x = 255;
        *y = 127;
        break;
    case DIR_DN|DIR_RT: // SE
        *x = 255;
        *y = 255;
        break;
    case DIR_DN: // S
        *x = 127;
        *y = 255;
        break; 
    case DIR_DN|DIR_LF: // SW
        *x = 0;
        *y = 255;
        break;
    case DIR_LF: // W
        *x = 0;
        *y = 127;
        break;
    case DIR_LF|DIR_UP: // NW
        *x = 0;
        *y = 0;
        break;
    case 0: // None - use analog stick instead
        *x = event->analog[ANALOG_LX];
        *y = event->analog[ANALOG_LY];
        break;
    }
}

// One player mode > update both players with one pad event
void update_joystick_dual(const input_event_t* event)
{
  uint8_t x,y,buttons;

  get_joystick_state(event, &x, &y, &buttons);
  joystate_struct.joy1_x=x;
  joystate_struct.joy1_y=y;
  joystate_struct.button_mask = ((~event->buttons)&0x0F)<<4;
  joystate_struct.joy2_x = event->analog[ANALOG_RX];
  joystate_struct.joy2_y = event->analog[ANALOG_RY];
  
/*  PM_INFO("Update (Dual): buttons=0x%02X, joy1=(%d, %d), joy2=(%d, %d)\n",
            joystate_struct.button_mask, joystate_struct.joy1_x, joystate_struct.joy1_y,
            joystate_struct.joy2_x, joystate_struct.joy2_y);
*/            
}

void update_joystick(const input_event_t* event, int player)
{
  if (player==-1) return;
  uint8_t x,y,buttons;
  get_joystick_state(event, &x, &y, &buttons);
  if (player==0)
  {
  joystate_struct.joy1_x=x;
  joystate_struct.joy1_y=y;
  joystate_struct.button_mask=(joystate_struct.button_mask&0xCF)|buttons<<4;
/*    PM_INFO("Update Play1: buttons=0x%02X, joy1=(%d, %d)\n",
            joystate_struct.button_mask, joystate_struct.joy1_x, joystate_struct.joy1_y);  */
  }
  else
  {
  joystate_struct.joy2_x=x;
  joystate_struct.joy2_y=y;
  joystate_struct.button_mask=(joystate_struct.button_mask&0x3F)|buttons<<6;
/*    PM_INFO("Update Play2: buttons=0x%02X, joy2=(%d, %d)\n",
            joystate_struct.button_mask, joystate_struct.joy2_x, joystate_struct.joy2_y);  */
  }
}

//https://pulkomandy.tk/pulkocms/cat/Electronique/joystick_specs.txt

//https://digipres.club/@timixretroplays/112821671154728372

// Gravis GamePad Pro button mapping:
// And button 1 as data Data packet consists of 24 bytes, as follows: 0 1 1 1 1 1 0 Select Start R2 Blue 0 L2 Green Yellow Red 0 L1 R1 Up Down 0 Right Left
/*
#define GR_LF     (1 << 0)
#define GR_RT     (1 << 1)

#define GR_DN     (1 << 3)
#define GR_UP     (1 << 4)
#define GR_R1     (1 << 5)
#define GR_L1     (1 << 6)

#define GR_RED    (1 << 8)
#define GR_YELLOW (1 << 9)
#define GR_GREEN  (1 << 10)
#define GR_L2     (1 << 11)

#define GR_BLUE   (1 << 13)
#define GR_R2     (1 << 14)
#define GR_START  (1 << 15)
#define GR_SELECT (1 << 16)
*/

#define GR_LF     (1 << 16)
#define GR_RT     (1 << 15)

#define GR_DN     (1 << 13)
#define GR_UP     (1 << 12)
#define GR_R1     (1 << 11)
#define GR_L1     (1 << 10)

#define GR_RED    (1 << 8)
#define GR_YELLOW (1 << 7)
#define GR_GREEN  (1 << 6)
#define GR_L2     (1 << 5)

#define GR_BLUE   (1 << 3)
#define GR_R2     (1 << 2)
#define GR_START  (1 << 1)
#define GR_SELECT (1 << 0)

void update_gravis_gamepad_p1(const input_event_t* event)
{
    static uint32_t prev_buttons;

    joystate_struct.Gravis1 = 0b0111110 << 17
/*    
    | (event->buttons & JP_BUTTON_B1 ? 0: GR_YELLOW )  // Yellow
    | (event->buttons & JP_BUTTON_B2 ? 0: GR_GREEN  )  // Green
    | (event->buttons & JP_BUTTON_B3 ? 0: GR_RED    )  // Red
    | (event->buttons & JP_BUTTON_B4 ? 0: GR_BLUE   )  // Blue
    | (event->buttons & JP_BUTTON_L1 ? 0: GR_L1     )  // L1
    | (event->buttons & JP_BUTTON_R1 ? 0: GR_R1     )  // R1
    | (event->buttons & JP_BUTTON_L2 ? 0: GR_L2     )  // L2
    | (event->buttons & JP_BUTTON_R2 ? 0: GR_R2     )  // R2
    | (event->buttons & JP_BUTTON_DU ? 0: GR_UP     )  // D-Up
    | (event->buttons & JP_BUTTON_DD ? 0: GR_DN     )  // D-Down
    | (event->buttons & JP_BUTTON_DL ? 0: GR_LF     )  // D-Left
    | (event->buttons & JP_BUTTON_DR ? 0: GR_RT     )  // D-Right
    | (event->buttons & JP_BUTTON_S1 ? 0: GR_SELECT )  // Select
    | (event->buttons & JP_BUTTON_S2 ? 0: GR_START  ); // Start
*/
    | (event->buttons & JP_BUTTON_B1 ? GR_YELLOW : 0)  // Yellow
    | (event->buttons & JP_BUTTON_B2 ? GR_GREEN  : 0)  // Green
    | (event->buttons & JP_BUTTON_B3 ? GR_RED    : 0)  // Red
    | (event->buttons & JP_BUTTON_B4 ? GR_BLUE   : 0)  // Blue
    | (event->buttons & JP_BUTTON_L1 ? GR_L1     : 0)  // L1
    | (event->buttons & JP_BUTTON_R1 ? GR_R1     : 0)  // R1
    | (event->buttons & JP_BUTTON_L2 ? GR_L2     : 0)  // L2
    | (event->buttons & JP_BUTTON_R2 ? GR_R2     : 0)  // R2
    | (event->buttons & JP_BUTTON_DU ? GR_UP     : 0)  // D-Up
    | (event->buttons & JP_BUTTON_DD ? GR_DN     : 0)  // D-Down
    | (event->buttons & JP_BUTTON_DL ? GR_LF     : 0)  // D-Left
    | (event->buttons & JP_BUTTON_DR ? GR_RT     : 0)  // D-Right
    | (event->buttons & JP_BUTTON_S1 ? GR_SELECT : 0)  // Select
    | (event->buttons & JP_BUTTON_S2 ? GR_START  : 0); // Start
    
    if (prev_buttons!=event->buttons)  PM_INFO("GrIP: %024b\n",joystate_struct.Gravis1);
    prev_buttons=event->buttons;

}

void update_gravis_gamepad_p2(const input_event_t* event)
{
}

void router_submit_input(const input_event_t* event) {

// if (event->buttons) PM_INFO("Submit input event: dev_addr=%d, instance=%d, type=%d, layout=%d, buttons=0x%08X, keys=0x%08X\n",
//         event->dev_addr, event->instance, event->type, event->layout, event->buttons, event->keys);

 switch(event->type) {
  case INPUT_TYPE_MOUSE:        
       pm_mouse.buttons=event->buttons;
       pm_mouse.x=event->delta_x;
       pm_mouse.y=event->delta_y;
       pm_mouse.event=true;
       break;
  case INPUT_TYPE_KEYBOARD:
       // Handle keyboard input (not implemented in this example)
       break;
  case INPUT_TYPE_PANEL:
#if USE_OLED
       pm_menu_button_event(event->buttons);  // Send the buttons to the menu handler (for navigation and selection)
#endif       
       break;
  case INPUT_TYPE_GAMEPAD:
        if (!joystate_struct.pl_total) 
           { printf("Joy: No player !"); break; }
        if (event->buttons==0x108) 
           {
            PM_INFO("[PAD] Set Standard Mode\n");
            joystate_struct.mode=1;  // One Gamepad
           }
//        if (event->buttons==0x102) joystate_struct.mode=2;  // Two Gamepads
        if (event->buttons==0x101)
           {
            PM_INFO("[PAD] Set Gravis Mode\n");
            joystate_struct.mode=3;  // Gravis Gamepad 
           }

        if (joystate_struct.mode==3) update_gravis_gamepad_p1(event);
        else if (joystate_struct.pl_total==1) update_joystick_dual(event);
                else {
                      update_joystick(event,find_player_index(event->dev_addr, event->instance));
                      }

       break;              
  }

}

void pm_input_init()
{

 sp_init();

}

void pm_input_task()
{

 sp_task();

}