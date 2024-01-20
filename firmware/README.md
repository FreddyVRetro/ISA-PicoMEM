# ISA PicoMEM Firmware Repository

How to read thhe Firmware name 

PM_MonthDay_D0 : PicoMEM Firmware released the Day/Month at Base Address D0

## How to update the Firmware ?

The firmware update is done like for any Pi Pico : 

Connect the Board with the MicroUSB to a PC/Laptop and press the BOOTSEL Button on the Pico at the same time:
- A Folder is the automùatically opened on the PC, you need to copy the file to this “Virtual Disk”, 
  wait until the folder automatically closes and it is finished.

Difference for the PicoMEM 1.1 : 
- The PicoMEM must be plugged into a PC so that the programming work. 
  Power on the PC (The Retro one) and press the button at the same time.

## Firmwares Revision History : 

### PM_Dec28: 5150 Support is Back
Corrected the 5150 Support, If someone can test it more to be sure….
Added BIOS Menu option to select uSD and PSRAM Speed, but not implemented in the Pico
Speed of I/O and Memory access increased globally, so may be worth trying on other machines

### PM_Nov26: 
Modified the uSD Code. This now use the native and unmodified FasFS SPI Code.
Faster and compatible with much more uSD.
uSD Clocked at 24MHz.

### PM_Nov11: WARNING : Does not work anymore on 5150, Use the Oct 13 code for it.
USB Mouse added
Floppy disk B: Working

### PM_Oct13: WARNING : Not reliable on 5150
Other BIOS Display corrected (5150, Other) Settings always On, Segment number.
Start always during the BOOT Menu
Disk Drive B: not working (Correction in progress)

### PM_Sept26: Last version Ok for 5150
Correct (Again) a new bug from previous version : Floppy image selection
Remove the IRQ Stop during Int13h if PSRAM is not used to emulate memory : 
            The Disktest result will be more “Correct” (No Timer IRQ Missed)

### PM_Sept25:
Corrected a bug in the image selection (Added by previous version)
Corrected a Display bug with some 5150 BIOS Version

### PM_Sept23:
Added EMS emulation display at boot time
Add Video RAM Segment exclusion from the emulation
