
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pm_board.h"     // PicoMEM Board Definitions (GPIO)
#include "../pm_debug.h"
#include "ff.h"           // forr FIL definition
#include "pm_defines.h"
#include "pm_disk.h"
#include "pm_display.h"
#include "pm_menu.h"
#include "../pm_libs/pm_bluetooth.h"
#include "../pm_libs/pm_bluetooth_scan.h"
#include "../isa_devices/cdrom/cd_player.h"
#include "../isa_devices/dev_cdrom_mke.h"
#include "../isa_devices/cdrom/cdrom_error_msg.h"

#define PM_MENU_COUNT 4           // Number of main, menus
#define PM_BUTTON_NEXT     0x01
#define PM_BUTTON_SELECT   0x02
#define PM_BUTTON_BACK     0x04
#define PM_BUTTON_PREVIOUS 0x08

bool pm_menu_home=true;
bool pm_menu_selected=false;
uint8_t pm_menu_lastmenu;
uint8_t pm_menu_currmenu=0;
uint32_t pm_menu_lastevent;  // Time when the last event occur

FSizeName_t pm_img_list[129];  //+1 to add none
uint8_t img_list_type=0;       // 0: none 1: Floppy, 2: CD, 3: CD Audio
uint8_t img_list_nb=0;
uint8_t img_index=0;
uint8_t floppy_confirm_index=0;
uint8_t cdrom_confirm_index=0;
bool cdrom_confirm_eject=false;
static uint8_t bluetooth_device_count_displayed;

void pm_sm_bluetooth_init()
{
  uint8_t device_count = pm_bluetooth_device_count();

  PM_INFO("Display Bluetooth device menu\n");
  img_list_type = 4;
  img_list_nb = device_count;
  img_index = 0;
  bluetooth_device_count_displayed = device_count;
  pm_bluetooth_scan_start();

  for (uint8_t index = 0; index < device_count; ++index) {
    const pm_bluetooth_device_t *device = pm_bluetooth_get_device(index);
    const char *type = device->type == PM_BLUETOOTH_DEVICE_MOUSE ? "MSE" :
                device->type == PM_BLUETOOTH_DEVICE_KEYBOARD ? "KBD" :
           device->type == PM_BLUETOOTH_DEVICE_JOYPAD ? "PAD" :
               device->type == PM_BLUETOOTH_DEVICE_HEADPHONES ? "HPH" :
               device->type == PM_BLUETOOTH_DEVICE_SPEAKER ? "SPK" :
               device->type == PM_BLUETOOTH_DEVICE_PHONE ? "PHN" :
               device->type == PM_BLUETOOTH_DEVICE_COMPUTER ? "PC" :
               device->type == PM_BLUETOOTH_DEVICE_WATCH ? "WCH" :
               device->type == PM_BLUETOOTH_DEVICE_HEART_RATE ? "HRT" :
               device->type == PM_BLUETOOTH_DEVICE_THERMOMETER ? "TMP" :
               device->type == PM_BLUETOOTH_DEVICE_HID ? "HID" : "DEV";
    snprintf(pm_img_list[index].name, sizeof(pm_img_list[index].name),
             "%.9s %.3s", device->name, type);
    pm_img_list[index].size = index;
  }

  if (img_list_nb == 0) {
    strcpy(pm_img_list[0].name, "Scanning...");
    pm_img_list[0].size = 0;
    img_list_nb = 1;
  }

  pm_display_img_list(img_index, img_list_nb, (FSizeName_t *)pm_img_list);
}

void pm_menu_bluetooth_refresh(void)
{
  uint8_t device_count;

  if (!pm_menu_selected || pm_menu_currmenu != SM_BLUETOOTH) {
    return;
  }

  device_count = pm_bluetooth_device_count();
  if (device_count != bluetooth_device_count_displayed) {
    pm_sm_bluetooth_init();
  }
}

void pm_bluetooth_next()
{
  if (img_index < img_list_nb - 1) {
    ++img_index;
    pm_display_img_list(img_index, img_list_nb, (FSizeName_t *)pm_img_list);
  }
}

void pm_bluetooth_prev()
{
  if (img_index > 0) {
    --img_index;
    pm_display_img_list(img_index, img_list_nb, (FSizeName_t *)pm_img_list);
  }
}

void pm_bluetooth_select()
{
  if (img_index >= pm_bluetooth_device_count()) {
    pm_sm_bluetooth_init();
    return;
  }

  if (!pm_bluetooth_connect_device(img_index)) {
    PM_INFO("[BT] OLED selection %d cannot pair\n", img_index);
  }
  pm_sm_bluetooth_init();
}

void pm_sm_cdrom_audio_init(bool keep_index)
{
  PM_INFO("Display CD Audio Select Menu: \n");
  img_list_type = 3;
  img_list_nb = 0;
  if (!keep_index) img_index = 0;

  uint8_t last_track = cdplayer_get_last_track_number();
  PM_INFO("[MENU] CD Audio init: last_track=%d\n", last_track);
  for (uint8_t track = 1; track <= last_track && img_list_nb < (sizeof(pm_img_list) / sizeof(pm_img_list[0])); track++) {
    cdplayer_track_t info;
    if (!cdplayer_get_track_info(track, &info))
      continue;  // data track, or track does not exist
    snprintf(pm_img_list[img_list_nb].name, sizeof(pm_img_list[img_list_nb].name),
             "T%02u %02u:%02u", info.number, info.min, info.sec);
    pm_img_list[img_list_nb].size = info.number;
    img_list_nb++;
  }
  if (img_index>=img_list_nb) img_index=img_list_nb-1;  // Check if index is valid  
  PM_INFO("[MENU] CD Audio init: img_list_nb=%d\n", img_list_nb);

  if (img_list_nb == 0) {
    const char no_tracks[] = "No CD Audio";
    strncpy(pm_img_list[0].name, no_tracks, sizeof(pm_img_list[0].name) - 1);
    pm_img_list[0].name[sizeof(pm_img_list[0].name) - 1] = '\0';
    pm_img_list[0].size = 0;
    img_list_nb = 1;
    img_index = 0;
  }

  pm_display_img_list(img_index, img_list_nb, (FSizeName_t *) pm_img_list);
}

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

void pm_sm_cdrom_init()
{
  PM_INFO("Display CD-ROM Select Menu: \n");
  if (img_list_type!=2) {
     img_list_type=2;
     pm_cd_InitSearchPath();
     pm_img_add_none();
     ListImages(3,&img_list_nb,(void *) &pm_img_list[1]);
     if (img_list_nb==0) PM_INFO("No CD-ROM Image found\n");
      else PM_INFO("%d CD-ROM Images found\n",img_list_nb);
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

void pm_cdrom_next()
{
  PM_INFO("CD-ROM : Next");
  if (img_index<img_list_nb-1) 
     {
      img_index++;
      pm_display_img_list(img_index, img_list_nb, (FSizeName_t *) pm_img_list);
     }
}

void pm_cdrom_prev()
{
  PM_INFO("CD-ROM : Previous");
  if (img_index>0) 
     {
      img_index--;
      pm_display_img_list(img_index, img_list_nb, (FSizeName_t *) pm_img_list);
     }
}

void pm_cdrom_audio_next()
{
  PM_INFO("CD-ROM Audio : Next");
  if (img_index<img_list_nb-1) 
     {
      img_index++;
      pm_display_img_list(img_index, img_list_nb, (FSizeName_t *) pm_img_list);
     }
}

void pm_cdrom_audio_prev()
{
  PM_INFO("CD-ROM Audio : Previous");
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

#if USE_CDROM
void pm_cdrom_select()
{
  PM_INFO("CD-ROM : Select");
  if (img_list_nb>0) 
     {
      PM_INFO("CD-ROM image %d selected: %s\n", img_index, pm_img_list[img_index].name);
      if (pm_img_list[img_index].size==0xFFFE) { // Folder
         PM_INFO("Folder selected: %s\n",pm_img_list[img_index].name);
         pm_cd_UpdateSearchPath(img_index);
         ListImages(3,&img_list_nb,(void *) &pm_img_list[1]);
         img_index=0;
         img_list_nb++;  // +1 to add the "None" at the beginning of the list
         pm_display_img_list(img_index, img_list_nb, (FSizeName_t *) pm_img_list);
         return;
      }
      cdrom_confirm_index = 0;
      cdrom_confirm_eject = (img_index==0);
      pm_menu_currmenu = SM_CDROM_CONFIRM;
      pm_display_cdrom_confirm(cdrom_confirm_eject, cdrom_confirm_index);
     }
}
#endif

#if USE_CD_AUDIO
void pm_cdrom_audio_select()
{
  PM_INFO("CD-ROM Audio : Select track %d\n", img_index);
  if (img_list_nb==0) return;

  if (img_list_nb == 1 && pm_img_list[0].size == 0) {
    PM_INFO("CD-ROM Audio : No tracks available\n");
    pm_display_cdrom_audio_status(0, "Error", "No CD Audio");
    pm_menu_currmenu = SM_CDROM_AUDIO_STATUS;
    pm_menu_selected = false;
    return;
  }

  uint8_t selected_track = (uint8_t) pm_img_list[img_index].size;
  if (selected_track == 0) {
    PM_INFO("CD-ROM Audio : Invalid track selected\n");
    pm_display_cdrom_audio_status(0, "Error", "Invalid track");
    pm_menu_currmenu = SM_CDROM_AUDIO_STATUS;
    pm_menu_selected = false;
    return;
  }

  if (cdplayer_play(selected_track)) {
    pm_display_cdrom_audio_status(selected_track, "Playing", "S=Pause  B=Back");
  } else {
    pm_display_cdrom_audio_status(selected_track, "Error", "Play failed");
  }
  pm_menu_currmenu = SM_CDROM_AUDIO_STATUS;
  pm_menu_selected = false;
}


// Buttons while the CD audio status screen is shown: Select=pause/resume,
// Back=stop (return to track list), Previous=rewind 10s, Next=forward 10s.
// Once playback has stopped/completed on its own, any button returns to
// the track list instead.
void pm_cdrom_audio_status_event(uint8_t buttons)
{
  cdplayer_status_t status = cdplayer_get_status();
  uint8_t track = (uint8_t) pm_img_list[img_index].size;

  if (status == CDPLAYER_STATUS_NO_AUDIO || status == CDPLAYER_STATUS_STOPPED ||
      status == CDPLAYER_STATUS_COMPLETED) {
    pm_menu_currmenu = SM_CDROM_AUDIO;
    pm_menu_selected = true;
    pm_sm_cdrom_audio_init(false);
    return;
  }

  if (buttons & PM_BUTTON_BACK) { // Back : Return to the track list
    if (status == CDPLAYER_STATUS_PAUSED)  // Stop if the music replay is pause
    {
      PM_INFO("CD-ROM Audio : Stop playback\n");
      cdplayer_stop();
    }
    pm_menu_currmenu = SM_CDROM_AUDIO;
    pm_menu_selected = true;
    pm_sm_cdrom_audio_init(true);  // Keep the current track index when returning to the track list
    return;
  }
  if (buttons & PM_BUTTON_SELECT) { // Select : Pause/resume toggle
    if (status == CDPLAYER_STATUS_PLAYING) {
      cdplayer_pause();
      pm_display_cdrom_audio_status(track, "Paused ", "S=Resume B=Back");
    } else if (status == CDPLAYER_STATUS_PAUSED) {
      cdplayer_resume();
      pm_display_cdrom_audio_status(track, "Playing", "S=Pause  B=Back");
    }
    return;
  }
  if (buttons & PM_BUTTON_PREVIOUS) { // Previous : rewind 10s
    cdplayer_rewind();
    pm_display_cdrom_audio_status(track, status == CDPLAYER_STATUS_PAUSED ? "Paused " : "Playing", "Rewind -10s");
    return;
  }
  if (buttons & PM_BUTTON_NEXT) { // Next : fast forward 10s
    cdplayer_fast_forward();
    pm_display_cdrom_audio_status(track, status == CDPLAYER_STATUS_PAUSED ? "Paused " : "Playing", "Forward +10s");
    return;
  }
}

void pm_menu_cdrom_audio_worker(void)
{
  if (pm_menu_currmenu != SM_CDROM_AUDIO_STATUS || pm_menu_selected ||
      cdplayer_get_status() != CDPLAYER_STATUS_COMPLETED) {
    return;
  }

  if (img_index + 1 < img_list_nb) {
    uint8_t next_track;

    ++img_index;
    next_track = (uint8_t) pm_img_list[img_index].size;
    PM_INFO("CD-ROM Audio: track completed, playing track %u\n", next_track);
    if (cdplayer_play(next_track)) {
      pm_display_cdrom_audio_status(next_track, "Playing", "S=Pause  B=Back");
    } else {
      PM_ERROR("CD-ROM Audio: failed to play next track %u\n", next_track);
      cdplayer_stop();
      pm_display_cdrom_audio_status(next_track, "Error", "Next track failed");
    }
    return;
  }

  PM_INFO("CD-ROM Audio: final track completed\n");
  cdplayer_stop();
  pm_display_cdrom_audio_status((uint8_t) pm_img_list[img_index].size,
                                "Finished", "B=Back");
}
#endif // USE_CD_AUDIO

#if USE_CDROM
void pm_cdrom_confirm_next()
{
  PM_INFO("CD-ROM Confirm : Next");
  cdrom_confirm_index = cdrom_confirm_index==0?1:0; // Toggle
  pm_display_cdrom_confirm(cdrom_confirm_eject, cdrom_confirm_index);
}

void pm_cdrom_confirm_prev()
{
  PM_INFO("CD-ROM Confirm : Previous\n");
  cdrom_confirm_index = cdrom_confirm_index==0?1:0; // Toggle
  pm_display_cdrom_confirm(cdrom_confirm_eject, cdrom_confirm_index);
}

void pm_cdrom_confirm_select()
{
  if (cdrom_confirm_index==0) {
    if (cdrom_confirm_eject) {
      PM_INFO("CD-ROM Ejected\n");
      SelectImage(6,0, (FSizeName_t *) pm_img_list);
#if USE_CDROM
      cdrom_errorstr_clear();
      dev_cdr_mount_from_config();
      if (cdrom_errorstr_is_set()) {
        pm_display_cdrom_status(false, cdrom_errorstr_get());
      } else {
        pm_display_cdrom_status(true, "CD Ejected");
      }
#else
      pm_display_cdrom_status(true, "CD Ejected");
#endif
    } else {
      PM_INFO("CD-ROM image %d confirmed: %s\n", img_index, pm_img_list[img_index].name);
      SelectImage(6,img_index+1, (FSizeName_t *) pm_img_list);
#if USE_CDROM
      cdrom_errorstr_clear();
      dev_cdr_mount_from_config();
      if (cdrom_errorstr_is_set()) {
        pm_display_cdrom_status(false, cdrom_errorstr_get());
      } else {
        pm_display_cdrom_status(true, "CD Mounted");
      }
#else
      pm_display_cdrom_status(true, "CD Mounted");
#endif
    }
    pm_menu_currmenu = SM_CDROM_STATUS;
    pm_menu_selected = false;
  } else {
    if (cdrom_confirm_eject) {
      PM_INFO("CD-ROM eject canceled\n");
    } else {
      PM_INFO("CD-ROM mount canceled\n");
    }
    pm_menu_currmenu = SM_CDROM;
    pm_sm_cdrom_init();
  }
}
#endif // USE_CDROM

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
#if USE_CDROM      
    case SM_CDROM:
      pm_display_menu_cdrom_select();
      break;
#endif      
    case SM_INFO: 
      pm_display_menu_info_select();
      break;
    case SM_BLUETOOTH:
      pm_display_menu_bluetooth_select();
      break;
#if USE_CD_AUDIO
    case SM_CDROM_AUDIO:
      pm_display_menu_cdaudio_select();
      break;         
  }
#endif
  // For demonstration, we'll just print the current menu selection to the console
  PM_INFO("Menu: %d", pm_menu_currmenu);
}

void pm_sub_menu_next() {
  PM_INFO("Sub-menu: %d Next\n", pm_menu_currmenu);
  switch (pm_menu_currmenu) {
    case SM_FLOPPY:
      pm_floppy_next();
      break;     
#if USE_CDROM
    case SM_CDROM:
      pm_cdrom_next();
      break;
    case SM_CDROM_CONFIRM:
      pm_cdrom_confirm_next();
      break;
#endif
#if USE_CD_AUDIO
    case SM_CDROM_AUDIO:
      pm_cdrom_audio_next();
      break;
#endif
    case SM_INFO: 
      pm_info_next();
      break;
    case SM_BLUETOOTH:
      pm_bluetooth_next();
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
#if USE_CDROM      
    case SM_CDROM:
      pm_cdrom_prev();
      break;
    case SM_CDROM_CONFIRM:
      pm_cdrom_confirm_prev();
      break;
#endif
#if USE_CD_AUDIO
    case SM_CDROM_AUDIO:
      pm_cdrom_audio_prev();
      break;
#endif
    case SM_INFO: 
      pm_info_prev();
      break;
    case SM_BLUETOOTH:
      pm_bluetooth_prev();
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
#if USE_CDROM
    case SM_CDROM:
      PM_INFO("CD-ROM Selected");
      pm_cdrom_select();
      break;
    case SM_CDROM_CONFIRM:
      pm_cdrom_confirm_select();
      break;  
#endif
#if USE_CD_AUDIO
    case SM_CDROM_AUDIO:
      PM_INFO("CD-ROM Audio Selected");
      pm_cdrom_audio_select();
      break;
#endif
    case SM_INFO: 
      PM_INFO("Info Selected");
      pm_info_select();
      break;
    case SM_BLUETOOTH:
      pm_bluetooth_select();
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
#if USE_CDROM
    case SM_CDROM:
      pm_menu_home=true;
      pm_display_pmhome();
      PM_INFO("Menu: Back to Home\n");
      break;
    case SM_CDROM_CONFIRM:
      pm_menu_currmenu=SM_CDROM;
      pm_sm_cdrom_init();
      break;  
#endif
#if USE_CD_AUDIO
    case SM_CDROM_AUDIO:
      pm_menu_home=true;
      pm_display_pmhome();
      PM_INFO("Menu: Back to Home\n");
      break;
#endif      
    case SM_INFO: 
      pm_menu_home=true;
      pm_display_pmhome();
      PM_INFO("Menu: Back to Home\n");
      break;
    case SM_BLUETOOTH:
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
    case SM_FLOPPY:
      pm_sm_floppy_init();
      PM_INFO("Floppy Menu Selected");
      break;
#if USE_CDROM
    case SM_CDROM:
      pm_sm_cdrom_init();
      PM_INFO("CD-ROM Menu Selected");
      break;
#endif
#if USE_CD_AUDIO
    case SM_CDROM_AUDIO:
      pm_sm_cdrom_audio_init(false);
      PM_INFO("CD-ROM Audio Menu Selected");
      break;
#endif
    case SM_INFO: 
      PM_INFO("Info Menu Selected");
      break;
    case SM_BLUETOOTH:
      pm_sm_bluetooth_init();
      PM_INFO("Bluetooth Menu Selected");
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

  PM_INFO("Menu Button Event: buttons=0x%02X\n", buttons);

  pm_menu_lastevent = to_ms_since_boot(get_absolute_time());
  pm_display_contrast(CONTRAST_HIGH);

  #if USE_CDROM
  if (pm_menu_currmenu == SM_CDROM_STATUS && buttons != 0) {
    pm_menu_currmenu = SM_CDROM;
    pm_menu_selected = true;
    pm_sm_cdrom_init();
    return;
  }
  #endif
  
  #if USE_CD_AUDIO
  if (pm_menu_currmenu == SM_CDROM_AUDIO_STATUS && buttons != 0) {
    pm_cdrom_audio_status_event(buttons);
    return;
  }
  #endif

  if (buttons & PM_BUTTON_SELECT)
  {
    pm_menu_select();
  }
  if (buttons & PM_BUTTON_BACK)
  {
    pm_menu_back();
  }
  if (buttons & PM_BUTTON_PREVIOUS)
  {
    pm_menu_prev(); 
  }
  if (buttons & PM_BUTTON_NEXT)
  {
    pm_menu_next();
  }
}

