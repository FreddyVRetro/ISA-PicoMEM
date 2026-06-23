// ms_sidewinder_commander.c - Microsoft SideWinder Strategic Commander
// VID: 0x045E  PID: 0x0033
//
// 90s PC RTS command controller with tilt X/Y axes, twist Rz axis,
// 12 buttons, and a 3-position toggle switch.
//
// Bit layout (48 bits / 6 bytes):
//   Bits  0-9:  X axis (10-bit signed, tilt L/R)
//   Bits 10-19: Y axis (10-bit signed, tilt F/B)
//   Bits 20-29: Rz axis (10-bit signed, twist rotation)
//   Bits 30-31: Constant padding
//   Bits 32-43: Buttons 1-12 (individual bits)
//   Bits 44-45: 3-position toggle (2-bit array: 1=pos1, 2=pos2, 3=pos3)
//   Bits 46-47: Constant padding
//
// SPDX-License-Identifier: Apache-2.0

#include "ms_sidewinder_commander.h"
#include "inputs/buttons.h"
#include "inputs/router/router.h"
#include "inputs/input_event.h"
#include <string.h>
#include "../../../pm_debug.h"

#define MICROSOFT_VID       0x045E
#define COMMANDER_PID       0x0033

static uint8_t prev_report[CFG_TUH_DEVICE_MAX + 1][CFG_TUH_HID][COMMANDER_REPORT_SIZE];
static uint8_t desired_leds[CFG_TUH_DEVICE_MAX + 1][CFG_TUH_HID][2];   // Desired LED state
static uint8_t sent_leds[CFG_TUH_DEVICE_MAX + 1][CFG_TUH_HID][2];     // Last sent LED state

static bool is_ms_sidewinder_commander(uint16_t vid, uint16_t pid) {
    return (vid == MICROSOFT_VID && pid == COMMANDER_PID);
}

static bool init_ms_sidewinder_commander(uint8_t dev_addr, uint8_t instance) {
    PM_INFO("[Commander] Device mounted: dev_addr=%d, instance=%d\n", dev_addr, instance);
    memset(&prev_report[dev_addr][instance], 0, COMMANDER_REPORT_SIZE);
    memset(&desired_leds[dev_addr][instance], 0, 2);
    memset(&sent_leds[dev_addr][instance], 0xFF, 2);  // Force initial send
    return true;
}

static void unmount_ms_sidewinder_commander(uint8_t dev_addr, uint8_t instance) {
    PM_INFO("[Commander] Device unmounted: dev_addr=%d, instance=%d\n", dev_addr, instance);
    memset(&desired_leds[dev_addr][instance], 0, 2);
    memset(&sent_leds[dev_addr][instance], 0xFF, 2);
}

// Task: send LED state if it changed (must run outside report callback)
static void task_ms_sidewinder_commander(uint8_t dev_addr, uint8_t instance,
                                          device_output_config_t* config) {
    (void)config;

    if (desired_leds[dev_addr][instance][0] == sent_leds[dev_addr][instance][0] &&
        desired_leds[dev_addr][instance][1] == sent_leds[dev_addr][instance][1]) {
        return;
    }

    // Feature Report 0x01 with report ID prepended (90s device quirk)
    static uint8_t led_report[3];
    led_report[0] = 0x01;  // Report ID
    led_report[1] = desired_leds[dev_addr][instance][0];
    led_report[2] = desired_leds[dev_addr][instance][1];

    tuh_hid_set_report(dev_addr, instance, 0x01, HID_REPORT_TYPE_FEATURE,
                        led_report, sizeof(led_report));

    sent_leds[dev_addr][instance][0] = desired_leds[dev_addr][instance][0];
    sent_leds[dev_addr][instance][1] = desired_leds[dev_addr][instance][1];
}

// Sign-extend a 10-bit value to int16_t
static inline int16_t sign_extend_10(uint16_t val) {
    if (val & 0x200) {
        return (int16_t)(val | 0xFC00);
    }
    return (int16_t)val;
}

static void process_ms_sidewinder_commander(uint8_t dev_addr, uint8_t instance,
                                             uint8_t const* report, uint16_t len) {
    // Report has report ID 0x01 as first byte — skip it to get data
    if (len < COMMANDER_REPORT_SIZE + 1) return;
    const uint8_t* data = report + 1;

    // Check for changes (compare data portion only)
    if (memcmp(data, prev_report[dev_addr][instance], COMMANDER_REPORT_SIZE) == 0) {
        return;
    }

    // Extract 10-bit X axis (bits 0-9)
    uint16_t raw_x = data[0] | ((uint16_t)(data[1] & 0x03) << 8);
    int16_t axis_x = sign_extend_10(raw_x);

    // Extract 10-bit Y axis (bits 10-19)
    uint16_t raw_y = ((data[1] >> 2) & 0x3F) | ((uint16_t)(data[2] & 0x0F) << 6);
    int16_t axis_y = sign_extend_10(raw_y);

    // Extract 10-bit Rz axis (bits 20-29)
    uint16_t raw_rz = ((data[2] >> 4) & 0x0F) | ((uint16_t)(data[3] & 0x3F) << 4);
    int16_t axis_rz = sign_extend_10(raw_rz);

    // Extract 12 buttons (bits 32-43: byte 4 bits 0-7 + byte 5 bits 0-3)
    uint16_t btns = data[4] | ((uint16_t)(data[5] & 0x0F) << 8);

    // Extract 3-position toggle (bits 44-45: byte 5 bits 4-5)
    uint8_t toggle = (data[5] >> 4) & 0x03;

    // Update desired LED state (sent by task loop, not here — avoids lockup)
    // Map buttons 1-6 to LEDs 1-6, button 12 to LED 7 (each LED is 2 bits: 0=off, 1=on)
    uint8_t led0 = 0, led1 = 0;
    if (btns & (1 << 0))  led0 |= 0x01;  // Button 1  → LED 1
    if (btns & (1 << 1))  led0 |= 0x04;  // Button 2  → LED 2
    if (btns & (1 << 2))  led0 |= 0x10;  // Button 3  → LED 3
    if (btns & (1 << 3))  led0 |= 0x40;  // Button 4  → LED 4
    if (btns & (1 << 4))  led1 |= 0x01;  // Button 5  → LED 5
    if (btns & (1 << 5))  led1 |= 0x04;  // Button 6  → LED 6
    if (btns & (1 << 11)) led1 |= 0x10;  // Button 12 → LED 7 (A2)
    desired_leds[dev_addr][instance][0] = led0;
    desired_leds[dev_addr][instance][1] = led1;

    // Scale 10-bit signed (-512..511) to 8-bit unsigned (1..255, center 128)
    uint8_t scaled_xy_x = (uint8_t)((axis_x + 512) * 255 / 1023);
    uint8_t scaled_xy_y = (uint8_t)((axis_y + 512) * 255 / 1023);
    uint8_t scaled_rz   = (uint8_t)((axis_rz + 512) * 255 / 1023);

    // Ensure non-zero (internal convention: 0 means "no data")
    if (scaled_xy_x == 0) scaled_xy_x = 1;
    if (scaled_xy_y == 0) scaled_xy_y = 1;
    if (scaled_rz == 0)   scaled_rz = 1;

    // Button mapping:
    //   Button  1  → L1        Button  7  → R1
    //   Button  2  → B3        Button  8  → R2
    //   Button  3  → B4        Button  9  → S1 (Select)
    //   Button  4  → L2        Button 10  → S2 (Start)
    //   Button  5  → B1        Button 11  → A1
    //   Button  6  → B2        Button 12  → A2
    uint32_t buttons = 0;
    if (btns & (1 << 0))  buttons |= JP_BUTTON_L1;
    if (btns & (1 << 1))  buttons |= JP_BUTTON_B3;
    if (btns & (1 << 2))  buttons |= JP_BUTTON_B4;
    if (btns & (1 << 3))  buttons |= JP_BUTTON_L2;
    if (btns & (1 << 4))  buttons |= JP_BUTTON_B1;
    if (btns & (1 << 5))  buttons |= JP_BUTTON_B2;
    if (btns & (1 << 6))  buttons |= JP_BUTTON_R1;
    if (btns & (1 << 7))  buttons |= JP_BUTTON_R2;
    if (btns & (1 << 8))  buttons |= JP_BUTTON_S1;
    if (btns & (1 << 9))  buttons |= JP_BUTTON_S2;
    if (btns & (1 << 10)) buttons |= JP_BUTTON_A1;
    if (btns & (1 << 11)) buttons |= JP_BUTTON_A2;

    // 3-position toggle controls X/Y axis assignment:
    //   Position 1: X/Y → left stick, twist → right stick X (default)
    //   Position 2: X/Y → d-pad only, twist → right stick X
    //   Position 3: X/Y → right stick (twist on ANALOG_RZ only)
    // Twist (Rz) always goes to ANALOG_RZ (used for scroll in KB/Mouse mode)
    uint8_t analog_lx, analog_ly, analog_rx, analog_ry;

    #define COMMANDER_DPAD_THRESHOLD 128

    switch (toggle) {
        case 2:  // D-pad mode (no analog for tilt)
            analog_lx = 128;
            analog_ly = 128;
            analog_rx = scaled_rz;
            analog_ry = 128;
            if (axis_x < -COMMANDER_DPAD_THRESHOLD) buttons |= JP_BUTTON_DL;
            if (axis_x >  COMMANDER_DPAD_THRESHOLD) buttons |= JP_BUTTON_DR;
            if (axis_y < -COMMANDER_DPAD_THRESHOLD) buttons |= JP_BUTTON_DU;
            if (axis_y >  COMMANDER_DPAD_THRESHOLD) buttons |= JP_BUTTON_DD;
            break;
        case 3:  // Right stick mode (tilt X/Y → right stick)
            analog_lx = 128;
            analog_ly = 128;
            analog_rx = scaled_xy_x;
            analog_ry = scaled_xy_y;
            break;
        default:  // Left stick mode (tilt → left, twist → right X)
            analog_lx = scaled_xy_x;
            analog_ly = scaled_xy_y;
            analog_rx = scaled_rz;
            analog_ry = 128;
            break;
    }

    input_event_t event = {
        .dev_addr = dev_addr,
        .instance = instance,
        .type = INPUT_TYPE_GAMEPAD,
        .transport = INPUT_TRANSPORT_USB,
        .buttons = buttons,
        .button_count = 12,
        .analog = {analog_lx, analog_ly, analog_rx, analog_ry, 0, 0, scaled_rz},
        .keys = 0,
    };
    router_submit_input(&event);

    memcpy(prev_report[dev_addr][instance], data, COMMANDER_REPORT_SIZE);
}

DeviceInterface ms_sidewinder_commander_interface = {
    .name = "Microsoft SideWinder Strategic Commander",
    .is_device = is_ms_sidewinder_commander,
    .init = init_ms_sidewinder_commander,
    .process = process_ms_sidewinder_commander,
    .task = task_ms_sidewinder_commander,
    .unmount = unmount_ms_sidewinder_commander,
};
