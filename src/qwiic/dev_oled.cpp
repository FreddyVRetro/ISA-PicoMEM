#include "dev_oled.h"
#include "pico/binary_info.h"
#include "string.h"
#include "pm_board.h"     // PicoMEM Board Definitions (GPIO)
#include "../pm_debug.h"
//#include "pm_display.h"
#include "pm_i2c.h"

#define OLED_ON

t_OledParams dev_oled_screen;

//t_OledTxtScroller* dev_oled_scroller1 = NULL; //dev_oled_floppy_main;
//t_OledTxtScroller* dev_oled_scroller2 = NULL; //dev_oled_floppy_next;
//t_OledTxtScroller* dev_oled_scroller3 = NULL;