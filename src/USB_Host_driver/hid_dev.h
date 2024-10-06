#pragma once
// Structures defining the HID Devices data for the PicoMEM

typedef struct pm_mouse_t {
     bool event;
     int8_t x;              // 8 Bit X (from latest sent position) 
     int8_t y;              // 8 Bit X (from latest sent position) 
     int8_t prevx;          // Previous x value (To update event)
     int8_t prevy;          // Previous y value (To update event)
     int16_t xl;            // 16 Bit X (from latest sent position)
     int16_t yl;            // 16 Bit Y (from latest sent position) 
     int8_t div;            // Speed (Divisor)
     uint8_t buttons;
     } pm_mouse_t;


typedef struct {
    uint8_t joy1_x;
    uint8_t joy1_y;
    uint8_t joy2_x;
    uint8_t joy2_y;
    uint8_t button_mask;
} joystate_struct_t;