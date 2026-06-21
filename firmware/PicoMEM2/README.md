# PicoMEM 2 Firmware Repository

**How to read the Firmware name:**<br /> 

**PM2_M_D_Y**     :  Standard Firmware Month, Day, Year)
**PM2_M_D_Y_MD**  :  Monochrome screen version

**WARNING:** One new firmware may add incompatibility, please try the previous versions of the firmware.<br />

## How to update the Firmware ?

**The firmware update is done like for any Pi Pico :**
Connect the Board with the USB C to a PC/Laptop and press the BOOT Button at the same time

## Firmwares Revision History :

### PM2-6-21-26: First Public Firmware release

 + .VHD Hard drive image support (Fixed size only)
 + HDD and Floppy images sorted alphabetically.
 + OLED parameter in config.txt : Select 3 type of OLED screen
 - MPU is now enabled via PMINIT
 - SB DSP commands E2 and 24 added.
 ! Now display RP2350 on the info page.
 ! Display bug from image selection when None is selected.
 ! Corrected support of sub folder from Floppy BIOS selection.
 ! IRQ 3 detection was not working, IRQ6 was not enabled.
 ! Corrected a BUG in IRQ detection (prevented some PC to BOOT)

"Bug" : PicoMEM IRQ is still hard coded, Jumper on IRQ7 is mandatory.

### PM2-5-15-26: First Public Firmware release
 - Firmware provided for the first PicoMEM testers, I will update it here quite soon.<br />
  To be used with the latest PMINIT to be able to enable the SB and GUS Modes.
 - General MIDI is enabled if you enable the Audio in the BIOS, and use Port 330<br />
   General MIDI Sound font : GMGSx.sf2 is in the firmware ROM