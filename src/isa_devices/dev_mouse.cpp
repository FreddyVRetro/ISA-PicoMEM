#include "pico/stdlib.h"
#include "pico/time.h"       // time_us_64()
#include "dev_mouse.h"
#include "../pm_defines.h"
#include "../pm_gvars.h"     // PM_DP_RAM
#include "../pm_debug.h"     // PM_INFO (diagnostic)

volatile pm_mouse_t pm_mouse;

// --- USB mouse -> PS/2 aux packet builder ---------------------------------
// A USB mouse streams reports faster than PMBIOS drains the mailbox (18.2 Hz),
// so we ACCUMULATE deltas here and coalesce them into one standard 3-byte PS/2
// packet per drain. Movement is never dropped: a delta larger than one packet
// can carry is clamped and the remainder is kept for the next packet.
static int     s_ps2_acc_x   = 0;
static int     s_ps2_acc_y   = 0;
static uint8_t s_ps2_btn     = 0;
static uint8_t s_ps2_btn_last = 0xFF;   // force the first packet to publish buttons

void pm_mouse_ps2_accumulate(int dx, int dy, uint8_t jp_buttons) {
    s_ps2_acc_x += dx;
    s_ps2_acc_y += dy;
    // Map JP_BUTTON_* (held in pm_mouse.buttons) to PS/2 L/R/M (bits 0/1/2).
    // Middle (JP_BUTTON_S2 = bit 9) is truncated by the uint8 pm_mouse.buttons
    // pipeline, so only L/R survive today -- see dev_mouse.h. (TODO: widen the
    // pipeline for the middle button.)
    uint8_t b = 0;
    if (jp_buttons & 0x01) b |= 0x01;   // JP_B1 -> left
    if (jp_buttons & 0x02) b |= 0x02;   // JP_B2 -> right
    s_ps2_btn = b;
}

// Publish one coalesced PS/2 packet into the DPRAM mailbox when the previous
// one has been consumed (Rdy==0) and there is pending movement or a button
// change. Called every Core 0 loop iteration.
void pm_mouse_ps2_tick(void) {
    if (BV_USBMouse_Rdy) return;                       // PMBIOS not done yet
    bool moved       = (s_ps2_acc_x != 0) || (s_ps2_acc_y != 0);
    bool btn_changed = (s_ps2_btn != s_ps2_btn_last);
    if (!moved && !btn_changed) return;

    // Clamp to the signed 8-bit range a packet carries; keep the remainder.
    int dx = s_ps2_acc_x;
    int dy = s_ps2_acc_y;
    if (dx >  127) dx =  127; else if (dx < -127) dx = -127;
    if (dy >  127) dy =  127; else if (dy < -127) dy = -127;
    s_ps2_acc_x -= dx;
    s_ps2_acc_y -= dy;

    // PS/2 mouse Y is positive-UP; USB/screen Y is positive-DOWN -> negate.
    int py = -dy;

    uint8_t b0 = 0x08 | (s_ps2_btn & 0x07);            // bit3 always 1
    if (dx < 0) b0 |= 0x10;                            // X sign
    if (py < 0) b0 |= 0x20;                            // Y sign
    BV_USBMouse_B0 = b0;
    BV_USBMouse_B1 = (uint8_t)(dx & 0xFF);
    BV_USBMouse_B2 = (uint8_t)(py & 0xFF);
    BV_USBMouse_Rdy = 1;                               // publish for PMBIOS

    s_ps2_btn_last = s_ps2_btn;
}
