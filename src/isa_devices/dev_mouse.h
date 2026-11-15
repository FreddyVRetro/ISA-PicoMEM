#pragma once

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

extern volatile pm_mouse_t pm_mouse;

// USB mouse -> PS/2 aux packet path (for Windows / any PS/2-mouse driver).
// Accumulate deltas + buttons as they arrive, then publish one coalesced PS/2
// packet into the DPRAM mailbox each tick (when the previous one is consumed).
// jp_buttons is the JP_BUTTON_* encoding held in pm_mouse.buttons.
void pm_mouse_ps2_accumulate(int dx, int dy, uint8_t jp_buttons);
void pm_mouse_ps2_tick(void);