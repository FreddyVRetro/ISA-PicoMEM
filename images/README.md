# ISA PicoMEM Images Repository

### Introduction

Folders contining images and Tips to use the PicoMEM Disk emulation function

## How to prepare the MicroSD ?

The PicoMEM uses a MicroSD to store the Floppy and Disk image and the various Configuration files.<br />

The MicroSD can be formatted with any Filesystem Type: FAT16, FAT32 or ExtFS.<br />

The Floppy and HDD image files need to be in Binary/Raw format, with the extension .IMG<br />
The file name is limited to 13 caracter.

The Disk images need to be store under a “HDD” folder<br />
The FLoppy images are stores under a “FLOPPY” folder<br />

Any type/Size of uSD is normally supported, but High Speed uSD of more than 4Gb is recommended.

### Configuration files : (in the uSD rood directory)

**wifi.txt :** Place the Wifi SSID first, then the password<br />

**config.txt :** This file contains parameters that will override the PICOMEM.CFG (BIOS) settings<br />

**2 parameters are supported :**<br />
BIOS xxxx  > Configure the BIOS Base Address  example, "BIOS C800"<br />
PREBOOT    > When Present, this will force the BIOS to start when the Option ROM is executed.<br />
This parameter may not wsork in some machines, as the BIOS is not totally initialized at the begining.<br />

**The config.txt is supported from the may 28 firmware version.**
