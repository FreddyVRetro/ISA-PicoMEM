# ISA PicoMEM HDD Images

The 2 images are DOS 3.31 and DOS 6.22 Bootable images with no application.

**NEW :** D33_200M_EN Image with some utilities, MTCP, MicroWEB and PicoMEM Drivers

### What is a .IMG File ?

.IMG files are disk binary images, with all the Disk sectors (512bytes) stored one after the other.<br />

.IMG files does not contain the disk geometry: Sector, Head, Cylinders.<br />

The PicoMEM will set the correct floppy geometry based on the files size.<br />
Hard Disk Geometry will be set as H:16, S:63, C:x Then, Image of disk with different Sector or Head count will not work !<br />
Images with the same number of Sector per Cylinder may be able to Boot but will not work.<br />
