# ISA PicoMEM Extension board (For 8086/8088 PC)

## Introduction
This folder contains the drivers needed for the PicoMEM<br />


## NE2000.COM

ne2000 TCP Driver for DOS, to be used with Mtcp<br />

command line : ne2000 0x60 0x3 0x300<br />

The IRQ can be changed in the BIOS Menu (Default is IRQ 3)<br />

## PM2000.COM

Same driver as NE2000 for the PicoMEM : IRQ and Port are Auto detected.<br />
Does not start if the PicoMEM is not detected<br />

### Rev 0.4 <br /> (January 24, 2025) 
- ! Corrected a bug occuring when a received packet has an odd size.<br />
- PM2000 has now less bug than NE2000 !<br />

### Rev 0.3 <br /> (January 2025)
- ! PM2000 Was not working on CPU >= 80186
- + A little faster on 8088/8086

09/04/2024 : Not loaded if the ne2000 emulation is not enabled. Parameters were displayed 2 times.<br />
03/03/2024 : Removed part of the Copyright text to reduce the display length<br />

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

/adlib x  - Enable/Disable the Adlib output (0:Off 1:On)<br />
/cms x    - Enable/Disable the CMS output   (0:Off 1:220 x:Port)<br />
/tdy x    - Enable/Disable the Tandy output (0:Off 1:2C0 x:Port)<br />
/mmb x    - Enable/Disable the Tandy output (0:Off 1:300 x:Port)<br />
/j x      - Enable/Disable the Joystick     (0:Off 1:On)<br />
/diag     - Start in Diagnostic Mode <br />

### Rev 0.4 <br /> (May 2025)
- + Added the Mindscape Music Board parameter (mmb)<br />

### Rev 0.3 <br /> (December 2024)
- ! Corrected the Tandy initialisation : Was not implemented<br />
- + Added some test in the Diagnostic Mode<br />

### Rev 0.2 : Added CMS, Adlib and Tandy initialisation (October 2024 Firmware)<br />


## PMDFS.EXE<br />

This driver will map the MicroSD and one USB dongle as "FileShare / Network redirector" type drive.<br />
Using this driver, you can hotplug, copy files from an USB, and store whatever you want to the MicroSD directly, even edit the PicoMEM Config files.<br />

This driver is derivated from the etherDFS driver by Mateusz Viste<br />

Usage: pmdfs rdrv-ldrv [rdrv2-ldrv2 ...] [options]<br />
       pmdfs /u<br />
    rdrv is S for SD, U for USB<br />

Options:<br />
/q      quiet mode (print nothing if loaded/unloaded successfully)<br />
/u      unload PMDFS from memory\<br />

Example: pmdfs S-D U-E<br />
> will map the SD to D and USB to E<br />

**Use PMDFS3 for DOS 3.2 to 3.31 and PMDFS for DOS 4 and more.**<br />

Warning : This driver can't be used if the PicoMEM BIOS is not loaded.<br />


## ASTCLOCK.COM<br />

The PicoMEM added the first AST 6 Pack board RTC emulation.<br />

ASTCLOCK can set the DOS date to the PicoMEM time and can update it if kept resident.<br />

Usage : ASTCLOCK /R  (/R for resident)<br />

**WARNING :** The PicoMEM RTC initially set the date to May 22 2025 and stay saved only if th ePicoMEM is kept powered on, via the USB port.<br />
Time is saved after a PC soft reboot.<br />
