# usb_midi_host
This README file contains the design notes and limitations of the
usb_midi_host application driver for TinyUSB. This driver supports
both C/C++ development and Arduino development.
The code in this project should run on any TinyUSB supported
processor with USB Host Bulk endpoint support, but the driver and
example code have only been tested on a RP2040 in a Raspberry Pi
Pico board.

By default, this driver supports up to 4 USB MIDI devices connected
through a USB hub, or a single device that may or may not be connected
through a hub.

# Table of Contents
- [ACKNOWLEDGEMENTS](#acknowledgements)
- [BUILDING APPLICATIONS WITH THIS DRIVER](#building-applications-with-this-driver)
- [HARDWARE](#hardware)
- [API](#api)
- [EXAMPLE PROGRAMS](#example-programs)
- [TROUBLESHOOTING, CONFIGURATION, and DESIGN DETAILS](#troubleshooting-configuration-and-design-details)

# ACKNOWLEDGEMENTS
The application driver code is based on code that rppicomidi submitted
to TinyUSB as pull request #1219. The pull request was never merged and
got stale. TinyUSB pull request #1627 by atoktoto started with pull request
#1219, but used simpler RP2040 bulk endpoint support from TinyUSB pull request
#1434. Pull request #1627 was reviewed by todbot, AndrewCapon, PaulHamsh,
and rppicomidi and was substantially functional. This driver copied
the `midi_host.c/h` files from pull request #1627 and renamed them
`usb_midi_host.c/h`. It also fixed some minor issues in the driver
API and added the application driver wrapper `usb_midi_host_app_driver.c`.
The driver C example code code is adapted from TinyUSB pull request #1219. 

# BUILDING APPLICATIONS WITH THIS DRIVER
Although it is possible in the future for the TinyUSB stack to
incorporate this driver into its builtin driver support, right now
it is used as an application driver external to the stack. Installing
the driver to the TinyUSB stack requires adding it to the array
of application drivers returned by the `usbh_app_driver_get_cb()`
function.

## Building C/C++ Applications

### Basic Environment Setup
Before you attempt to build any C/C++ applications, be sure
you have the toolchain properly installed and the `pico-sdk`
installed. Please make sure you can build and
run the blink example found in the [Getting Started Guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf).
Version 2.0 or later of the `pico-sdk` offers the best support
for this project.

### ${PICO_SDK_PATH}
If you are following Chapter 3 of the [Getting Started Guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf),
you installed VS Code and the Official Raspberry Pi Pico VS Code extension.
The VS Code extension installed the `pico-sdk` in the
`${HOME}/.pico-sdk/sdk/2.0.0` directory. If you followed the manual
toolchain installation per Appendix C of the [Getting Started Guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf),
then you installed the `pico-sdk` in `${HOME}/pico`.

### TinyUSB Library
You will also need to make sure that the the TinyUSB library is installed.
If there is nothing in the directory `${PICO_SDK_PATH}/lib/tinyusb`, or
the directory does not exist, please run the following commands
```
cd ${PICO_SDK_PATH}/pico-sdk
git submodule update --init
```
### TinyUSB for Pre-Version 2.0 `pico-sdk`
You will need a version of the TinyUSB library that supports
USB Host Application drivers. This feature was introduced to TinyUSB on
15-Aug-2023 with commit 7537985c080e439f6f97a021ce49f5ef48979c78
which is release 0.16.0 or later. Version 2.0 of the `pico-sdk` is
compatible with this. Older versions are not.

If you must use an older version of the of the `pico-sdk`, it ships
configured to use TinyUSB 0.14 or earlier. You will need to update the
TinyUSB library.  Please make sure you have a current TinyUSB code
library in your pico-sdk by using the following commands:

```
cd ${PICO_SDK_PATH}/lib/tinyusb
git fetch origin
git checkout 525406597627fb9307425539b86dddf10278eca8
```

### `Pico-PIO-USB` Library
If you are using the `Pico-PIO-USB` Library to implement the
USB Host hardware (see the [HARDWARE](#hardware) section, below),
you need to manually install the `Pico-PIO-USB` Library where
TinyUSB can find it.

TinyUSB provides python script that TinyUSB to install it, but the script
in the version of TinyUSB that ships with `pico-sdk` version 2.0
will install a version of the library that won't build with
`pico-sdk` version 2.0. Use these commands instead.

```
cd ${PICO_SDK_PATH}/lib/tinyusb/hw/mcu
mkdir -p raspberry_pi/Pico-PIO-USB
cd raspberry_pi/Pico-PIO-USB
git init
git remote add origin https://github.com/sekigon-gonnoc/Pico-PIO-USB.git
git fetch --depth 1 origin 7902e9fa8ed4a271d8d1d5e7e50516c2292b7bc2
git checkout FETCH_HEAD 
```

If you are using an older version of the `pico-sdk`, and you do not
have python installed, please run the above but replace the `git fetch` line with
```
git fetch --depth 1 origin fe3b1e22436386f3b2be6c1c5f66658cbc32e1ba
```

If you do have python installed and you are using an older pico-sdk,
it is easier to run the Python script, see [Dependencies](https://docs.tinyusb.org/en/latest/reference/getting_started.html#dependencies).
```
cd ${PICO_SDK_PATH}/lib/tinyusb
python3 tools/get_deps.py rp2040
```

Hopefully, the `pico-sdk` will someday do all of this for you.

### Building the `usb_midi_host` Library in Your Project
The `CMakeLists.txt` file contains two `INTERFACE` libraries.
If this driver is your only application USB host driver external
to the TinyUSB stack, you should install this driver by
adding the `usb_midi_host_app_driver` library to your main
application's `CMakeLists.txt` file's `target_link_libraries`.
If you want to add multiple host drivers, you must implement
your own `usbh_app_driver_get_cb()` function and you should
add the `usb_midi_host` library to your main application's
`CMakeLists.txt` file's `target_link_libraries` instead.

See the files in `examples/C-code/usb_midi_host_example`
or `examples/C-code/usb_midi_host_pio_example`
for examples.

## Building Arduino Applications
Include this library, the Adafruit TinyUSB Arduino Library,
and, if your host port hardware requires it, the Pico_PIO_USB
Library using the Arduino IDE Library Manager. If this library
is not available in the Arduino IDE Library Manager yet,
please copy this library code to a `usb_midi_host` directory
under your sketech folder `libraries` directory.

To build any Arduino application on the RP2040, you should
install the Earle Philhower [arduino-pico](https://github.com/earlephilhower/arduino-pico) core for the Arduino IDE.

You need to set up the board under the Tools Menu.
If you are using the `Pico-PIO-USB` Library to implement your
USB Host hardware:
```
Tools->CPU Speed->120 MHz (or 240 MHz (Overclock))
Tools->USB Stack->"Adafruit TinyUSB"
Tools->Debug Port->Serial
Tools->Optimize: You can choose any option except Small (-Os) (Standard). I generally use Optimize Even More (-O3)
```

If you are using the native RP2040 USB hardware to implement
your USB Host hardware, please configure the core as follows:
```
Tools->CPU Speed->133 MHz (or faster, if you wish)
Tools->USB Stack->"Adafruit TinyUSB Host"
Tools->Debug Port->Serial1
Tools->Optimize: (Choose anything)
```
NOTE: The USB Stack option "Adafruit TinyUSB Host" is not
available in the `arduino-pico` package 3.6.2 and earlier.
You must install version 3.6.3 or later to make this option
available.

# HARDWARE
The example programs have been tested on a Raspberry Pi Pico board,
a Pico W board, and an Adafruit RP2040 Feather with USB A Host board.
The Pico boards, like most RP2040-based boards, do not ship with a
USB Host friendly connector, and the development environments
generally assume you are using the USB connector in Device mode
to provide power to the board, and to allow software update, to
provide a serial port interface for a serial console monitor, etc.
You will likely have to modify your board to add a USB Host interface
connector. The RP2040-based boards offer two approaches.

## Software-based USB Host Port: `Pico-PIO-USB` Library
The `Pico-PIO-USB` library, which works for both C/C++ and Arduino,
uses the RP2040 PIO 0 and CPU core 1 to to efficiently
bit-bang a full-speed USB host port on 2 GPIO pins. Adafruit makes
a [RP2040 board](https://www.adafruit.com/product/5723) that uses
this method for USB Host.

If you are not using the Adafruit or similar board, you need to
wire up something yourself. Wire a USB A jack to the GPIO and
power pins on the Pico board as follows:
```
Pico/Pico W board pin   USB A connector pin
23 (or any GND pin)  ->     GND
21 (GP16)            ->     D+  (via a 22 ohm resistor should improve things)
22 (GP17)            ->     D-  (via a 22 ohm resistor should improve things)
24 (GP18)            ->     TinyUSB drives this pin high to enable VBus on the Adafruit Feather board
40 (VBus)            ->     VBus (safer if it has current limiting on the pin)
```
I use a low-cost USB breakout board and solder the 22 ohm resistors to cut
traces on the D+ and D- lines. I leave GP18 unconnected.

TODO Insert photos of my setup here.

The main advantages of this approach are:
- Your board will have both a USB Device port and
a USB Host port, which, in an Arduino environment
especially, is convenient. For example, the Arduino
`Serial` object will work with the Serial Monitor
console, and firmware update via the Arduino IDE
is supported. 
- If you need both a USB MIDI Host port
and a USB MIDI Device port at the same time, you can
do it. See the [pico-usb-midi-filter](https://github.com/rppicomidi/pico-usb-midi-filter),
[pico-usb-midi-processor](https://github.com/rppicomidi/pico-usb-midi-processor),
and [midi2piousbhub](https://github.com/rppicomidi/midi2piousbhub) projects
for examples of this.
- You can buy
[off-the-shelf hardware](https://www.adafruit.com/product/5723)
already wired to support a Host port using this method.

The disadvantages of this approach are:
- The RP2040 clock must run at a multiple of 120MHz. This
  is a bit slower than the default of 133MHz.
- It consumes 2 GPIO pins
- It consumes the PIO 0 module
- It consumes CPU 1
- It takes a bit more code storage space and RAM space.
- The Pico_PIO_USB library can conflict with the drivers for the
Pico W WiFi/Bluetooth module. To prevent the conflicts, please
initialze the TinyUSB library before the WiFi/Bluetooth module lbiraries.

## RP2040 Native USB Hardware
The RP2040 USB core natively supports a host mode that is
good enough for MIDI. The minimum modification to a Pico
board is to connect a USB OTG adapter to the Micro USB
B connector and add an external 5V power supply between
the VBus pin (pin 40 on the Pico board) and any ground pin
on the Pico board. As long as your 5V power supply is clean
and protected against short circuit, it should be OK. It is
how I test native hardware USB Host.

TODO Insert photo of my setup here.

The main advantages of this approach are
- It does not consume 3 GPIO pins
- It does not consume PIO 0
- It does not restrict CPU operating speed
- It does not consume CPU 1
- It does not need the memory the Pico_PIO_USB library uses

The disadvantages of this approach are
- Serial port console now has to use a UART; you have to
provide external hardware to interface that UART to your
computer terminal software (e.g., a Picoprobe). In Arduino,
you have to use `Serial1` or `Serial2` UARTs for serial
monitor to work.
- Software update either requires you to unplug the OTG
connector and connect the RP2040 in flash drive mode,
or you have to use a Picoprobe or similar debug interface.
- Depending on how you want to mount the development board, adapting
the board's native USB connector to USB A may be harder than just
soldering to a few GPIO pins.
- There appears to be a [bug in the RP2040 USB controller
hardware](https://github.com/rppicomidi/usb_midi_host/issues/14)
that prevents connection with the Arturia Beatstep Pro.
Other MIDI hardware may have the same problem.

NOTE: If you are using native USB hardware in USB Host mode
for an Arduino project, please look at the note in the
[Building Arduino Applications](#building-arduino-applications)
section.

# API

## Connection Management
There are two connection management functions every application must implement:
- `tuh_midi_mount_cb()`
- `tuh_midi_unmount_cb()`

Each device connected to the USB Host, either directly, or through
a hub, is identified by its device address, which is an 8-bit number.
In addition, during enumeration, the host discovers how many virtual
MIDI IN cables and how many virtual MIDI OUT cables the device has.
When someone plugs a MIDI device to the USB Host, this driver will
call the `tuh_midi_mount_cb()` function so the application can save
the device address and virtual cable information.

When someone unplugs a MIDI device from the USB Host, this driver
will call the `tuh_midi_unmount_cb()` function so the application
can mark the previous device address as invalid.

## MIDI Message Communication
There is one function that every application that supports MIDI IN
must implement
- `tuh_midi_rx_cb()`

When the USB Host receives MIDI IN packets from the MIDI device,
this driver calls `tuh_midi_rx_cb()` to notify the application
that MIDI data is available. The application should read that
MIDI data as soon as possible.

There are two ways to handle USB MIDI messages:
- As 4-byte raw USB MIDI 1.0 packets
    - `tuh_midi_packet_read()`
    - `tuh_midi_packet_write()`

- As serial MIDI 1.0 byte streams
    - `tuh_midi_stream_read()`
    - `tuh_midi_stream_write()`

Both `tuh_midi_packet_write()` and `tuh_midi_stream_write()`
only write MIDI data to a queue. Once you are done writing
all MIDI messages that you want to send in a single
USB Bulk transfer (usually 64 bytes but sometimes only
8 bytes), you must call `tuh_midi_stream_flush()`.

The `examples` folder contains both C-Code and Arduino
code examples of how to use the API.

## MIDI Device Strings API
A USB MIDI device can attach a string descriptor to any or
all virtual MIDI cables. This driver can retrieve the indices
to the strings using these functions:
```
tuh_midi_get_rx_cable_istrings();
tuh_midi_get_tx_cable_istrings();
tuh_midi_get_all_istrings()
```

You can then use the TinyUSB API for retrieving a device string
by string index to get the UTF-16LE string.

NOTE: For non-Arduino builds, to save space and to speed up
enumeration, by default, `CFG_MIDI_HOST_DEVSTRINGS` is set to 0,
so the MIDI Device String API is disabled by default. If you want
to use the MIDI Device String API, please add, either before you
include this library, or to your C/C++ application project `tusb_config.h`
```
#define CFG_MIDI_HOST_DEVSTRINGS 1
```
For Arduino builds, because the Arduino IDE does not allow letting library
files include files from the sketch directory, the default is to enable the
MIDI Device String API. Disabling it requires adding
```
#define  CFG_MIDI_HOST_DEVSTRINGS 0
```
in the library file `usb_midi_host.h`.

## Arduino MIDI Library API
This library API is designed to be relatively low level and is well
suited for applications that require the application to touch
all MIDI messages (for example, bridging and filtering). If you want
your application to use the Arduino MIDI library API
to access the devices attached to your USB MIDI host,
install the [EZ_USB_MIDI_HOST](https://github.com/rppicomidi/EZ_USB_MIDI_HOST) wrapper library in addition
to this library. See that repository for more information.

# EXAMPLE PROGRAMS

## Software
Each example program does the same thing:
- play a 5 note sequence on MIDI cable 0
- print out every MIDI message it receives on cable 0.

The only difference among them is whether they are C/C++
examples or Arduino examples, and whether they use native
rp2040 hardware (in directory with name `usb_midi_host_example`)
or the Pico_PIO_USB software USB Host (in directory with name
`usb_midi_host_pio_example`).

## Building C-Code Examples
First, set up your environment for command line `pico-sdk`
program builds. If you are new to this, please see
the [Getting Started Guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) and build the blink example
before you try this.

Next, install the libraries as described in the [Building C/C++
Applications](#building-cc-applications) section.

### Command Line Build
To build via command line (see Appendix C of the `Getting Started Guide`);

```
cd examples/C-code/[example program directory name]
mkdir build
cd build
cmake ..
make
```
Note: if you are building the `usb_midi_host_pio_example` for
the Adafruit RP2040 Feather with USB
Type A Host board, you should replace `cmake ..` with
```
-DPICO_BOARD=adafruit_feather_rp2040_usb_host ..
```
If you don't do this, then the board will work right after you
program it, and will not work on reset or reboot. If you are
using any other board other than a Pico board, change
`adafruit_feather_rp2040_usb_host` to the name of theUn
file for your board (without the `.h` extension)
found in `${PICO_SDK_PATH}/src/boards/include/boards`.

### VS Code Build
To build using VS Code, for Version 2.0 of the `pico-sdk`, import the project to VS Code.
1. Click the `Raspberry Pi Pico Project` icon in the left toolbar.
2. Click `Import Project`
3. Chenge the Location to point to the `examples/C-code/[example program directory name]` directory
4. Make sure Pico-SDK version is 2.0.0.
5. Choose the Debugger and any advanced options
6. Click Import. If you are not using a Pico board and your Raspberry Pi
   Pico Extenstion for VS Code is 0.15.2 or later, do this:
    1. Click the Click the `Raspberry Pi Pico Project` icon in the left toolbar.
    2. Under `Project` click `Switch Board`.
    3. Find your board in the board list and click on the name.
7. Click the CMake icon in the left toolbar. If you are not using a Pico board
    and your Raspberry Pi Pico Extenstion for VS Code is 0.15.1 or earlier, do this:
    1. On the `PROJECT STATUS` line of the CMAKE window, click the `Open CMake Tool Extenstions Settings` gear icon. You have to mouse over
    the `PROJECT STATUS` line for the icon to appear.
    2. In the new Settings tab that opened in the editor pane, click the `Workspace` tab.
    3. Scroll down to the `CMake:Configure Args` item and click the `Add Item` button.
    4. Enter `-DPICO_BOARD=[your board name goes here]` and click OK. The board name is the file name without
       extension from `${PICO_SDK_PATH}/src/boards/include/boards`. For example, if you are using a
       adafruit_feather_rp2040_usb_host board, you would enter `-DPICO_BOARD=adafruit_feather_rp2040_usb_host`
8. On the `PROJECT STATUS` line of the CMAKE window,
   select the `Delete Cache and Reconfigure` icon.
   You have to mouse over the `PROJECT STATUS` line for the icon to appear.
9. Under the `Configure` option, select the Pico Kit.
10. Choose whether you want `Debug`, `Release` or `RelWithDebugInfo`. The `MinSizeRel` option can cause issues, so do not choose it.
11. Click Build

If you are using an older version of the `pico-sdk`, then the project
is already set up for VS Code. Just use the VS Code `File` menu to
open the project. Select the toolchain when prompted, and use the
CMake icon on the left toolbar to build the code (see steps 7-11, above).

## Testing C-Code Examples
To test, first prepare your development board as described
in the [Hardware](#hardware) section of this document. Next,
copy the UF2 file to the Pico board using whatever
method you prefer. Make sure the board powers up and displays the message
```
Pico MIDI Host Example
```
before you attach your USB MIDI device.

Attach your USB MIDI device to the USB A connector your
hardware provides. You should see a message similar
to
```
MIDI device address = 1, IN endpoint 2 has 1 cables, OUT endpoint 1 has 1 cables
```
If your MIDI device can generate sound, you should start hearing a pattern
of notes from B-flat to D. If your device is Mackie Control compatible, then
the transport LEDs should sequence. If you use a control on your MIDI device, you
should see the message traffic displayed on the serial console.

NOTE: Unfortunately, the `pico-sdk` does not currently allow you to use the
USB device port for console I/O and the PIO USB host port at the same time.
You can install a [library](https://github.com/rppicomidi/cdc_stdio_lib)
that lets you do this for you own projects. In these examples,
all printf() output goes to the UART 0 serial port. If you want an
example that uses this library and uses the native USB port for both
MIDI device and console output, see the [midi2piousbhub](https://github.com/rppicomidi/midi2piousbhub) project.

## Building and Testing Arduino Examples
To build and run the Arduino examples, in the Arduino IDE,
use the Library Manager to install this library and accept
all of its dependencies. If your hardware requires it,
install the Pico PIO USB library too. Next, in the IDE, select
File->Examples->usb_midi_host->arduino->[your example program name]

Use the Arduino IDE to build and run the code. Make sure to start a serial
monitor or else the code will appear to lock up.

Attach a MIDI device to the USB A port.
You should see something like this in the Serial Port Monitor (of course,
your connected MIDI device will likely be different).
```
MIDI device address = 1, IN endpoint 1 has 1 cables, OUT endpoint 2 has 1 cables
Device attached, address = 1
  iManufacturer       1     KORG INC.
  iProduct            2     nanoKONTROL2
  iSerialNumber       0

```
If your MIDI device can generate sound, you should start hearing a pattern
of notes from B-flat to D. If your device is Mackie Control compatible, then
the transport LEDs should sequence. If you use a control on your MIDI device, you
should see the message traffic displayed on the Serial Port Monitor.

# TROUBLESHOOTING, CONFIGURATION, and DESIGN DETAILS
In addition to this section, you might find
[this guide](https://github.com/rppicomidi/pico_usb_host_troubleshooting)
helpful.

## Config (Configuration) File
In C/C++, the config file for your project is called `tusb_config.h`.
It should be in the include path of your project.

In Arduino code,the config file is stored in the `libraries` directory of
your sketch directory as the file
`Adafruit_TinyUSB/src/arduino/ports/${target}/tusb_config_${target}.h`,
where `${target}` is the processor name of the processor on the target
hardware. For example, for a Rapsberry Pi Pico board, the file is
`libraries/Adafruit_TinyUSB/src/arduino/ports/rp2040/tusb_config_rp2040.h`.
Sadly, any changes you make to the config file will disappear if you update
`Adafruit_TinyUSB_Library`.

To make configuring parameters specific to the USB MIDI Host a bit simpler
for Arduino IDE users, see the function `tuh_midih_define_limits()`.

## Size of the Enumeration Buffer
When the USB Host driver tries to enumerate a device, it reads the
USB descriptors into a byte buffer. By default, that buffer is 256 bytes
long. Complete MIDI devices, or device that also have audio interfaces,
tend to have much longer USB descriptors. If a device fails to enumerate,
locate the line in your config file that contains `#define CFG_TUH_ENUMERATION_BUFSIZE` and change the default 256 to something larger
(for example, 512).
## Debug Log
If a device fails to enumerate, a debug log printout may be helpful.
Debug log levels go from 0 (no debug logging) to 3 (very verbose). The
default log level is 0. The most direct way to set the debug level is to
define `CFG_TUSB_DEBUG` in your config file. For example, to set the log
level to 2, make sure your config file contains the lines
```
#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 2
#endif
```
The conditional is in case you choose to change the debug level by
setting an environment variable.

### Bug #384 in RP2040/Raspberry Pi Pico and Adafruit_TinyUSB_Library 3.0
For Arduino, the `Adafruit TinyUSB Host` option seems to require you to define
the function `log_printf` if you use a debug log level other than 0. Adding
the following function to your program sketch should suffice as long as none
of the debug log output lines is longer than 256 bytes.
```
// Debugging
int log_printf(const char * format, ...)
{
  char outstr[256];
  va_list va;
  va_start(va, format);
  int ret = vsprintf(outstr, format, va);
  // Uncomment the next line to send the debug log to the Serial1 output
  return Serial1.print(outstr);
}
```

## Maximum Number of MIDI Devices Attached to the Host
You should define the value `CFG_TUH_DEVICE_MAX` in the tuh_config.h file to
match the number of USB hub ports attached to the USB host port. For
example
```
// max device support (excluding hub device)
#define CFG_TUH_DEVICE_MAX          (CFG_TUH_HUB ? 4 : 1) // hub typically has 4 ports
```
The Arduino IDE makes configuring this is difficult because tuh_config.h is part
of the processor core (for RP2040, anyway)

## Maximum Number of USB Endpoints
Although the USB MIDI 1.0 Class specification allows an arbitrary number
of endpoints, this driver supports at most one USB BULK DATA IN endpoint
and one USB BULK DATA OUT endpoint. Each endpoint can support up to 16 
virtual cables. If a device has multiple IN endpoints or multiple OUT
endpoints, it will fail to enumerate.

Most USB MIDI devices contain both an IN endpoint and an OUT endpoint,
but not all do. For example, some USB pedals only support an IN endpoint.
This driver allows that.

## Maximum Number of Virtual Cables
A USB MIDI 1.0 Class message can support up to 16 virtual cables. The function
`tuh_midi_stream_write()` uses 6 bytes of data stored in an array in
an internal data structure to deserialize a MIDI byte stream to a
particular virtual cable. To properly handle all 16 possible virtual cables,
`CFG_TUH_DEVICE_MAX*16*6` data bytes are required. If the application
needs to save memory, in file `tusb_cfg.h` set `CFG_TUH_CABLE_MAX` to
something less than 16 as long as it is at least 1.

For Arduino builds, you can configure this parameter at runtime by calling
tuh_midih_define_limits().

## Subclass of Audio Control
A MIDI device is supposed to have an Audio Control Interface and it may
have an Audio Streaming Interface before the the MIDI Streaming Interface,
that this driver supports. Many commercial devices do not have even the
Audio Control Interface. To support these devices, the descriptor parser
in this driver will skip past Audio Control Interface and Audio Streaming
Interface descriptors and open only the MIDI Interface.

An audio streaming host driver can use this driver by passing a pointer
to the MIDI interface descriptor that is found after the audio streaming
interface to the midih_open() function. That is, an audio streaming host
driver would parse the audio control interface descriptor and then the
audio streaming interface and endpoint descriptors. When the next descriptor
pointer points to a MIDI interface descriptor, call midih_open() with that
descriptor pointer.

## Class Specific Interface and Requests
The host driver only makes use of the informaton in the class specific
interface descriptors to extract string descriptors from each IN JACK and
OUT JACK. To use these, you must set `CFG_MIDI_HOST_DEVSTRINGS` to 1 in
your application's tusb_config.h file. It does not parse ELEMENT items
for string descriptors.

This driver does not support class specific requests to control
ELEMENT items, nor does it support non-MIDI Streaming bulk endpoints.

## MIDI Class Specific Descriptor Total Length Field Ignored
I have observed at least one keyboard by a leading manufacturer that
sets the wTotalLength field of the Class-Specific MS Interface Header
Descriptor to include the length of the MIDIStreaming Endpoint
Descriptors. This is wrong per my reading of the specification.

## Message Buffer Details
Messages buffers composed from USB data received on the IN endpoint will never
contain running status because USB MIDI 1.0 class does not support that. Messages
buffers to be sent to the device on the OUT endpont can contain running status
(the message might come from a UART data stream from a 5-pin DIN MIDI IN
cable on the host, for example), and thanks to pull request#3 from @moseltronics,
this driver should correctly parse or
compose 4-byte USB MIDI Class packets from streams encoded with running status.

Message buffers to be sent to the device may contain real time messages
such as MIDI clock. Real time messages may be inserted in the message 
byte stream between status and data bytes of another message without disrupting
the running status. However, because MIDI 1.0 class messages are sent 
as four byte packets, a real-time message so inserted will be re-ordered
to be sent to the device in a new 4-byte packet immediately before the
interrupted data stream.

Real time messages the device sends to the host can only appear between
the status byte and data bytes of the message in System Exclusive messages
that are longer than 3 bytes.

## Poorly Formed USB MIDI Data Packets from the Device
Some devices do not properly encode the code index number (CIN) for the
MIDI message status byte even though the 3-byte data payload correctly encodes
the MIDI message. This driver looks to the byte after the CIN byte to decide
how many bytes to place in the message buffer.

Some devices do not properly encode the virtual cable number. If the virtual
cable number in the CIN data byte of the packet is not less than bNumEmbMIDIJack
for that endpoint, then the host driver assumes virtual cable 0 and does not
report an error.

Some MIDI devices will always send back exactly wMaxPacketSize bytes on
every endpoint even if only one 4-byte packet is required (e.g., NOTE ON).
These devices send packets with 4 packet bytes 0. This driver ignores all
zero packets without reporting an error.

## Enumeration Failures
The host may fail to enumerate a device if it has too many endpoints, if it has
if it has a Standard MS Transfer Bulk Data Endpoint Descriptor (not supported),
if it has a poorly formed descriptor, or if the configuration
descriptor is too long for the host to read the whole thing.
The most common failure, though, is the descriptor is too long. See
[Size of the Enumeration Buffer](#size-of-the-enumeration-buffer) for
how to address that.
