# ISA PicoMEM Extension board (For 8086/8088 PC)

**PicoMEM 1.14:**<br />
<a href="url"><img src="https://github.com/FreddyVRetro/ISA-PicoMEM/blob/main/jpg/PM114_Close.jpg" align="middle" height=40% width=40% ></a>

## Introduction

The PicoMEM is designed as a way to run Emulated ISA boards on a real PC.<br />
The PicoMEM Board currently connect the full 8Bit Memory and I/O Bus plus an IRQ to a Raspberry Pi Pico, through some multiplexor/Level shifter chip.<br />
The Pi Pico also has a 8Mbyte PSRAM connected in SPI and a MicroSD Slot.<br />

The PicoMEM Board can be seen as both a working PC extention board as well as a Development platform.

Full Firmware and drivers sources available.<br />
**PicoMEM Hardware is not open.**<br />

**NEW : SD and USB disk direct access** A Network redirector driver allow full access to the SD and USB for DOS 3.2 + !<br />
The PicoMEM is now the only board providing full USB drive in FAT23/ExtFS from DOS.

To follow the PicoMEM news, see it in action on multiple machines you can follow me on X:<br />
[FreddyV on X](https://x.com/FreddyVETELE)

To see my other projects, here is my Youtube Channel:<br />
[FreddyV Retro Zone](https://www.youtube.com/@freddyvretrozone2849)

### How to get a PicoMEM ?

**The PicoMEM is available at Texelec ! (USA / World)**
[Texelec PicoMEM link](https://texelec.com/product/picomem/?highlight=picomem)

**The PicoMEM at Serdashop (Europe / World)**
[Serdashop PicoMEM](https://www.serdashop.com/PicoMEM)

**The PicoMEM at Flamelily Shop (UK /World) - BOARDS On SALE**
[Flamelily PicoMEM link](https://shop.flamelily.co.uk/picomem)

You can have one from me (Mainly for Europe, with wait time), on my form:
[FreddyV PicoMEM Form](https://docs.google.com/forms/d/e/1FAIpQLScwYPvnVoGynLLgP_hiLMH_qn9uBX1sxims7Ah4LabjQ0mSLw/viewform)


### How to use the PicoMEM ?

Please go to the Wiki page : https://github.com/FreddyVRetro/ISA-PicoMEM/wiki

## Board description

The PicoMEM exist in 4 Versions : 1.0, 1.1, 1.11, 1.14 and 1.2A<br />

PicoMEM 1.2A : It is a PicoMEM 1.14 with the DAC Added on Board, DAC Added by Serdaco.<br />
PicoMEM LP 1.0 : PicoMEM variant designed for Low profile ISA Slot, like on the Sinclair PC200.<br />

**Picomem LP (Low profile):**<br /> 
<a href="url"><img src="https://github.com/FreddyVRetro/ISA-PicoMEM/blob/main/jpg/PM10LP.jpg" align="middle" height=30% width=30% ></a>

**Hardware :**
  - 2 Layers ISA Board connecting a Raspberry Pi Pico through some buffers/Multiplexor and inverter.
  - The Pi Pico is connected to the full Memory and I/O Address space, and one IRQ. (No DMA)
  - The Pi Pico is connected to a PSRAM (8Mb, 100/130MHz, PIO) and a MicroSD (12/24MHz, SPI) slot in SPI.
  - The Pi Pico is overclocked at 270MHz.
  - The MicroSD connector share the SPI BUS of the PSRAM, adding some limitations. (We need to stop the PC IRQ during Disk Access)
  - One IRQ Line that can be connected to IRQ 3 or 5 for Rev 1.0, 2,5 or 7 for Rev 1.1.
  - QwiiC Connector (SPI) (Added to V1.1)

**NEW** PicoMEM LP 1.0 the Low profile variant of the PicoMEM will soon be available !

**Software :**
  - A Full BIOS with a "Phoenix BIOS Like" text interface in assembly.
  - C/C++ Code, PIO and ARM assembly on the Pi Pico.
  - Multiple other projects library used. (List below)
  - The PC can send multiple command to ask the Pico to perform tasks.
  - In the reverse, the Pico can also send commands to the Pico.

## Current Functionality

- **Memory emulation with 16Kb Address granularity:**<br />
   128Kb of RAM can be emulated from the Pi Pico internal RAM with No Wait State.<br />
   It can emulate the whole 1Mb of RAM address space from the PSRAM. (With 4-5 Wait States added)<br />
   4MB of EMS Emulation.<br />
   Memory emulation is used to add 4Kb of "Private" memory for the PicoMEM BIOS Usage.<br />
   PicoMEM Disks data transfer done via the emulated Memory.<br />

- **ROM Emulation** for its internal BIOS and custom ROM loaded from the MicroSD. (Custom ROM not implemented yet)<br />
   The Board has its own BIOS, used to automatically detect/Extend/Configure the RAM emulation and select Floppy/Disk images.
- **Floppy and Disk** "emulation" from .img files stored in uSD through FasFs and DosBOX int13h emulation code.<br />
   Emulate 2 Floppy and 4 Disk (80h to 83h), Disk up to 4Gb (More later)
- **NEW : SD and USB disk direct access** A Network redirector driver allow full access to the SD and USB for DOS 3.2 +<br />
   As it is a network redirector, the SD and USB filesystem can be anything (Even FAT32/ExtFS)<br />
- **USB Mouse** support through a USB OTC Adapter. (Micro USB to USB A or USB Hub)
- **POST Code** (Port 80 Display in Hexa) via the QwiiC connector: https://www.sparkfun.com/products/16916
- **ne2000 network card** emulation via Wifi (Pico W PicoMEM only)
- **USB Joystick** for PS4 and Xinput controllers.
- **Adlib** using a PCM5102 I2S module.
- **CMS/Game Blaster and Tandy** sound chip are now supported.
- **Tandy 1000** (Old models with Tandy Graphic) now supported, even for RAM upgrade.

## Future Functionality

- There is already a mechanism implemented, so that the Pi Pico can send command to the PC, we can have the Pi Pico taking "Virtually" the control of the PC.
  > This can be used to perform ROM/RAM dump, Disk/Floppy DUMP/Write, display/kb redirection....
- More USB Host to be added  (Keyboard, MIDI...)
- Bluetooth support for device like Gamepad may be added.
- Use of the Qwiic connector for more information display (OLED), maybe RTC and other.

## Compatibility/Limitations
 
The Board can't be used for Video emulation, as it require a way for the Pico to actually display something, and only 3 pins are "Free".
The Pi Pico is limmited in its speed, this is excellent and bad at the same time:
- Multiple complex function can't be emulated at the same time, choices need to be done.
- We can still program the Pico and Feel like doing coding on a "Retro" Machine, so we don't have the effect "Look, it is easy, he put a processor in the PC that can emulate the full PC"
- There are still problems on various machines.

## Memory emulation details :

As the PicoMEM is an 8Bit ISA Card, the RAM Emulation is most of the time slower than the PC own RAM.
The PicoMEM is then more suitable to extend a **512KB PC to 640KB**, Add some UMB (For Driver) than **extend an IBM PC With 128Kb of RAM**.
(Other RAM Card for 5150/5160 is still recommended, as full 0Wait State)

### Memory emulation capabilities :

- The PicoMEM BIOS auto detect (At Hardware level) the 1st Mb of RAM and Display the RAM MAP with a "Checkit" like Display.
- You can select to add RAM from the Internal SRAM (128Kb with No Wait State)
- Emulation from the PSRAM (External RAM) can add RAM to any Address Space. (2x slower than SRAM emulated RAM)
- EMS Emulation emulate a LOTECH Board with Up to 4Mb.
- Disk access on PM RAM emulated memory are done at the Pico uSD speed. (3Mb/s)

### Memory emulation limitations :

- Memory emulation with PSRAM is quite slow for the moment, but multiple mechanism like a 32bit cache will improve this. (And Maybe DMA)
- **The emulated Memory does not support DMA**, Add support for it may be done in one or two months (May/June 2024) <br />
  Then :
    - As the real floppy use DMA, **you should disable temporarily the RAM emulation** if the real Disk access are not working.
    - For SoundCard, if the PC has 512Kb of base RAM, it is really unlikely that the DMA Buffer will be placed in emulated RAM, it may work 90% of the time.
    - Boot/Use of MFM / XT Hard Drive with DMA will fail.

## Disk "emulation"

The PicoMEM can add 4 disks to the BIOS, Disk up to 4Gb.<br />
It can also mount Floppy image as A: or B:

The PicoMEM does not emulate disk, it send the BIOS Disk access commands to the Pi Pico. <br />
Then, it is more a "Disk BIOS" emulator than a Disk emulator. <br />

Another particularity is that it use Memory to perform the Data transfer, this allow for the maximum possible transfer speed, even on 8088 CPU. <br />
Anyway, single sector read is slower than multiple sector reas as the PicoMEM need to read the sector from the SD, then send the data to the PC Memory. <br />
With multiple sector Read, the Pico read the next sector while the data is copied to the PC Memory.

It is highly recommended to use reasonable disk image size, below 500Mb. <br /> 
100 or 200Mb are ideal size, as it will be recognized and usable by DOS 6 and DOS 3.31 <br />
As you can add 4 Disk with 4 partitions, you can create diferent disk images for different usage, like a partition with games, another with music ... <br />

**Warning :** You may have compatibility problem with some uSD, even if the compatibility has been greatly improved in the November 2023 firmware.<br />

## ne2000 emulation via Wifi

The PicoMEM can emulate a ne2000 network card via Wifi.<br />
The wifi code is preliminary, the PicoMEM can't see if the access point is connected or not, code improvement in progress.

**How to use it :**
- Create a wifi.txt file with the SSID in the first line and the Password in the 2nd line.<br />
- Use ne2000.com 8Bit (command line : ne2000 0x60 0x3 0x300) or pm2000 (command line : pm2000 0x60)<br />
- The PicoMEM can't tell if the connection fail and does not try to re connect.<br />
- The Wifi Access point need to be relatively close to increase the chance of connection success. The IRQ Can be changed in the BIOS Menu (Default is IRQ 3)<br />

## Tested machines :
- IBM 5150, 5160, 5170
- IBM PS/1, IBM PS/2 30 286, PS2 8535 (Warning : As its HDD use DMA, does not boot if emulated RAM is added)
- Compaq Portable 2 (286): Ok
- **New : Tandy 1000** : Now tested on a Tandy 1000 SX, EX and HX other to confirm. (October 2024 firmware)
- Tandy 1000 RLX Rev B Need a new BIOS to work.
- Amstrad PC1512, PC1640, Sinclair PC200 (It is my DEV Machine)
- Schneider Euro PC1 (Fast RAM Firmware), EuroPC2, Olivetti M21, Olivetti M24, Sega TeraDrive.
- Commodore PC1, PC10/PC20. (Need Fast RAM Firmware)
- Worked on Various 486, 386, 286 (Has more chance to work with lower ISA Clocks)
- Tested with some Pentium, Pentium MMX, Pentium 2, AMD K6 ...<br />
- Amiga 2000 with a A2286/A2386 SX Board. (Use the BIOS in C800)
- Book8088 (At 4.77MHz and 8MHz with the latest firmware), **The Power need to be connected at 8MHz**. EMS/PSRAM based RAM is failing.

## Failing Machines :
- The number of failing machines is decreasing, if it does not work on your machine, you can post an issue in this GitHub repository.
- Fail on some fast 286 (16MHz)
- IBM PS/2 M25
- Amstrad PPC512 /PPC640 : Problem on some PPC (Can't Boot) Not sure if it depends on the external ISA connectors boards used. <br />
  > Problem identified on some ISA expantion boards, 100uF Low ESR filtering capacitors may be needed.
- NuXT / EMM8088 homebrew computers seems to have problem. (NEW: EMM8088 working with the last GlaBIOS) <br />
- Amiga 2000 with a A2088.
- Tandy 1000 RXS Fail, but work if the BIOS is updated (See the Wiki)
- Pocket8086 : IO Port error
- Pocket386 : A BUG on this machine ISA BUS prevent the PicoMEM and any BIOS Based board to boot.

## Known bug :
- The PC may crash when using a PS/2 Mouse (Except with Windows 95)

## License

Software is Available on this GitHub as GNU 2. <br />
Hardware is currently closed. <br />

## Contributors / External Libraries

* [Ian Scott](https://github.com/polpo/): Idea to use the Pi Pico instead of the not available Pi 2/4 and Zero plus help on the Hardware design.
* [PSRAM Code by Ian Scott](https://github.com/polpo/rp2040-psram) : PSRAM PIO Code.
* [FatFS by Chan](http://elm-chan.org/fsw/ff/00index_e.html) : ExFS Library for microcontrollers.
* [FatFS SD by Carl J Kugler III](https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico) : FatFS for SD Access in SPI with a Pi Pico
* DOSBox (www.dosbox.com): DosBOX BIOS Int13h Code, heavily modified for ExFS support and PicoMEM Interface, with bug correction.
* ne2000 Emulation code, adapted by yyzkevin.
* [XInput library](https://github.com/Ryzee119/tusb_xinput.git) : XInput USB Joystick.
* [Mitsutaka Okazaki / Graham Sanderson emu8950 v1.1.0] (https://github.com/digital-sound-antiques/emu8950) : Adlib/OPL2 emulation
* Aaron Giles DREAMM / SCUMM Games emulator : CMS / Tandy emulation
