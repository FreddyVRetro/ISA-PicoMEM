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

extern volatile uint8_t * PMBIOS;  //

#define FDD_USB 0
#define HDD_USB 0

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
#if HDD_USB
const char hdd_dir[]="1:/HDD/";
#else
const char hdd_dir[]="0:/HDD/";
#endif    
#if FDD_USB
const char fdd_dir[]="1:/FLOPPY/";
#else
const char fdd_dir[]="0:/FLOPPY/";
#endif 
const char rom_dir[]="0:/ROM/";

const char cfgn[]="0:/PICOMEM.CFG";
const char cfg2n[]="0:/config.txt";

char image_path[64];  // Search path for Image disk load

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

  strcat(diskfilename,".img");
  PM_INFO("pm_mountdiskimage : %s Disk: %d\n",diskfilename,hddnumber);
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
  PM_INFO("finfo: %s [size=%llu] \n", finfo.fname, finfo.fsize);
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
         PM_INFO("HDD Mount fail\n");     
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
           PM_INFO(" c%d h%d s%d\n",c,h,s);
          if ((c>0)&&(c<64)&&(s>0)&&(s<=512)&&(h>0)&&(h<17))
          {
           chs_fromfile=true;
           imageDiskList[hddnumber+1]->Set_Geometry(h,c,s, 512); // H C S
          }
         }
     } else PM_INFO("No CHS file found\n");

   // Compute the CHS value with S=63 if not available from a chs file
   if (!chs_fromfile) imageDiskList[hddnumber+1]->Set_Geometry(16, (uint32_t)(finfo.fsize / (512 * 63 * 16)), 63, 512); // H C S  
  }

// !! DOSBOX X Also get the geometry infos from the Boot Sector
    imageDiskList[hddnumber+1]->Read_Sector(0,0,1,&mbrData);
    PM_INFO("MBR Signature: %02X %02X\n",mbrData.magic1, mbrData.magic2);

#if PM_PRINTF        
		if (mbrData.magic1!= 0x55 ||	mbrData.magic2!= 0xaa) PM_INFO("Possibly invalid partition table in disk image.\n");
       else PM_INFO("Valid MBR Signature found in disk image.\n");
#endif

    PM_INFO("List Partitions :\n");
		for (int i=0;i<4;i++)
    {
      PM_INFO("MBR #%u: bootflag=%u parttype=0x%02x beginchs=0x%02x%02x%02x endchs=0x%02x%02x%02x start=%llu size=%llu\n",
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
                //sprintf(imgpath, "1:/HDD/%s.img", PM_Config->HDD[hddnumber].name);
                sprintf(imgpath, "0:/HDD/%s%s.img", PM_Config->HDDPath[hddnumber], PM_Config->HDD[hddnumber].name);
                PM_INFO("HDD%d: %s\n",hddnumber,imgpath);
              
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
}

void pm_disk_UpdateSearchPath(uint8_t id,FSizeName_t *DiskListPrt)
{
  FSizeName_t *DSKLIST=DiskListPrt;  // Pointer to the 128 file size/name  
  //FSizeName_t (*DSKLIST)[MAX_DISKIMAGES]=(FSizeName_t (*)[128])&PCCR_Param;  // Pointer to the 128 file size/name
  PM_INFO("Change DIR: %s ",DSKLIST[id].name);
  if (!strcmp(DSKLIST[id].name,".."))
     {
      image_path[strlen(image_path)-1]=0;        // Remove the "/" at the end
      char *lastslash=strrchr(image_path,'/');
      PM_INFO("Remove last %d/: %s\n",(lastslash-image_path),image_path);
      if (lastslash!=NULL) *(lastslash+1)=0;     // Remove the last folder in the path
         else image_path[0]=0;                   // No more slash > Home directory
     }
     else
     {
      if (strlen(DSKLIST[id].name)+strlen(image_path)<64) {
          strcat(image_path,DSKLIST[id].name);
          strcat(image_path,+"/");
         }
     }
  PM_INFO("New Path: %s\n",image_path);
}

static int pm_disk_image_compare(const FSizeName_t *a, const FSizeName_t *b)
{
  bool aDir = (a->size == 0xFFFE);
  bool bDir = (b->size == 0xFFFE);
  if (aDir != bDir)
    return aDir ? -1 : 1;
  return strcmp(a->name, b->name);
}

static void pm_disk_sort_images(FSizeName_t *list, uint8_t count)
{
  for (uint8_t i = 1; i < count; i++) {
    FSizeName_t temp = list[i];
    int j = i;
    while (j > 0 && pm_disk_image_compare(&list[j-1], &temp) > 0) {
      list[j] = list[j-1];
      j--;
    }
    list[j] = temp;
  }
}

// * SD is already enabled
uint8_t AddFoldersToList(char *currpath, FSizeName_t *DiskListPrt)
{
  uint8_t foldercnt=0;
  DIR dp;
  FILINFO fno;
  FRESULT f_res;
//  FSizeName_t (*DIRLIST)[MAX_DISKIMAGES]=(FSizeName_t (*)[128])&PCCR_Param;  // Pointer to the 128 file size/name
  FSizeName_t *DIRLIST=DiskListPrt;  // Pointer to the 128 file size/name
  
  if (image_path[0]!=0)
    { // Add ".." if not Home directory
     strcpy(DIRLIST[0].name,"..");
     foldercnt++;
    }

  PM_INFO("AddFoldersToList\n");
  f_res = f_opendir(&dp,currpath);
  if(f_res != FR_OK) {
    f_closedir(&dp);
    return (0);
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

// Copy the selected image to the config for the selected device (FDD0..1, HDD0..3)
uint8_t SelectImage(uint8_t device,uint8_t imageid, FSizeName_t *DiskListPrt)
  {
    FSizeName_t *DSKLIST=DiskListPrt;  // Pointer to the 128 file size/name
   // FSizeName_t (*DSKLIST)[MAX_DISKIMAGES]=(FSizeName_t (*)[128])&PCCR_Param;  // Pointer to the 128 file size/name
    imageid--;  // Remove the "None" from the list
    PM_INFO("SelectImage: device %d imageid %d (%s)\n",device,imageid,DSKLIST[imageid].name);
    switch(device) {
      case 0: // FDD0
      case 1: // FDD1
              if (imageid==0xFF)
                 { // None > Remove the image
                   PM_Config->FDD[device].size=0;
                   PM_Config->FDD[device].name[0]=0;
                   PM_Config->FDDPath[device][0]=0;
                   PM_INFO("FDD%d: None\n",device);
                   break;
                 }
              PM_Config->FDD[device].size=DSKLIST[imageid].size;
              strcpy(PM_Config->FDD[device].name,DSKLIST[imageid].name);
              strcpy(PM_Config->FDDPath[device],image_path);
              PM_INFO("FDD%d: %s%s\n",device, PM_Config->FDDPath[device], PM_Config->FDD[device].name);
              break;
      case 2 ... 5:  // HDD0..3
              if (imageid==0xFF)
                 { // None > Remove the image
                   PM_Config->HDD[device-2].size=0;
                   PM_Config->HDD[device-2].name[0]=0;
                   PM_Config->HDDPath[device-2][0]=0;
                   PM_INFO("HDD%d: None\n",device-2);
                   break;
                 }      
              PM_Config->HDD[device-2].size=DSKLIST[imageid].size;
              strcpy(PM_Config->HDD[device-2].name,DSKLIST[imageid].name);
              strcpy(PM_Config->HDDPath[device-2],image_path);
              PM_INFO("HDD%d: %s%s\n",device-2, PM_Config->HDDPath[device-2], PM_Config->HDD[device-2].name);
             break;
      default:
        PM_WARNING("Invalid device type %d\n",device);
        return 1;
    }
    return 0;
  }

// Build a list of images
// dtype: 0,Floppy 1,HDD 2, ROM
uint8_t ListImages(uint8_t dtype,uint8_t *ImageNb, FSizeName_t *DiskListPrt)
  {
    uint8_t imagecnt;
    uint16_t IMGSize;
    FRESULT fr; /* Return value */
    char image_fullapth[64+10]; // Buffer for the full path of the image
    FSizeName_t *DSKLIST=DiskListPrt;  // Pointer to the 128 file size/name
//    FSizeName_t (*DSKLIST)[MAX_DISKIMAGES]=(FSizeName_t (*)[128])&PCCR_Param;  // Pointer to the 128 file size/name

  //  char const *p_dir;
    if (!PM_EnableSD()) return 0;  // SD Not enabled > No Disk to list
    DIR dj;      /* Directory object */
    FILINFO fno; /* File information */
    memset(&dj, 0, sizeof dj);
    memset(&fno, 0, sizeof fno);
    uint8_t LDIError;

    switch(dtype) {
      case 0:  // Get Floppy image list 
        sprintf(image_fullapth,"%s%s",fdd_dir,image_path);
        PM_INFO("Nb of Floppy: %d\n",PC_FloppyNB);
        PM_INFO("FDD Images path: %s\n", image_fullapth);

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
        sprintf(image_fullapth,"%s%s",hdd_dir,image_path);
        PM_INFO("Nb of HDD: %d\n",PC_DiskNB);
        PM_INFO("HDD Images: %s\n", image_fullapth);

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
      }

    if (FR_OK != fr) {
        PM_INFO("f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
        *ImageNb=0;
        return 0; //LDIError;
        }

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
  PM_INFO("Name: %s Size(MB) : %d Size : %d\n",NewFilename,NewHDD->size,NewSize); 

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
void read_str_from_config_line(char* config_line, char* val) {    
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    sscanf(config_line, "%*s %s\n", val);
}

void PM_ResetConfig()
{
// memset((void *) &PM_DP_RAM[CONF_OFFS],0,sizeof(PMCFG_t));
 memset((void *) &PM_DP_RAM[CONF_OFFS],0,CONF_SIZE);
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
      PM_INFO("Can't Open picomem.cfg %d\n",fr);     
      cfg_result=1;
     } 
     else
     {
      fr = f_stat (cfgn, &finfo);					/* Get file status to read file size */
      PM_INFO("Config size:%d File Size:",sizeof(PMCFG_t),finfo.fsize );
    	fr = f_read(&cfg,(void *) &PM_DP_RAM[CONF_OFFS],sizeof(PMCFG_t),&bw);   // Read from the configuration area
	    if (FR_OK != fr)
	       { 
           PM_WARNING("picomem.cfg read Fail : %d \n",fr);
           f_close(&cfg);
           PM_ResetConfig(); // Clear the configuration area if read fail           
           cfg_result=2;
	       }
        else
         {
           PM_INFO("picomem.cfg read Ok (%d)\n",512);
           f_close(&cfg);
           cfg_result=0;
         }
       }
f_close(&cfg);

//return cfg_result;

PM_Config->PREBOOT=false;    // Clear the preboot parameter.
PM_Config->BIOSBOOT=false;   // Clear the preboot parameter.
BV_IRQ=0;
BV_TdyRAM=0;
BV_Tandy=0;

// Now open and load config.txt
PM_INFO("Read 0:/config.txt\n");
	fr = f_open(&cfg, cfg2n, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
	if (FR_OK != fr)
     {
      PM_WARNING("Can't Open config.txt %d\n",fr);     
     } 
     else
     {
     while(!f_eof(&cfg)) {
        f_gets(buf, CONFIG_LINE_BUFFER_SIZE, &cfg);
//        busy_wait_ms(100);
//        PM_INFO("%s",buf);
//        busy_wait_ms(100);

        if (buf[0] == '#' || buf[0] == ';' || strlen(buf) < 4) {
            continue;        
        }
        if ((strstr(buf, "BIOS ")&&!BIOS_Changed)) {   // Redefine the BIOS Base address
           uint32_t res;
           if (read_hex_from_config_line(buf,&res))
           {
            if ((res>=0xC000)&&(res<0xF000))
               {
                PM_INFO("> BIOS %X",res);
                BIOS_Changed=true;
                PM_BIOSADDRESS=res;
                res=res<<4;
                // Cancel the BIOS Config
                dev_memorytype_remove(MEM_BIOS);
                dev_memorytype_remove(MEM_DISK);
                // Configure the BIOS ROM Address
                SetMEMType(res,MEM_BIOS,MEM_S_16k);
                SetMEMIndex(res,MEM_BIOS,MEM_S_16k);
                RAM_Offset_Table[MEM_BIOS]=&PMBIOS[-res];
                // Configure the BIOS RAM Address
                res=res+0x4000;
                SetMEMType(res,MEM_DISK,1);  // 8kb only
                SetMEMIndex(res,MEM_DISK,1);
                RAM_Offset_Table[MEM_DISK]=&PM_DP_RAM[-res];
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
                PM_INFO("> POST %x",res);
                dev_post_setPort2(res);

               } else if (res==0) {     // Disable POST Code

               }
           }        
        if (strstr(buf, "BIOSBOOT")) {  // Redirect also the BIOS Boot (BASIC) Not implemented for the moment
          PM_Config->BIOSBOOT=true;
          PM_INFO("> BIOSBOOT On\n");
        }            
        }        
        if (strstr(buf, "IRQ ")) {
           uint32_t res;
           if (read_dec_from_config_line(buf,&res))
              if ((res==3)||(res==5)||(res==7)) 
              {
                BV_IRQ=res;
                PM_INFO("> IRQ %d\n",res);
              }
        }
        if (strstr(buf, "OLED ")) {
           uint32_t res;
           if (read_dec_from_config_line(buf,&res))
              if (res<5) 
              {
                PM_OLED_Type=res;
                PM_INFO("> OLED %d\n",res);
              }
        }        
        if (strstr(buf, "TANDY")) {  // Disable all the emulation
          uint32_t res;
          PM_INFO("> Tandy Mode\n");
          BV_Tandy=1;
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
               PM_INFO("> Add %dKB RAM (%d) %d\n",BV_TdyRAM*16,BV_TdyRAM,res);
             }
        }        
        if (strstr(buf, "ADLIB ")) {
           uint32_t res;
           if (read_hex_from_config_line(buf,&res))
             {
              PM_INFO("> ADLIB %d\n",res);
              if (res==0)
                    {PM_Config->Adlib=false;}
               else {PM_Config->Adlib=true;}
             }
        }
        if (strstr(buf, "HDD0 ")) {  // Not implemented
           if (read_char13_from_config_line(buf,&imgname[0]))
             { 
              PM_INFO("> HDD0 %s\n",imgname); 
             }
        if (strstr(buf, "HDD1 ")) {
           if (read_char13_from_config_line(buf,&imgname[0]))
              PM_INFO("> HDD1 %s\n",imgname);
        }
        if (strstr(buf, "HDD2 ")) {
           if (read_char13_from_config_line(buf,&imgname[0]))
              PM_INFO("> HDD2 %s\n",imgname);
        }
        if (strstr(buf, "HDD3 ")) {
           if (read_char13_from_config_line(buf,&imgname[0]))
              PM_INFO("> HDD3 %s\n",imgname);
        }
        }
       }
//      PM_INFO("Close config.txt ");       
      f_close(&cfg);
     }
PM_INFO("Load Config End\n");

//for (;;) {}
return cfg_result;     
}

uint8_t pm_SaveConfig()
{
  const char cfgn[]="0:/PICOMEM.CFG";  
  FRESULT fr; /* Return value */
  FIL cfg;
  uint bw;

	fr = f_open(&cfg, cfgn, FA_CREATE_ALWAYS | FA_READ | FA_WRITE);
	if (FR_OK != fr) 
     { 
       PM_WARNING("f_open failed %d\n",fr);
       return 1; // Return error 1
     }
	fr = f_write(&cfg,(void *) &PM_DP_RAM[CONF_OFFS],sizeof(PMCFG_t),&bw);
	if (FR_OK != fr) 
	   {  
      PM_WARNING("f_write failed : %d \n",fr);    
      return 2; // Return error 2
	   }  
  
  PM_INFO("Ok\n");
  f_close(&cfg);
 return 0; // Return error 1
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
//    dev_audiomix_pause()
//		DSTATUS res=sd_disk_status();
//    dev_audiomix_resume()
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
	switch (pdrv) {

	case DEV_SD :
//      dev_audiomix_pause()
//      DSTATUS res=sd_disk_initialize();
//      dev_audiomix_resume()     
			return sd_disk_initialize();
      break;
#if USE_USBHOST
	case DEV_USB :
		  return usb_disk_initialize();
#endif
	}
	return STA_NOINIT;
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
//    dev_audiomix_pause()
//    DRESULT res=sd_disk_read(buff, sector, count);
//    dev_audiomix_resume()
    DBG_ON_IRQ_SB();
		return sd_disk_read(buff, sector, count);;
#if USE_USBHOST
	case DEV_USB :
		return usb_disk_read(buff, sector, count);
#endif
  }
	return RES_PARERR;
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
		return sd_disk_write(buff, sector, count);
#if USE_USBHOST
	case DEV_USB :
		return usb_disk_write(buff, sector, count);
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
