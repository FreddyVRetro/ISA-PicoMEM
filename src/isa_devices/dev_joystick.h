#pragma once

typedef struct {
    uint8_t joy1_x;
    uint8_t joy1_y;
    uint8_t joy2_x;
    uint8_t joy2_y;
    uint8_t button_mask;
    uint8_t mode;          // 0 : Standard analog, 2 : Gravis Gamepad Pro
    uint32_t Gravis1;      // Gravis GrIP data for player 1
    uint32_t Gravis2;      // Gravis GrIP data for player 2
    uint8_t pl_total;      // Number of players / Joystick connected
    uint8_t pl_dev[2];     // Players device number
    uint8_t pl_inst[2];    // Players instance number
    uint8_t display_event; // Event to display on the screen
    } joystate_struct_t;

extern joystate_struct_t joystate_struct;

extern uint8_t dev_joystick_install();
extern void dev_joystick_remove();
extern void dev_joystick_update();
extern bool dev_joystick_ior(uint32_t Addr,uint8_t *Data);
extern void dev_joystick_iow(uint32_t Addr,uint8_t Data);