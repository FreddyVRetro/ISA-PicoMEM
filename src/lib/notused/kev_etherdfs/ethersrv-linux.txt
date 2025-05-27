
            ethersrv-linux Copyright (C) 2017, 2018 Mateusz Viste
                       http://etherdfs.sourceforge.net


ethersrv-linux is an implementation of EtherSRV for Linux. It allows a Linux
computer to share its drive over Ethernet with multiple DOS clients running
EtherDFS.

Requirements:
ethersrv-linux requires a fairly recent Linux operating system to run, and gcc
to compile.

How to build:
just type 'make', then an 'ethersrv-linux' binary should appear (unless things
go wrong).

How to run:
ethersrv-linux [options] iface directory [directory2] ... [directoryN]

Note that, in order to be able to operate on ethernet sockets, ethersrv-linux
needs to have the CAP_NEW_RAW system capability. Usually this means that you
have to run it as root.

where:
 'iface'     is the name of the network interface you wish to hook ethersrv to
             (for example eth0)
 'directory' is the directory in your filesystem you wish to share (for
             example /mnt/dosfs). You can mention several directories (up to
             24), the first one will always be mapped to a virtual "C:" drive,
             the second to "D:", etc.

available options:
 -f          do not daemonize the process (stay in foreground)


Notes:
 * it is HIGHLY recommended to run ethersrv-linux over a FAT filesystem.
   Other file systems might work, too, but FAT attributes will be unavailable
   and DOS might get puzzled with upper/lower case filenames. The presence
   and/or order of '.' and '..' directory entries might also end up being
   unusual, possibly confusing applications that rely on the assumption that
   both these entries are first in non-root directories.
 * some Linux distributions treat FAT volumes in a case-sensitive way by
   default. Such behavior leads to troubles when used by DOS (through
   EtherDFS). I tested two Linux modules for FAT support: 'msdos' and 'vfat'.
   The 'msdos' module worked consistently for me at all times. The 'vfat'
   module, on the other hand, is known to interpret file names as UTF-8 on
   some distributions, which makes the file system case-sensitive. To avoid
   this, either use the 'msdos' module, or mount your vfat volume with an
   explicit iocharset. Examples:
     # mount /dev/sda2 /mnt/fat -t msdos
     # mount /dev/sda2 /mnt/fat -t vfat -o iocharset=cp437
   You can also create a FAT disk image and mount that. Example:
     # fallocate -l 1024M fat.img
     # mkfs.msdos fat.img
     # mount -o loop fat.img /mnt/fat


===[ License ]================================================================

ethersrv-linux is distributed under the terms of the MIT License, as listed
below.

Copyright (C) 2017, 2018 Mateusz Viste

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
