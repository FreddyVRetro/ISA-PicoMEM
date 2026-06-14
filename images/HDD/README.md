# ISA PicoMEM HDD Images

The 2 images are DOS 3.31 and DOS 6.22 Bootable images with no application.<br />

**NEW :** D33_200M_EN Image with some utilities, MTCP, MicroWEB and PicoMEM Drivers

If you want to have more complex/pre generated images, wou can now use DOSForge:<br />
https://github.com/flynnsbit/dosforge


### What is a .IMG File ?

.IMG files are disk binary images, with all the Disk sectors (512bytes) stored one after the other.<br />
.IMG files does not contain the disk geometry: Sector, Head, Cylinders.<br />

**For Floppy**, the correct image geometry will be set, as it is based on the image size.<br />

For Hard Disk, The Geometry will be set as H:16, S:63, C:x<br />
Then, Image of disk with different Sector or Head count will not work !<br />
Images with the same number of Sector per Cylinder may be able to Boot but will not work.<br />

**New: On the PicoMEM 2 and future PicoMEM 1 firmware**, you can define the disk geometry.<br />
Just add a text file names "imagename".chs containing:<br />
Clusters,Heads,Sectors  in Decimal.<br />
<br />
Example for a 245MB Disk: 985,16,32<br />

14/6/2026 :

- Add a 2GB DOS 6.22 "blank" image (D62_2G_B)

D33_200M_EN:
 - Update PMINIT for PicoMEM 2 Support and PM2000
 - Add default BLASTER/GUS environment variable with default values (IRQ5, DMA1)
 - Added the sub folder MUSIC with the latest Mod Master XT and some music examples
 - Updated MicroWEB (https://github.com/jhhoward/MicroWeb/releases)
 - Updated mTCP (http://brutmanlabs.org/mTCP/)

16/11/25 : 

- Updated the PMINIT Drivers in the images

29/09/24 : 

- Updated the DOS 3.3 image with the latest drivers.