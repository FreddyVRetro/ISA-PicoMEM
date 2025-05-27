#include "dev_oled.h"
#include "pico/binary_info.h"
#include "string.h"

#define OLED_ON

t_OledParams dev_oled_screen;

//t_OledTxtScroller* dev_oled_scroller1 = NULL; //dev_oled_floppy_main;
//t_OledTxtScroller* dev_oled_scroller2 = NULL; //dev_oled_floppy_next;
//t_OledTxtScroller* dev_oled_scroller3 = NULL;

bool screen_active;
uint32_t screen_blanking_timer_start = 0;
uint32_t screen_blanking_time;

void configOled(t_OledParams *oled) {
#ifdef OLED_ON
    oled->i2c         = i2c0;
    oled->SDA_PIN     = 44;
    oled->SCL_PIN     = 45;

    oled->ctlrType    = CTRL_SH1106; // CTRL_SSD1306
    oled->i2c_address = 0x3C;
    oled->height      = H_64;
    oled->width       = W_132; //W_128

    // this will configure the OLED module and then
    //  clear the screen.
    oledI2cConfig(oled);
#endif
}

void dev_oled_setup_screen()
{
#ifdef OLED_ON
    screen_active = true;
    configOled(&dev_oled_screen);
#endif
}

void dev_oled_draw_welcome()
{
#ifdef OLED_ON
    oledClear(&dev_oled_screen, BLACK);
    const char *buf1 = "Pico";
    const char *buf2 = "MEM";
    oledDraw_string(&dev_oled_screen, 75, 14, buf1, DOUBLE_SIZE, WHITE);
    oledDraw_string(&dev_oled_screen, 80, 32, buf2, DOUBLE_SIZE, WHITE);

//    oledDraw_simple_picture(&dev_oled_screen,0,0,64,64,dev_oled_croco,false);
    oledDraw_simple_picture(&dev_oled_screen,0,0,64,64,dev_oled_elephant,false);    
    
    const char *buf3 = "Init.....";
    oledDraw_string(&dev_oled_screen, 65, 54, buf3, NORMAL_SIZE , WHITE);

    oledDisplay(&dev_oled_screen);
    //oledSet_scrolling(&dev_oled_screen, HORIZONTAL_LEFT, 0, 2, 0b100); // every 2 frames
   // dev_oled_scroller1->size = 0;
   // dev_oled_scroller2->size = 0;
#endif
}

void dev_oled_wizard_welcome()
{
#ifdef OLED_ON
    dev_oled_lightup_screen();
    oledSet_scrolling(&dev_oled_screen, NO_SCROLLING, 0, 0, 0);
    oledClear(&dev_oled_screen, BLACK);
    const char *buf1 = "~~ Welcome ~~";
    const char *buf2 = "~~ Bienvenue ~~";
    const char *buf3 = "~~ Bienvenido ~~";
    oledDraw_string(&dev_oled_screen, 0, 0, buf1, DOUBLE_SIZE, WHITE);
    oledDraw_string(&dev_oled_screen, 0, 32-8, buf2, DOUBLE_SIZE, WHITE);
    oledDraw_string(&dev_oled_screen, 0, 63-16, buf3, DOUBLE_SIZE, WHITE);
    oledDisplay(&dev_oled_screen);

/*    
    if (!dev_oled_scroller1) {
        dev_oled_scroller1 = (t_OledTxtScroller*)malloc(sizeof(t_OledTxtScroller));
    }
    if (!dev_oled_scroller2) {
        dev_oled_scroller2 = (t_OledTxtScroller*)malloc(sizeof(t_OledTxtScroller));
    }
    if (!dev_oled_scroller3) {
        dev_oled_scroller3 = (t_OledTxtScroller*)malloc(sizeof(t_OledTxtScroller));
    }
    strcpy(dev_oled_scroller1->name, buf1);
    dev_oled_scroller1->X = 0;
    dev_oled_scroller1->position = 0;
    dev_oled_scroller1->Y = 0;
    dev_oled_scroller1->size = strlen(dev_oled_scroller1->name);
    dev_oled_scroller1->increment = 4;
    dev_oled_scroller1->wait_loop = 0;
    dev_oled_scroller1->doublesize = true;

    strcpy(dev_oled_scroller2->name, buf2);
    dev_oled_scroller2->X = 0;
    dev_oled_scroller2->position = 0;
    dev_oled_scroller2->Y = 32-8;
    dev_oled_scroller2->size = strlen(dev_oled_scroller2->name);
    dev_oled_scroller2->increment = 4;
    dev_oled_scroller2->wait_loop = 0;
    dev_oled_scroller2->doublesize = true;

    strcpy(dev_oled_scroller3->name, buf3);
    dev_oled_scroller3->X = 0;
    dev_oled_scroller3->position = 0;
    dev_oled_scroller3->Y = 63-16;
    dev_oled_scroller3->size = strlen(dev_oled_scroller3->name);
    dev_oled_scroller3->increment = 4;
    dev_oled_scroller3->wait_loop = 0;
    dev_oled_scroller3->doublesize = true;
*/    
#endif
}

/*
void dev_oled_prepare_floppy(const char* name, uint8_t number, uint8_t count, char drive)
{
#ifdef OLED_ON  
    dev_oled_lightup_screen();
    oledSet_scrolling(&dev_oled_screen, NO_SCROLLING, 0, 0, 0);
    oledClear(&dev_oled_screen, BLACK);
    oledDraw_string(&dev_oled_screen, 0, 0, name, DOUBLE_SIZE, WHITE);
    
    char buffer[8];
    sprintf(buffer, "Drive %c", drive);
    oledDraw_string(&dev_oled_screen, 0, 42, buffer, NORMAL_SIZE, WHITE);

    sprintf(buffer,"%u/%u", number, count);
    oledDraw_string(&dev_oled_screen, 0, 8*7, buffer, NORMAL_SIZE, WHITE);
    oledDisplay(&dev_oled_screen);
    dev_oled_update_floppy(0, 0);
    if (!dev_oled_scroller1) {
        dev_oled_scroller1 = (t_OledTxtScroller*)malloc(sizeof(t_OledTxtScroller));
    }
    strcpy(dev_oled_scroller1->name, name);
    dev_oled_scroller1->X = 0;
    dev_oled_scroller1->position = 0;
    dev_oled_scroller1->Y = 0;
    dev_oled_scroller1->size = strlen(name);
    dev_oled_scroller1->increment = 4;
    dev_oled_scroller1->wait_loop = 0;
    dev_oled_scroller1->doublesize = true;
#endif
}

void dev_oled_display_next_floppy(char drv)
{
#ifdef OLED_ON 
    char *next = dev_sd_get_next_floppy(drv);
    if (!next)
        return;
    oledDraw_rectangle(&dev_oled_screen, 0 ,18, 128, 37, tFillmode::SOLID, BLACK);
    oledDraw_string(&dev_oled_screen, 0, 18, "Next disk:", NORMAL_SIZE, WHITE);
    oledDraw_string(&dev_oled_screen, 0, 28, next, NORMAL_SIZE, WHITE);
    oledDisplay(&dev_oled_screen);
    if (!dev_oled_scroller2) {
        dev_oled_scroller2 = (t_OledTxtScroller*)malloc(sizeof(t_OledTxtScroller));
    }
    strcpy(dev_oled_scroller2->name, next);
    dev_oled_scroller2->X = 0;
    dev_oled_scroller2->position = 0;
    dev_oled_scroller2->Y = 28;
    dev_oled_scroller2->size = strlen(next);
    dev_oled_scroller2->increment = 1;
    dev_oled_scroller2->wait_loop = 0;
    dev_oled_scroller2->doublesize = false;
#endif
}

void dev_oled_clear_next()
{
#ifdef OLED_ON 
    oledDraw_rectangle(&dev_oled_screen, 0 ,18, 128, 37, tFillmode::SOLID, BLACK);
    oledDisplay(&dev_oled_screen);
    if (dev_oled_scroller2)
        free(dev_oled_scroller2);
#endif
}

void dev_oled_update_floppy(uint8_t side, uint8_t track)
{
#ifdef OLED_ON 
    char buffer[9];
    oledDraw_rectangle(&dev_oled_screen, 80,8*7, 128, 64, tFillmode::SOLID, BLACK);
    sprintf(buffer,"S:%u T:%02u", side, track);
    oledDraw_string(&dev_oled_screen, 80, 8*7, buffer, NORMAL_SIZE, WHITE);
    oledDisplay(&dev_oled_screen);
#endif
}

void dev_oled_eject_floppy()
{
#ifdef OLED_ON    
    if (dev_oled_scroller1) 
        free(dev_oled_scroller1);
    dev_oled_draw_welcome();
#endif
}
*/

/*
void dev_oled_update_1_scroller(t_OledTxtScroller* scroller)
{
    uint8_t nbchars, pixelinc, hauteur;
    if (scroller->doublesize) {
        nbchars = 10;
        pixelinc = 12;
        hauteur = 15;
    } else {
        nbchars = 21;
        pixelinc = 6;
        hauteur = 9;
    }
    if (scroller->size > nbchars) {   // No need to scroll is <= 11 chars
        if (scroller->wait_loop) {
            scroller->wait_loop--;
        } else {
            uint8_t charScrollSize = scroller->size - nbchars;

//            if (scroller == dev_oled_scroller1)
//                printf("%u\n", charScrollSize);

            uint16_t pixelScrollSize = charScrollSize * pixelinc;
            scroller->position += scroller->increment;
            if (scroller->position > pixelScrollSize) {
                scroller->position = scroller->X;
                scroller->wait_loop = 7;     // We are at the begining, do not start strait away
            } else if (scroller->position + scroller->increment > pixelScrollSize) {
                scroller->wait_loop = 4;     // We are at the end of the scrool, next loop will reset
            }
            
            uint8_t charPosition = scroller->position / pixelinc;
            char display[nbchars+2];
            memcpy(display, scroller->name + charPosition, nbchars);
            display[nbchars] = '\0';
            oledDraw_rectangle(&dev_oled_screen, 0, scroller->Y, 128, scroller->Y+hauteur, tFillmode::SOLID, BLACK);
            oledDraw_string(&dev_oled_screen, (scroller->position % pixelinc)*(-1), scroller->Y, display, DOUBLE_SIZE, WHITE);
            oledDisplay(&dev_oled_screen);
        }
    }
}

void dev_oled_update_scrollers()
{
#ifdef OLED_ON
    if (dev_oled_scroller1) {
        dev_oled_update_1_scroller(dev_oled_scroller1);
    }
    if (dev_oled_scroller2) {
        dev_oled_update_1_scroller(dev_oled_scroller2);
    }
    if (dev_oled_scroller3) {
        dev_oled_update_1_scroller(dev_oled_scroller3);
    }
    screen_blanking_time = time_us_32() - screen_blanking_timer_start;
    if (!dev_floppy_get_active() && screen_blanking_time > 1000000 * 15)
    {
        if (screen_active)
            dev_oled_blank_screen();
        screen_blanking_timer_start = time_us_32();
    }
    if (dev_floppy_get_active())
    {
        if (!screen_active)
            dev_oled_lightup_screen();
        screen_blanking_timer_start = time_us_32();
    }
#endif
}

*/

void dev_oled_blank_screen()
{
    screen_active = false;
    oledSet_contrast(&dev_oled_screen, 0x00);
}

void dev_oled_lightup_screen()
{
    screen_active = true;
    oledSet_contrast(&dev_oled_screen, 0x7F);
    screen_blanking_timer_start = time_us_32();
}

void dev_oled_sd_warning()
{
#ifdef OLED_ON 
    dev_oled_lightup_screen();
    oledSet_scrolling(&dev_oled_screen, NO_SCROLLING, 0, 0, 0);
    oledClear(&dev_oled_screen, BLACK);
    oledDraw_line(&dev_oled_screen, 64-39,62, 64+39,62, WHITE);
    oledDraw_line(&dev_oled_screen, 64-39,62, 64,0, WHITE);
    oledDraw_line(&dev_oled_screen, 64+39,62, 64,0, WHITE);
    const char *buf1 = "NO";
    const char *buf2 = "uSD";
    const char *buf3 = "Warning";
    oledDraw_string(&dev_oled_screen, 64-12, 24, buf1, DOUBLE_SIZE, WHITE);
    oledDraw_string(&dev_oled_screen, 64-18, 42, buf2, DOUBLE_SIZE, WHITE);
    oledDraw_string(&dev_oled_screen, 0, 8, buf3, NORMAL_SIZE, WHITE);
    oledDraw_string(&dev_oled_screen, 128-(7*6), 8, buf3, NORMAL_SIZE, WHITE);
    oledDisplay(&dev_oled_screen);
#endif
}

void dev_oled_blick_screen()
{
#ifdef OLED_ON 
    oledSet_invert(&dev_oled_screen, true);
    sleep_ms(1000);
    oledSet_invert(&dev_oled_screen, false);
    sleep_ms(1000);
#endif
}