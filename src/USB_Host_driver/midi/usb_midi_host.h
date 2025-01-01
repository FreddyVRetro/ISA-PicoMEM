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

#ifndef _TUSB_MIDI_HOST_H_
#define _TUSB_MIDI_HOST_H_

#include "class/audio/audio.h"
#include "class/midi/midi.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+
// Use this function in Arduino or other environments where modifying
// the tusb_config.h file is not practical.
// Call this before midih_init() gets called by the TinyUSB stack,
// which is before the application calls tusb_init() or tuh_init().
// Calling this function after midih_init() will cause problems.
//
// Note: To figure out how long a USB MIDI 1.0 stream needs to be
// in bytes, multiply the number of bytes in the stream by 4/3 and
// round up to the nearest 4 bytes.
// For exampe, to send a 146 byte SysEx message, 146*4/3 = 194.66...
// The next nearest 4 byte boundary is 196. So the buffer is 196 bytes,
// or 49 4-byte USB MIDI packets.
//
// Parameters:
// midi_rx_buffer_bytes is the number of bytes the USB Host can buffer
// from the device. This has to be at least equal to the maximum bulk
// transfer size of 64 bytes, but it is a good idea to set this to
// at least the maximum SysEx message size in MIDI packets to improve
// throughput.
//
// midi_tx_buffer_bytes is the maximum number of bytes the application
// can write out to the interface in a single transaction. This should
// be at least as large as the maximum bulk transfer size of 64 bytes.
// To send long SysEx messages, you should make this buffer at least as
// long as the longest message in MIDI packets or else SysEx message
// writes may get truncated.
//
// max_cables defaults to 16. If you know you only need to convert
// serial MIDI data to USB MIDI packets from cable numbers 0:N,
// and N is less than 15, you can save a small amount
// if RAM if you set this value to something less than 16.
// For example, if your application is to convert a single
// Serial Port MIDI to USB, then the highest cable number
// is 0, so you only need one virtual cable. You will save the
// deserialization buffer for 15 virtual cables (about 90 bytes).
//
void tuh_midih_define_limits(size_t midi_rx_buffer_bytes, size_t midi_tx_buffer_bytes, uint8_t max_cables);

#ifndef CFG_MIDI_HOST_DEVSTRINGS
#ifdef ARDUINO
#define CFG_MIDI_HOST_DEVSTRINGS 1
#else
#define CFG_MIDI_HOST_DEVSTRINGS 0
#endif
#endif

//--------------------------------------------------------------------+
// Application API (Single Interface)
//--------------------------------------------------------------------+

bool     tuh_midi_configured      (uint8_t dev_addr);

// return the number of virtual midi cables on the device's OUT endpoint
uint8_t tuh_midih_get_num_tx_cables (uint8_t dev_addr);

// return the number of virtual midi cables on the device's IN endpoint
uint8_t tuh_midih_get_num_rx_cables (uint8_t dev_addr);

// Queue a packet to the device. The application
// must call tuh_midi_stream_flush to actually have the
// data go out. It is up to the application to properly
// format this packet; this function does not check.
// Using this function with tuh_midi_stream_write()
// might produce undefined behavior.
// Returns true if the packet was successfully queued.
bool tuh_midi_packet_write (uint8_t dev_addr, uint8_t const packet[4]);

// Queue a message to the device. The application
// must call tuh_midi_stream_flush to actually have the
// data go out. Note that cable_num must be < CFG_TUH_CABLE_MAX
// (note CFG_TUH_CABLE_MAX default is 16)
uint32_t tuh_midi_stream_write (uint8_t dev_addr, uint8_t cable_num, uint8_t const* p_buffer, uint32_t bufsize);

/// Return true if the MIDI OUT FIFO has enough space for at
/// least one more message
bool tuh_midi_can_write_stream (uint8_t dev_addr);

// Send any queued packets to the device if the host hardware is able to do it
// Returns the number of bytes flushed to the host hardware or 0 if
// the host hardware is busy or there is nothing in queue to send.
uint32_t tuh_midi_stream_flush( uint8_t dev_addr);

// Get the MIDI stream from the device. Set the value pointed
// to by p_cable_num to the MIDI cable number intended to receive it.
// The MIDI stream will be stored in the buffer pointed to by p_buffer.
// Return the number of bytes added to the buffer.
// Note that this function ignores the CIN field of the MIDI packet
// because a number of commercial devices out there do not encode
// it properly.
uint32_t tuh_midi_stream_read (uint8_t dev_addr, uint8_t *p_cable_num, uint8_t *p_buffer, uint16_t bufsize);

// Read a raw MIDI packet from the connected device
// This function does not parse the packet format
// Return true if a packet was returned
bool tuh_midi_packet_read (uint8_t dev_addr, uint8_t packet[4]);

uint8_t tuh_midi_get_num_rx_cables(uint8_t dev_addr);
uint8_t tuh_midi_get_num_tx_cables(uint8_t dev_addr);
#if CFG_MIDI_HOST_DEVSTRINGS
uint8_t tuh_midi_get_rx_cable_istrings(uint8_t dev_addr, uint8_t* istrings, uint8_t max_istrings);
uint8_t tuh_midi_get_tx_cable_istrings(uint8_t dev_addr, uint8_t* istrings, uint8_t max_istrings);
uint8_t tuh_midi_get_all_istrings(uint8_t dev_addr, const uint8_t** istrings);
#endif
//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
bool midih_init       (void);
bool midih_deinit     (void);
bool midih_open       (uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *desc_itf, uint16_t max_len);
bool midih_set_config (uint8_t dev_addr, uint8_t itf_num);
bool midih_xfer_cb    (uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
void midih_close      (uint8_t dev_addr);

//--------------------------------------------------------------------+
// Callbacks (Weak is optional)
//--------------------------------------------------------------------+

// Invoked when device with MIDI interface is mounted.
// If the MIDI host application requires MIDI IN, it should requst an
// IN transfer here. The device will likely NAK this transfer. How the driver
// handles the NAK is hardware dependent.
TU_ATTR_WEAK void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx);

// Invoked when device with MIDI interface is un-mounted
// For now, the instance parameter is always 0 and can be ignored
TU_ATTR_WEAK void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance);

TU_ATTR_WEAK void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets);
TU_ATTR_WEAK void tuh_midi_tx_cb(uint8_t dev_addr);
#ifdef __cplusplus
}
#endif

#endif /* _TUSB_MIDI_HOST_H_ */
