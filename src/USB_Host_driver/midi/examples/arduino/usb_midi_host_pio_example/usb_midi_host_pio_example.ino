/*********************************************************************
 MIT license, check LICENSE for more information
 Copyright (c) 2023 rppicomidi
 Modified from device_info.ino from 
 https://github.com/sekigon-gonnoc/Pico-PIO-USB/blob/main/examples/arduino/device_info/device_info.ino
 2nd Copyright notice below included per license terms.
*********************************************************************/

/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/
#ifdef __OPTIMIZE_SIZE__
#error "Please use the Tools->Optimize: Menu to choose any Optimize setting other than -Os"
#endif
#ifndef ARDUINO_ARCH_RP2040
#error "This program only works with RP2040-based boards"
#endif
#if defined(USE_TINYUSB_HOST) || !defined(USE_TINYUSB)
#error "Please use the Menu to select Tools->USB Stack: Adafruit TinyUSB"
#endif
#include "pio_usb.h"

// The following definitions apply to the custom hardware decribed in the
// README.md file for the usb_midi_host library and for the commercial
// Adafruit RP2040 Feather with USB A Host board. If you have your own
// hardware, please change the definitions below

// Pin D+ for host, D- = D+ + 1
#ifndef PIN_USB_HOST_DP
#define PIN_USB_HOST_DP  16
#endif

// Pin for enabling Host VBUS. comment out if not used
#ifndef PIN_5V_EN
#define PIN_5V_EN        18
#endif

#ifndef PIN_5V_EN_STATE
#define PIN_5V_EN_STATE  1
#endif

#include "Adafruit_TinyUSB.h"

#define LANGUAGE_ID 0x0409  // English

// Add USB MIDI Host support to Adafruit_TinyUSB
#include "usb_midi_host.h"

// USB Host object
Adafruit_USBH_Host USBHost;

// holding device descriptor
tusb_desc_device_t desc_device;

// holding the device address of the MIDI device
uint8_t midi_dev_addr = 0;


// the setup function runs once when you press reset or power the board
void setup()
{
  Serial.begin(115200);
  while (!Serial) {
    delay(100);   // wait for native usb
  }

  Serial.printf("TinyUSB MIDI Host Example\r\n");
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
    // New note every interval ms
    if (millis() - start_ms < interval_ms) {
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

void loop()
{
  bool connected = midi_dev_addr != 0 && tuh_midi_configured(midi_dev_addr);

  send_next_note(connected);
}

// core1's setup
void setup1() {
  while (!Serial) {
    delay(100);   // wait for native usb
  }
  Serial.printf("Core1 setup to run TinyUSB host with pio-usb\r\n");

  // Check for CPU frequency, must be multiple of 120Mhz for bit-banging USB
  uint32_t cpu_hz = clock_get_hz(clk_sys);
  if ( cpu_hz != 120000000UL && cpu_hz != 240000000UL ) {
    delay(2000);   // wait for native usb
    Serial.printf("Error: CPU Clock = %u, PIO USB require CPU clock must be multiple of 120 Mhz\r\n", cpu_hz);
    Serial.printf("Change your CPU Clock to either 120 or 240 Mhz in Menu->CPU Speed \r\n", cpu_hz);
    while(1) delay(1);
  }

  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = PIN_USB_HOST_DP;
 
 // Change the next line to #if 1 if the Pico-PIO-USB library version is 0.5.3 or older.
 #if 0
 #if defined(ARDUINO_RASPBERRY_PI_PICO_W)
  /* Need to swap PIOs so PIO code from CYW43 PIO SPI driver will fit */
  pio_cfg.pio_rx_num = 0;
  pio_cfg.pio_tx_num = 1;
 #endif /* ARDUINO_RASPBERRY_PI_PICO_W */
 #endif
 
  USBHost.configure_pio_usb(1, &pio_cfg);
#ifdef PIN_5V_EN
  pinMode(PIN_5V_EN, OUTPUT);
  digitalWrite(PIN_5V_EN, PIN_5V_EN_STATE);
#endif
  // Optionally, configure the buffer sizes here
  // The commented out code shows the default values
  // tuh_midih_define_limits(64, 64, 16);

  // run host stack on controller (rhport) 1
  // Note: For rp2040 pico-pio-usb, calling USBHost.begin() on core1 will have most of the
  // host bit-banging processing works done in core1 to free up core0 for other works
  USBHost.begin(1);
}

// core1's loop
void loop1()
{
  USBHost.task();
}

//--------------------------------------------------------------------+
// TinyUSB Host callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted (configured)
void tuh_mount_cb (uint8_t daddr)
{
  Serial.printf("Device attached, address = %d\r\n", daddr);

  // Get Device Descriptor
  tuh_descriptor_get_device(daddr, &desc_device, 18, print_device_descriptor, 0);
}

/// Invoked when device is unmounted (bus reset/unplugged)
void tuh_umount_cb(uint8_t daddr)
{
  Serial.printf("Device removed, address = %d\r\n", daddr);
}

void print_device_descriptor(tuh_xfer_t* xfer)
{
  if ( XFER_RESULT_SUCCESS != xfer->result )
  {
    Serial.printf("Failed to get device descriptor\r\n");
    return;
  }

  uint8_t const daddr = xfer->daddr;
  // Get String descriptor using Sync API
  uint16_t temp_buf[128];

  Serial.printf("  iManufacturer       %u     "     , desc_device.iManufacturer);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_manufacturer_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)) )
  {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  Serial.printf("\r\n");

  Serial.printf("  iProduct            %u     "     , desc_device.iProduct);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_product_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)))
  {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  Serial.printf("\r\n");

  Serial.printf("  iSerialNumber       %u     "     , desc_device.iSerialNumber);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_serial_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)))
  {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  Serial.printf("\r\n");
}

//--------------------------------------------------------------------+
// String Descriptor Helper
//--------------------------------------------------------------------+

static void _convert_utf16le_to_utf8(const uint16_t *utf16, size_t utf16_len, uint8_t *utf8, size_t utf8_len) {
  // TODO: Check for runover.
  (void)utf8_len;
  // Get the UTF-16 length out of the data itself.

  for (size_t i = 0; i < utf16_len; i++) {
    uint16_t chr = utf16[i];
    if (chr < 0x80) {
      *utf8++ = chr & 0xff;
    } else if (chr < 0x800) {
      *utf8++ = (uint8_t)(0xC0 | (chr >> 6 & 0x1F));
      *utf8++ = (uint8_t)(0x80 | (chr >> 0 & 0x3F));
    } else {
      // TODO: Verify surrogate.
      *utf8++ = (uint8_t)(0xE0 | (chr >> 12 & 0x0F));
      *utf8++ = (uint8_t)(0x80 | (chr >> 6 & 0x3F));
      *utf8++ = (uint8_t)(0x80 | (chr >> 0 & 0x3F));
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
}

// Count how many bytes a utf-16-le encoded string will take in utf-8.
static int _count_utf8_bytes(const uint16_t *buf, size_t len) {
  size_t total_bytes = 0;
  for (size_t i = 0; i < len; i++) {
    uint16_t chr = buf[i];
    if (chr < 0x80) {
      total_bytes += 1;
    } else if (chr < 0x800) {
      total_bytes += 2;
    } else {
      total_bytes += 3;
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
  return total_bytes;
}

static void print_utf16(uint16_t *temp_buf, size_t buf_len) {
  size_t utf16_len = ((temp_buf[0] & 0xff) - 2) / sizeof(uint16_t);
  size_t utf8_len = _count_utf8_bytes(temp_buf + 1, utf16_len);

  _convert_utf16le_to_utf8(temp_buf + 1, utf16_len, (uint8_t *) temp_buf, sizeof(uint16_t) * buf_len);
  ((uint8_t*) temp_buf)[utf8_len] = '\0';

  Serial.printf((char*)temp_buf);
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
  Serial.printf("MIDI device address = %u, IN endpoint %u has %u cables, OUT endpoint %u has %u cables\r\n",
      dev_addr, in_ep & 0xf, num_cables_rx, out_ep & 0xf, num_cables_tx);

  if (midi_dev_addr == 0) {
    // then no MIDI device is currently connected
    midi_dev_addr = dev_addr;
  }
  else {
    Serial.printf("A different USB MIDI Device is already connected.\r\nOnly one device at a time is supported in this program\r\nDevice is disabled\r\n");
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  if (dev_addr == midi_dev_addr) {
    midi_dev_addr = 0;
    Serial.printf("MIDI device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
  }
  else {
    Serial.printf("Unused MIDI device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
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
        Serial.printf("MIDI RX Cable #%u:", cable_num);
        for (uint32_t idx = 0; idx < bytes_read; idx++) {
          Serial.printf("%02x ", buffer[idx]);
        }
        Serial.printf("\r\n");
      }
    }
  }
}

void tuh_midi_tx_cb(uint8_t dev_addr)
{
    (void)dev_addr;
}

void parse_configuration_descriptor_cb(uint8_t drv_id, uint32_t ptr) {
  Serial.printf("Parsing for driver ID %u of %p\r\n", drv_id, ptr);
}
