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
#include "ff.h"
#include "hw_config.h"  /* The SDCARD and SPI definitions  in \FatFs_SPI_PIO\sd_driver\ */
#include "ff.h"			    /* Obtains integer types */
#include "diskio.h"		  /* Declarations of disk functions */

//DOSBOX Code
#include "../dosbox/bios_disk.h"

#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "../pm_libs/pm_disk.h"   // for PC_DB_Start
#include "dev_memory.h"
extern volatile uint8_t * PMBIOS;  //

#define FDD_USB 0
#define HDD_USB 0

#define MAX_DISKIMAGES  128
#define MAX_DISKNAMELEN 14 // 14+2 > 16

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
             PM_INFO("Error: File does not exist\n");
             return CMDERR_FILEREAD;
      case FR_DISK_ERR:
             PM_INFO("Error: Physical disk failure\n");
             return CMDERR_DISK;
      case FR_NO_PATH:                
             PM_INFO("Error: Folder does not exist\n");
             return CMDERR_NOHDDDIR;                      
      default:
             PM_INFO("f_open failed %d\n",fr);
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
  char buf[16]; // Buffer for the CHS file data
//	struct partTable mbrData;
	
  PM_INFO("pm_mountfddimage : %s FDD%d\n",diskfilename,fddnumber);
  fr = f_stat (diskfilename, &finfo);					/* Get file status to read file size */

  fr=pm_convert_fopen_error((uint8_t) fr); // Convert fr to PicoMEM Error code
  if (fr!=0) return fr;

  PM_INFO("finfo: %s [size=%llu] \n", finfo.fname, finfo.fsize);
  
 // Open the FDD image file 
  //malloc(FDDfile[fddnumber],sizeof(FIL));
	fr = f_open(&FDDfile[fddnumber], diskfilename, FA_OPEN_EXISTING | FA_READ | FA_WRITE); /* ! If the file is already opened, does not work */

  fr=pm_convert_fopen_error((uint8_t) fr); // Convert fr to PicoMEM Error code
  if (fr!=0) return fr;

 // Mount FDD image  
     imageDiskList[fddnumber] = new imageDisk(&FDDfile[fddnumber], (Bit8u *)diskfilename, (finfo.fsize/1024), false);
	    if(!imageDiskList[fddnumber]) {     
         PM_INFO("Floppy Mount fail\n");   
		     return 0x03;
	       }
  // Don't need to set the Geometry for Floppy (Done in the imageDisk init)

  return 0;
	}

// * Mount disks for Int13h
// * All the Files must be closed before ( HDDfile[] )
uint8_t pm_mounthddimage(char *diskfilename, uint8_t hddnumber)
{

  int fr;
  FILINFO finfo;
  Bit32u filesize;
	struct partTable mbrData;
  FIL chs;
  char buf[16];

  PM_INFO("pm_mountdiskimage : %s Disk: %d\n",diskfilename,hddnumber);
  fr = f_stat (diskfilename, &finfo);					/* Get file status to read file size */

  fr=pm_convert_fopen_error((uint8_t) fr); // Convert fr to PicoMEM Error code
  if (fr!=0) return fr;

// Open the HDD image file 
  PM_INFO("finfo: %s [size=%llu] \n", finfo.fname, finfo.fsize);
	fr = f_open(&HDDfile[hddnumber-1], diskfilename, FA_OPEN_EXISTING | FA_READ | FA_WRITE);

  fr=pm_convert_fopen_error((uint8_t) fr); // Convert fr to PicoMEM Error code
  if (fr!=0) return fr;

// Mount HDD Image
  imageDiskList[hddnumber+1] = new imageDisk(&HDDfile[hddnumber-1], (Bit8u *)diskfilename, (finfo.fsize/1024), true);
	if (!imageDiskList[hddnumber+1]) 
         {     
         PM_INFO("HDD Mount fail\n");     
		     return CMDERR_MOUTFAIL;
         f_close(&HDDfile[hddnumber-1]);
	       }

  bool chs_fromfile=false;

// try to open the .chs file
  diskfilename[strlen(diskfilename)-3]=0;  // remove the extention
  strcat(diskfilename,"chs");
  PM_INFO("CHS file %s\n",diskfilename);

	fr = f_open(&chs, diskfilename, FA_OPEN_EXISTING | FA_READ);
	if (FR_OK == fr)
     {
      uint16_t c,h,s;
      f_gets(buf, 16, &chs);
      if (sscanf(buf, "%d %d %d",c,h,s)==1)
         {
          if ((c>0)&&(c<64)&&(s>0)&&(s<=512)&&(s>0))
          {
           PM_INFO(" c%d h%d s%s\n",c,h,s);
          }
         }     
     } 

  // Compute the chs value if not available from a chs file
  if (!chs_fromfile) imageDiskList[hddnumber+1]->Set_Geometry(16, (uint32_t)(finfo.fsize / (512 * 63 * 16)), 63, 512); // H C S  

// !! DOSBOX X Also get the geometry infos from the Boot Sector

    //imageDiskList[hddnumber+1]->Read_Sector(0,0,1,&mbrData);
#if PM_PRINTF        
		//if(mbrData.magic1!= 0x55 ||	mbrData.magic2!= 0xaa) PM_INFO("Possibly invalid partition table in disk image.");
#endif

		//startSector = 63;
		//
/*
		for (int i=0,i<4,i++)
    {
      printf("MBR #%u: bootflag=%u parttype=0x%02x beginchs=0x%02x%02x%02x endchs=0x%02x%02x%02x start=%llu size=%llu",
			(unsigned int)i,mbrData.pentry[i].bootflag&0x80,mbrData.pentry[i].parttype,
			mbrData.pentry[i].beginchs[0],mbrData.pentry[i].beginchs[1],mbrData.pentry[i].beginchs[2],
			mbrData.pentry[i].endchs[0],mbrData.pentry[i].endchs[1],mbrData.pentry[i].endchs[2],
			(unsigned long long)mbrData.pentry[i].absSectStart,
			(unsigned long long)mbrData.pentry[i].partSize);
    }
*/

			/* Pick the first available partition */
//    for(m=0;m<4;m++) {
//	      if(mbrData.pentry[m].partSize != 0x00) {
		//		LOG_MSG("Using partition %d on drive; skipping %d sectors", m, mbrData.pentry[m].absSectStart);
		//		startSector = mbrData.pentry[m].absSectStart;
		//		break;
		//	}

    return 0;
	}

#define FAT_DIR 16

// * SD is already enabled
uint8_t AddFoldersToList(char *currpath)
{
  uint8_t foldercnt=0;
  DIR dp;
  FILINFO fno;
  FRESULT f_res;  
  FSizeName_t (*DIRLIST)[MAX_DISKIMAGES]=(FSizeName_t (*)[128])&PCCR_Param;  // Pointer to the 128 file size/name
  
  PM_INFO("AddFoldersToList\n");
  f_res = f_opendir(&dp,currpath);
  if(f_res != FR_OK) {
    f_closedir(&dp);
    return (0);
  }

  for (;;) {
    f_res = f_readdir(&dp,&fno);    
    if(f_res != FR_OK || fno.fname[0] == 0) break;
    if(fno.fattrib & FAT_DIR) 
      { // copy directories names
       if (strlen(fno.fname)>13) continue;     
       PM_INFO("Found dir: %s\n",fno.fname);
       strcpy((*DIRLIST)[foldercnt].name,fno.fname);
       (*DIRLIST)[foldercnt].size=0xFFFE;  // Folder name
       if (foldercnt++==MAX_DISKIMAGES) break;
      } else continue;    // Skip files
  }
  f_closedir(&dp);

return foldercnt;
}

// Build a list of images
// dtype: 0,Floppy 1,HDD 2, ROM
uint8_t ListImages(uint8_t dtype,uint8_t *ImageNb)
  {
    uint8_t imagecnt;
    uint16_t IMGSize;
    FRESULT fr; /* Return value */
//    char *DSKLIST=(char *)&PCCR_Param;  // DISK Size/Name placed in the PCCR_Param Zone
    FSizeName_t (*DSKLIST)[MAX_DISKIMAGES]=(FSizeName_t (*)[128])&PCCR_Param;  // Pointer to the 128 file size/name

  //  char const *p_dir;
    if (!PM_EnableSD()) return 0;  // SD Not enabled > No Disk to list
    DIR dj;      /* Directory object */
    FILINFO fno; /* File information */
    memset(&dj, 0, sizeof dj);
    memset(&fno, 0, sizeof fno);
    uint8_t LDIError;

    switch(dtype) {
      case 0:  // Get Floppy image list 
        PM_INFO("Nb of Floppy: %d\n",PC_FloppyNB);
        PM_INFO("FDD Images: %s\n", fdd_dir);

// Insert folder list
        imagecnt=AddFoldersToList((char *)fdd_dir);
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
        fr = f_findfirst(&dj, &fno, fdd_dir, "*.img");
        LDIError=CMDERR_NOFDDDIR;
        break;
      case 1:  // Get HDD image list 
        PM_INFO("Nb of HDD: %d\n",PC_DiskNB);
        PM_INFO("HDD Images: %s\n", hdd_dir);

// Insert folder list
        imagecnt=AddFoldersToList((char *)hdd_dir);
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
        fr = f_findfirst(&dj, &fno, hdd_dir, "*.img");
        LDIError=CMDERR_NOHDDDIR;
        break;
      case 2: // Get ROM image List     
        PM_INFO("ROM Images: %s\n", rom_dir);
        fr = f_findfirst(&dj, &fno, hdd_dir, "*.bin");
        LDIError=CMDERR_NOROMDIR;        
      }

    if (FR_OK != fr) {
//        PM_INFO("f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
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
            (*DSKLIST)[imagecnt].size=0xFFFF;
//            *(DSKLIST++)=0xFF;
//            *(DSKLIST++)=0xFF;  // Put 0xFFFF for Error
           } 
           else 
           {
            fno.fname[fnamelength-4]=0;  // Remove extension
            (*DSKLIST)[imagecnt].size=IMGSize;           
//            *(DSKLIST++)=(uint8_t)IMGSize;
//            *(DSKLIST++)=(uint8_t)(IMGSize>>8);  // Store the disk size in MB
           }
        strcpy((*DSKLIST)[imagecnt].name,fno.fname);
//           strcpy(DSKLIST,fno.fname);
//        DSKLIST+=14; // Move to the next entry
        
        if (imagecnt++==MAX_DISKIMAGES) return MAX_DISKIMAGES;  // Increment the number of Disk image, no more than 100
        }  // if (validimg)
        fr = f_findnext(&dj, &fno); /* Search for next item */
    }
    f_closedir(&dj);

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
        PM_INFO("Error: Image already exist\n");
        return CMDERR_IMGEXIST;
	  case FR_NO_PATH:
        PM_INFO("Error: Folder does not exist\n");
        return CMDERR_NOHDDDIR;
	  case FR_NO_FILE:  /* The Disk image is created here */
        PM_INFO("No file, Create it\n");

	      fr = f_open(&img, NewFilename, FA_OPEN_ALWAYS | FA_READ);
	      if (FR_OK != fr) 
          { 
           PM_INFO("Fail to create: %d\n",fr);       
           return CMDERR_DISK;
          }

 	     fr = f_write(&img,(void *) &newmbr,512,&bw);   
	     if (FR_OK != fr) 
	        {  
           PM_INFO("f_write failed : %d\n",fr);
           return CMDERR_WRITE;
	        }

       //Clean the first Mega
       for (int i=0;i<2048*2-1;i++)
         {
 	       fr = f_write(&img,(void *) &NullSect,512,&bw);   
	       if (FR_OK != fr) 
	         {
            PM_INFO("f_write failed : %d\n",fr);
            return CMDERR_WRITE;
	         }
         }

	     f_lseek(&img,NewSize-512);
 	     fr = f_write(&img,(void *) &NullSect,512,&bw);   
	     if (FR_OK != fr) 
	        {   
           PM_INFO("f_write failed : %d\n",fr);
           return CMDERR_WRITE;
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


uint8_t PM_LoadConfig()
{
  char cfgn[]="0:/PICOMEM.CFG";
  char cfg2n[]="0:/config.txt";
  char buf[CONFIG_LINE_BUFFER_SIZE];
  char imgname[14];
  bool BIOS_Changed=false;
  FRESULT fr; /* Return value */
  FIL cfg;
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
    	fr = f_read(&cfg,(void *) &PM_DP_RAM[PMRAM_CFG_Offset],512,&bw);
	    if (FR_OK != fr)
	       { 
           PM_INFO("picomem.cfg read Fail : %d \n",fr);
           f_close(&cfg);
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
      PM_INFO("Can't Open config.txt %d\n",fr);     
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
                PM_INFO("> BIOS %x",res);
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
                PC_DB_Start=res+PM_DB_Offset;
                PM_INFO(" - Done\n",res);

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
     //     DelPortType(DEV_PM);             // Remove the PicoMEM Ports
        }
        if (strstr(buf, "PREBOOT")) {
          PM_Config->PREBOOT=true;
          PM_INFO("> PREBOOT On\n");
        }
        if (strstr(buf, "BIOSBOOT")) {  // Redirect also the BIOS Boot (BASIC) Not implemented for the moment
          PM_Config->BIOSBOOT=true;
          PM_INFO("> BIOSBOOT On\n");
        }
        if (strstr(buf, "RTCBIOS")) {   // Redefine the BIOS Base address
          uint32_t res;
          if (read_hex_from_config_line(buf,&res))
          {
           if ((res>=0xC000)&&(res<0xF000))
              {
               PM_INFO("> RTCBIOS\n");

               uint32_t B_addr=((uint32_t) PM_BIOSADDRESS<<4)+(16+8)*1024; // end of the PicoMEM BIOS
               
               // Configure the BIOS ROM Address
               SetMEMType(B_addr,MEM_BIOS_EXT,MEM_S_16k);
               SetMEMIndex(B_addr,MEM_BIOS_EXT,MEM_S_16k);
               RAM_Offset_Table[MEM_BIOS_EXT]=&PMBIOS[-B_addr];
               PM_INFO(" - Done\n",res);

              } else if (res==0) {   // Picomem in no RAM/ROM Mode
               PM_INFO("> BIOS OFF !\n");
               dev_memorytype_remove(MEM_BIOS);
               dev_memorytype_remove(MEM_DISK);
              }
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
     //     DelPortType(DEV_PM);             // Remove the PicoMEM Ports
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

uint8_t PM_SaveConfig()
{
  char cfgn[]="0:/PICOMEM.CFG";  
  FRESULT fr; /* Return value */
  FIL cfg;
  uint bw;

	fr = f_open(&cfg, cfgn, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
	if (FR_OK != fr) 
     { 
       PM_INFO("f_open failed %d\n",fr);
       return 1; // Return error 1
     }
	fr = f_write(&cfg,(void *) &PM_DP_RAM[PMRAM_CFG_Offset],512,&bw);
	if (FR_OK != fr) 
	   {  
      PM_INFO("f_write failed : %d \n",fr);    
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

	case DEV_USB :
		return usb_disk_status();
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

	case DEV_USB :
		  return usb_disk_initialize();
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
		return sd_disk_read(buff, sector, count);;

	case DEV_USB :
		return usb_disk_read(buff, sector, count);
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
/*    dev_audiomix_pause()
    DRESULT res=sd_disk_write(buff, sector, count);
    dev_audiomix_resume()    
*/    
		return sd_disk_write(buff, sector, count);

	case DEV_USB :
		return usb_disk_write(buff, sector, count);
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

	case DEV_USB :
		return usb_disk_ioctl(cmd, buff);
	}

	return RES_PARERR;
}
