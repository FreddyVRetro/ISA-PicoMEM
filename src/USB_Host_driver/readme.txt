PicoMEM USB Host

-- Support --
Joystick : PS4 and xinput
Mouse    : This seems to ne standard/generic ans should work with all mouse

-- Lib/Code used --

It is currently a mix of PicoGUS Code, Rumbledethumps and tusb_xinput by Ryzee119

From PicoGUS      : PS4 Joystick
From rumblethumps : Detection logic and connection Status update
From tusb_xinput  : Full tinyUSB driver integrated as a lib

https://github.com/picocomputer/rp6502

The goal is to manage HID devices mount, events and update in real time one data structure per device type.
The data on these structures are then used by the ISA devices code to send the data to the PC / emulate devices


hid_app.c : Contains the fonctions called by TinyUSB, detect the device type and redirect/dispatch to joystick/mouse
usb.c     : Code to update the USB Status variables
hid_dev.h : PicoMEM hid devices structures


https://github.com/OpenStickCommunity/GP2040-CE ?
https://github.com/wiredopposite/OGX-Mini/
https://github.com/necroware/gameport-adapter

USB Ethernet: (Device mode :( )
https://github.com/anticitizn/pico-web-sensor/tree/master

https://github.com/rppicomidi/usb_midi_host
