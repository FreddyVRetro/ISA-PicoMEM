# ISA PicoMEM Extension board (For 8086/8088 PC)

<a href="url"><img src="https://github.com/FreddyVRetro/ISA-PicoMEM/blob/main/jpg/PM111_Close.jpg" align="middle" height=50% width=50% ></a>

## Introduction

The PicoMEM is designed as a way to run Emulated ISA boards on a real PC.<br />
The PicoMEM Board currently connect the full 8Bit Memory and I/O Bus plus an IRQ to a Raspberry Pi Pico, through some multiplexor/Level shifter chip.<br />
The Pi Pico also has a 8Mbyte PSRAM connected in SPI and a MicroSD Slot.<br />

The PicoMEM Board can be seen as both a working PC extention board as well as a Development platform.

This GitHub Repository does not contains the Firmware for the moment.<br />
However, PMMOUSE, PMEMM and PM2000 Source are available.

To follow the PicoMEM news, see it in action on multiple machines you can follow me on X:
[FreddyV on X](https://x.com/FreddyVETELE)

To see my other projects, here is my Youtube Channel:
[FreddyV Retro Zone](https://www.youtube.com/@freddyvretrozone2849)

### How to get a PicoMEM ?

**The PicoMEM is available at Texelec !**
[Texelec PicoMEM link](https://texelec.com/product/picomem/?highlight=picomem)

You can have one from me (Mainly for Europe, with wait time), on my form:
[FreddyV PicoMEM Form](https://docs.google.com/forms/d/e/1FAIpQLScwYPvnVoGynLLgP_hiLMH_qn9uBX1sxims7Ah4LabjQ0mSLw/viewform)

Gerber and full sources are not available for the moment, It is planned to have them open source (no date for the moment)

## Board description

The PicoMEM exist in 3 Releases : 1.0, 1.1 and 1.11<br />

**Hardware :**
  - 2 Layers ISA Board connecting a Raspberry Pi Pico through some buffers/Multiplexor and inverter.
  - The Pi Pico is connected to the full Memory and I/O Address space, and one IRQ. (No DMA)
  - The Pi Pico is connected to a PSRAM (8Mb, 100/130MHz, PIO) and a MicroSD (12/24MHz, SPI) slot in SPI.
  - The Pi Pico is overclocked at 270MHz.
  - The MicroSD connector share the SPI BUS of the PSRAM, adding some limitations. (We need to stop the PC IRQ during Disk Access)
  - One IRQ Line that can be connected to IRQ 3 or 5 for Rev 1.0, 2,5 or 7 for Rev 1.1.
  - QwiiC Connector (SPI) (Added to V1.1)
  - Rev 1.11 Changes : Allow for programming with the Board disconnected and USB Powser Jumper can be always On.

**Software :**
  - a Full BIOS with a "Phoenix BIOS Like" text interface in assembly.
  - C/C++ Code, PIO and ARM assembly on the Pi Pico.
  - Multiple other projects library used. (List below)
  - The PC can send multiple command to ask the Pico to perform tasks.
  - In the reverse, the Pico can also send commands to the Pico.

## Current Functionality

**- Memory emulation with 16Kb Address granularity:**
  > 128Kb of RAM can be emulated from the Pi Pico internal RAM with No Wait State.
  > We can emulate the whole 1Mb of RAM address space from the PSRAM. (With 6 Wait Stated minimum added)
  > EMS Emulation of Up to 6/7 Mb. (Only 4Mb for the moment as using the LoTech EMS Driver)
  > Memory emulation is used to add 4Kb of "Private" memory for the PicoMEM BIOS Usage.
  > 4Kb of RAM is also added for disk access (Or other) 512b only is used for the moment.

- **ROM Emulation** for its internal BIOS and custom ROM loaded from the MicroSD. (Custom ROM not implemented yet)<br />
  The Board has its own BIOS, used to automatically detect/Extend/Configure the RAM emulation and select Floppy/Disk images.
- **Floppy and Disk** "emulation" from .img files stored in uSD through FasFs and DosBOX int13h emulation code.<br />
  Emulate 2 Floppy and 4 Disk (80h to 83h), Disk up to 4Gb (More later)
- **USB Mouse** support through a USB OTC Adapter. (Micro USB to USB A or USB Hub)
- **POST Code** on Rev 1.1 and 1.11 (Port 80 Display in Hexa) through this device : https://www.sparkfun.com/products/16916
- NEW: (Still "Beta") **ne2000 network card** emulation via Wifi is working on boards with a PicoW.
- NEW: **USB Joystick** for PS4 and Xinput controllers.

## Future Functionality

- There is already a mechanism implemented, so that the Pi Pico can send command to the PC, we can have the Pi Pico taking "Virtually" the control of the PC.
  > This can be used to perform ROM/RAM dump, Disk/Floppy DUMP/Write, display/kb redirection....
- More USB Host to be added  (Keyboard, MIDI...)
- I added a connector on the board, that can open the door for lot of stuff. <br />
(More or less "Secret" for the moment, I need to keep some surprize for myself)
- Use the Pi Pico W for ne2000 network card emulation through Wifi : Proof of concept done already.
- Bluetooth support for device like Gamepad may be added.
- Use of the Qwiic connector for more information display (OLED), maybe RTC and other.

## Compatibility/Limitations
 
The Board can't be used for Video emulation, as it require a way for the Pico to actually display something, and only 3 pins are "Free".
The Pi Pico is limmited in its speed, this is excellent and bad at the same time:
- Multiple complex function can't be emulated at the same time, choices need to be done.
- We can still program the Pico and Feel like doing coding on a "Retro" Machine, so we don't have the effect "Look, it is easy, he put a processor in the PC that can emulate the full PC"

## Memory emulation

As the PicoMEM is an 8Bit ISA Card, the RAM Emulation may be slower than the PC own RAM.
The PicoMEM is then **more suitable to extend a 512Kb PC to 640K**, Add some UMB (For Driver) **than extend an IBM PC With 128Kb of RAM**.
(RAM Card for 5150/5160 is still recommended)

### Memory emulation capabilities :

- The PicoMEM BIOS auto detect (At Hardwsare level) the 1st Mb of RAM and Display the RAM MAP with a "Checkit" like Display.
- You can select to add RAM from the Internal SRAM (128Kb with No Wait State)
- Emulation from the PSRAM (External RAM) can add RAM to any Address Space.
- Then, EMS Emulation emulate a LOTECH Board with Up to 4Mb.
- Disk access on PM RAM emulated memory are done at the Pico uSD speed. (3Mb/s)

### Memory emulation limitations :

- Memory emulation with PSRAM is quite slow for the moment, but multiple mechanism like a 32bit cache will improve this. (And Maybe DMA)
- **The emulated Memory does not support DMA**, Add support for it may be done in one or two months (May/June 2024)
  Anyway:  
    - As the real floppy use DMA, **you should disable temporarily the RAM emulation** if the real Disk access are not working.
    - For SoundCard, if the PC has 512Kb of base RAM, it is really unlikely that the DMA Buffer will be placed in emulated RAM, it may work 90% of the time.

## Disk "emulation"

The PicoMEM can add 4 disks to the BIOS, Disk up to 4Gb.<br />
It can also mount Floppy image as A: or B:

The PicoMEM does not emulate disk, it send the BIOS Disk access commands to the Pi Pico. <br />
Then, it is more a "Disk BIOS" emulator than a Disk emulator. <br />

Another particularity is that it use Memory to perform the Data transfer, this allow for the maximum possible transfer speed, even on 8088 CPU. <br />
Anyway, single sector read is slower than multiple sector reas as the PicoMEM need to read the sector from the SD, then send the data to the PC Memory. <br />
With multiple sector Read, the Pico read the next sector while the data is copied to the PC Memory.

It is highly recommended to use reasonable disk size, below 500Mb. <br /> 
100 or 200Mb are ideal size, as it will be recognized and usable by DOS 6 and DOS 3.31 <br />
As you can add 4 Disk with 4 partitions, you can create diferent disk images for different usage, like a partition with games, another with music ...

**Warning :** You may have compatibility problem with some uSD, even if the compatibility has been greatly improved in the November 2023 firmware.<br />

## ne2000 emulation via Wifi

The PicoMEM can emulate a ne2000 network card via Wifi.<br />
The wifi code is preliminary, the PicoMEM can't see if the access point is connected or not, code improvement in progress.

**How to use it :**
- Create a wifi.txt file with the SSID in the first line and the Password in the 2nd line
- use ne2000.com 8Bit (command line : ne2000 0x60 0x3 0x300) or pm2000 (command line : pm2000 0x60)
- The PicoMEM can't tell if the connection fail and does not try to re connect.
- The Wifi Access point need to be relatively close to increase the chance of connection success. The IRQ Can be changed in the BIOS Menu (Default is IRQ 3)<br />

## Tested machines :
- IBM 5150, 5160, 5170
- IBM PS/2 30 286 (Warning : As its HDD use DMA, does not boot if emulated RAM is added)
- Compaq Portable 2 (286): Ok
- Amstrad PC1512, PC1640, PPC1640 (Address D000), Sinclair PC200 (It is my DEV Machine)
- Schneider Euro PC2, Olivetti M21, Sega TeraDrive.
- Worked on Various 486, 386, 286 (Has more chance to work with lower ISA Clocks)
- Tested with some Pentium, Pentium MMX, Pentium 2, AMD K6 ...<br />
- Amiga 2000 with a A2286 Board. (Use the BIOS in C800)
- Book8088 (At 4.77MHz, next FW will work at 8MHz)

## Failing Machines :
- Failed on a 386 with 12MHz ISA Clock
- Fail on some fast 286 (16MHz)
- Commodore PC10 / PC20 (Timing issue, fix in progress)
- Schneider Euro PC 1
- **Tandy 1000** : Does not work yet due to the specific memory map. (SX, EX, HX, TL ...)<br />

## License

Hardware and Software are currently closed. <br />

As the PicoMEM contains various public source, I Will publish it as well as Public.<br />
Anyway, I consider it not ready to be opened yet, as I am not sure about the License to use and how to manage it.<br />
If you want to participate, You can ask me access to the source on demand.<br />

## Contributors / External Libraries

* [Ian Scott](https://github.com/polpo/): Idea to use the Pi Pico instead of the not available Pi 2/4 and Zero plus help on the Hardware design.
* [PSRAM Code by Ian Scott](https://github.com/polpo/rp2040-psram) : PSRAM PIO Code.
* [FatFS by Chan](http://elm-chan.org/fsw/ff/00index_e.html) : ExFS Library for microcontrollers.
* [FatFS by Carl J Kugler III](https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico) : FatFS for SD Access in SPI with a Pi Pico
* DOSBox (www.dosbox.com): DosBOX BIOS Int13h Code, heavily modified for ExFS support and PicoMEM Interface, with bug correction.
* ne2000 Emulation code, adapted by yyzkevin.
* XInput library for the USB Joystick.
