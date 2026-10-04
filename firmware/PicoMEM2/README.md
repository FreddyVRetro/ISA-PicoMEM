# PicoMEM 2 Firmware Repository

**How to read the Firmware name:**<br /> 

**PM2_M_D_Y**     :  Standard Firmware Month, Day, Year)
**PM2_M_D_Y_MD**  :  Monochrome screen version

**WARNING:** One new firmware may add incompatibility, please try the previous versions of the firmware.<br />

## How to update the Firmware ?

**The firmware update is done like for any Pi Pico :**<br />
Connect the Board with the USB C to a PC/Laptop and press the BOOT Button at the same time

## Firmwares Revision History :

### PM2-10-4-26: CD-ROM and ROM emulation
- + CD-ROM and CD Audio emulation added (MKE Interface)
- + ROM emulation added : ROM image is configured in the config.txt file
- + HDD and Floppy images can now be selected from USB
- + CD-ROM selection and CD player added to the PicoMEM Front Panel
- + Front panel can be controled by a USB Joystick (Home+Right button)
- + OLED display when a Joystick is plugged in
- + ADPCM added in the Sound Blaster emulation (Duke Nukem 2)
- + ArdSynth rendering added to the General MIDI mode. (https://github.com/DLehenbauer/arduino-midi-sound-module)
- + Multiple PicoMEM POST Code added
- ! SB DSP Does not answer init end directly
- ! IO ports emulation code called from a table
- ! PicoMEM interrupt can be defined to any IRQ (IRQ parameter in config.txt)
- ! Multiple bug fix in the RTC emulation for GlaTick support
- ! Now the PicoMEM written file has the correct date/time (PMDFS, Config)
- ! Multiple bug fix in the DFS code (USB/SD access)
- ! Sound Blaster IRQ/DMA config was not implemented (Was using default)
- ! Define Use PicoMEM Boot by default

 **It is recommended to updatte PMDFSD and PMINIT as well**

### PM2-6-21-26: First Public Firmware release

- + .VHD Hard drive image support (Fixed size only)<br />
- + HDD and Floppy images sorted alphabetically.<br />
- + OLED parameter in config.txt : Select 3 type of OLED screen<br />
- - MPU is now enabled via PMINIT<br />
- - SB DSP commands E2 and 24 added.<br />
- ! Now display RP2350 on the info page.<br />
- ! Display bug from image selection when None is selected.<br />
- ! Corrected support of sub folder from Floppy BIOS selection.<br />
- ! IRQ 3 detection was not working, IRQ6 was not enabled.<br />
- ! Corrected a BUG in IRQ detection (prevented some PC to BOOT)<br />

"Bug" : PicoMEM IRQ is still hard coded, Jumper on IRQ7 is mandatory.

### PM2-5-15-26: First Public Firmware release
 - Firmware provided for the first PicoMEM testers, I will update it here quite soon.<br />
  To be used with the latest PMINIT to be able to enable the SB and GUS Modes.
 - General MIDI is enabled if you enable the Audio in the BIOS, and use Port 330<br />
   General MIDI Sound font : GMGSx.sf2 is in the firmware ROM
