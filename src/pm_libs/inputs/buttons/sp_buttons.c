// Serdaco Panel buttons  I2C plus 5 Buttons
//
// Buttons layout: 
//  1  2
//  3  4

#include <stdio.h>
#include "pm_debug.h"
#include "pm_board.h"

#include "inputs/router/router.h"
#include "inputs/input_event.h"

void sp_init() {
#if FP_HEADER  
    PM_INFO("SP Buttons Init\n");
    // Initialize GPIOs for buttons (Assuming active-low buttons with pull-ups)
    gpio_init(FPH_IO1);
    gpio_set_dir(FPH_IO1, GPIO_IN);
    gpio_pull_up(FPH_IO1);

    gpio_init(FPH_IO2);
    gpio_set_dir(FPH_IO2, GPIO_IN);
    gpio_pull_up(FPH_IO2);

    gpio_init(FPH_IO3);
    gpio_set_dir(FPH_IO3, GPIO_IN);  
    gpio_pull_up(FPH_IO3);

    gpio_init(FPH_IO4);
    gpio_set_dir(FPH_IO4, GPIO_IN);
    gpio_pull_up(FPH_IO4);

    gpio_init(FPH_IO5);
    gpio_set_dir(FPH_IO5, GPIO_IN);
    gpio_pull_up(FPH_IO5);
#endif    
}

void sp_task()
{
#if FP_HEADER  
  static uint32_t prev_buttons = 0;
  uint32_t buttons=!gpio_get(FPH_IO1) | (!gpio_get(FPH_IO2)<<1) | (!gpio_get(FPH_IO3)<<2) | (!gpio_get(FPH_IO4)<<3) | (!gpio_get(FPH_IO5)<<4);
  
  if (buttons != prev_buttons) {
    prev_buttons = buttons;
    PM_INFO("Panel Buttons: %05b\n", buttons);
    input_event_t event = {
        .dev_addr = 0,
        .instance = 0,
        .type = INPUT_TYPE_PANEL,
        .transport = INPUT_TRANSPORT_NONE,
        .buttons = buttons,
        .button_count = 4,
        .analog = 0,
        .keys = 0,
        .has_motion = false,
        .has_pressure = false
      };
    router_submit_input(&event);    
  }
#endif
}