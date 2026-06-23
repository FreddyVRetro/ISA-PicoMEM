// sinput_host.h - SInput USB Host Driver
// Reads SInput controllers as USB HID input devices.
// Reuses sinput_descriptors.h for shared report structures and constants.

#ifndef SINPUT_HOST_H
#define SINPUT_HOST_H

#include "../../hid_device.h"
#include "sinput_descriptors.h"
//#include "usb/usbd/descriptors/sinput_descriptors.h"

extern DeviceInterface sinput_host_interface;

#endif // SINPUT_HOST_H
