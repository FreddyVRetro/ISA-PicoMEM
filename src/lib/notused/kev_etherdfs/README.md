# etherdfs-server

Original project migrated from Sourceforge. MIT License.

_Copyright © 2017, 2018 Mateusz Vistelink_

### EtherDFS - The Ethernet DOS File System

EtherDFS is a client/server filesystem that allows a modern Linux host (the server) to easily share files with an old PC running DOS. The client is a __TSR for DOS__, also [available on Github](https://github.com/BrianHoldsworth/etherdfs-client). The basic functionality is to map a drive from the server to a local DOS drive letter, using raw Ethernet frames to communicate. It can be used in place of obsolete DOS-to-DOS file sharing applications like [Laplink](https://books.google.com/books?id=kggOZ4-YEKUC&pg=PA92#v=onepage&q&f=false).

### Server Build

The server code for EtherDFS contained in this repository has few dependencies and should be able to build in most x86 Linux environments with `gcc`. Build with the included Makefile by simply typing `make` in the cloned Github directory.

### Server Usage

The server needs to be run with root privileges in order to directly access the Ethernet interface. You simply specify the name of the interface and one or more directories that will be mapped to drive letters on the DOS/client side.
```
$ sudo ethersrv eth0 /mnt/c_drive /mnt/d_drive
```

The order of the directories correspond to the server's notion of what the drive letter default should be on the client side. However, the client is free to specify the actual drive letters that are used for each of these server directories. This mapping is specified when launching the client.

There are no restrictions on the filesystems on the Linux side for the shared directories, but __there will be problems using non-FAT filesystems__. For better compatibility on the client-side, use only directories that are mounted using FAT, such as your FAT-formatted USB drive. The directories shared should be mounts of type `msdos`, where possible, because there are known issues with `vfat` mounts.

Raw disk images of FAT partitions can also be used easily via the Linux `loop device`.
```
$ mount -t msdos -o loop,uid={your_uid},gid={your_gid} fat_disk.img /mnt/c_drive
```
Using a mount command like this, you can copy whatever disk contents you need into your disk image, then share that disk image to the DOS client with maximal compatibility. Many DOS applications will run fine and at reasonable speed from the shared/mapped drive.
