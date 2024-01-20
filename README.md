# ISA PicoMEM Extension board (For 8086/8088 PC)

### Introduction

The PicoMEM is designed as a way to run Emulated ISA boards on a real PC.
The PicoMEM Board currently connect the full 8Bit Memory and I/O Bus plus an IRQ to a Raspberry Pi Pico, through some multiplexor/Level shifter chip.
The Pi Pico also has a 8Mbyte PSRAM connected in SPI and a MicroSD Slot.

The PicoMEM Board can be seen as both a working PC extention board as well as a Development platform.

This GitHub Repository does not contains the Firmware for the moment.
However, PMMOUSE and PMEMM Source are available.


### Board description

The PicoMEM exist in 3 Releases : 1.0, 1.1 and 1.11

Hardware : 
  - 2 Layers ISA Board connecting a Raspberry Pi Pico through some buffers/Multiplexor and inverter.
  - The Pi Pico is connected to the full Memory and I/O Address space, and one IRQ. (No DMA)
  - The Pi Pico is connected to a PSRAM (8Mb, 100/130MHz, PIO) and a MicroSD (12/24MHz, SPI) slot in SPI.
  - The Pi Pico is overclocked at 270MHz.
  - The MicroSD connector share the SPI BUS of the PSRAM, adding some limitations. (We need to stop the PC IRQ during Disk Access)
  - One IRQ Line that can be connected to IRQ 3 or 5 for Rev 1.0, 2,5 or 7 for Rev 1.1.
  - QwiiC Connector (SPI) (Added to V1.1)
  - Rev 1.11 Changes : Allow for programming with the Board disconnected and USB Powser Jumper can be always On.

Software :
  - a Full BIOS with a "Phoenix BIOS Like" text interface in assembly.
  - C/C++ Code in the Pi Pico.
  - Multiple other projects library used. (List below)
  - The PC can send multiple command to ask the Pico to perform tasks.
  - In the reverse, the Pico can also send commands to the Pico.

### Current Functionality

- Memory emulation with 16Kb Address granularity:
  > 128Kb of RAM can be emulated from the Pi Pico internal RAM with No Wait State.
  > We can emulate the whole 1Mb of RAM address space from the PSRAM. (With 6 Wait Stated minimum added)
  > EMS Emulation of Up to 6/7 Mb. (Only 4Mb for the moment as using the LoTech EMS Driver)
  > Memory emulation is used to add 4Kb of "Private" memory for the PicoMEM BIOS Usage.
  > 16Kb of RAM is also added for disk access (Or other) 512b only is used for the moment.

- ROM Emulation for its internal BIOS and custom ROM loaded from the MicroSD. (Custom ROM not implemented yet)
- The Board has its own BIOS, used to automatically detect/Extend/Configure the RAM emulation and select Floppy/Disk images.
- Floppy and Disk emulation from .img files stored in uSD through FasFs and DosBOX int13h emulation code.
- Emulate 2 Floppy and 4 Disk (80h to 83h), Disk up to 4Gb (More later)
- USB Mouse support through a USB OTC Adapter. (Micro USB to USB A or USB Hub)

### Future Functionality

- There is already a mecanism implemented so that the Pi Pico can send command to the PC, ve can have the Pi Pico taking "Virtually" the control of the PC.
  > This can be used to perform ROM/RAM dump, Disk/Floppy DUMP/Write, display/kb redirection....
- More USB Host to be added  (Keyboard, Joystick...)
- I added a connector on the board, that can open the door for lot of stuff (More or less "Secret" for the moment, I need to keep some surprize for myself)
- Use the Pi PicoW for ne2000 network card emulation through Wifi : Proof of concept done already.
- Bluetooth support for device like Gamepad may be added.

### Compatibility/Limitations
 
The Board can't be used for Video emulation, as it require a way for the Pico to actually display something, and only 3 pins are "Free".
The Pi Pico is limmited in its speed, this is excellent and bad at the same time:
- Multiple complex function can't be emulated at the same time, choices need to be done.
- We can still program the Pico and Feel like doing coding on a "Retro" Machine, so we don't have the effect "Look, it is easy, he put a processor in the PC that can emulate the full PC"

Memory emulation:
- Memory emulation with PSRAM is quite slow for the moment, but multiple mecanism like a 32bit cache will improve this. (And Maybe DMA)
  
- The emulated Memory does not support DMA, I did not proof it is not 100% feasible, but it require quite complex coding to be possible (Snif the DMA registers, change code that may add unstability)
  Anyway:  
    - As the PicoMEM emulate the Floppy, we can disable temporarily the RAM emulation if the Disk access are not working.
    - For SoundCard, if the PC has 512Kb of base RAM, it is really unlikely that the DMA Buffer will be placed in emulated RAM, it may work 90% of the time.

Disk emulation:
uSD Disk access are really fast even compared to the XTIDE, but it it currently limitted by the uSD acces time for write.
You may have compatibility problem with some uSD, even if the compatibility has been greatly improved in the November 2023 firmware.

Tested machines:
- IBM 5150, 5160, 5170 : All Ok, except keyboard not responding on 5170.
- Compaq Portable 2 (286): Ok
- Amstrad PC1512, PC1640, PC200: Working, but fail to start all the times on one PC1512 and sometime on PC1640 (Corrected in Rev 1.1).
- Tandy 1000 : Does not work yet due to the specific memory map.
- Worked on Various 486, 386 (No confirmation of the ISA Clock speed yet)
- Failed on a 386 with 12MHz ISA Clock


### License

GNU v2
For Commercial use, you must contact me before.

### Contributors / External Libraries

* [Ian Scott](https://github.com/polpo/): Idea to use the Pi Pico instead of the not available Pi 2/4 and Zero plus help on the Hardware design.
* [PSRAM Code by Ian Scott](https://github.com/polpo/rp2040-psram) : PSRAM PIO Code.
* [FatFS by Chan](http://elm-chan.org/fsw/ff/00index_e.html) : ExFS Library for microcontrollers.
* [FatFS by Carl J Kugler III] (https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico) : FatFS for SD Access in SPI with a Pi Pico
* DOSBox (www.dosbox.com): DosBOX BIOS Int13h Code, heavily modified for ExFS support and PicoMEM Interface, with bug correction.
