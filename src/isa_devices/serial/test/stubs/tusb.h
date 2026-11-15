// Host-sim stub for tusb.h -- just the HID mouse report type + button bits
// (layout matches TinyUSB's hid.h).
#pragma once
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t buttons;
    int8_t  x;
    int8_t  y;
    int8_t  wheel;
    int8_t  pan;
} hid_mouse_report_t;

enum {
    MOUSE_BUTTON_LEFT     = 1,
    MOUSE_BUTTON_RIGHT    = 2,
    MOUSE_BUTTON_MIDDLE   = 4,
    MOUSE_BUTTON_BACKWARD = 8,
    MOUSE_BUTTON_FORWARD  = 16,
};
