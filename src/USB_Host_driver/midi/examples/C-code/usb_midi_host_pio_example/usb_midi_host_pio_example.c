/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 rppicomidi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

/**
 * This demo program is designed to test the USB MIDI Host driver for a single USB
 * MIDI device connected to the USB Host port. It sends to the USB MIDI device the
 * sequence of half-steps from B-flat to D whose note numbers correspond to the
 * transport button LEDs on a Mackie Control compatible control surface. It also
 * prints to a UART serial port console the messages received from the USB MIDI device.
 *
 * This program works with a single USB MIDI device connected via a USB hub, but it
 * does not handle multiple USB MIDI devices connected at the same time.
 */
#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "bsp/board_api.h"
#include "pico/multicore.h"
#include "pio_usb.h"
#include "tusb.h"
#include "usb_midi_host.h"
#ifdef RASPBERRYPI_PICO_W
#include "pico/cyw43_arch.h"
#endif
// Because the PIO USB code runs in core 1
// and USB MIDI OUT sends are triggered on core 0,
// need to synchronize core startup
static volatile bool core1_booting = true;
static volatile bool core0_booting = true;

static uint8_t midi_dev_addr = 0;

static void blink_led(void)
{
    static uint32_t previous_timestamp = {0};

    static bool led_state = false;

    uint32_t now = board_millis();
    
    int64_t diff = (int64_t)now - (int64_t)previous_timestamp;
    if (diff < 0)
        diff = -diff;
    if (diff > 1000) {
#ifdef RASPBERRYPI_PICO_W
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
#else
        board_led_write(led_state);
#endif
        led_state = !led_state;
        previous_timestamp = now;
    }
}

static void send_next_note(bool connected)
{
    static uint8_t first_note = 0x5b; // Mackie Control rewind
    static uint8_t last_note = 0x5f; // Mackie Control stop
    static uint8_t message[6] = {
        0x90, 0x5f, 0x00,
        0x90, 0x5b, 0x7f,
    };
    // toggle NOTE On, Note Off for the Mackie Control channels 1-8 REC LED
    const uint32_t interval_ms = 1000;
    static uint32_t start_ms = 0;

    // device must be attached and have at least one endpoint ready to receive a message
    if (!connected || tuh_midih_get_num_tx_cables(midi_dev_addr) < 1)
        return;

    // transmit any previously queued bytes
    tuh_midi_stream_flush(midi_dev_addr);
    // Blink every interval ms
    if ( board_millis() - start_ms < interval_ms) {
        return; // not enough time
    }
    start_ms += interval_ms;

    uint32_t nwritten = 0;
    // Transmit the note message on the highest cable number
    uint8_t cable = tuh_midih_get_num_tx_cables(midi_dev_addr) - 1;
    nwritten = 0;
    nwritten += tuh_midi_stream_write(midi_dev_addr, cable, message, sizeof(message));
 
    if (nwritten != 0)
    {
        ++message[1];
        ++message[4];
        if (message[1] > last_note)
            message[1] = first_note;
        if (message[4] > last_note)
            message[4] = first_note;
    }
    tuh_midi_stream_flush(midi_dev_addr);
}

void core1_main() {
    sleep_ms(10);

    tuh_init(BOARD_TUH_RHPORT);
    core1_booting = false;
    while(core0_booting) {
    }
    while (true) {
        tuh_task(); // tinyusb host task
    }
}

int main()
{
    board_init(); // must be called before tuh_init()
    multicore_reset_core1();
    // all USB task run in core1
    multicore_launch_core1(core1_main);
    // wait for core 1 to finish claiming PIO state machines and DMA
    while(core1_booting) {
    }
    printf("Pico MIDI Host Example\r\n");
#ifdef RASPBERRYPI_PICO_W
    // The Pico W LED is attached to the CYW43 WiFi/Bluetooth module
    // Need to initialize it so the the LED blink can work
    // This must be called after tuh_init(). Waiting for core1 to
    // boot does this.
    if (cyw43_arch_init()) {
        printf("WiFi/Bluetooth module init for board LED failed");
        return -1;
    }
#endif
    core0_booting = false;
    while (1) {
        blink_led();
        bool connected = midi_dev_addr != 0 && tuh_midi_configured(midi_dev_addr);

        send_next_note(connected);
    }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx)
{
  printf("MIDI device address = %u, IN endpoint %u has %u cables, OUT endpoint %u has %u cables\r\n",
      dev_addr, in_ep & 0xf, num_cables_rx, out_ep & 0xf, num_cables_tx);

  if (midi_dev_addr == 0) {
    // then no MIDI device is currently connected
    midi_dev_addr = dev_addr;
  }
  else {
    printf("A different USB MIDI Device is already connected.\r\nOnly one device at a time is supported in this program\r\nDevice is disabled\r\n");
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  if (dev_addr == midi_dev_addr) {
    midi_dev_addr = 0;
    printf("MIDI device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
  }
  else {
    printf("Unused MIDI device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
  }
}

void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets)
{
  if (midi_dev_addr == dev_addr) {
    if (num_packets != 0) {
      uint8_t cable_num;
      uint8_t buffer[48];
      while (1) {
        uint32_t bytes_read = tuh_midi_stream_read(dev_addr, &cable_num, buffer, sizeof(buffer));
        if (bytes_read == 0)
          return;
        printf("MIDI RX Cable #%u:", cable_num);
        for (uint32_t idx = 0; idx < bytes_read; idx++) {
          printf("%02x ", buffer[idx]);
        }
        printf("\r\n");
      }
    }
  }
}

void tuh_midi_tx_cb(uint8_t dev_addr)
{
    (void)dev_addr;
}
