# ISA PicoMEM Images Repository

### Introduction

Folders contining images and Tips to use the PicoMEM Disk emulation function

## How to prepare the MicroSD ?

The PicoMEM uses a MicroSD to store the Floppy and Disk image and the various Configuration files.<br />

The MicroSD can be formatted with any Filesystem Type: FAT16, FAT32 or ExtFS.<br />

The Floppy and HDD image files need to be in Binary/Raw format, with the extension `.IMG`<br />
HDD images can also be in `.VHD (Fixed size)`<br />
The file name is limited to 13 caracter.

The Disk images need to be store under a `HDD` folder<br />
The Floppy images are stores under a `FLOPPY` folder<br />
The CD-ROM images are stored under the `CDROM` folder (PicoMEM 2 only)<br />
The ROM binaries are stored in the `ROM` folder (PicoMEM 2 only)<br />

```
/
├── floppy/                     # Floppy images
│   ├── dos311.img
│   └── cpm.img
├── hdd/                        # Hard disk images
│   ├── mydisk.img
│   └── windows95.vhd
├── rom/                        # ROM binaries
│   ├── clatick.bin
│   └── xtide.bin
├── cdrom /                     # CD-ROM images
│   ├── game.iso
│   ├── game2.cue
│   ├── gmae2.bin
│   └── Windows/
│       └── Disc 1.iso
│       └── Disc 2.iso
├── config.txt                  # Editable configuration file
└── picomem.cfg                 # PicoMEM configuration file
```

Any type/Size of uSD is normally supported, but High Speed uSD of more than 4Gb is recommended.

### Configuration files : (in the uSD rood directory)

Description of the configuration file is in this [PicoMEM Wiki page](https://github.com/FreddyVRetro/ISA-PicoMEM/wiki/Configuration-files)
