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
#include "tusb_option.h"

#if (TUSB_OPT_HOST_ENABLED)

#include "host/usbh.h"
#include "host/usbh_pvt.h"

#include "usb_midi_host.h"

// Install the USB MIDI Host class driver
usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count)
{
  static usbh_class_driver_t host_driver = {
  #if CFG_TUSB_DEBUG >= 2
    .name = "MIDIH",
  #endif
    .init=midih_init,
    .deinit=midih_deinit,
    .open=midih_open,
    .set_config=midih_set_config,
    .xfer_cb = midih_xfer_cb,
    .close = midih_close };
  *driver_count = 1;
  return &host_driver;
}
#endif