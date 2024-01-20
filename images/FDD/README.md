# ISA PicoMEM Floppy images How To

For an expanded version of this document, go to this Article :
https://adafruit-playground.com/u/AnneBarela/pages/ms-dos-disk-images

Images included in this repository:
 - sv86-720.img : Bootable Svarog86 FreeDOS Distribution : https://svarog86.sourceforge.net/ 
 

# Operating System Floppy Boot Images on the Internet

Site that has MS-DOS 3 to 6: https://www.allbootdisks.com/download/dos.html (tested ok)
Site that has DOS 1 to 7.1: https://winworldpc.com/product/ms-dos/1x (untested)
Svarog86 (FreeDOS) : https://svarog86.sourceforge.net/
SvarDOS (New relase of Swarog86): http://svardos.org/
Site that Malwarebytes flags as having riskware: bootdisk.com (BEWARE)

# Software Floppy Images on the Internet

Software: https://www.goodolddays.net/diskimages/ (untested)

# Using Floppy Images on PicoMEM

Use .img files for PicoMEM in the /FLOPPY directory
.ima and IBM .dsk images can usually be renamed to .img images. For more information, see https://www.dosdays.co.uk/topics/floppy_disk_images.php

WARNING: 
- The file name (excluding .img) must not be longer that 13 Character, otherwise, the file will not be listed.
- It is currently not possible to change a Floppy image after the Boot.

# Manipulating Floppy Disk Images in Modern Windows

Method 1: Use WinImage https://www.winimage.com/winimage.htm (GUI, shareware)

Method 2: ImDisk Toolkit Files https://sourceforge.net/projects/imdisk-toolkit/ is a WinXP through Win 10 GUI set of programs. One is to open disk images (floppy or hard drive) as a drive letter (or directory mount point) in Windows. Files can be copied back and forth freely.
Website - http://www.ltr-data.se/opencode.html/#ImDisk
GitHub - https://github.com/LTRData/ImDisk 

Method 3: Arsenal Image Mounter - https://arsenalrecon.com/products/arsenal-image-mounter
and GitHub https://github.com/ArsenalRecon/Arsenal-Image-Mounter

# Making Floppy Disk Images on a Modern Windows Machine (besides above)

To make empty formatted floppy and unformatted hard drive images, use NetDrive 
https://www.brutman.com/mTCP/mTCP_NetDrive.html (Windows command line)

netdrive create floppy 360 360.img
netdrive create floppy 1200 1200.img
netdrive create floppy 720 720.img
netdrive create floppy 1440 1440.img


Floppy image Guide Written with Anne Barela / Adafruit