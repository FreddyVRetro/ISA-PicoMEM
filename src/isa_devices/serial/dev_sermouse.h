// dev_sermouse.h -- WonderMCA serial-mouse device (COM2 0x2F8 / IRQ3).
//
// Thin wrapper over the PicoGUS 8250uart + sermouse emulation so a Microsoft
// serial mouse works in DOS AND Windows 3.x (Windows reads a serial mouse
// directly; the 8042 PS/2-aux inject is virtualized away in 386 Enhanced mode).
//
// Dispatch: the Core-1 MCA bus handler routes the decoded 0x2F8 block here via
//   case DEV_SERMOUSE: dev_sermouse_ior/iow (see wm_mca_rp2350_v10.h).
// Pump: dev_sermouse_task() runs on the Core-0 main loop (UART worker + mouse
//   report timing). Feed: dev_sermouse_feed() from process_hid_mouse().
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// One-time init. iobase = 0x2F8 (COM2), irqnb = 3.
void dev_sermouse_init(uint16_t iobase, uint8_t irqnb);

// I/O port access (called from the Core-1 bus handler). addr_lo = register
// index 0..7 within the decoded 8-byte block.
void dev_sermouse_ior(uint8_t addr_lo, uint8_t *data);
void dev_sermouse_iow(uint8_t addr_lo, uint8_t data);

// Core-0 pump: UART worker (drains the TX msg pipe) + mouse report timing.
void dev_sermouse_task(void);

// POST/reboot quiesce: sermouse_quiesce() + force the MCA IRQ line low.
// Called from the config-apply CMD at every boot. Safe no-op before init.
void dev_sermouse_quiesce(void);

// Feed a USB HID mouse report (hid_mouse_report_t*) -- call from the USB path.
void dev_sermouse_feed(const void *hid_report);

#ifdef __cplusplus
}
#endif
