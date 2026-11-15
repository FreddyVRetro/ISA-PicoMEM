/* Copyright (C) 2023 Freddy VETELE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. 
If not, see <https://www.gnu.org/licenses/>.
*/
/* pm_disk.cpp : PicoMEM Library containing the HDD and Floppy emulation functions
   Support SDCard "0:" and USB "1:"
 */ 

#include <stdio.h>
#include <stdlib.h>
// File system
#include "f_util.h"
#include "hw_config.h"  /* The SDCARD and SPI definitions  in \FatFs_SPI_PIO\sd_driver\ */
#include "ff.h"			    /* Obtains integer types */
#include "diskio.h"		  /* Declarations of disk functions */

//DOSBOX Code
#include "../dosbox/bios_disk.h"

#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "../pm_libs/pm_disk.h"   // for PC_DB_Start

#include "pmdev.h"        // PicoMEM ressources (PSDAM, SD, USB) code
#include "dev_memory.h"
#include "dev_post.h"

#if USE_CDROM
// From isa_devices/cdrom/cdrom_image_manager.c -- real MB size for a CD image
// path (.cue sums the referenced .bin; .iso stats the file). Forward-declared
// here to avoid pulling the whole cdrom.h into the BIOS disk lister.
extern "C" uint32_t cd_image_size_mb(const char *fullpath);
#endif

#if USE_USBHOST
// Forward-declare usb_msc_volume_ready() instead of including the full
// usb.h, which transitively needs the tinyusb headers (CFG_TUH_DEVICE_MAX)
// that this translation unit otherwise has no reason to pull in. The
// definition lives in src/USB_Host_driver/msc/fatfs_usb.c.
extern "C" bool usb_msc_volume_ready(void);
#endif

extern volatile uint8_t * PMBIOS;  //

// Dual-volume floppy/HDD navigation state.
//   g_current_vol == 0xFF -> menu is at the virtual root, showing
//                            [SD] and (if mounted) [USB] entries.
//   g_current_vol == 0    -> inside the SD volume  (FatFs "0:").
//   g_current_vol == 1    -> inside the USB volume (FatFs "1:").
// Updated by pm_disk_InitSearchPath / pm_disk_UpdateSearchPath as the
// user descends [SD]/[USB] and walks "..". The lister and SelectImage
// read it to pick the right FatFs prefix and to record bit 6 of
// FDD_Attribute / HDD_Attribute (see pm_defines.h).
static uint8_t g_current_vol = 0xFF;

// Per-volume image root prefixes. The legacy fdd_dir/hdd_dir constants
// (kept below for callers that don't use the virtual-root flow) pick
// one or the other at compile time via FDD_USB / HDD_USB. The
// fdd_dir_for / hdd_dir_for helpers select at runtime based on the
// volume index, which is what the dual-volume lister needs.
static inline const char *fdd_dir_for(uint8_t vol) {
    return (vol == 1) ? "1:/FLOPPY/" : "0:/FLOPPY/";
}
static inline const char *hdd_dir_for(uint8_t vol) {
    return (vol == 1) ? "1:/HDD/"    : "0:/HDD/";
}

#define MAX_DISKIMAGES  128
#define MAX_DISKNAMELEN 14 // 14+2 > 16

#define SZ_TBL 32 // For FatFS FastSeek

// Disk Files (2 Floppy, 4 HDD)
// > To Do :Can be changed to Pointers
FIL FDDfile[2];
FIL HDDfile[4];
/*
FIL *FDDfile[2];
FIL *HDDfile[4];
*/

const char device_path[2][4]={"0:/", "1:/"};

// Images home directory
const char hdd_dir[]="0:/HDD/";
const char fdd_dir[]="0:/FLOPPY/";
const char rom_dir[]="0:/ROM/";

const char cfgn[]="0:/PICOMEM.CFG";
const char cfg2n[]="0:/config.txt";

char image_path[64];  // Search path for Image disk load (FDD/HDD)

// CD-ROM folder-browse state. CD images live under 0:/CDROM on the SD
// card only (no [SD]/[USB] virtual root), so CD uses its own sub-path
// rather than the dual-volume image_path/g_current_vol above. cd_image_path
// is the sub-path under 0:/CDROM currently being browsed (empty = the CDROM
// root; otherwise a "sub/dir/" fragment with a trailing '/'), mirroring the
// FDDPath/HDDPath convention. Persisted to PM_Config->CDRPath on select.
static const char cdrom_dir[] = "0:/CDROM/";
char cd_image_path[64];

//unsigned char mbr[512] = {
const uint8_t __in_flash() newmbr[512] = {
	0xFA, 0x33, 0xC0, 0x8E, 0xD0, 0xBC, 0x00, 0x7C, 0x8B, 0xF4, 0x50, 0x07,
	0x50, 0x1F, 0xFB, 0xFC, 0xBF, 0x00, 0x06, 0xB9, 0x00, 0x01, 0xF2, 0xA5,
	0xEA, 0x1D, 0x06, 0x00, 0x00, 0xBE, 0xBE, 0x07, 0xB3, 0x04, 0x80, 0x3C,
	0x80, 0x74, 0x0E, 0x80, 0x3C, 0x00, 0x75, 0x1C, 0x83, 0xC6, 0x10, 0xFE,
	0xCB, 0x75, 0xEF, 0xCD, 0x18, 0x8B, 0x14, 0x8B, 0x4C, 0x02, 0x8B, 0xEE,
	0x83, 0xC6, 0x10, 0xFE, 0xCB, 0x74, 0x1A, 0x80, 0x3C, 0x00, 0x74, 0xF4,
	0xBE, 0x8B, 0x06, 0xAC, 0x3C, 0x00, 0x74, 0x0B, 0x56, 0xBB, 0x07, 0x00,
	0xB4, 0x0E, 0xCD, 0x10, 0x5E, 0xEB, 0xF0, 0xEB, 0xFE, 0xBF, 0x05, 0x00,
	0xBB, 0x00, 0x7C, 0xB8, 0x01, 0x02, 0x57, 0xCD, 0x13, 0x5F, 0x73, 0x0C,
	0x33, 0xC0, 0xCD, 0x13, 0x4F, 0x75, 0xED, 0xBE, 0xA3, 0x06, 0xEB, 0xD3,
	0xBE, 0xC2, 0x06, 0xBF, 0xFE, 0x7D, 0x81, 0x3D, 0x55, 0xAA, 0x75, 0xC7,
	0x8B, 0xF5, 0xEA, 0x00, 0x7C, 0x00, 0x00, 0x49, 0x6E, 0x76, 0x61, 0x6C,
	0x69, 0x64, 0x20, 0x70, 0x61, 0x72, 0x74, 0x69, 0x74, 0x69, 0x6F, 0x6E,
	0x20, 0x74, 0x61, 0x62, 0x6C, 0x65, 0x00, 0x45, 0x72, 0x72, 0x6F, 0x72,
	0x20, 0x6C, 0x6F, 0x61, 0x64, 0x69, 0x6E, 0x67, 0x20, 0x6F, 0x70, 0x65,
	0x72, 0x61, 0x74, 0x69, 0x6E, 0x67, 0x20, 0x73, 0x79, 0x73, 0x74, 0x65,
	0x6D, 0x00, 0x4D, 0x69, 0x73, 0x73, 0x69, 0x6E, 0x67, 0x20, 0x6F, 0x70,
	0x65, 0x72, 0x61, 0x74, 0x69, 0x6E, 0x67, 0x20, 0x73, 0x79, 0x73, 0x74,
	0x65, 0x6D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xAA
};

const uint8_t __in_flash() NullSect[512] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Convert file operation error to PicoMEM error code
uint8_t pm_convert_fopen_error(uint8_t fr)
{
if (FR_OK != fr) 
   { 
    switch (fr)
     {
      case FR_NO_FILE:
             PM_WARNING("Error: File does not exist\n");
             return CMDERR_FILEREAD;
      case FR_DISK_ERR:
             PM_WARNING("Error: Physical disk failure\n");
             return CMDERR_DISK;
      case FR_NO_PATH:                
             PM_WARNING("Error: Folder does not exist\n");
             return CMDERR_NOHDDDIR;                      
      default:
             PM_WARNING("f_open failed %d\n",fr);
             return CMDERR_DISK;
     }
   }
return 0;       
}

static void pm_load_rom_file(uint32_t addr, const char *romname)
{
    char rompath[80];
    FIL romfile;
    FILINFO rominfo;
    UINT br;
    uint8_t *rombuffer = NULL;
    uint32_t romsize = 0;
    uint8_t *PM_ROM_Buffer = NULL;
    uint32_t PM_ROM_Buffer_Size = 0;    

    snprintf(rompath, sizeof(rompath), "%s%s", rom_dir, romname);
    PM_INFO("Loading ROM file: %s\n", rompath);

    if (f_stat(rompath, &rominfo) == FR_OK) {
        romsize = (uint32_t)rominfo.fsize;
        PM_INFO("ROM size: %u\n", romsize);
        if (PM_ROM_Buffer != NULL) {
            free(PM_ROM_Buffer);
            PM_ROM_Buffer = NULL;
            PM_ROM_Buffer_Size = 0;
        }
        rombuffer = (uint8_t *)malloc(romsize);
        if (rombuffer != NULL) {
            if (f_open(&romfile, rompath, FA_OPEN_EXISTING | FA_READ) == FR_OK) {
                if (f_read(&romfile, rombuffer, romsize, &br) == FR_OK && br == romsize) {
                    PM_ROM_Buffer = rombuffer;
                    PM_ROM_Buffer_Size = romsize;
                    dev_rom_add(addr, PM_ROM_Buffer_Size/1024, PM_ROM_Buffer);
                } else {
                    PM_WARNING("ROM read failed (%u/%u)\n", br, romsize);
                    free(rombuffer);
                }
                f_close(&romfile);
            } else {
                PM_WARNING("ROM open failed: %s\n", rompath);
                free(rombuffer);
            }
        } else {
            PM_WARNING("ROM RAM alloc failed\n");
        }
    } else {
        PM_WARNING("ROM file not found: %s\n", rompath);
    }
}

// * Mount floppy for Int13h
// * All the Files must be closed before ( FDDfile[] )
uint8_t pm_mountfddimage(char *diskfilename, uint8_t fddnumber)
{
  int fr;
  FILINFO finfo;
  Bit32u filesize;
  FIL chs;
  PM_INFO("pm_mountfddimage : %s FDD%d\n",diskfilename,fddnumber);
  fr = f_stat (diskfilename, &finfo);					  /* Get file status to read file size */ 
  fr = pm_convert_fopen_error((uint8_t) fr);    // Convert fr to PicoMEM Error code
  if (fr!=0) return fr;

  PM_INFO("finfo: %s [size=%llu] \n", finfo.fname, finfo.fsize);

  // Reflect the real image size in the config so the BIOS "FDD size" line
  // (PMCFG_FDD0Size) matches the mounted image. Both the boot auto-mount and
  // the screenpad mount come through here; previously only the menu "select
  // image" path set FDD[].size, so auto-/screenpad-mounted images showed 0 kB
  // for a real (e.g. 1440 kB) floppy. uint16 KB is fine for floppies.
  if (fddnumber < 2)
      PM_Config->FDD[fddnumber].size = (uint16_t)(finfo.fsize / 1024);

 // Open the FDD image file
	fr = f_open(&FDDfile[fddnumber], diskfilename, FA_OPEN_EXISTING | FA_READ | FA_WRITE); /* ! If the file is already opened, does not work */
  fr = pm_convert_fopen_error((uint8_t) fr); // Convert fr to PicoMEM Error code
  if (fr!=0) return fr;

 // Mount FDD image  
     imageDiskList[fddnumber] = new imageDisk(&FDDfile[fddnumber], (Bit8u *)diskfilename, (finfo.fsize/1024), false);
	    if(!imageDiskList[fddnumber]) {     
         PM_WARNING("Floppy Mount fail\n");   
		     return 0x03;
	       }
  // Don't need to set the Geometry for Floppy (Done in the imageDisk init)

  return 0;
	}

 // Mount the FDD from the config info
 // input : FDD number (0 or 1)
 void pm_disk_mount_fdd(uint8_t fdd_nb)
 {
   char imgpath[64+30];

   f_close(&FDDfile[fdd_nb]); // Close the Floppy file

   PM_Config->FDD_Attribute[fdd_nb]=fdd_nb;
   if (imageDiskList[fdd_nb]!=NULL) {
        delete imageDiskList[fdd_nb];
        imageDiskList[fdd_nb]=NULL;
       }
   if ((PM_Config->FDD[fdd_nb].size>>8)!=0xFE)
           {
            if (strcmp(PM_Config->FDD[fdd_nb].name,""))
               {
                sprintf(imgpath, "0:/FLOPPY/%s%s.img",PM_Config->FDDPath[fdd_nb], PM_Config->FDD[fdd_nb].name);
                 
                PM_INFO("FDD%d: %s\n",fdd_nb,imgpath);
               
                int8_t mountres=pm_mountfddimage(imgpath,fdd_nb);       // Mount the floppy image
                if (!mountres) PM_Config->FDD_Attribute[fdd_nb]=0x80;   // res=0 > PicoMEM Disk
               } else PM_INFO("FDD%d: None\n",fdd_nb);
           } else  // Not an image, but a Physical disk redirection
           {
             PM_Config->FDD_Attribute[fdd_nb]=((uint8_t) PM_Config->FDD[fdd_nb].size)&0x03;
             PM_INFO("PC Floppy nb: %x %x",(uint8_t) PM_Config->FDD[fdd_nb].size,PM_Config->FDD_Attribute[fdd_nb]);
           }
 }

void pm_disk_update_fdd_nb()
{
 if (PC_FloppyNB<2)   // To not reduce the Nb of floppy if >=2
     {
       New_FloppyNB=0;
       for (int i=0;i<2;i++)
           {
            if (PM_Config->FDD_Attribute[i]>=0x80) New_FloppyNB++;        // Image Mounted
            if (PM_Config->FDD_Attribute[i]<PC_FloppyNB) New_FloppyNB++;  // Read existing Floppy
          // PM_INFO(" - %s Attr:%x\n",PM_Config->FDD[i].name,PM_Config->FDD_Attribute[i]);
           }
         } else New_FloppyNB=PC_FloppyNB;
 PM_INFO("Total FDD : %d / %d\n",New_FloppyNB,PC_FloppyNB);  
}

// * Mount disks for Int13h
// * All the Files must be closed before ( HDDfile[] )
uint8_t pm_mounthddimage(char *diskfilename, uint8_t hddnumber)
{

  int fr;
  FILINFO finfo;
  Bit32u filesize;
	partTable mbrData;
  VHDFooter vhdData;
  
  FIL chs;
  bool vhd=false;
  char buf[16];

  PM_INFO("[DSK] pm_mountdiskimage : %s Disk: %d\n",diskfilename,hddnumber);
  fr = f_stat (diskfilename, &finfo);			 /* Get file status to read file size */
  fr=pm_convert_fopen_error((uint8_t) fr); // Convert fr to PicoMEM Error code
  if (fr!=0) // .img not found, try .vhd
     {
       diskfilename[strlen(diskfilename)-3]=0;  // remove the extention
       strcat(diskfilename,"vhd");
       PM_INFO("Try to mount VHD file : %s\n",diskfilename);
       fr = f_stat (diskfilename, &finfo);			/* Get file status to read file size */
       fr=pm_convert_fopen_error((uint8_t) fr); // Convert fr to PicoMEM Error code
       if (fr!=0) return CMDERR_MOUNTFAIL;
       vhd=true;
     };

// Open the HDD image file 
  PM_INFO("[DSK] finfo: %s [size=%llu] \n", finfo.fname, finfo.fsize);
	fr = f_open(&HDDfile[hddnumber-1], diskfilename, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
  fr=pm_convert_fopen_error((uint8_t) fr); // Convert fr to PicoMEM Error code
  if (fr!=0) return CMDERR_MOUNTFAIL;

  if (vhd)
    { // Read the VHD file footer, then check if fixed type
      PM_INFO("Reading VHD Footer %d\n",sizeof(VHDFooter));
      f_lseek(&HDDfile[hddnumber-1], finfo.fsize - 512);
      f_read(&HDDfile[hddnumber-1], &vhdData, sizeof(VHDFooter), NULL);
      // Debug: print VHD footer cookie and disk type
      PM_INFO("VHD Footer cookie: %.8s diskType: %u\n", vhdData.cookie, (unsigned)vhdData.diskType);
      if (memcmp(vhdData.cookie, "conectix", 8) == 0 && vhdData.diskType == VHD_TYPE_FIXED)
        {
          PM_INFO("VHD is fixed type, mounting\n");
          // cylinders is stored in big-endian format, so we need to swap the bytes to get the correct value
          vhdData.geometry.cylinders = (uint16_t)((vhdData.geometry.cylinders << 8) | (vhdData.geometry.cylinders >> 8));
          vhd=true;
        }
      else
        {
          PM_INFO("VHD is not fixed type, cannot mount\n");
          f_close(&HDDfile[hddnumber-1]);
          return CMDERR_MOUNTFAIL;
        }
    }

// Mount HDD Image
  imageDiskList[hddnumber+1] = new imageDisk(&HDDfile[hddnumber-1], (Bit8u *)diskfilename, (finfo.fsize/1024), true);
	if (!imageDiskList[hddnumber+1]) 
         {     
         PM_INFO("[DSK][HDD] HDD Mount fail\n");     
		     return CMDERR_MOUNTFAIL;
         f_close(&HDDfile[hddnumber-1]);
	       }

// Create the Table for image fast Seek

  HDDfile[hddnumber-1].cltbl=imageDiskList[hddnumber+1]->clmt;
  imageDiskList[hddnumber+1]->clmt[0]=SZ_TBL;
  if (f_lseek(&HDDfile[hddnumber-1], CREATE_LINKMAP) == FR_NOT_ENOUGH_CORE) {
            // Size of clmt is too small for the file - it is too fragmented
            PM_INFO("File too fragmented for the cluster link map table. Falling back to slow seek\n");
       }

// Now set the disk Geometry
  if (vhd) { 
    // Get Geometry from vhd footer   
    imageDiskList[hddnumber+1]->Set_Geometry(vhdData.geometry.heads, vhdData.geometry.cylinders, vhdData.geometry.sectors, 512);
  } 
   else
  {
   // try to open the .chs file to get the geometry of the disk
   // if not available from the chs file, the geometry is computed with a default value
   bool chs_fromfile=false;
   diskfilename[strlen(diskfilename)-3]=0;  // remove the extention
   strcat(diskfilename,"chs");
	 fr = f_open(&chs, diskfilename, FA_OPEN_EXISTING | FA_READ);
   PM_INFO("CHS file %s :",diskfilename);  

   if (FR_OK == fr)
     {
      uint16_t c,h,s;
      f_gets(buf, 16, &chs);
      if (sscanf(buf,"%d,%d,%d",&c,&h,&s)==3)
         {
           PM_INFO("[DSK][HDD] c%d h%d s%d\n",c,h,s);
          if ((c>0)&&(c<64)&&(s>0)&&(s<=512)&&(h>0)&&(h<17))
          {
           chs_fromfile=true;
           imageDiskList[hddnumber+1]->Set_Geometry(h,c,s, 512); // H C S
          }
         }
     } else PM_INFO("[DSK][HDD] No CHS file found\n");

   // Compute the CHS value with S=63 if not available from a chs file
   if (!chs_fromfile) imageDiskList[hddnumber+1]->Set_Geometry(16, (uint32_t)(finfo.fsize / (512 * 63 * 16)), 63, 512); // H C S  
  }

// !! DOSBOX X Also get the geometry infos from the Boot Sector
    imageDiskList[hddnumber+1]->Read_Sector(0,0,1,&mbrData);

    for (int i=0;i<16;i++)
    {
      PM_INFO("%02x ",((uint8_t*)&mbrData)[i]);
    }
     PM_INFO("\n");


    for (int i=0;i<16;i++)
    {
      PM_INFO("%02x/%02x ",((uint8_t*)&mbrData)[i+446], mbrData.booter[i]);
    }
     PM_INFO("\n");     
    for (int i=0;i<16;i++)
    {
      PM_INFO("%02x ",((uint8_t*)&mbrData)[i+462]);
    }
     PM_INFO("\n"); 
    for (int i=0;i<16;i++)
    {
      PM_INFO("%02x ",((uint8_t*)&mbrData)[i+478]);
    }
     PM_INFO("\n");  

    PM_INFO("[DSK][HDD] MBR Signature: %02x %02x / %02x %02x\n", ((uint8_t*)&mbrData)[510], ((uint8_t*)&mbrData)[511], mbrData.magic1, mbrData.magic2);

#if PM_PRINTF        
		if (mbrData.magic1!= 0x55 ||	mbrData.magic2!= 0xaa) PM_INFO("Possibly invalid partition table in disk image.\n");
       else PM_INFO("Valid MBR Signature found in disk image.\n");
#endif

    PM_INFO("List Partitions :\n");
		for (int i=0;i<4;i++)
    {
      PM_INFO("[DSK][HDD] MBR #%u: bootflag=%u parttype=0x%02x beginchs=0x%02x%02x%02x endchs=0x%02x%02x%02x start=%llu size=%llu\n",
			(unsigned int)i,mbrData.pentry[i].bootflag&0x80,mbrData.pentry[i].parttype,
			mbrData.pentry[i].beginchs[0],mbrData.pentry[i].beginchs[1],mbrData.pentry[i].beginchs[2],
			mbrData.pentry[i].endchs[0],mbrData.pentry[i].endchs[1],mbrData.pentry[i].endchs[2],
			(unsigned long long)mbrData.pentry[i].absSectStart,
			(unsigned long long)mbrData.pentry[i].partSize);
    }

			/* Pick the first available partition */
//    for(m=0;m<4;m++) {
//	      if(mbrData.pentry[m].partSize != 0x00) {
		//		PM_INFO("Using partition %d on drive; skipping %d sectors", m, mbrData.pentry[m].absSectStart);
		//		startSector = mbrData.pentry[m].absSectStart;
		//		break;
		//	}

    return 0;
	}

#define FAT_DIR 16

void pm_mount_hdd(uint8_t hddnumber)
{
  char imgpath[64+30];

  f_close(&HDDfile[hddnumber]);  // Close the previous HDD file
  PM_Config->HDD_Attribute[hddnumber]=hddnumber;
  if (imageDiskList[hddnumber+2]!=NULL) { 
            delete imageDiskList[hddnumber+2];   // Delete the previous HDD Object
            imageDiskList[hddnumber+2]=NULL;
           }
  //Mount the disk image
  if ((PM_Config->HDD[hddnumber].size>>8)!=0xFE)
         {        
           if (strcmp(PM_Config->HDD[hddnumber].name,""))  // Skip if disk name is empty
               { // Create the disk image Path string
                sprintf(imgpath, "0:/HDD/%s%s", PM_Config->HDDPath[hddnumber], PM_Config->HDD[hddnumber].name);
                int8_t mountres=pm_mounthddimage(imgpath,hddnumber+1);    // Mount the disk image
                if (!mountres) PM_Config->HDD_Attribute[hddnumber]=0x80;  // res=0 > PicoMEM Disk
               }
          } else  // Not an image, but a Physical disk redirection
          {
             PM_Config->HDD_Attribute[hddnumber]=((uint8_t) PM_Config->HDD[hddnumber].size)&0x03;
             PM_INFO("PC Disk nb: %x %x\n",(uint8_t) PM_Config->HDD[hddnumber].size,PM_Config->HDD_Attribute[hddnumber]);
          }
}

/*
void imageDisk::Set_GeometryForHardDisk()
{
	sector_size = 512;
	partTable mbrData;
	for (int m = (Read_AbsoluteSector(0, &mbrData) ? 0 : 4); m--;)
	{
		if(!mbrData.pentry[m].partSize) continue;
		bootstrap bootbuffer;
		if (Read_AbsoluteSector(mbrData.pentry[m].absSectStart, &bootbuffer)) continue;
		bootbuffer.sectorspertrack = var_read(&bootbuffer.sectorspertrack);
		bootbuffer.headcount = var_read(&bootbuffer.headcount);
		uint32_t setSect = bootbuffer.sectorspertrack;
		uint32_t setHeads = bootbuffer.headcount;
		uint32_t setCyl = (mbrData.pentry[m].absSectStart + mbrData.pentry[m].partSize) / (setSect * setHeads);
		Set_Geometry(setHeads, setCyl, setSect, 512);
		return;
	}
	if (!diskimg) return;
	uint32_t diskimgsize;
	fseek(diskimg,0,SEEK_END);
	diskimgsize = (uint32_t)ftell(diskimg);
	fseek(diskimg,current_fpos,SEEK_SET);
	Set_Geometry(16, diskimgsize / (512 * 63 * 16), 63, 512);
} */

void pm_disk_InitSearchPath()
{
image_path[0]=0;
#if BOARD_WM10
g_current_vol = 0xFF;  // start at virtual root [SD]/[USB] selector
#else
g_current_vol = 0;     // default to SD
#endif
}

void pm_disk_UpdateSearchPath(uint8_t id,FSizeName_t *DiskListPrt)
{
  FSizeName_t *DSKLIST=DiskListPrt;  // Pointer to the 128 file size/name  

  PM_INFO("Change DIR: %s ",DSKLIST[id].name);

  // At the virtual root the only meaningful picks are the [SD] / [USB]
  // entries the lister synthesised. Decide volume, clear image_path,
  // and bail out -- we are not inside a real FatFs directory yet.
  if (g_current_vol == 0xFF) {
#if BOARD_WM10
      if (!strcmp(DSKLIST[id].name, "[SD]"))  { g_current_vol = 0; image_path[0] = 0; PM_INFO("Enter [SD]\n");  return; }
      if (!strcmp(DSKLIST[id].name, "[USB]")) { g_current_vol = 1; image_path[0] = 0; PM_INFO("Enter [USB]\n"); return; }
#else
      if (!strcmp(DSKLIST[id].name, "SD"))  { g_current_vol = 0; image_path[0] = 0; PM_INFO("Enter [SD]\n");  return; }
      if (!strcmp(DSKLIST[id].name, "USB")) { g_current_vol = 1; image_path[0] = 0; PM_INFO("Enter [USB]\n"); return; }
#endif      
      // Any other pick at the virtual root is a no-op (the lister
      // shouldn't have produced one, but be defensive).
      PM_INFO("Ignored: pick %s at virtual root\n", DSKLIST[id].name);
      return;
  }

  if (!strcmp(DSKLIST[id].name,"Select SD/USB")) {
      if (image_path[0]==0)
        {    
         PM_INFO("Leave volume %d -> virtual root\n", g_current_vol);
         g_current_vol = 0xFF;
         return;
        } else PM_INFO("Ignored: pick %s inside volume %d\n", DSKLIST[id].name, g_current_vol);
  }

  if (!strcmp(DSKLIST[id].name,".."))
     {
      // ".." inside a volume root walks back out to the virtual root
      // rather than dead-ending in image_path[0]==0.
      if (image_path[0]==0)
        {
         PM_INFO("Leave volume %d -> virtual root\n", g_current_vol);
         g_current_vol = 0xFF;
         return;
        }
      image_path[strlen(image_path)-1]=0;        // Remove the "/" at the end
      char *lastslash=strrchr(image_path,'/');
      PM_INFO("Remove last %d/: %s\n",(lastslash-image_path),image_path);
      if (lastslash!=NULL) *(lastslash+1)=0;     // Remove the last folder in the path
         else image_path[0]=0;                   // No more slash > Volume home directory
     }
     else
     {
      if (strlen(DSKLIST[id].name)+strlen(image_path)<64) {
          strcat(image_path,DSKLIST[id].name);
          strcat(image_path,+"/");
         }
     }
  PM_INFO("New Path (vol %d): %s\n", g_current_vol, image_path);
}

static int pm_disk_image_compare(const FSizeName_t *a, const FSizeName_t *b)
{
  bool aDir = (a->size == 0xFFFE);
  bool bDir = (b->size == 0xFFFE);
  if (aDir != bDir)
    return aDir ? -1 : 1;

  const unsigned char *aName = (const unsigned char *)a->name;
  const unsigned char *bName = (const unsigned char *)b->name;
  while (*aName && *bName) {
    unsigned char aChar = (*aName >= 'A' && *aName <= 'Z') ? *aName + ('a' - 'A') : *aName;
    unsigned char bChar = (*bName >= 'A' && *bName <= 'Z') ? *bName + ('a' - 'A') : *bName;
    if (aChar != bChar)
      return aChar < bChar ? -1 : 1;
    aName++;
    bName++;
  }
  return *aName ? 1 : (*bName ? -1 : 0);
}

static void pm_disk_sort_images(FSizeName_t *list, uint8_t count)
{
  // Keep the volume selector at the top instead of sorting it among folders.
  int first = count && list[0].size == 0xFFFE && !strcmp(list[0].name, "Select SD/USB") ? 1 : 0;
  for (int i = first + 1; i < count; i++) {
    FSizeName_t temp = list[i];
    int j = i;
    while (j > first && pm_disk_image_compare(&list[j-1], &temp) > 0) {
      list[j] = list[j-1];
      j--;
    }
    list[j] = temp;
  }
}

// CD-ROM browse: reset to the 0:/CDROM root.
void pm_cd_InitSearchPath()
{
  cd_image_path[0]=0;
}

// CD-ROM browse: descend into the picked sub-folder or pop ".." one level.
// Unlike the FDD/HDD path there is no [SD]/[USB] virtual root -- CD is
// SD-only, so ".." at the CDROM root is simply a no-op.
void pm_cd_UpdateSearchPath(uint8_t id)
{
  FSizeName_t (*DSKLIST)[MAX_DISKIMAGES]=(FSizeName_t (*)[128])&PCCR_Param;  // Pointer to the 128 file size/name
  PM_INFO("[CDR] Change DIR: %s\n",(*DSKLIST)[id].name);

  if (!strcmp((*DSKLIST)[id].name,".."))
    {
     if (cd_image_path[0]==0) { PM_INFO("[CDR] Already at CDROM root\n"); return; }
     cd_image_path[strlen(cd_image_path)-1]=0;    // Remove the trailing "/"
     char *lastslash=strrchr(cd_image_path,'/');
     if (lastslash!=NULL) *(lastslash+1)=0;       // Remove the last folder in the path
        else cd_image_path[0]=0;                  // No more slash > CDROM root
    }
    else
    {
     if (strlen((*DSKLIST)[id].name)+strlen(cd_image_path)<63) {
         strcat(cd_image_path,(*DSKLIST)[id].name);
         strcat(cd_image_path,"/");
        }
    }
  PM_INFO("[CDR] New Path: %s\n", cd_image_path);
}

// * SD is already enabled
// with_dotdot: prepend a ".." navigation entry (folder sentinel 0xFFFE)
// before the sub-folders scanned from currpath.
static uint8_t AddFoldersToList_impl(char *currpath, FSizeName_t *DiskListPrt, bool with_dotdot)
{
  uint8_t foldercnt=0;
  DIR dp;
  FILINFO fno;
  FRESULT f_res;
  FSizeName_t *DIRLIST=DiskListPrt;  // Pointer to the 128 file size/name

  if (with_dotdot)
    {
     if (image_path[0]==0) {
        PM_INFO("AddFoldersToList: at volume root, adding '..' to list\n");
        strcpy(DIRLIST[0].name,"Select SD/USB");
      }
      else 
      {
        PM_INFO("AddFoldersToList: inside volume, adding '..' to list\n");
        strcpy(DIRLIST[0].name,"..");
      }
     DIRLIST[0].size = 0xFFFE;  // Mark as folder so BIOS treats it as navigation
     foldercnt++;
    }

  PM_INFO("AddFoldersToList\n");
  f_res = f_opendir(&dp,currpath);
  if(f_res != FR_OK) {
    // Path doesn't exist (e.g. user entered [USB] but the stick has no
    // /FLOPPY or /HDD folder). Don't drop the ".." entry we may have
    // already added -- without it the BIOS sees an empty list and
    // kicks the user out of the menu with no way back to the virtual
    // [SD]/[USB] selector. Return whatever count we built so far.
    f_closedir(&dp);
    return foldercnt;
  }

  for (;;) {
    f_res = f_readdir(&dp,&fno);    
    if(f_res != FR_OK || fno.fname[0] == 0) break;
    if(fno.fattrib & AM_DIR) 
      { // copy directories names
       if (strlen(fno.fname)>13) continue;     
       PM_INFO("%d > %s\n",foldercnt,fno.fname);
       strcpy(DIRLIST[foldercnt].name,fno.fname);
       DIRLIST[foldercnt].size=0xFFFE;  // Folder name
       if (foldercnt++==MAX_DISKIMAGES) break;
      } else continue;    // Skip files
  }
  f_closedir(&dp);

return foldercnt;
}

// FDD/HDD folder lister. ".." is emitted for any in-volume directory:
// either we're deeper than the volume root (image_path populated) OR we
// are at a volume root and need a way back out to the virtual [SD]/[USB]
// selector. The only place ".." is NOT useful is the virtual root itself,
// but this function is never reached there (the lister short-circuits at
// g_current_vol==0xFF and returns early).
uint8_t AddFoldersToList(char *currpath,FSizeName_t *DiskListPrt)
{
  return AddFoldersToList_impl(currpath, DiskListPrt, image_path[0]!=0 || g_current_vol != 0xFF);
}

// Copy the selected image to the config for the selected device (FDD0..1, HDD0..3)
// Persists the source volume in FDD_Attribute / HDD_Attribute bit 6
// (the "USB or SD" bit reserved in pm_defines.h). The mount path
// construction in pm_cmd.cpp reads bit 6 to pick "0:/" vs "1:/".
uint8_t SelectImage(uint8_t device,uint8_t imageid, FSizeName_t *DiskListPrt)
  {
    FSizeName_t *DSKLIST=DiskListPrt;  // Pointer to the 128 file size/name

    imageid--;  // Remove the "None" from the list
    PM_INFO("SelectImage: device %d imageid %d (%s) vol %d\n",device,imageid,DSKLIST[imageid].name,g_current_vol);
    // Bit 6 of FDD_Attribute / HDD_Attribute encodes the source volume
    // (0 = SD, 1 = USB). If the user is somehow at the virtual root
    // when this fires (shouldn't happen -- the lister returns only
    // [SD]/[USB] there), default to SD.
    uint8_t vol_bit = (g_current_vol == 1) ? 0x40 : 0x00;
    switch(device) {
      case 0: // FDD0
      case 1: // FDD1
              if (imageid==0xFF)
                 { // None > Remove the image
                   PM_Config->FDD[device].size=0;
                   PM_Config->FDD[device].name[0]=0;
                   PM_Config->FDDPath[device][0]=0;
                   PM_Config->FDD_Attribute[device] = device;   // clear vol bit when no image
                   PM_INFO("[DSK][FDD] FDD%d: None\n",device);
                   break;
                 }
              PM_Config->FDD[device].size=DSKLIST[imageid].size;
              strcpy(PM_Config->FDD[device].name,DSKLIST[imageid].name);
              strcpy(PM_Config->FDDPath[device],image_path);
              PM_Config->FDD_Attribute[device] = device | vol_bit;
              PM_INFO("[DSK][FDD] FDD%d: %s%s [vol %d]\n",device, PM_Config->FDDPath[device], PM_Config->FDD[device].name, g_current_vol);
              break;
      case 2 ... 5:  // HDD0..3
              if (imageid==0xFF)
                 { // None > Remove the image
                   PM_Config->HDD[device-2].size=0;
                   PM_Config->HDD[device-2].name[0]=0;
                   PM_Config->HDDPath[device-2][0]=0;
                   PM_Config->HDD_Attribute[device-2] = device-2;
                   PM_INFO("[DSK][HDD] HDD%d: None\n",device-2);
                   break;
                 }
              PM_Config->HDD[device-2].size=DSKLIST[imageid].size;
              strcpy(PM_Config->HDD[device-2].name,DSKLIST[imageid].name);
              strcpy(PM_Config->HDDPath[device-2],image_path);
              PM_Config->HDD_Attribute[device-2] = (device-2) | vol_bit;
              PM_INFO("[DSK][HDD] HDD%d: %s%s [vol %d]\n",device-2, PM_Config->HDDPath[device-2], PM_Config->HDD[device-2].name, g_current_vol);
             break;
      case 6:  // CD-ROM (0:/CDROM with sub-folder browsing, SD only -- no vol bit)
              if (imageid==0xFF)
                 { // None > Eject
                   PM_Config->CDR.size=0;
                   PM_Config->CDR.name[0]=0;
                   PM_Config->CDRPath[0]=0;
                   PM_Config->CDR_Attribute=0;
                   PM_INFO("[DSK][CDR] CD: None\n");
                   break;
                 }
              PM_Config->CDR.size=DSKLIST[imageid].size;
              strcpy(PM_Config->CDR.name,DSKLIST[imageid].name);
              strcpy(PM_Config->CDRPath,cd_image_path);
              PM_Config->CDR_Attribute=0;
              PM_INFO("[DSK][CDR] CD: %s%s\n",PM_Config->CDRPath,PM_Config->CDR.name);
             break;
      default:
        PM_WARNING("[DSK] Invalid device type %d\n",device);
        return 1;
    }
    return 0;
  }

// Build a list of images
// dtype: 0,Floppy 1,HDD 2 ROM, 3,CD-ROM
uint8_t ListImages(uint8_t dtype,uint8_t *ImageNb, FSizeName_t *DiskListPrt)
  {
    uint8_t imagecnt;
    uint16_t IMGSize;
    FRESULT fr; /* Return value */
    char image_fullapth[64+10]; // Buffer for the full path of the image
    FSizeName_t *DSKLIST=DiskListPrt;  // Pointer to the 128 file size/name

  //  char const *p_dir;
    if (!PM_EnableSD()) return 0;  // SD Not enabled > No Disk to list
    DIR dj;      /* Directory object */
    FILINFO fno; /* File information */
    memset(&dj, 0, sizeof dj);
    memset(&fno, 0, sizeof fno);
    uint8_t LDIError;

    switch(dtype) {
      case 0:  // Get Floppy image list
        // Virtual-root flow: when g_current_vol == 0xFF the user is on
        // the top-level [SD]/[USB] selector. Emit those entries and
        // skip the FatFs scan entirely -- there is no real directory
        // at this level. The "size=0xFFFE" marker matches what
        // AddFoldersToList uses for sub-folders, so PMBIOS draws them
        // with the existing folder icon and pm_disk_UpdateSearchPath
        // re-enters this function once the user picks a volume.
        if (g_current_vol == 0xFF) {
            imagecnt = 0;
#if BOARD_WM10
            strcpy(DSKLIST[imagecnt].name, "[SD]");
#else
            strcpy(DSKLIST[imagecnt].name, "SD");
#endif            
            DSKLIST[imagecnt].size = 0xFFFE;
            imagecnt++;
#if USE_USBHOST
            if (usb_msc_volume_ready()) {
#if BOARD_WM10
                strcpy(DSKLIST[imagecnt].name, "[USB]");
#else
                strcpy(DSKLIST[imagecnt].name, "USB");
#endif                
                DSKLIST[imagecnt].size = 0xFFFE;
                imagecnt++;
            }
#endif
            PM_INFO("[DSK][FDD] FDD virtual root: %d entries\n", imagecnt);
            *ImageNb = imagecnt;
            return imagecnt;
        }
        sprintf(image_fullapth,"%s%s",fdd_dir_for(g_current_vol),image_path);
        PM_INFO("[DSK][FDD] Nb of Floppy: %d (vol %d)\n",PC_FloppyNB,g_current_vol);
        PM_INFO("[DSK][FDD] FDD Images path: %s\n", image_fullapth);

// Insert folder list
        imagecnt=AddFoldersToList((char *)image_fullapth, DiskListPrt);
        PM_INFO("Folder count: %d\n",imagecnt);

// Insert Legacy floppy
/*       if (PC_FloppyNB!=0 && PC_FloppyNB<3)
           for (int i=0;i<PC_FloppyNB;i++)
            {
              imagecnt++;
              char *SAVEPtr=DSKLIST;
              *(DSKLIST++)=i;
              *(DSKLIST++)=0xFE;          // Tell it is a real disk
              sprintf(DSKLIST,"<PC floppy %d>",i);          
              PM_INFO("%s \n",DSKLIST);   
              DSKLIST=SAVEPtr+16;
            }
*/
        fr = f_findfirst(&dj, &fno, image_fullapth, "*.img");
        LDIError=CMDERR_NOFDDDIR;
        break;
      case 1:  // Get HDD image list
        // Same virtual-root flow as the floppy case above -- mirror
        // the [SD]/[USB] selector so HDD images can live on either
        // volume too. See the floppy case for the rationale.
        if (g_current_vol == 0xFF) {
            imagecnt = 0;
#if BOARD_WM10
            strcpy(DSKLIST[imagecnt].name, "[SD]");
#else
            strcpy(DSKLIST[imagecnt].name, "SD");
#endif            
            DSKLIST[imagecnt].size = 0xFFFE;
            imagecnt++;
#if USE_USBHOST
            if (usb_msc_volume_ready()) {
#if BOARD_WM10
                strcpy(DSKLIST[imagecnt].name, "[USB]");
#else
                strcpy(DSKLIST[imagecnt].name, "USB");
#endif
                DSKLIST[imagecnt].size = 0xFFFE;
                imagecnt++;
            }
#endif
            PM_INFO("[DSK][HDD] HDD virtual root: %d entries\n", imagecnt);
            *ImageNb = imagecnt;
            return imagecnt;
        }
        sprintf(image_fullapth,"%s%s",hdd_dir_for(g_current_vol),image_path);
        PM_INFO("[DSK][HDD] Nb of HDD: %d (vol %d)\n",PC_DiskNB,g_current_vol);
        PM_INFO("[DSK][HDD]HDD Images: %s\n", image_fullapth);

// Insert folder list
        imagecnt=AddFoldersToList((char *)image_fullapth, DiskListPrt);
        PM_INFO("Folder count: %d\n",imagecnt);

// Insert Legacy hard disk
/*        if (PC_DiskNB!=0 && PC_DiskNB<3)
           for (int i=0;i<PC_DiskNB;i++)
            {
              imagecnt++;
              char *SAVEPtr=DSKLIST;
              *(DSKLIST++)=i+0x80;
              *(DSKLIST++)=0xFE;          // Tell it is a real disk

              sprintf(DSKLIST,"<PC disk %d>",i);
              PM_INFO("%s \n",DSKLIST);      
              DSKLIST=SAVEPtr+16;
            }
*/            
        fr = f_findfirst(&dj, &fno, image_fullapth, "*.img");
        LDIError=CMDERR_NOHDDDIR;
        break;
      case 2: // Get ROM image List
        PM_INFO("ROM Images: %s\n", rom_dir);
        fr = f_findfirst(&dj, &fno, rom_dir, "*.bin");
        LDIError=CMDERR_NOROMDIR;
        break;
      case 3: // Get CD-ROM image list -- 0:/CDROM on the SD with sub-folder
              // browsing (SD only, no [SD]/[USB] root). Folders + ".." come
              // first via AddFoldersToList_impl, then *.iso and (2nd pass
              // below) *.cue. Extension is stripped for display; the mount
              // side (dev_cdr_mount_from_config) re-tries .iso then .cue.
              // Keep in sync with CDMAN_DIR in cdrom_image_manager.c.
        sprintf(image_fullapth,"%s%s",cdrom_dir,cd_image_path);  // "0:/CDROM/" + sub-path
        PM_INFO("[DSK][CDR] CD Images path: %s\n", image_fullapth);
        imagecnt=AddFoldersToList_impl((char *)image_fullapth, DiskListPrt, cd_image_path[0]!=0);  // ".." only below CDROM root
        PM_INFO("[DSK][CDR] Folder count: %d\n",imagecnt);
        fr = f_findfirst(&dj, &fno, image_fullapth, "*.iso");
        LDIError=CMDERR_NOHDDDIR;
        break;
      }

    if (FR_OK != fr) {
        // No images matched (or path missing). Don't reset imagecnt to
        // zero -- AddFoldersToList may have populated ".." and/or
        // subfolders that the BIOS still needs to render so the user
        // can navigate. Return what we have.
        PM_INFO("f_findfirst error: %s (%d) -- keeping %d folder entries\n",
                FRESULT_str(fr), fr, imagecnt);
        *ImageNb = imagecnt;
        return imagecnt; //LDIError;
        }

    uint8_t cd_scan_pass = 0;
scan_loop:
    while (fr == FR_OK && fno.fname[0]) { /* Repeat while an item is found */

     	 bool validimg = true;
       switch(dtype) {
         case 0: // HDD
          IMGSize=fno.fsize/(1024);       // Floppy size in Kb
          PM_INFO("%s [%dKB]\n", fno.fname, IMGSize);
          if ((IMGSize<160) || (IMGSize>2880)) validimg = false;
          break;
         case 1: // FDD
          IMGSize=fno.fsize/(1024*1024);  // Disk size in MB
          PM_INFO("%s [%dMB]\n", fno.fname, IMGSize);
          if ((IMGSize<2) || (IMGSize>4096)) validimg = false;
          break;
         case 2: // ROM
          if (((IMGSize % 1024)==0) && (IMGSize<=64*1024)) validimg = false;
          IMGSize=IMGSize/1024;
          break;
         case 3: // CD-ROM
          IMGSize=fno.fsize/(1024*1024);  // ISO size in MB
#if USE_CDROM
          { size_t fl = strlen(fno.fname);   // .cue: fsize is the tiny text sheet -> use the .bin size
            if (fl >= 4 && (fno.fname[fl-3]|0x20)=='c' && (fno.fname[fl-2]|0x20)=='u' && (fno.fname[fl-1]|0x20)=='e') {
                char cp[sizeof(image_fullapth)+20];
                snprintf(cp, sizeof cp, "%s%s", image_fullapth, fno.fname);  // image_fullapth ends with '/'
                IMGSize = cd_image_size_mb(cp);
            } }
#endif
          PM_INFO("%s [%dMB]\n", fno.fname, IMGSize);
          if (IMGSize>900) validimg = false;   // > 900MB is not a CD image
          break;
         }

        if (validimg) {
        uint16_t fnamelength=strlen(fno.fname);
        if (fnamelength>17)     // 17 is 13 + 4 for extension
           {
            fno.fname[13]=0;    // Filename size error
            DSKLIST[imagecnt].size=0xFFFF;
           } 
           else 
           {
            fno.fname[fnamelength-4]=0;  // Remove extension
            DSKLIST[imagecnt].size=IMGSize;           
           }
        strcpy(DSKLIST[imagecnt].name,fno.fname);
        
        if (imagecnt++==MAX_DISKIMAGES) return MAX_DISKIMAGES;  // Increment the number of Disk image, no more than 100
        }  // if (validimg)
        fr = f_findnext(&dj, &fno); /* Search for next item */
    }


    if (dtype==1) //if HDD, search also .vhd
      {
        PM_INFO("Search for VHD images\n");
        fr = f_findfirst(&dj, &fno, image_fullapth, "*.vhd");
        while (fr == FR_OK && fno.fname[0]) { /* Repeat while an item is found */
      	bool validimg = true;
        IMGSize=fno.fsize/(1024*1024);  // Disk size in MB
        PM_INFO("%s [%dMB]\n", fno.fname, IMGSize);
        if ((IMGSize<2) || (IMGSize>4096)) validimg = false;   

        if (validimg) {
        uint16_t fnamelength=strlen(fno.fname);
         if (fnamelength>17)     // 17 is 13 + 4 for extension
           {
            fno.fname[13]=0;    // Filename size error
            DSKLIST[imagecnt].size=0xFFFF;
           } 
           else 
           {
            fno.fname[fnamelength-4]=0;  // Remove extension
            DSKLIST[imagecnt].size=IMGSize;           
           }
         strcpy(DSKLIST[imagecnt].name,fno.fname);
        
         if (imagecnt++==MAX_DISKIMAGES) return MAX_DISKIMAGES;  // Increment the number of Disk image, no more than 100
         }  // if (validimg)
        fr = f_findnext(&dj, &fno); /* Search for next item */
       }
    }

    f_closedir(&dj);

    if (dtype == 3 && cd_scan_pass == 0) {
        // CD-ROM second pass: also list *.cue (mixed-mode / audio discs --
        // an .iso can only carry the data track). Appended after the .iso
        // entries; on a name collision the mount side prefers name.iso.
        cd_scan_pass = 1;
        fr = f_findfirst(&dj, &fno, image_fullapth, "*.cue");
        if (fr == FR_OK) goto scan_loop;
    }

    pm_disk_sort_images(DSKLIST, imagecnt);

    *ImageNb=imagecnt;
    return imagecnt;
}

uint8_t NewHDDImage()
{
  char hdd_dir[]="0:/HDD/";
  char NewFilename[25];  //13+5+7
  FSizeName_t *NewHDD=(FSizeName_t *)&PCCR_Param;
  FRESULT fr; /* Return value */
  FIL img;
  uint bw;  // Nb of Bytes Written
  uint NewSize;

  strcpy(NewFilename,hdd_dir);  
  strcat(NewFilename,NewHDD->name);
  strcat(NewFilename,".img");
  NewSize=(NewHDD->size*(1024*1024)/((512 * 63 * 16)))*(512 * 63 * 16);
  PM_INFO("[DSK] Name: %s Size(MB) : %d Size : %d\n",NewFilename,NewHDD->size,NewSize); 

	fr = f_open(&img, NewFilename, FA_READ );
	switch (fr)
    {
    case FR_OK :
        PM_WARNING("Error: Image already exist\n");
        return CMDERR_IMGEXIST;
	  case FR_NO_PATH:
        PM_WARNING("Error: Folder does not exist\n");
        return CMDERR_NOHDDDIR;
	  case FR_NO_FILE:  /* The Disk image is created here */
        PM_WARNING("No file, Create it\n");

	      fr = f_open(&img, NewFilename, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
	      if (FR_OK != fr) 
          { 
           PM_WARNING("Fail to create: %d\n",fr);
           return CMDERR_DISK;
          }

       // Write at the end of the file to create it (With the correct size)
	     f_lseek(&img,NewSize-512);
 	     fr = f_write(&img,(void *) &NullSect,512,&bw);
	     if (FR_OK != fr)
	        {
           PM_WARNING("f_write failed : %d\n",fr);
           return CMDERR_WRITE;
	        }

       // Go back to the begining
       f_lseek(&img,0);
 	     fr = f_write(&img,(void *) &newmbr,512,&bw);
	     if (FR_OK != fr)
	        {
           PM_WARNING("f_write failed : %d\n",fr);
           f_close(&img);
           return CMDERR_WRITE;
	        }

       //Clean the first 2 Mega      
       int sectcnt;
       NewSize=NewSize/512;  // Convert the size to a Nb of sector
       sectcnt=2048*2;
       if (sectcnt>NewSize) sectcnt=NewSize;
       PM_INFO("Sectors to clean:%d, Size:%d\n",sectcnt,sectcnt*512);
       
       f_lseek(&img,512);
       for (int i=0;i<sectcnt-1;i++)
         {
 	       fr = f_write(&img,(void *) &NullSect,512,&bw);
	       if (FR_OK != fr)
	         {
            PM_WARNING("f_write failed : %d\n",fr);
            f_close(&img);
            return CMDERR_WRITE;
	         }
         }

        f_close(&img);
        return 0;
    default:
        return CMDERR_DISK;
    }
}

#define CONFIG_LINE_BUFFER_SIZE 64
#define MAX_CONFIG_VARIABLE_LEN 16

bool read_hex_from_config_line(char* config_line, uint32_t *val) {
    char prm_name[MAX_CONFIG_VARIABLE_LEN];

  if (sscanf(config_line, "%*s %x", val)==1)
     { return true; }
  return false;
}
bool read_dec_from_config_line(char* config_line, uint32_t *val) {
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    
    if (sscanf(config_line, "%*s %d", val)==1)
     { return true; }
  return false;
}

bool read_char13_from_config_line(char* config_line, char *val) {
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    
    if (sscanf(config_line, "%*s %13s", val)==1)
     { return true; }
  return false;
}

/*void read_double_from_config_line(char* config_line, double* val) {    
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    sscanf(config_line, "%s %lf\n", prm_name, val);
} */
bool read_str_from_config_line(char* config_line, char* val) {    
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    if (sscanf(config_line, "%*s %s\n", val)==1)
     { return true; }
  return false;
}

void PM_ResetConfig()
{
// memset((void *) &PM_DP_RAM[CONF_OFFS],0,sizeof(PMCFG_t));
 memset((void *) &PM_DP_RAM[CONF_OFFS],0,CONF_SIZE);
 // Set the default config values
 PM_Config->PMBOOT=1;
}

// =========================================================================
// pm_DumpConfig — human-readable serial dump of the currently loaded
// PMCFG_t in PM_DP_RAM[CONF_OFFS..], plus the BV_* variables that live
// before it. Used to diagnose corrupted / wrong PICOMEM.CFG loads: call
// from pm_LoadConfig (post-read) and pm_SaveConfig (pre-write) so the
// before/after byte image is on the UART.
//
// `tag` is a short label printed in the header so consecutive dumps can
// be told apart (e.g. "load", "save", "post-cfgtxt").
// =========================================================================
void pm_DumpConfig(const char *tag)
{
    PMCFG_t *c = (PMCFG_t *)(void *)&PM_DP_RAM[CONF_OFFS];

    PM_INFO("=== PM_DumpConfig [%s] CONF_OFFS=%u sizeof(PMCFG_t)=%u ===\n",
            tag ? tag : "?", (unsigned)CONF_OFFS, (unsigned)sizeof(PMCFG_t));

    // BV_* variables (before CONF_OFFS) -- not part of PMCFG_t but the
    // ones most often misread as side-effects of a bad load.
    PM_INFO(" BV: VARID=0x%02X InitCnt=%u BootCnt=%u IRQ=%u Tandy=%u TdyRAM=%u\n",
            (unsigned)BV_VARID, (unsigned)BV_InitCount, (unsigned)BV_BootCount,
            (unsigned)BV_IRQ, (unsigned)BV_PC_Model, (unsigned)BV_TdyRAM);
    PM_INFO(" BV: PSRAMInit=0x%02X SDInit=0x%02X USBInit=0x%02X CFGInit=0x%02X PortInit=0x%02X\n",
            (unsigned)BV_PSRAMInit, (unsigned)BV_SDInit, (unsigned)BV_USBInit,
            (unsigned)BV_CFGInit, (unsigned)BV_PortInit);

    // Hex dump of first 64 bytes of CONF area -- useful when the named
    // fields look weird; shows exactly what's in PM_DP_RAM.
    PM_INFO(" Raw CONF[+0..+63]:\n");
    for (int row = 0; row < 4; row++) {
        volatile uint8_t *p = &PM_DP_RAM[CONF_OFFS + row * 16];
        PM_INFO("  +%02X:", row * 16);
        for (int i = 0; i < 16; i++) PM_INFO(" %02X", (unsigned)p[i]);
        PM_INFO("\n");
    }

    // FDD / HDD images
    for (int i = 0; i < 2; i++)
        PM_INFO(" FDD[%d]: sz=%u attr=0x%02X name='%.14s'\n",
                i, (unsigned)c->FDD[i].size, (unsigned)c->FDD_Attribute[i],
                c->FDD[i].name);
    for (int i = 0; i < 4; i++)
        PM_INFO(" HDD[%d]: sz=%u attr=0x%02X name='%.14s'\n",
                i, (unsigned)c->HDD[i].size, (unsigned)c->HDD_Attribute[i],
                c->HDD[i].name);
    PM_INFO(" CDR[0]: sz=%u attr=0x%02X name='%.14s'\n",
                (unsigned)c->CDR.size, (unsigned)c->CDR_Attribute,
                c->CDR.name);

    // Memory map summary (full 64 entries, packed)
    PM_INFO(" MEM_Map:");
    for (int i = 0; i < 64; i++) {
        PM_INFO(" %u", (unsigned)c->MEM_Map[i]);
        if ((i & 0xF) == 0xF && i < 63) PM_INFO("\n        ");
    }
    PM_INFO("\n");

    // Misc options
    PM_INFO(" EMS_Port=%u EMS_Addr=%u PMRAM_Ext=%u PSRAM_Ext=%u Max_Conv=%u\n",
            (unsigned)c->EMS_Port, (unsigned)c->EMS_Addr,
            (unsigned)c->PMRAM_Ext, (unsigned)c->PSRAM_Ext, (unsigned)c->Max_Conv);
    PM_INFO(" FastBoot=%u PMBOOT=%u IgnoreAB=%u EnableUSB=%u EnableWifi=%u EnableJOY=%u\n",
            (unsigned)c->FastBoot, (unsigned)c->PMBOOT, (unsigned)c->IgnoreAB,
            (unsigned)c->EnableUSB, (unsigned)c->EnableWifi, (unsigned)c->EnableJOY);
    PM_INFO(" SD_Speed=%u RAM_Speed=%u Boot_HDD=%u B_MAX_PMRAM=%u B_MAX_ROM=%u\n",
            (unsigned)c->SD_Speed, (unsigned)c->RAM_Speed, (unsigned)c->Boot_HDD,
            (unsigned)c->B_MAX_PMRAM, (unsigned)c->B_MAX_ROM);
    PM_INFO(" PREBOOT=%u BIOSBOOT=%u ne2000IRQ=%u ne2000Port=0x%04X\n",
            (unsigned)c->PREBOOT, (unsigned)c->BIOSBOOT,
            (unsigned)c->ne2000IRQ, (unsigned)c->ne2000Port);
    PM_INFO(" AudioOut=%u Adlib=%u SBEnable=%u TDYPort=0x%04X CMSPort=0x%04X SBPort=0x%04X SBIRQ=%u\n",
            (unsigned)c->AudioOut, (unsigned)c->Adlib, (unsigned)c->SBEnable,
            (unsigned)c->TDYPort, (unsigned)c->CMSPort,
            (unsigned)c->SBPort, (unsigned)c->SBIRQ);
    PM_INFO(" ColorPr=%u UseRTC=%u\n",
            (unsigned)c->ColorPr, (unsigned)c->UseRTC);

    // Cheap corruption sniff: any disk-name byte that's neither 0 nor
    // printable ASCII is a strong sign of garbage in the load.
    int suspicious = 0;
    for (int i = 0; i < 2; i++)
        for (int k = 0; k < 14; k++) {
            unsigned char ch = (unsigned char)c->FDD[i].name[k];
            if (ch == 0) break;
            if (ch < 0x20 || ch > 0x7E) { suspicious++; break; }
        }
    for (int i = 0; i < 4; i++)
        for (int k = 0; k < 14; k++) {
            unsigned char ch = (unsigned char)c->HDD[i].name[k];
            if (ch == 0) break;
            if (ch < 0x20 || ch > 0x7E) { suspicious++; break; }
        }
    if (suspicious)
        PM_INFO(" !! SUSPICIOUS: %d disk-name field(s) contain non-ASCII bytes -> likely corrupted CFG\n",
                suspicious);

    PM_INFO("=== /PM_DumpConfig [%s] ===\n", tag ? tag : "?");
}

uint8_t pm_LoadConfig()
{
  char buf[CONFIG_LINE_BUFFER_SIZE];
  char imgname[14];
  bool BIOS_Changed=false;
  FRESULT fr; /* Return value */
  FIL     cfg;
  FILINFO finfo;
  uint bw;

  uint8_t cfg_result;

	fr = f_open(&cfg, cfgn, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
	if (FR_OK != fr)
     {
      PM_INFO("[DSK][CFG] Can't Open picomem.cfg %d\n",fr);     
      cfg_result=1;
      PM_ResetConfig(); // Clear the configuration area if read fail
     } 
     else
     {
      fr = f_stat (cfgn, &finfo);					/* Get file status to read file size */
      PM_INFO("[DSK][CFG] Config size:%d File Size:%d\n",sizeof(PMCFG_t),finfo.fsize );
    	fr = f_read(&cfg,(void *) &PM_DP_RAM[CONF_OFFS],sizeof(PMCFG_t),&bw);   // Read from the configuration area
	    if (FR_OK != fr)
	       {
           PM_WARNING("[DSK][CFG] picomem.cfg read Fail : %d \n",fr);
           f_close(&cfg);
           PM_ResetConfig(); // Clear the configuration area if read fail
           cfg_result=2;
	       }
        else
         {
           PM_INFO("[DSK][CFG] picomem.cfg read Ok (read %u of %u, file=%u)\n",
                   (unsigned)bw, (unsigned)sizeof(PMCFG_t), (unsigned)finfo.fsize);
           if (bw != sizeof(PMCFG_t))
              PM_WARNING("[DSK][CFG] picomem.cfg short read: got %u, expected %u (file size %u)\n",
                         (unsigned)bw, (unsigned)sizeof(PMCFG_t), (unsigned)finfo.fsize);
           f_close(&cfg);
           cfg_result=0;
         }
       }
f_close(&cfg);

// Dump what we just loaded so anything weird (corrupted CFG, short
// read, ROM-side garbage in BV_*) is visible on the UART before any
// later code reads it.
//pm_DumpConfig("after-load");

// Those values are cleared before loading config.txt and autodetect after init ends
PM_Config->PREBOOT=false;    // Clear the preboot parameter.
PM_Config->BIOSBOOT=false;   // Clear the preboot parameter.
BV_IRQ=0;                    // Reset BV_IRQ to force another autodetect
BV_TdyRAM=0;
BV_PC_Model=0;

// Now open and load config.txt
PM_INFO("[DSK][CFG] Open config.txt\n");
	fr = f_open(&cfg, cfg2n, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
	if (FR_OK != fr)
     {
      PM_WARNING("[DSK][CFG] Can't Open config.txt %d\n",fr);     
     } 
     else
     {
     while(!f_eof(&cfg)) {
        f_gets(buf, CONFIG_LINE_BUFFER_SIZE, &cfg);
        if (buf[0] == '#' || buf[0] == ';' || strlen(buf) < 4) {
            continue;        
        }
        if ((strstr(buf, "BIOS ")&&!BIOS_Changed)) {   // Redefine the BIOS Base address
           uint32_t res;
           if (read_hex_from_config_line(buf,&res))
           {
            if ((res>=0xC000)&&(res<0xF000))
               {
                PM_INFO("[DSK][CFG] BIOS %x",res);
                BIOS_Changed=true;
                PM_BIOSADDRESS=res;
                res=res<<4;
                // Cancel the BIOS Config
                dev_memorytype_remove(MEM_BIOS);
                dev_memorytype_remove(MEM_DISK);
                // Configure the BIOS ROM Address
                RAM_Offset_Table[MEM_BIOS]=&PMBIOS[-res];                
                SetMEMType(res,MEM_BIOS,MEM_S_16k);
                SetMEMIndex(res,MEM_BIOS,MEM_S_16k);
                // Configure the BIOS RAM Address
                res=res+0x4000;
                RAM_Offset_Table[MEM_DISK]=&PM_DP_RAM[-res];                
                SetMEMType(res,MEM_DISK,1);  // 8kb only
                SetMEMIndex(res,MEM_DISK,1);
                PC_DB_Start=res+PM_DB_Offset;       // Update the Disk Buffer Address
                //PM_INFO(" - Done\n",res);

               } else if (res==0) {   // Picomem in no RAM/ROM Mode
                PM_INFO("> BIOS OFF !\n");
                dev_memorytype_remove(MEM_BIOS);
                dev_memorytype_remove(MEM_DISK);
               }
           }
        }
        if (strstr(buf, "OFF")) {  // Disable all the emulation
          PM_INFO("> Disable all\n");
          PM_BIOSADDRESS=0;
          dev_memorytype_remove(MEM_BIOS); // Remove the BIOS
          dev_memorytype_remove(MEM_DISK); // Remove the BIOS RAM
     //     DelPortType(DEV_PM);           // Remove the PicoMEM Ports
        }
        if (strstr(buf, "PREBOOT")) {
          PM_Config->PREBOOT=true;
          PM_INFO("> PREBOOT On\n");
        }
        if ((strstr(buf, "POST "))) {   // Redefine the BIOS Base address
           uint32_t res;
           if (read_hex_from_config_line(buf,&res))
           {
            if ((res>=0x80)&&(res<0x1000))
               {
                PM_INFO("[DSK][CFG] POST %x",res);
                dev_post_setPort2(res);

               } else if (res==0) {     // Disable POST Code

               }
           }           
        }
        if (strstr(buf, "ROM ")) {   // Load ROM files
           uint32_t res;
           char romname[31];
           if (sscanf(buf, "%*s %x %30s", &res, romname)==2)
            {
              PM_INFO("> ROM %X %s\n",res,romname);
              if (res<0xF000 && res>=0xC000)
              {
                pm_load_rom_file(res, romname);
              }
            } else {
              PM_WARNING("Invalid ROM line: %s %X %s\n", buf, res, romname);
            }
        }           
        if (strstr(buf, "BIOSBOOT")) {  // Redirect also the BIOS Boot (BASIC) Not implemented for the moment
          PM_Config->BIOSBOOT=true;
          PM_INFO("> BIOSBOOT On\n");
        }              
        if (strstr(buf, "IRQ ")) {
           uint32_t res;
           if (read_dec_from_config_line(buf,&res))
              if ((res==3)||(res==5)||(res==7)) 
              {
                BV_IRQ=res;      // BV_IRQ is set, should not be auto detected later
                PM_INFO("[DSK][CFG] IRQ %d\n",res);
              }
        }
#if !BOARD_PMJR
        if (strstr(buf, "TANDY")) {  // Disable all the emulation
          uint32_t res;
          PM_INFO(">[DSK][CFG] Tandy Mode\n");
          BV_PC_Model=1;   //1 > Tandy
          if (read_dec_from_config_line(buf,&res)) // Add the conv RAM size to emulate
             {
              switch (res) {
                 case 128 : BV_TdyRAM=8;  // number of 16Kb RAM Block
                            break;
                 case 256 : BV_TdyRAM=16;
                            break;
                 case 384 : BV_TdyRAM=24;
                            break;
               }
               PM_INFO("[DSK][CFG] Add %dKB RAM (%d) %d\n",BV_TdyRAM*16,BV_TdyRAM,res);
             }
        }
#endif        
        if (strstr(buf, "ADLIB ")) {
           uint32_t res;
           if (read_hex_from_config_line(buf,&res))
             {
              PM_INFO("[DSK][CFG] ADLIB %d\n",res);
              if (res==0)
                    {PM_Config->Adlib=false;}
               else {PM_Config->Adlib=true;}
             }
        }
        if (strstr(buf, "SB ")) {   // Sound Blaster On/Off (geometry from MCA POS)
           uint32_t res;
           if (read_hex_from_config_line(buf,&res))
             {
              PM_INFO("[DSK][CFG] SB %d\n",res);
              PM_Config->SBEnable = (res!=0);
             }
        }
        if (strstr(buf, "HDD0 ")) {  // Not implemented
           if (read_char13_from_config_line(buf,&imgname[0]))
             { 
              PM_INFO("[DSK][CFG] HDD0 %s\n",imgname); 
             }
        if (strstr(buf, "HDD1 ")) {
           if (read_char13_from_config_line(buf,&imgname[0]))
              PM_INFO("[DSK][CFG] HDD1 %s\n",imgname);
        }
        if (strstr(buf, "HDD2 ")) {
           if (read_char13_from_config_line(buf,&imgname[0]))
              PM_INFO("[DSK][CFG] HDD2 %s\n",imgname);
        }
        if (strstr(buf, "HDD3 ")) {
           if (read_char13_from_config_line(buf,&imgname[0]))
              PM_INFO("[DSK][CFG] HDD3 %s\n",imgname);
        }
        }
       }
//      PM_INFO("Close config.txt ");       
      f_close(&cfg);
     }
PM_INFO("[DSK][CFG] Load Config End\n");

return cfg_result;     
}

uint8_t pm_SaveConfig()
{
  const char cfgn[]="0:/PICOMEM.CFG";
  FRESULT fr;
  FIL cfg;
  UINT bw;

  // Dump what we're about to commit to SD. If this dump shows garbage,
  // we're saving garbage -- and the corruption is upstream of f_write
  // (e.g. menu wrote bad values into PM_DP_RAM, or a stale half-load).
  pm_DumpConfig("before-save");

	fr = f_open(&cfg, cfgn, FA_CREATE_ALWAYS | FA_READ | FA_WRITE);
	if (FR_OK != fr)
     {
       PM_WARNING("[DSK][CFG] f_open failed %d\n", fr);
       return 1; // open failed
     }

	fr = f_write(&cfg, (void *) &PM_DP_RAM[CONF_OFFS], sizeof(PMCFG_t), &bw);
	if (FR_OK != fr)
	   {
      PM_WARNING("[DSK][CFG] f_write failed : %d\n", fr);
      f_close(&cfg);          // best-effort close on error
      return 2; // write call failed
	   }

	// f_write can return FR_OK with bw < requested on short write (SD
	// full, sector ECC retry that dropped bytes). Without this check
	// the file is truncated and the next pm_LoadConfig() picks up
	// garbage past `bw` bytes -> stale config silently restored.
	if (bw != sizeof(PMCFG_t))
	   {
	  PM_WARNING("[DSK][CFG] f_write short: %u/%u bytes\n",
	             (unsigned)bw, (unsigned)sizeof(PMCFG_t));
	  f_close(&cfg);
	  return 3; // short write
	   }

	// FatFS commits FAT entries + directory metadata on close. If close
	// fails, the file may show 0 bytes / wrong cluster chain on the next
	// boot even though everything above succeeded. Capture the result.
	fr = f_close(&cfg);
	if (FR_OK != fr)
	   {
	  PM_WARNING("[DSK][CFG] f_close (flush) failed: %d\n", fr);
	  return 4; // metadata flush failed
	   }

  PM_INFO("[DSK][CFG] Ok (%u bytes)\n", (unsigned)bw);
 return 0; // success
}

// Command lines for Disk

void pm_ls(const char *dir) {
    char cwdbuf[FF_LFN_BUF] = {0};
    FRESULT fr; // Return value 
    char const *p_dir;
    if (dir[0]) {
        p_dir = dir;
    } else {
        fr = f_getcwd(cwdbuf, sizeof cwdbuf);
        if (FR_OK != fr) {
            PM_INFO("f_getcwd error: %s (%d)\n", FRESULT_str(fr), fr);
            return;
        }
        p_dir = cwdbuf;
    }
    PM_INFO("Directory Listing: %s\n", p_dir);
    DIR dj;      // Directory object
    FILINFO fno; // File information
    memset(&dj, 0, sizeof dj);
    memset(&fno, 0, sizeof fno);
    fr = f_findfirst(&dj, &fno, p_dir, "*");
    if (FR_OK != fr) {
        PM_INFO("f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    while (fr == FR_OK && fno.fname[0]) { /* Repeat while an item is found */
        // Create a string that includes the file name, the file size and the
        // attributes string.
        const char *pcWritableFile = "writable file",
                   *pcReadOnlyFile = "read only file",
                   *pcDirectory = "directory";
        const char *pcAttrib;
        // Point pcAttrib to a string that describes the file.
        if (fno.fattrib & AM_DIR) {
            pcAttrib = pcDirectory;
        } else if (fno.fattrib & AM_RDO) {
            pcAttrib = pcReadOnlyFile;
        } else {
            pcAttrib = pcWritableFile;
        }
        // Create a string that includes the file name, the file size and the
        // attributes string.
        PM_INFO("%s [%s] [size=%llu]\n", fno.fname, pcAttrib, fno.fsize);

        fr = f_findnext(&dj, &fno); /* Search for next item */
    }
    f_closedir(&dj);
}


// FATFS DiskIO Code

/* Definitions of physical drive number for each drive */
//#define DEV_RAM		0	/* Example: Map Ramdisk to physical drive 0 */
#define DEV_SD		0	/* Example: Map MMC/SD card to physical drive 1 */
#define DEV_USB		1	/* Example: Map USB MSD to physical drive 2 */

extern "C" {
extern DSTATUS sd_disk_status();
extern DSTATUS sd_disk_initialize();
extern DRESULT sd_disk_read(BYTE *buff, LBA_t sector, UINT count);
extern DRESULT sd_disk_write(const BYTE *buff, LBA_t sector, UINT count);
extern DRESULT sd_disk_ioctl(BYTE cmd, void *buff);


extern DSTATUS usb_disk_status();
extern DSTATUS usb_disk_initialize();
extern DRESULT usb_disk_read(BYTE *buff, LBA_t sector, UINT count);
extern DRESULT usb_disk_write(const BYTE *buff, LBA_t sector, UINT count);
extern DRESULT usb_disk_ioctl(BYTE cmd, void *buff);

}

#include "pm_cache.h"  // PSRAM sector cache in front of sd_disk_*/usb_disk_*

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	DSTATUS stat;
	int result;

	switch (pdrv) {
	case DEV_SD :
		return sd_disk_status();;    
#if USE_USBHOST
	case DEV_USB :
		return usb_disk_status();
#endif
	} 
	return STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive number to identify the drive */
)
{
	DSTATUS stat = STA_NOINIT;
	switch (pdrv) {

	case DEV_SD :    
			stat = sd_disk_initialize();
      break;
#if USE_USBHOST
	case DEV_USB :
		  stat = usb_disk_initialize();
      break;
#endif
	default :
		  return STA_NOINIT;
	}
	if (stat == 0) pm_cache_init(pdrv);  // media ready: (re)arm the PSRAM sector cache
	return stat;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	switch (pdrv) {
	case DEV_SD :
    DBG_ON_IRQ_SB();
		return pm_cache_read(pdrv, buff, sector, count);
#if USE_USBHOST
	case DEV_USB :
		return pm_cache_read(pdrv, buff, sector, count);
#endif
  }
	return RES_PARERR;
}

// === WM10 USB chain self-test (boot, Core 0, NO Int13h / no active MCA cycle) ===
// Heavy multi-iteration stress of the pure USB host -> MSC -> FatFs read chain,
// isolated from the Int13h/MCA path. Picks the largest regular file in "1:/",
// then reads it in full ITER times, verifying the checksum is IDENTICAL every
// pass (catches intermittent transport failures AND silent data corruption).
extern "C" void pm_usb_chain_test(void)
{
    DIR dir; FILINFO fno; FRESULT fr;
    char path[96]; path[0] = 0;
    unsigned long fsize = 0;

    printf("\n[USBTEST] === pure USB chain stress (Core0, boot, no Int13h) ===\n");
    fr = f_opendir(&dir, "1:/");
    printf("[USBTEST] opendir(1:/) rc=%d\n", (int)fr);
    if (fr != FR_OK) return;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        printf("[USBTEST]   %-28s %10lu%s\n", fno.fname,
               (unsigned long)fno.fsize, (fno.fattrib & AM_DIR) ? " <DIR>" : "");
        if (!(fno.fattrib & AM_DIR) && fno.fsize > fsize) {   // pick the LARGEST file
            fsize = (unsigned long)fno.fsize;
            snprintf(path, sizeof path, "1:/%s", fno.fname);
        }
    }
    f_closedir(&dir);
    if (path[0] == 0) { printf("[USBTEST] no regular file to read\n"); return; }

    const unsigned long ITER     = 100;                 // passes over the file
    const unsigned long READ_CAP = 4UL * 1024 * 1024;   // cap per pass (4 MB)
    unsigned long cap = (fsize < READ_CAP) ? fsize : READ_CAP;
    printf("[USBTEST] target=%s size=%lu  -> %lu passes, %lu B/pass\n",
           path, fsize, ITER, cap);

    // UART-hypothesis test: drain the console, then read SILENTLY so the async
    // UART TX IRQ is completely idle during every read. If USB reads clean with
    // the UART quiet (but fails when console output drains during a read), the
    // async-UART IRQ preempting the EPX re-arm is the cause.
    printf("[USBTEST] flushing console, then SILENT read loop (UART idle during reads)...\n");
    sleep_ms(400);   // let the async UART ring buffer fully drain

    static BYTE buf[4096];
    unsigned long ref_sum = 0; int have_ref = 0;
    unsigned long ok = 0, fail = 0, mismatch = 0, grand = 0;
    int last_fail_rc = 0; unsigned long last_fail_at = 0, last_it = 0;
    uint32_t t0 = time_us_32();

    for (unsigned long it = 0; it < ITER; it++) {
        FIL f; UINT br; unsigned long total = 0, sum = 0;
        last_it = it;
        fr = f_open(&f, path, FA_READ);
        if (fr != FR_OK) { fail++; last_fail_rc = (int)fr; last_fail_at = 0; continue; }
        do {
            fr = f_read(&f, buf, sizeof buf, &br);   // SILENT: no printf in the loop
            for (UINT i = 0; i < br; i++) sum += buf[i];
            total += br;
        } while (fr == FR_OK && br == sizeof buf && total < cap);
        f_close(&f);
        grand += total;
        if (fr != FR_OK) { fail++; last_fail_rc = (int)fr; last_fail_at = total; continue; }
        if (!have_ref) { ref_sum = sum; have_ref = 1; }
        else if (sum != ref_sum) mismatch++;
        ok++;
    }
    uint32_t dt = time_us_32() - t0;
    printf("[USBTEST] === DONE (UART idle): %lu passes  ok=%lu fail=%lu mismatch=%lu\n",
           ITER, ok, fail, mismatch);
    printf("[USBTEST]     lastit=%lu lastfail rc=%d @%lu B  total=%lu B in %lu ms  sum=%08lx ===\n",
           last_it, last_fail_rc, last_fail_at, grand, (unsigned long)(dt / 1000), ref_sum);
}


/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0
DRESULT disk_write (
	BYTE pdrv,			  /* Physical drive number to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,	 	  /* Start sector in LBA */
	UINT count			  /* Number of sectors to write */
)
{
	switch (pdrv) {
	case DEV_SD :  
   // PM_INFO("disk_write SD p%p s%d c%d\n",buff,sector,count);
		return pm_cache_write(pdrv, buff, sector, count);
#if USE_USBHOST
	case DEV_USB :
		return pm_cache_write(pdrv, buff, sector, count);
#endif  
  }
	return RES_PARERR;
}
#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,	/* Physical drive number (0..) */
	BYTE cmd,		/* Control code */
	void *buff	/* Buffer to send/receive control data */
)
{
  //PM_INFO("disk_ioctl %d",pdrv);
	switch (pdrv) {
	case DEV_SD :   
		return sd_disk_ioctl(cmd, buff);;
#if USE_USBHOST
	case DEV_USB :
		return usb_disk_ioctl(cmd, buff);
#endif  
  }
	return RES_PARERR;
}
