#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define D_INIT      0   // PicoMEM Logo, init Display
#define D_PCPOST    1   // POST Code display event
#define D_PMPOST    2   // PicoMEM POST messages event
#define D_PMHOME    3   // PicoMEM Home Screen
#define D_PAD       4   // Gamepad/Joystick event display


#define D_CMS       7   // CMS Screen (when active)
#define D_ADLIB     6   // Adlib Screen (when active)
#define D_SB        5   // Sound Blaster Screen (when active)
#define D_GUS       8   // Gravis Ultrasound Screen (when active)
#define D_GMIDI     9   // General MIDI Screen (when active)

#define D_AUDIO_BAR 20  // 32 vertical volume bar
#define D_AUDIO_OSC 21  // Audio oscilloscope

#define D_FLOPPY_S  30  // Floppy Select Menu
#define D_INFO_S    31  // Info Select Menu

#define D_IMG_LIST  40
#define D_FDD_CONF  41  // Floppy confirm

#define CONTRAST_HIGH 0x80
#define CONTRAST_LOW  0x20
#define CONTRAST_OFF  0x00

typedef struct pm_display_t {
   bool enabled;                // True when available
   uint8_t screen;
   uint8_t prev_screen;
   uint8_t next_screen;
   bool    temporary_screen;       // True if the current screen is temporary (like POST code) and should return to prev_screen after a delay
   bool    interactive;            // Tell if the current screen is interactive
   volatile uint8_t hc_delay;   // counter for the high contrast timeout
   uint8_t temporary_delay;     // delay before going back to the previous screen (1/10th of second)
   volatile uint8_t contrast;   // Actual brightness level
} pm_display_t;

extern pm_display_t pm_display;

bool pm_display_init(uint8_t oled_type);

void pm_display_img_list(uint8_t img_index, uint8_t img_list_nb, FSizeName_t *pm_img_list);
void pm_display_floppy_confirm(uint8_t choice);

void pm_display_pmhome();
void pm_display_menu_floppy_select();
void pm_display_menu_info_select();

void pm_display_event(uint8_t ScreenID, uint8_t code1, uint8_t code2, char *event_msg);
void pm_display_worker(uint32_t ms_since_last);
void pm_display_blink();
void pm_display_contrast(uint8_t contrast);

#ifdef __cplusplus
}  // extern "C"
#endif