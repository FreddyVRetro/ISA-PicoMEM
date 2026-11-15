#pragma once

// LED Error codes
#define PM_ERR_NOPSRAM 1  //PSRAM not found
#define PM_ERR_MEM     2  

//#if PM_DEBUG
//#define PM_PRINTF pm_printf
//#else
//#define PM_PRINTF(fmt, args...) /* Don't do anything in release builds*/
//#endif

extern void pm_timefrominit();
extern void pm_timefromlast();

extern void gpio_move_5s (uint8_t pin);

__force_inline void gpio_init_put(uint8_t pin, uint8_t value)
{
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_OUT);
  gpio_put(pin, value);
}

extern void pm_led(uint8_t onoff);
extern void err_blink(uint8_t nblong, uint8_t nbshort,uint8_t nbrepeat);
//uint8_t pm_mountdiskimage(char *diskfilename, uint8_t floppynb, uint8_t hddnumber);

extern void pm_display_claimed_pio_sm();
extern void joy_test();

#if defined(RASPBERRYPI_PICO2) || defined(PIMORONI_PICO_PLUS2_RP2350)
extern void measure_freqs(void);
extern void p2_psram_test();
#endif