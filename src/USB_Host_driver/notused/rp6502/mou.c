/*
 * Copyright (c) 2023 Rumbledethumps
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "usb/mou.h"
#include "sys/mem.h"

static struct
{
    uint8_t buttons;
    uint8_t x;
    uint8_t y;
    uint8_t wheel;
    uint8_t pan;
} mou_xram_data;
static uint16_t mou_xram = 0xFFFF;

void mou_init(void)
{
    mou_stop();
}

void mou_stop(void)
{
    mou_xram = 0xFFFF;
}

bool mou_xreg(uint16_t word)
{
    if (word != 0xFFFF && word > 0x10000 - sizeof(mou_xram_data))
        return false;
    mou_xram = word;
    return true;
}

void mou_report(hid_mouse_report_t const *report)
{
    mou_xram_data.buttons = report->buttons;
    mou_xram_data.x += report->x;
    mou_xram_data.y += report->y;
    mou_xram_data.wheel += report->wheel;
    mou_xram_data.pan += report->pan;
    if (mou_xram != 0xFFFF)
        memcpy(&xram[mou_xram], &mou_xram_data, sizeof(mou_xram_data));
}
