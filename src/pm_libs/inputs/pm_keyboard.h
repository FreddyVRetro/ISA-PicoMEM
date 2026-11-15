// pm_keyboard.h -- USB HID keyboard -> PS/2 scan code (set 1) translation +
//                   DPRAM injection plumbing for the WonderMCA keyboard
//                   emulation path.
//
// Pipeline: process_hid_keyboard() (USB_Host_Driver2) calls
//   pm_keyboard_submit_report() AFTER it builds its router event. We
//   keep the previous HID report (modifier byte + 6 keycodes), diff
//   against the new report to detect transitions, translate each new
//   make/break into a PS/2 scan code (set 1), and write the scan code
//   into the DPRAM byte BV_USBKbdScan (PM_DP_RAM[29]).
//
//   PMBIOS's INT 1Ch hook reads BV_USBKbdScan every BIOS tick. If it's
//   non-zero, PMBIOS sets the USB_KBD_PENDING flag, copies the scan
//   into USB_KBD_SCAN (PMBIOS-private), clears BV_USBKbdScan, and
//   software-fires INT 9. Our PMBIOS_Int9h handler picks the scan up
//   and pushes it to the BIOS keyboard buffer at 40:1E via the BIOS's
//   own routines.
//
//   The router's INPUT_TYPE_KEYBOARD case stays a no-op -- we bypass
//   the router for keyboard events because the router's event type
//   loses the modifier byte and only carries 3 of 6 keycodes.
//
// Gated end-to-end on PM_Config->EnableUSBKbd. With the flag OFF,
// pm_keyboard_submit_report() is a no-op and PMBIOS doesn't install
// the INT 9h hook.

#ifndef PM_KEYBOARD_H
#define PM_KEYBOARD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Submit a complete HID keyboard report for translation + DPRAM injection.
// `modifier` is the HID modifier byte (bit0=LCtrl ... bit7=RGUI).
// `keycodes[6]` is the array of currently-pressed HID keycodes; unused
// slots are 0.
//
// Compares against the previously-submitted report. For each newly-
// pressed key, writes a "make" scan code. For each newly-released key,
// writes a "break" scan code (high bit set). Modifier transitions are
// handled the same way using their own scan codes (e.g. LShift = 0x2A
// make / 0xAA break).
//
// Side effect: copies the new report into internal "previous" state so
// the next call can diff. No-op when PM_Config->EnableUSBKbd is 0.
void pm_keyboard_submit_report(uint8_t modifier, const uint8_t keycodes[6]);

// Reset internal "previous report" state. Call on USB keyboard unmount
// so a re-plug doesn't see ghost-released keys.
void pm_keyboard_reset(void);

// Typematic repeat pump. Call frequently from the Core 0 main loop (next to
// tuh_task()). Re-emits the make code of the currently-held key after the
// initial delay, then at the repeat rate, since USB HID reports only on
// change. No-op when nothing is held or USB keyboard emulation is disabled.
void pm_keyboard_tick(void);

#ifdef __cplusplus
}
#endif

#endif // PM_KEYBOARD_H
