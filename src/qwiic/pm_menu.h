#ifndef PM_MENU_H
#define PM_MENU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SM_FLOPPY 0
#if USE_CDROM
#define SM_CDROM  1
#endif
#define SM_CDROM_AUDIO 2
#define SM_INFO   3
#define SM_BLUETOOTH 4
#define SM_FLOPPY_CONFIRM 5
#if USE_CDROM
#define SM_CDROM_CONFIRM 6
#define SM_CDROM_STATUS 7
#endif
#if USE_CD_AUDIO
#define SM_CDROM_AUDIO_STATUS 8
#endif

void pm_menu_button_event(uint8_t buttons);
void pm_menu_bluetooth_refresh(void);
#if USE_CD_AUDIO
void pm_menu_cdrom_audio_worker(void);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // PM_MENU_H