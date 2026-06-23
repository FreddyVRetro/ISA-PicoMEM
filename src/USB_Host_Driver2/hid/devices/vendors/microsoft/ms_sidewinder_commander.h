// ms_sidewinder_commander.h - Microsoft SideWinder Strategic Commander
// VID: 0x045E  PID: 0x0033
// 6-byte report with three 10-bit axes, 12 buttons, 3-position toggle
// SPDX-License-Identifier: Apache-2.0

#ifndef MS_SIDEWINDER_COMMANDER_H
#define MS_SIDEWINDER_COMMANDER_H

#include "../../../hid_device.h"
#include "../../../hid_utils.h"
#include "tusb.h"

// Report is 6 bytes with bit-packed fields spanning byte boundaries.
// Parsed manually in the .c file rather than using a packed struct.
//
// Bit layout (48 bits / 6 bytes):
//   Bits  0-9:  X axis (10-bit signed, tilt L/R)
//   Bits 10-19: Y axis (10-bit signed, tilt F/B)
//   Bits 20-29: Rz axis (10-bit signed, twist rotation)
//   Bits 30-31: Constant padding
//   Bits 32-43: Buttons 1-12 (individual bits)
//   Bits 44-45: 3-position toggle (2-bit array: 1=pos1, 2=pos2, 3=pos3)
//   Bits 46-47: Constant padding
#define COMMANDER_REPORT_SIZE 6

extern DeviceInterface ms_sidewinder_commander_interface;

#endif
