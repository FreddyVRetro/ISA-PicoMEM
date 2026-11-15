/*
 * NE2000 transport backend interface.
 *
 * The DP8390 emulator in ne2000.c is board-independent. Per-transport
 * glue (CYW43 WiFi on Pico W, W5500 SPI on WonderMCA, ...) plugs in via
 * the vtable defined below. Exactly one backend translation unit is
 * linked per build; that TU defines `ne2000_backend`.
 *
 * Ownership:
 *   - Emulator calls ne2000_backend.send() to transmit a frame.
 *   - Backends call ne2000_deliver_rx() to hand a received frame to the
 *     emulator (push model from the CYW43 driver, or pull model from a
 *     W5500 poll/IRQ).
 *   - ne2000_backend.poll() is called from the Core 0 main loop if the
 *     backend is pull-model. Push-model backends set it to NULL.
 *
 * Transport-level bring-up (power-on, SSID join, MAC programming, link
 * monitoring) is intentionally OUT of this interface. Each backend
 * exposes its own higher-level entry points (e.g. CYW43's
 * PM_ConnectWifi()), so the existing bring-up flow in pm_cmd/picomem.cpp
 * is not disturbed.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ne2000_backend {
    const char *name;

    // Transmit one complete Ethernet frame (including dst/src MAC + type
    // + payload). len is total byte count.
    void (*send)(const uint8_t *frame, uint16_t len);

    // Pull-model RX drain. Push-model backends set this to NULL. When
    // non-NULL, call once per Core 0 main-loop iteration.
    void (*poll)(void);
} ne2000_backend_t;

// Selected backend. One (and only one) translation unit must define this
// symbol -- see e.g. src/ne2000/backends/cyw43_backend.c.
extern ne2000_backend_t ne2000_backend;

// Hand one received Ethernet frame to the DP8390 emulator. Backends call
// this either from their own RX callback (push) or from their poll()
// (pull).
void ne2000_deliver_rx(const uint8_t *frame, uint16_t len);

// True when the DP8390 would accept a delivered frame right now (chip
// started + receive ring programmed). A pull-model backend can hold frames
// in its own ring across a transient guest stop-start rather than dropping
// them via ne2000_deliver_rx().
bool ne2000_rx_ready(void);

#ifdef __cplusplus
}
#endif
