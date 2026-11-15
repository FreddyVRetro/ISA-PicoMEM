// pm_keyboard.c -- USB HID keyboard -> PS/2 set-1 scancode STREAM.
//
// See pm_keyboard.h for the architecture and DPRAM contract.
//
// 2026-07-14: switched from "cooked" keystroke injection (INT 16h AH=05h
// with a US-only ASCII table) to a RAW PS/2 set-1 scancode stream. We push
// make codes on press, break codes (scan | 0x80) on release, 0xE0 prefixes
// as their own byte, and modifier make/break transitions. PMBIOS_Int1C
// drains the ring and injects each byte into the real system 8042 via
// command 0xD2, which raises a genuine IRQ1 -> chassis INT 9h. The BIOS and
// DOS KEYB then process it exactly like the physical keyboard, so KEYB xx
// (FR/US/DE/BE/...) owns the layout -- no per-language ASCII tables here.
//
// Translation table: USB HID Usage Tables 1.12 (Keyboard/Keypad page 0x07)
// cross-referenced against PS/2 Scan Code Set 1. Nav cluster / keypad-/ /
// keypad-Enter / Menu are E0-prefixed (see key_is_e0); the right-hand
// Ctrl/Alt/GUI modifiers are E0-prefixed too (see MOD_E0_MASK).

#include "pm_keyboard.h"

#include <string.h>
#include <stdbool.h>
#include "../../pm_defines.h"
#include "../../pm_gvars.h"     // PM_DP_RAM, PM_Config
#include "../../pm_debug.h"
#include "pico/time.h"          // time_us_64() -- typematic repeat timing

// HID keycode -> PS/2 set 1 make code (base, without any E0 prefix).
// 0x00 = no mapping. Nav-cluster / keypad-/ / keypad-Enter / Menu carry a
// base code here and are flagged E0 by key_is_e0(); their base bytes collide
// with the numeric keypad on purpose -- the 0xE0 prefix disambiguates them
// on the wire, exactly as a real PS/2 keyboard does.
static const uint8_t s_hid_to_ps2_set1[256] = {
    [0x04] = 0x1E, // A
    [0x05] = 0x30, // B
    [0x06] = 0x2E, // C
    [0x07] = 0x20, // D
    [0x08] = 0x12, // E
    [0x09] = 0x21, // F
    [0x0A] = 0x22, // G
    [0x0B] = 0x23, // H
    [0x0C] = 0x17, // I
    [0x0D] = 0x24, // J
    [0x0E] = 0x25, // K
    [0x0F] = 0x26, // L
    [0x10] = 0x32, // M
    [0x11] = 0x31, // N
    [0x12] = 0x18, // O
    [0x13] = 0x19, // P
    [0x14] = 0x10, // Q
    [0x15] = 0x13, // R
    [0x16] = 0x1F, // S
    [0x17] = 0x14, // T
    [0x18] = 0x16, // U
    [0x19] = 0x2F, // V
    [0x1A] = 0x11, // W
    [0x1B] = 0x2D, // X
    [0x1C] = 0x15, // Y
    [0x1D] = 0x2C, // Z
    [0x1E] = 0x02, // 1
    [0x1F] = 0x03, // 2
    [0x20] = 0x04, // 3
    [0x21] = 0x05, // 4
    [0x22] = 0x06, // 5
    [0x23] = 0x07, // 6
    [0x24] = 0x08, // 7
    [0x25] = 0x09, // 8
    [0x26] = 0x0A, // 9
    [0x27] = 0x0B, // 0
    [0x28] = 0x1C, // Enter
    [0x29] = 0x01, // Escape
    [0x2A] = 0x0E, // Backspace
    [0x2B] = 0x0F, // Tab
    [0x2C] = 0x39, // Space
    [0x2D] = 0x0C, // - _
    [0x2E] = 0x0D, // = +
    [0x2F] = 0x1A, // [ {
    [0x30] = 0x1B, // ] }
    [0x31] = 0x2B, // \ |
    [0x32] = 0x2B, // non-US # ~ (same key as \| on ISO)
    [0x33] = 0x27, // ; :
    [0x34] = 0x28, // ' "
    [0x35] = 0x29, // ` ~
    [0x36] = 0x33, // , <
    [0x37] = 0x34, // . >
    [0x38] = 0x35, // / ?
    [0x39] = 0x3A, // Caps Lock
    [0x3A] = 0x3B, // F1
    [0x3B] = 0x3C, // F2
    [0x3C] = 0x3D, // F3
    [0x3D] = 0x3E, // F4
    [0x3E] = 0x3F, // F5
    [0x3F] = 0x40, // F6
    [0x40] = 0x41, // F7
    [0x41] = 0x42, // F8
    [0x42] = 0x43, // F9
    [0x43] = 0x44, // F10
    [0x44] = 0x57, // F11
    [0x45] = 0x58, // F12
    [0x47] = 0x46, // Scroll Lock
    [0x49] = 0x52, // Insert     (E0)
    [0x4A] = 0x47, // Home       (E0)
    [0x4B] = 0x49, // Page Up    (E0)
    [0x4C] = 0x53, // Delete     (E0)
    [0x4D] = 0x4F, // End        (E0)
    [0x4E] = 0x51, // Page Down  (E0)
    [0x4F] = 0x4D, // Right      (E0)
    [0x50] = 0x4B, // Left       (E0)
    [0x51] = 0x50, // Down       (E0)
    [0x52] = 0x48, // Up         (E0)
    [0x53] = 0x45, // Num Lock
    // Keypad
    [0x54] = 0x35, // KP /       (E0)
    [0x55] = 0x37, // KP *
    [0x56] = 0x4A, // KP -
    [0x57] = 0x4E, // KP +
    [0x58] = 0x1C, // KP Enter   (E0)
    [0x59] = 0x4F, // KP 1
    [0x5A] = 0x50, // KP 2
    [0x5B] = 0x51, // KP 3
    [0x5C] = 0x4B, // KP 4
    [0x5D] = 0x4C, // KP 5
    [0x5E] = 0x4D, // KP 6
    [0x5F] = 0x47, // KP 7
    [0x60] = 0x48, // KP 8
    [0x61] = 0x49, // KP 9
    [0x62] = 0x52, // KP 0
    [0x63] = 0x53, // KP .
    [0x64] = 0x56, // non-US \ | (ISO key next to left Shift)
    [0x65] = 0x5D, // Application / Menu (E0)
};

// HID keycodes that must be prefixed with 0xE0 in PS/2 set 1.
static inline bool key_is_e0(uint8_t k) {
    switch (k) {
        case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: // Ins/Home/PgUp/Del/End
        case 0x4E: case 0x4F: case 0x50: case 0x51: case 0x52: // PgDn/Right/Left/Down/Up
        case 0x54:  // KP /
        case 0x58:  // KP Enter
        case 0x65:  // Application / Menu
            return true;
        default:
            return false;
    }
}

// HID modifier bit -> PS/2 set 1 base make code. The HID modifier byte
// (report->modifier) bits:
//   bit 0: LCtrl   bit 1: LShift  bit 2: LAlt   bit 3: LGUI
//   bit 4: RCtrl   bit 5: RShift  bit 6: RAlt   bit 7: RGUI
// Right Ctrl/Alt and both GUI keys are E0-prefixed (MOD_E0_MASK).
static const uint8_t s_mod_scan[8] = {
    0x1D, // bit 0: Left Ctrl
    0x2A, // bit 1: Left Shift
    0x38, // bit 2: Left Alt
    0x5B, // bit 3: Left GUI   (E0)
    0x1D, // bit 4: Right Ctrl (E0)
    0x36, // bit 5: Right Shift
    0x38, // bit 6: Right Alt  (E0 -- AltGr; DOS KEYB uses this for the 3rd level)
    0x5C, // bit 7: Right GUI  (E0)
};
#define MOD_E0_MASK 0xD8   // bits 3,4,6,7 need an E0 prefix

// Previous HID report -- one byte modifier + 6 keycodes. Used to diff
// against the incoming report so we emit make/break only on transitions.
static uint8_t s_prev_modifier = 0;
static uint8_t s_prev_keycodes[6] = {0};

// Typematic repeat (2026-07-14). A USB HID keyboard only reports on state
// CHANGE, so a held key produces one make + one break -- no auto-repeat.
// A real PS/2 keyboard's hardware re-sends the make of the LAST-pressed key
// after a delay, then at the repeat rate; we reproduce that here by
// re-emitting the make from pm_keyboard_tick() (pumped on Core 0). Only the
// most-recently-pressed key repeats; releasing it stops the repeat.
#define KBD_REPEAT_DELAY_US 500000ULL   // initial delay before first repeat (~0.5 s)
#define KBD_REPEAT_RATE_US  100000ULL   // repeat interval (~10 chars/s)
static uint8_t  s_repeat_hid  = 0;      // HID keycode currently repeating (0 = none)
static uint64_t s_repeat_next = 0;      // time_us_64() at which to emit the next make

// Is this HID keycode present in the given 6-key report array?
static bool key_in(uint8_t hid_code, const uint8_t list[6]) {
    if (hid_code == 0) return false;
    for (int i = 0; i < 6; i++)
        if (list[i] == hid_code) return true;
    return false;
}

// Push one raw PS/2 scancode byte into the DPRAM ring (2026-07-14). SPSC:
// the sole producer is Core 0's USB callback; PMBIOS_Int1C is the sole
// consumer. PM_DP_RAM is volatile, so the data byte is written before Head
// advances -- the consumer reads Head, then buf[Tail]. Drops on full (64
// bytes = ~32 key events between two 18 Hz drains, far beyond any human
// burst). Head/Tail are zero-init at boot.
static inline void emit_scan(uint8_t b) {
    uint8_t h    = BV_USBKbd_Head & (USBKBD_RING_SIZE - 1);
    uint8_t next = (uint8_t)((h + 1) & (USBKBD_RING_SIZE - 1));
    if (next == (BV_USBKbd_Tail & (USBKBD_RING_SIZE - 1))) {
        PM_INFO("[USB][kbd] ring FULL -- drop %02X\n", b);
        return;                // ring full -> drop (consumer hasn't drained)
    }
    BV_USBKbd_Buf(h) = b;      // write data first...
    BV_USBKbd_Head   = next;   // ...then publish (volatile keeps this order)
}

static inline void emit_make(uint8_t base, bool e0) {
    if (!base) return;                 // unmapped key
    if (e0) emit_scan(0xE0);
    emit_scan(base);
}
static inline void emit_break(uint8_t base, bool e0) {
    if (!base) return;
    if (e0) emit_scan(0xE0);
    emit_scan((uint8_t)(base | 0x80));
}

void pm_keyboard_reset(void) {
    s_prev_modifier = 0;
    memset(s_prev_keycodes, 0, sizeof s_prev_keycodes);
    s_repeat_hid = 0;
    // Flush the DPRAM scancode ring: scancodes queued before a warm reboot
    // (e.g. the Ctrl-Alt-Del break codes / typematic repeats) would otherwise
    // be injected into the 8042 during the NEXT boot and abort the PMBIOS
    // boot countdown. Producer side (Core 0) drops everything pending.
    BV_USBKbd_Head = BV_USBKbd_Tail;
}

// Typematic repeat pump. Call frequently from the Core 0 main loop (next to
// tuh_task()). Re-emits the make of the currently-held repeat key once the
// initial delay has elapsed, then every KBD_REPEAT_RATE_US. Cheap no-op when
// nothing is held or USB keyboard emulation is disabled.
void pm_keyboard_tick(void) {
    if (s_repeat_hid == 0) return;
    if (!PM_Config || PM_Config->EnableUSBKbd == 0) return;
    uint64_t now = time_us_64();
    if ((int64_t)(now - s_repeat_next) < 0) return;     // not time yet
    emit_make(s_hid_to_ps2_set1[s_repeat_hid], key_is_e0(s_repeat_hid));
    s_repeat_next = now + KBD_REPEAT_RATE_US;
}

void pm_keyboard_submit_report(uint8_t modifier, const uint8_t keycodes[6]) {
    if (!PM_Config || PM_Config->EnableUSBKbd == 0) {
        // Keep the diff baseline current so re-enabling doesn't spew phantom
        // break codes for keys "released" while disabled.
        s_prev_modifier = modifier;
        memcpy(s_prev_keycodes, keycodes, sizeof s_prev_keycodes);
        return;
    }

    // Emit a make/break scancode stream from the delta between the previous
    // report and this one. Order: modifier makes (so Shift/AltGr wrap the
    // key), key breaks, key makes, modifier breaks. The BIOS/KEYB track
    // shift state from these codes, so both edges must be sent.

    // 1. Modifier MAKES -- bits newly set (0 -> 1).
    uint8_t mod_on = (uint8_t)(modifier & ~s_prev_modifier);
    for (int b = 0; b < 8; b++)
        if (mod_on & (1u << b))
            emit_make(s_mod_scan[b], (MOD_E0_MASK >> b) & 1);

    // 2. Key BREAKS -- keys held last time but gone now.
    for (int i = 0; i < 6; i++) {
        uint8_t k = s_prev_keycodes[i];
        if (k == 0 || key_in(k, keycodes)) continue;   // 0 / still held
        emit_break(s_hid_to_ps2_set1[k], key_is_e0(k));
        if (k == s_repeat_hid) s_repeat_hid = 0;        // stop repeating a released key
    }

    // 3. Key MAKES -- keys newly pressed this report.
    for (int i = 0; i < 6; i++) {
        uint8_t k = keycodes[i];
        if (k == 0 || key_in(k, s_prev_keycodes)) continue; // 0 / already held
        uint8_t base = s_hid_to_ps2_set1[k];
        emit_make(base, key_is_e0(k));
        if (base) {                                     // last mapped key pressed wins the repeat
            s_repeat_hid  = k;
            s_repeat_next = time_us_64() + KBD_REPEAT_DELAY_US;
        }
    }

    // 4. Modifier BREAKS -- bits newly cleared (1 -> 0).
    uint8_t mod_off = (uint8_t)(s_prev_modifier & ~modifier);
    for (int b = 0; b < 8; b++)
        if (mod_off & (1u << b))
            emit_break(s_mod_scan[b], (MOD_E0_MASK >> b) & 1);

    // Persist new state for the next diff.
    memcpy(s_prev_keycodes, keycodes, sizeof s_prev_keycodes);
    s_prev_modifier = modifier;
}
