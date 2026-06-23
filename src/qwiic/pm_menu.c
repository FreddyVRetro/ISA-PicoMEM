
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pm_board.h"     // PicoMEM Board Definitions (GPIO)
#include "../pm_debug.h"
#include "ff.h"           // forr FIL definition
#include "pm_defines.h"
#include "pm_disk.h"
#include "pm_display.h"


#define PM_MENU_COUNT 2

#define SM_FLOPPY 0
#define SM_INFO   1
#define SM_FLOPPY_CONFIRM 2

bool pm_menu_home=true;
bool pm_menu_selected=false;
uint8_t pm_menu_lastmenu;
uint8_t pm_menu_currmenu=0;
uint32_t pm_menu_lastevent;  // Time when the last event occur

FSizeName_t pm_img_list[129];  //+1 to add none
uint8_t img_list_type=0;       // 0: none 1: Floppy, 2: CD
uint8_t img_list_nb=0;
uint8_t img_index=0;
uint8_t floppy_confirm_index=0;

void pm_img_add_none() {
  const char imgnone[] = "  -=None=-   ";
  strcpy(pm_img_list[0].name, imgnone);
  pm_img_list[0].size=0x1;
  pm_display_img_list(img_index, img_list_nb, (FSizeName_t *) pm_img_list);
}

void pm_sm_floppy_init()
{
  PM_INFO("Display Floppy Select Menu: \n");
  if (img_list_type!=1) {
     img_list_type=1;
     pm_disk_InitSearchPath();
     pm_img_add_none();
     ListImages(0,&img_list_nb,(void *) &pm_img_list[1]);
     if (img_list_nb==0) PM_INFO("No Floppy Image found\n");
      else PM_INFO("%d Floppy Images found\n",img_list_nb);
     img_list_nb++;  // +1 to add the "None" at the beginning of the list 
     img_index=0;
     }

  pm_display_img_list(img_index, img_list_nb, (FSizeName_t *) pm_img_list);
}

void pm_floppy_next()
{
  PM_INFO("Floppy : Next");
  if (img_index<img_list_nb-1) 
     {
      img_index++;
      pm_display_img_list(img_index, img_list_nb, (FSizeName_t *) pm_img_list);
     }        
}

void pm_floppy_prev()
{
  PM_INFO("Floppy : Previous");  
  if (img_index>0) 
     {
      img_index--;
      pm_display_img_list(img_index, img_list_nb, (FSizeName_t *) pm_img_list);
     }
}

void pm_floppy_select()
{
  PM_INFO("Floppy : Select");
  if (img_list_nb>0) 
     {
      PM_INFO("Floppy image %d selected: %s\n", img_index, pm_img_list[img_index].name);
      if (pm_img_list[img_index].size==0xFFFE) { // Folder
         PM_INFO("Folder selected: %s\n",pm_img_list[img_index].name);
         pm_disk_UpdateSearchPath(img_index, (FSizeName_t *) pm_img_list);
         ListImages(0,&img_list_nb,(void *) &pm_img_list[1]);
         img_index=0;
         img_list_nb++;  // +1 to add the "None" at the beginning of the list
         pm_display_img_list(img_index, img_list_nb, (FSizeName_t *) pm_img_list);
         return;
      }
      floppy_confirm_index=0;
      pm_menu_currmenu=SM_FLOPPY_CONFIRM;
      pm_display_floppy_confirm(0);
     }
}

void pm_floppy_confirm_next()
{
  PM_INFO("Floppy Confirm : Next");
  floppy_confirm_index=floppy_confirm_index==0?1:0;  // Toggle the selection
  pm_display_floppy_confirm(floppy_confirm_index);  
}
 
void pm_floppy_confirm_prev()
{
  PM_INFO("Floppy Confirm : Previous\n");
  floppy_confirm_index=floppy_confirm_index==0?1:0;  // Toggle the selection
  pm_display_floppy_confirm(floppy_confirm_index);
}

void pm_floppy_confirm_select()
{
  if (floppy_confirm_index==0) 
     {
      PM_INFO("Floppy image %d confirmed: %s on A:\n", img_index, pm_img_list[img_index].name);
      SelectImage(0,img_index+1, (FSizeName_t *) pm_img_list);
      pm_disk_mount_fdd(0);    // Mount the FDD0
      pm_menu_currmenu=SM_FLOPPY;
      pm_sm_floppy_init();
     } else {
      PM_INFO("Floppy image %d confirmed: %s on B:\n", img_index, pm_img_list[img_index].name);
      SelectImage(1,img_index+1, (FSizeName_t *) pm_img_list);
      pm_disk_mount_fdd(1);    // Mount the FDD1
      pm_menu_currmenu=SM_FLOPPY;
      pm_sm_floppy_init();
     }
  pm_disk_update_fdd_nb();
}

void pm_info_select()
{
  PM_INFO("Display Info Menu: \n");
//  pm_display_info();
}

void pm_info_next()
{ 
  PM_INFO("Info Select : Next\n");
}

void pm_info_prev()
{
  PM_INFO("Info Select : Previous\n");
}

void pm_display_menu_selection() {

  switch (pm_menu_currmenu) {
    case SM_FLOPPY:
      pm_display_menu_floppy_select();
      break;
    case SM_INFO: 
      pm_display_menu_info_select();
      break;         
  }
  // For demonstration, we'll just print the current menu selection to the console
  PM_INFO("Menu: %d", pm_menu_currmenu);
}

void pm_sub_menu_next() {
  PM_INFO("Sub-menu: %d Next\n", pm_menu_currmenu);
  switch (pm_menu_currmenu) {
    case SM_FLOPPY:
      pm_floppy_next();
      break;
    case SM_INFO: 
      pm_info_next();
      break;
    case SM_FLOPPY_CONFIRM:
      pm_floppy_confirm_next();    
      break;            
  }  
}

void pm_sub_menu_prev() {
  PM_INFO("Sub-menu: %d Previous\n", pm_menu_currmenu);
  switch(pm_menu_currmenu) {
    case SM_FLOPPY:
      pm_floppy_prev();
      break;
    case SM_INFO: 
      pm_info_prev();
      break;
    case SM_FLOPPY_CONFIRM:
      pm_floppy_confirm_prev();
      break;
  }
}

void pm_sub_menu_select() {
  PM_INFO("Sub-menu: %d Selected\n", pm_menu_currmenu);
  switch(pm_menu_currmenu) {
    case SM_FLOPPY:
      PM_INFO("Floppy Selected");
      pm_floppy_select();
      break;
    case SM_INFO: 
      PM_INFO("Info Selected");
      pm_info_select();
      break;
    case SM_FLOPPY_CONFIRM:
      pm_floppy_confirm_select();
      break;
  }
}

void pm_sub_menu_back() {
  PM_INFO("Sub-menu: %d Back\n", pm_menu_currmenu);
  pm_menu_selected=false;
  switch(pm_menu_currmenu) {
    case SM_FLOPPY:
      pm_menu_home=true;
      pm_display_pmhome();
      PM_INFO("Menu: Back to Home\n");
      break;
    case SM_INFO: 
      pm_menu_home=true;
      pm_display_pmhome();
      PM_INFO("Menu: Back to Home\n");
      break;
    case SM_FLOPPY_CONFIRM:
      pm_menu_currmenu=SM_FLOPPY;
      pm_sm_floppy_init();
      break;
  } 
}

// called when the "Next" button is pressed
void pm_menu_next()
{
  if (!pm_menu_selected) {  
     pm_menu_lastmenu=pm_menu_currmenu;
     pm_menu_currmenu=(pm_menu_currmenu+1)%PM_MENU_COUNT;
     PM_INFO("Next Menu: %d\n",pm_menu_currmenu);
     pm_display_menu_selection();
    }
    else {
     pm_sub_menu_next();
    }
}

// called when the "Previous" button is pressed
void pm_menu_prev()
{
  if (!pm_menu_selected) {
    pm_menu_lastmenu=pm_menu_currmenu;
    if (pm_menu_currmenu==0) pm_menu_currmenu=PM_MENU_COUNT-1;
       else pm_menu_currmenu=(pm_menu_currmenu-1)%PM_MENU_COUNT;
    PM_INFO("Prev Menu: %d\n",pm_menu_currmenu);
    pm_display_menu_selection();
   } else {
    pm_sub_menu_prev();
   }
}

// called when the "Select" button is pressed
void pm_menu_select()
{
  if (!pm_menu_selected) {
  pm_menu_lastmenu=pm_menu_currmenu;
  pm_menu_selected=true;
  switch (pm_menu_currmenu) {
    case 0:
      pm_sm_floppy_init();
      PM_INFO("Floppy Menu Selected");
      break;
    case 1: 
      PM_INFO("Info Menu Selected");
      break;
  }
  PM_INFO("Menu: %d selected\n",pm_menu_currmenu);
  }
  else {
    pm_sub_menu_select();    
  }
}

// called when the "Back" button is pressed
void pm_menu_back()
{
  if (!pm_menu_selected) {  
     pm_menu_home=true;
     pm_display_pmhome();
     PM_INFO("Menu: Back to Home\n");
    } else {
     pm_sub_menu_back();
    }    
}

void pm_menu_button_event(uint8_t buttons)
{
  pm_menu_lastevent = to_ms_since_boot(get_absolute_time());
  pm_display_contrast(CONTRAST_HIGH);
  if (buttons & 0x01) // Button 1 > Select
  {
    pm_menu_select();
  }
  if (buttons & 0x02) // Button 2 > Black
  {
    pm_menu_back();
  }
  if (buttons & 0x04) // Button 3 > Previous
  {
    pm_menu_prev(); 
  }
  if (buttons & 0x08) // Button 4 > Next
  {
    pm_menu_next();
  }
}

