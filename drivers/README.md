# ISA PicoMEM Extension board (For 8086/8088 PC)

## Introduction
This folder contains the drivers needed for the PicoMEM

17/05/24 : PMMOUSE Updated to correct a bug with ne2000 on IRQ 3

## NE2000.COM

ne2000 TCP Driver for DOS, to be used with Mtcp

command line : ne2000 0x60 0x3 0x300

The IRQ can be changed in the BIOS Menu (Default is IRQ 3)

## PM2000.COM

Same driver as NE2000 for the PicoMEM : IRQ and Port are Auto detected.
Does not start if the PicoMEM is not detected

09/04/2024 : Not loaded in the ne2000 emulation is not enabled. Parameters were displayed 2 times.
03/03/2024 : Removed part of the Copyright text to reduce the display length

## PMMOUSE.COM

PicoMEM Mouse driver is a modified CTMouse driver.
It can detect the PicoMEM and does not need any parameter (IRQ is Auto detected)

## PMEMM

EMS Driver derivated LTEMM.
You need to add it in CONFIG.SYS, like this :

DEVICE=PMEMM.EXE /n

The Driver is able to detect the PicoMEM, the EMS Port and Address.
Then, /p and /i parameters are not needed.

## PMINIT.EXE

PicoMEM Initialisation tool.<br />
I will regularly update the tools to add init fonctions when DOS is Booted.<br />

To enable the Key Shortcut: (Left Shift + Ctrl + F1 for information and F2 For A: Floppy change)<br />
PMINIT /k

Rev 0.2 : Added CMS, Adlib and Tandy initialisation (October 2024 Firmware)<br />

/adlib x  - Enable/Disable the Adlib output (0:Off 1:On)<br />
/cms x    - Enable/Disable the CMS output   (0:Off 1:220 x:Port)<br />
/tdy x    - Enable/Disable the Tandy output (0:Off 1:2C0 x:Port)<br />
/ x       - Enable/Disable the Joystick     (0:Off 1:On)<br />

