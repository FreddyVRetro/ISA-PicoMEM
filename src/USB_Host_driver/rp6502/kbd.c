/*
 * Copyright (c) 2023 Rumbledethumps
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "main.h"
#include "api/api.h"
#include "sys/cfg.h"
#include "usb/kbd.h"
#include "usb/kbd_deu.h"
#include "usb/kbd_eng.h"
#include "usb/kbd_pol.h"
#include "usb/kbd_swe.h"
#include "pico/stdio/driver.h"
#include "fatfs/ff.h"
#include "string.h"

static int kbd_stdio_in_chars(char *buf, int length);

static stdio_driver_t kbd_stdio_app = {
    .in_chars = kbd_stdio_in_chars,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = PICO_STDIO_DEFAULT_CRLF
#endif
};

#define KBD_REPEAT_DELAY 500000
#define KBD_REPEAT_RATE 30000

static absolute_time_t kbd_repeat_timer;
static uint8_t kbd_repeat_keycode;
static hid_keyboard_report_t kbd_prev_report;
static char kbd_key_queue[16];
static uint8_t kbd_key_queue_head;
static uint8_t kbd_key_queue_tail;
static uint8_t kdb_hid_leds = KEYBOARD_LED_NUMLOCK;
static bool kdb_hid_leds_need_report;
static uint16_t kbd_xram;
static uint8_t kbd_xram_keys[32];

#define KBD_KEY_QUEUE(pos) kbd_key_queue[(pos) & 0x0F]

#define HID_KEYCODE_TO_UNICODE_(kb) HID_KEYCODE_TO_UNICODE_##kb
#define HID_KEYCODE_TO_UNICODE(kb) HID_KEYCODE_TO_UNICODE_(kb)
static DWORD const __in_flash("keycode_to_unicode")
    KEYCODE_TO_UNICODE[128][4] = {HID_KEYCODE_TO_UNICODE(RP6502_KEYBOARD)};

void kbd_hid_leds_dirty()
{
    kdb_hid_leds_need_report = true;
}

static void kbd_queue_str(const char *str)
{
    // All or nothing
    for (size_t len = strlen(str); len; len--)
        if (&KBD_KEY_QUEUE(kbd_key_queue_head + len) == &KBD_KEY_QUEUE(kbd_key_queue_tail))
            return;
    while (*str)
        KBD_KEY_QUEUE(++kbd_key_queue_head) = *str++;
}

static void kbd_queue_seq(const char *str, const char *mod_seq, int mod)
{
    char s[16];
    if (mod == 1)
        return kbd_queue_str(str);
    sprintf(s, mod_seq, mod);
    return kbd_queue_str(s);
}

static void kbd_queue_seq_vt(int num, int mod)
{
    char s[16];
    if (mod == 1)
        sprintf(s, "\33[%d~", num);
    else
        sprintf(s, "\33[%d;%d~", num, mod);
    return kbd_queue_str(s);
}

static void kbd_queue_key(uint8_t modifier, uint8_t keycode, bool initial_press)
{
    bool key_shift = modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
    bool key_alt = modifier & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT);
    bool key_ctrl = modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL);
    bool key_gui = modifier & (KEYBOARD_MODIFIER_LEFTGUI | KEYBOARD_MODIFIER_RIGHTGUI);
    bool is_numlock = kdb_hid_leds & KEYBOARD_LED_NUMLOCK;
    bool is_capslock = kdb_hid_leds & KEYBOARD_LED_CAPSLOCK;
    // Set up for repeat
    kbd_repeat_keycode = keycode;
    kbd_repeat_timer = delayed_by_us(get_absolute_time(),
                                     initial_press ? KBD_REPEAT_DELAY : KBD_REPEAT_RATE);
    // When not in numlock, and not shifted, remap num pad
    if (keycode >= HID_KEY_KEYPAD_1 &&
        keycode <= HID_KEY_KEYPAD_DECIMAL &&
        (!is_numlock || (key_shift && is_numlock)))
    {
        if (is_numlock)
            key_shift = false;
        switch (keycode)
        {
        case HID_KEY_KEYPAD_1:
            keycode = HID_KEY_END;
            break;
        case HID_KEY_KEYPAD_2:
            keycode = HID_KEY_ARROW_DOWN;
            break;
        case HID_KEY_KEYPAD_3:
            keycode = HID_KEY_PAGE_DOWN;
            break;
        case HID_KEY_KEYPAD_4:
            keycode = HID_KEY_ARROW_LEFT;
            break;
        case HID_KEY_KEYPAD_5:
            keycode = HID_KEY_NONE;
            break;
        case HID_KEY_KEYPAD_6:
            keycode = HID_KEY_ARROW_RIGHT;
            break;
        case HID_KEY_KEYPAD_7:
            keycode = HID_KEY_HOME;
            break;
        case HID_KEY_KEYPAD_8:
            keycode = HID_KEY_ARROW_UP;
            break;
        case HID_KEY_KEYPAD_9:
            keycode = HID_KEY_PAGE_UP;
            break;
        case HID_KEY_KEYPAD_0:
            keycode = HID_KEY_INSERT;
            break;
        case HID_KEY_KEYPAD_DECIMAL:
            keycode = HID_KEY_DELETE;
            break;
        }
    }
    // Find plain typed or AltGr character
    char ch = 0;
    if (keycode < 128 && !((modifier & (KEYBOARD_MODIFIER_LEFTALT |
                                        KEYBOARD_MODIFIER_LEFTGUI |
                                        KEYBOARD_MODIFIER_RIGHTGUI))))
    {
        if (modifier & KEYBOARD_MODIFIER_RIGHTALT)
        {
            ch = ff_uni2oem(KEYCODE_TO_UNICODE[keycode][2], cfg_get_codepage());
            if ((key_shift && !is_capslock) || 
                (!key_shift && is_capslock))
                ch = ff_uni2oem(KEYCODE_TO_UNICODE[keycode][3], cfg_get_codepage());
        }
        else if ((key_shift && !is_capslock) ||
                 (key_shift && keycode > HID_KEY_Z) ||
                 (!key_shift && is_capslock && keycode <= HID_KEY_Z))
            ch = ff_uni2oem(KEYCODE_TO_UNICODE[keycode][1], cfg_get_codepage());
        else
            ch = ff_uni2oem(KEYCODE_TO_UNICODE[keycode][0], cfg_get_codepage());
    }
    // ALT characters not found in AltGr get escaped
    if (key_alt && !ch && keycode < 128)
    {
        if (key_shift)
            ch = ff_uni2oem(KEYCODE_TO_UNICODE[keycode][1], cfg_get_codepage());
        else
            ch = ff_uni2oem(KEYCODE_TO_UNICODE[keycode][0], cfg_get_codepage());
        if (key_ctrl)
        {
            if (ch >= '`' && ch <= '~')
                ch -= 96;
            else if (ch >= '@' && ch <= '_')
                ch -= 64;
            else if (keycode == HID_KEY_BACKSPACE)
                ch = '\b';
        }
        if (ch)
        {
            if (&KBD_KEY_QUEUE(kbd_key_queue_head + 1) != &KBD_KEY_QUEUE(kbd_key_queue_tail) &&
                &KBD_KEY_QUEUE(kbd_key_queue_head + 2) != &KBD_KEY_QUEUE(kbd_key_queue_tail))
            {
                KBD_KEY_QUEUE(++kbd_key_queue_head) = '\33';
                KBD_KEY_QUEUE(++kbd_key_queue_head) = ch;
            }
            return;
        }
    }
    // Promote ctrl characters
    if (key_ctrl)
    {
        if (ch >= '`' && ch <= '~')
            ch -= 96;
        else if (ch >= '@' && ch <= '_')
            ch -= 64;
        else if (keycode == HID_KEY_BACKSPACE)
            ch = '\b';
        else
            ch = 0;
    }
    // Queue a regularly typed key
    if (ch)
    {
        if (&KBD_KEY_QUEUE(kbd_key_queue_head + 1) != &KBD_KEY_QUEUE(kbd_key_queue_tail))
            KBD_KEY_QUEUE(++kbd_key_queue_head) = ch;
        return;
    }
    // Non-repeating special key handler
    if (initial_press)
        switch (keycode)
        {
        case HID_KEY_DELETE:
            if (key_ctrl && key_alt)
            {
                kbd_key_queue_tail = kbd_key_queue_head;
                main_break();
                return;
            }
            break;
        case HID_KEY_NUM_LOCK:
            kdb_hid_leds ^= KEYBOARD_LED_NUMLOCK;
            kbd_hid_leds_dirty();
            break;
        case HID_KEY_CAPS_LOCK:
            kdb_hid_leds ^= KEYBOARD_LED_CAPSLOCK;
            kbd_hid_leds_dirty();
            break;
        }
    // Special key handler
    int ansi_modifier = 1;
    if (key_shift)
        ansi_modifier += 1;
    if (key_alt)
        ansi_modifier += 2;
    if (key_ctrl)
        ansi_modifier += 4;
    if (key_gui)
        ansi_modifier += 8;
    switch (keycode)
    {
    case HID_KEY_ARROW_UP:
        return kbd_queue_seq("\33[A", "\33[1;%dA", ansi_modifier);
    case HID_KEY_ARROW_DOWN:
        return kbd_queue_seq("\33[B", "\33[1;%dB", ansi_modifier);
    case HID_KEY_ARROW_RIGHT:
        return kbd_queue_seq("\33[C", "\33[1;%dC", ansi_modifier);
    case HID_KEY_ARROW_LEFT:
        return kbd_queue_seq("\33[D", "\33[1;%dD", ansi_modifier);
    case HID_KEY_F1:
        return kbd_queue_seq("\33OP", "\33[1;%dP", ansi_modifier);
    case HID_KEY_F2:
        return kbd_queue_seq("\33OQ", "\33[1;%dQ", ansi_modifier);
    case HID_KEY_F3:
        return kbd_queue_seq("\33OR", "\33[1;%dR", ansi_modifier);
    case HID_KEY_F4:
        return kbd_queue_seq("\33OS", "\33[1;%dS", ansi_modifier);
    case HID_KEY_F5:
        return kbd_queue_seq_vt(15, ansi_modifier);
    case HID_KEY_F6:
        return kbd_queue_seq_vt(17, ansi_modifier);
    case HID_KEY_F7:
        return kbd_queue_seq_vt(18, ansi_modifier);
    case HID_KEY_F8:
        return kbd_queue_seq_vt(19, ansi_modifier);
    case HID_KEY_F9:
        return kbd_queue_seq_vt(10, ansi_modifier);
    case HID_KEY_F10:
        return kbd_queue_seq_vt(21, ansi_modifier);
    case HID_KEY_F11:
        return kbd_queue_seq_vt(23, ansi_modifier);
    case HID_KEY_F12:
        return kbd_queue_seq_vt(24, ansi_modifier);
    case HID_KEY_HOME:
        return kbd_queue_seq("\33[H", "\33[1;%dH", ansi_modifier);
    case HID_KEY_INSERT:
        return kbd_queue_seq_vt(2, ansi_modifier);
    case HID_KEY_DELETE:
        return kbd_queue_seq_vt(3, ansi_modifier);
    case HID_KEY_END:
        return kbd_queue_seq("\33[F", "\33[1;%dF", ansi_modifier);
    case HID_KEY_PAGE_UP:
        return kbd_queue_seq_vt(5, ansi_modifier);
    case HID_KEY_PAGE_DOWN:
        return kbd_queue_seq_vt(6, ansi_modifier);
    }
}

static int kbd_stdio_in_chars(char *buf, int length)
{
    int i = 0;
    while (i < length && &KBD_KEY_QUEUE(kbd_key_queue_tail) != &KBD_KEY_QUEUE(kbd_key_queue_head))
    {
        buf[i++] = KBD_KEY_QUEUE(++kbd_key_queue_tail);
    }
    return i ? i : PICO_ERROR_NO_DATA;
}

static void kbd_prev_report_to_xram()
{
    // Update xram if configured
    if (kbd_xram != 0xFFFF)
    {
        // Check for phantom state
        bool phantom = false;
        for (uint8_t i = 0; i < 6; i++)
            if (kbd_prev_report.keycode[i] == 1)
                phantom = true;
        // Preserve previous keys in phantom state
        if (!phantom)
            memset(kbd_xram_keys, 0, sizeof(kbd_xram_keys));
        bool any_key = false;
        for (uint8_t i = 0; i < 6; i++)
        {
            uint8_t keycode = kbd_prev_report.keycode[i];
            if (keycode >= HID_KEY_A)
            {
                any_key = true;
                kbd_xram_keys[keycode >> 3] |= 1 << (keycode & 7);
            }
        }
        // modifier maps directly
        kbd_xram_keys[HID_KEY_CONTROL_LEFT >> 3] = kbd_prev_report.modifier;
        // No key pressed
        if (!any_key && !kbd_prev_report.modifier && !phantom)
            kbd_xram_keys[0] |= 1;
        // NUMLOCK
        if (kdb_hid_leds & KEYBOARD_LED_NUMLOCK)
            kbd_xram_keys[0] |= 4;
        // CAPSLOCK
        if (kdb_hid_leds & KEYBOARD_LED_CAPSLOCK)
            kbd_xram_keys[0] |= 8;
        // Send it to xram
        memcpy(&xram[kbd_xram], kbd_xram_keys, sizeof(kbd_xram_keys));
    }
}

void kbd_report(uint8_t dev_addr, uint8_t instance, hid_keyboard_report_t const *report)
{
    static uint8_t prev_dev_addr = 0;
    static uint8_t prev_instance = 0;
    // Only support key presses on one keyboard at a time.
    if (kbd_prev_report.keycode[0] >= HID_KEY_A &&
        ((prev_dev_addr != dev_addr) || (prev_instance != instance)))
        return;

    // Extract presses for queue
    uint8_t modifier = report->modifier;
    for (uint8_t i = 0; i < 6; i++)
    {
        // fix unusual modifier reports
        uint8_t keycode = report->keycode[i];
        if (keycode >= HID_KEY_CONTROL_LEFT && keycode <= HID_KEY_GUI_RIGHT)
            modifier |= 1 << (keycode & 7);
    }
    for (uint8_t i = 0; i < 6; i++)
    {
        uint8_t keycode = report->keycode[i];
        if (keycode >= HID_KEY_A &&
            !(keycode >= HID_KEY_CONTROL_LEFT && keycode <= HID_KEY_GUI_RIGHT))
        {
            bool held = false;
            for (uint8_t j = 0; j < 6; j++)
            {
                if (keycode == kbd_prev_report.keycode[j])
                    held = true;
            }
            if (!held)
                kbd_queue_key(modifier, keycode, true);
        }
    }
    prev_dev_addr = dev_addr;
    prev_instance = instance;
    kbd_prev_report = *report;
    kbd_prev_report.modifier = modifier;
    kbd_prev_report_to_xram();
}

void kbd_init(void)
{
    stdio_set_driver_enabled(&kbd_stdio_app, true);
    kbd_stop();
}

void kbd_task(void)
{
    if (kbd_repeat_keycode && absolute_time_diff_us(get_absolute_time(), kbd_repeat_timer) < 0)
    {
        for (uint8_t i = 0; i < 6; i++)
        {
            uint8_t keycode = kbd_prev_report.keycode[5 - i];
            if (kbd_repeat_keycode == keycode)
            {
                kbd_queue_key(kbd_prev_report.modifier, keycode, false);
                return;
            }
        }
        kbd_repeat_keycode = 0;
    }

    if (kdb_hid_leds_need_report)
    {
        kdb_hid_leds_need_report = false;
        for (uint8_t dev_addr = 0; dev_addr < CFG_TUH_DEVICE_MAX; dev_addr++)
            for (uint8_t instance = 0; instance < CFG_TUH_HID; instance++)
                if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD)
                    tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT,
                                       &kdb_hid_leds, sizeof(kdb_hid_leds));
    }
}

void kbd_stop(void)
{
    kbd_xram = 0xFFFF;
}

bool kbd_xreg(uint16_t word)
{
    if (word != 0xFFFF && word > 0x10000 - sizeof(kbd_xram_keys))
        return false;
    kbd_xram = word;
    kbd_prev_report_to_xram();
    return true;
}
