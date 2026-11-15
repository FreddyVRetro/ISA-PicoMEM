#ifndef PM_BLUETOOTH_SCAN_H
#define PM_BLUETOOTH_SCAN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// One BLE HID connection slot per role. PicoMEM can have a mouse, a
// keyboard, and a joypad/gamepad connected simultaneously, each tracked
// independently (separate connection handle + HIDS client id).
typedef enum {
    PM_BT_SLOT_MOUSE = 0,
    PM_BT_SLOT_KEYBOARD,
    PM_BT_SLOT_JOYPAD,
    PM_BT_SLOT_COUNT,
} pm_bluetooth_slot_t;

bool pm_bluetooth_scan_init(void);
bool pm_bluetooth_scan_start(void);
bool pm_bluetooth_connect_device(uint8_t device_index);

// Kept for compatibility: overall scan/pairing status (mouse-era name).
const char *pm_bluetooth_mouse_status(void);

// Per-slot connection status string (e.g. "Not connected", "Connected").
const char *pm_bluetooth_slot_status(pm_bluetooth_slot_t slot);
const char *pm_bluetooth_slot_name(pm_bluetooth_slot_t slot);

#ifdef __cplusplus
}
#endif

#endif