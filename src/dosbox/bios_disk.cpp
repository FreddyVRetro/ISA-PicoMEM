/*
 *  Copyright (C) 2002-2010  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. 
 * If not, see <https://www.gnu.org/licenses/>.
 */

/* $Id: bios_disk.cpp,v 1.40 2009-08-23 17:24:54 c2woody Exp $ */
// The Original DOSBox code needed lot of change, Reference file stored in /original

#include <stdio.h>
#ifndef PMDOSBOX_H
#include "pmdosbox.h"  // Picomem version of dosbox.h
#endif
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"

#if USE_PSRAM
#if USE_PSRAM_DMA
#include "psram_spi.h"
extern psram_spi_inst_t psram_spi;
#else
#include "../psram_spi2.h"
extern pio_spi_inst_t psram_spi;
#endif
#endif

#include "dev_memory.h"   // Need access to the emulated RAM infos
#include "../pm_pccmd.h"
#include "bios.h"
#include "bios_disk.h"
#include "pm_audio.h"    // For Audio Pause / Resume

//PM File functions:
#include "ff.h"

const diskGeo DiskGeometryList[] = {
	{ 160,  8, 1, 40, 0},
	{ 180,  9, 1, 40, 0},
	{ 200, 10, 1, 40, 0},
	{ 320,  8, 2, 40, 1},
	{ 360,  9, 2, 40, 1},
	{ 400, 10, 2, 40, 1},
	{ 720,  9, 2, 80, 3},
	{1200, 15, 2, 80, 2},
	{1440, 18, 2, 80, 4},
	{2880, 36, 2, 80, 6},
	{0, 0, 0, 0, 0}
};

/*
diskGeo HDDGeometryList[] = {
	{ 160,  8, 1, 40, 0},
	{ 180,  9, 1, 40, 0},
	{ 200, 10, 1, 40, 0},
	{ 320,  8, 2, 40, 1},
	{ 360,  9, 2, 40, 1},
	{ 400, 10, 2, 40, 1},
	{ 720,  9, 2, 80, 3},
	{1200, 15, 2, 80, 2},
	{1440, 18, 2, 80, 4},
	{2880, 36, 2, 80, 6},
	{0, 0, 0, 0, 0}
};
*/

//Bitu call_int13;
//Bitu diskparm0, diskparm1;
static Bit8u last_status;
static Bit8u last_drive;
Bit16u imgDTASeg;
RealPt imgDTAPtr;
//DOS_DTA *imgDTA;
bool killRead;
//static bool swapping_requested;

// PM void CMOS_SetRegister(Bitu regNr, Bit8u val); //For setting equipment word
uint32_t PC_DB_Start;  // Start of the Shared disk buffer in the Host (PC) RAM

/* 2 floppys and 2 harddrives, max  PM: 4 */ 
imageDisk *imageDiskList[MAX_DISK_IMAGES];
//imageDisk *diskSwap[MAX_SWAPPABLE_DISKS];
Bits swapPosition;

// PM : To be Declared
void reg_SetCF()
{
reg_flagl=reg_flagl|1;  // Set Carry (Bit 0)
}

void reg_ClearCF()
{
reg_flagl=reg_flagl&0xFE;  // Clear Carry (Bit 0)
}

void updateDPT(void) {
	Bit32u tmpheads, tmpcyl, tmpsect, tmpsize;
	if(imageDiskList[2] != NULL) {  //HDD0
		imageDiskList[2]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
        PM_DP_RAM[OFFS_DPT0]=(Bit8u)tmpcyl;
        PM_DP_RAM[OFFS_DPT0+0x1]=(Bit8u)(tmpcyl>>8);		
        PM_DP_RAM[OFFS_DPT0+0x2]=(Bit8u)tmpheads;
        PM_DP_RAM[OFFS_DPT0+0x3]=0;
        PM_DP_RAM[OFFS_DPT0+0x4]=0;
        PM_DP_RAM[OFFS_DPT0+0x5]=0xFF;
        PM_DP_RAM[OFFS_DPT0+0x6]=0xFF;
        PM_DP_RAM[OFFS_DPT0+0x7]=0;
        PM_DP_RAM[OFFS_DPT0+0x8]=(0xc0 | (((imageDiskList[2]->heads) > 8) << 3));
        PM_DP_RAM[OFFS_DPT0+0x9]=0;
        PM_DP_RAM[OFFS_DPT0+0xA]=0;
        PM_DP_RAM[OFFS_DPT0+0xB]=0;
        PM_DP_RAM[OFFS_DPT0+0xC]=0;
        PM_DP_RAM[OFFS_DPT0+0xD]=0;
        PM_DP_RAM[OFFS_DPT0+0xE]=(Bit8u)tmpsect;
//		phys_writew(dp0physaddr,(Bit16u)tmpcyl);
//		phys_writeb(dp0physaddr+0x2,(Bit8u)tmpheads);
//		phys_writew(dp0physaddr+0x3,0);
//		phys_writew(dp0physaddr+0x5,(Bit16u)-1);
//		phys_writeb(dp0physaddr+0x7,0);
//		phys_writeb(dp0physaddr+0x8,(0xc0 | (((imageDiskList[2]->heads) > 8) << 3)));
//		phys_writeb(dp0physaddr+0x9,0);
//		phys_writeb(dp0physaddr+0xa,0);
//		phys_writeb(dp0physaddr+0xb,0);
//		phys_writew(dp0physaddr+0xc,(Bit16u)tmpcyl);
//		phys_writeb(dp0physaddr+0xe,(Bit8u)tmpsect);
	}
	if(imageDiskList[3] != NULL) {  //HDD1
		imageDiskList[3]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
        PM_DP_RAM[OFFS_DPT1+0x0]=(Bit8u)tmpcyl;
        PM_DP_RAM[OFFS_DPT1+0x1]=(Bit8u)(tmpcyl>>8);
        PM_DP_RAM[OFFS_DPT1+0x2]=(Bit8u)tmpheads;
        PM_DP_RAM[OFFS_DPT1+0x3]=0;
        PM_DP_RAM[OFFS_DPT1+0x4]=0;
        PM_DP_RAM[OFFS_DPT1+0x5]=0xFF;
        PM_DP_RAM[OFFS_DPT1+0x6]=0xFF;
        PM_DP_RAM[OFFS_DPT1+0x7]=0;
        PM_DP_RAM[OFFS_DPT1+0x8]=(0xc0 | (((imageDiskList[2]->heads) > 8) << 3));
        PM_DP_RAM[OFFS_DPT1+0x9]=0;
        PM_DP_RAM[OFFS_DPT1+0xA]=0;
        PM_DP_RAM[OFFS_DPT1+0xB]=0;
        PM_DP_RAM[OFFS_DPT1+0xC]=0;
        PM_DP_RAM[OFFS_DPT1+0xD]=0;
        PM_DP_RAM[OFFS_DPT1+0xE]=(Bit8u)tmpsect;
	}
}

/*	//Not used in PicoMEM
void swapInDisks(void) {
	bool allNull = true;
	Bits diskcount = 0;
	Bits swapPos = swapPosition;
	int i;

	// Check to make sure there's atleast one setup image
	for(i=0;i<MAX_SWAPPABLE_DISKS;i++) {
		if(diskSwap[i]!=NULL) {
			allNull = false;
			break;
		}
	}

	// No disks setup... fail
	if (allNull) return;

	// If only one disk is loaded, this loop will load the same disk in dive A and drive B
	while(diskcount<2) {
		if(diskSwap[swapPos] != NULL) {
			//LOG_MSG("Loaded disk %d from swaplist position %d ", diskcount, swapPos);
			imageDiskList[diskcount] = diskSwap[swapPos];
			diskcount++;
		}
		swapPos++;
		if(swapPos>=MAX_SWAPPABLE_DISKS) swapPos=0;
	}
}
*/

/*	//Not used in PicoMEM
bool getSwapRequest(void) {
	bool sreq=swapping_requested;
	swapping_requested = false;
	return sreq;
}


/*	//Not used in PicoMEM
void swapInNextDisk(bool pressed) {
	if (!pressed)
		return;
//PM	DriveManager::CycleAllDisks();
//   Hack/feature: rescan all disks as well
//   LOG_MSG("Diskcaching reset for normal mounted drives.");
//	 for(Bitu i=0;i<DOS_DRIVES;i++) {
//		if (Drives[i]) Drives[i]->EmptyCache();
//	}
	swapPosition++;
	if(diskSwap[swapPosition] == NULL) swapPosition = 0;
	swapInDisks();
	swapping_requested = true;
}
*/

Bit32u imageDisk::CHS_to_LBA(Bit32u head,Bit32u cylinder,Bit32u sector) {
	Bit32u sectnum;
//PM_INFO("CHS to LBA C%d H%d S%d\n",cylinder,head,sector);
//PM_INFO("Disk Geometry C%d H%d S%d\n",cylinders,heads,sectors);

	return ( (cylinder * heads + head) * sectors ) + sector - 1L;
}

Bit8u imageDisk::Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data) {
	Bit32u sectnum;
//PM_INFO("Disk Geometry C%d H%d S%d\n",cylinders,heads,sectors);

    PM_INFO("ReadSector C%d H%d S%d\n",cylinder,head,sector);

	sectnum = ( (cylinder * heads + head) * sectors ) + sector - 1L;

	return Read_AbsoluteSector(sectnum, data);
}

Bit8u imageDisk::Read_AbsoluteSector(Bit32u sectnum, void * data) {
	Bit32u 	bytenum = sectnum * sector_size;
    uint byteread;
    FRESULT fr;
	f_lseek(diskimg,bytenum);
	fr = f_read(diskimg,data,sector_size,&byteread);
	if (FR_OK != fr) 
	{ 
	  PM_ERROR("Fail: %d \n",fr);
	  return 0x04;  //04h : sector not found/read error (BIOS error code)
	}
//PM_INFO("Ok \n");

//	fseek(diskimg,bytenum,SEEK_SET);
//	fread(data, 1, sector_size, diskimg);
	return 0x00;
}

Bit8u imageDisk::Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data) {
	Bit32u sectnum;

	sectnum = ( (cylinder * heads + head) * sectors ) + sector - 1L;

	return Write_AbsoluteSector(sectnum, data);
}


Bit8u imageDisk::Write_AbsoluteSector(Bit32u sectnum, void *data) {
	Bit32u 	bytenum = sectnum * sector_size;
    uint bytewrite;
    FRESULT fr;

//PM_INFO("fwrite: pos %d - ",bytenum);
	f_lseek(diskimg,bytenum);
	fr = f_write(diskimg,data,sector_size,&bytewrite);
	if (FR_OK != fr) 
	{ 
	  PM_ERROR("Fail: %d \n",fr);
	  return 0x04;  //04h : sector not found/write error
	}

	//LOG_MSG("Writing sectors to %ld at bytenum %d", sectnum, bytenum);

	return 0x00;
}

imageDisk::imageDisk(FIL *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk) {
	heads = 0;
	cylinders = 0;
	sectors = 0;
	sector_size = 512;  // PM To put as constant
	diskimg = imgFile;
	
//	memset(diskname,0,512);
//	if(strlen((const char *)imgName) > 511) {
//		memcpy(diskname, imgName, 511);
//	} else {
//		strcpy((char *)diskname, (const char *)imgName);
//	}
#if PM_PRINTF	
	PM_INFO("Mount New imageDisk - Size:%d\n",imgSizeK);
#endif	
	active = false;
	hardDrive = isHardDisk;
	if(!isHardDisk) {		
  		Bit8u i=0;
		bool founddisk = false;
		while (DiskGeometryList[i].ksize!=0x0) {
			if ((DiskGeometryList[i].ksize==imgSizeK) ||
				(DiskGeometryList[i].ksize+1==imgSizeK)) 
				{

				if (DiskGeometryList[i].ksize!=imgSizeK)			
	                PM_WARNING("Floppy Image file with additional data, might not load!\n");

				founddisk = true;
				active = true;
				floppytype = i;
				heads = DiskGeometryList[i].headscyl;
				cylinders = DiskGeometryList[i].cylcount;
				sectors = DiskGeometryList[i].secttrack;
				break;
			    }
			i++;
		}
		if(!founddisk) 
		   {				
            PM_ERROR("Floppy format not found\n");
			active = false;
		   } 
		else 
		  {
			//PM Bit16u equipment=mem_readw(BIOS_CONFIGURATION);
			//if(equipment&1) {
			//	Bitu numofdisks = (equipment>>6)&3;
			//	numofdisks++;
			//	if(numofdisks > 1) numofdisks=1;//max 2 floppies at the moment
			//	equipment&=~0x00C0;
			//	equipment|=(numofdisks<<6);
			//} else equipment|=1;
			//PM mem_writew(BIOS_CONFIGURATION,equipment);
			//PM CMOS_SetRegister(0x14, (Bit8u)(equipment&0xff));
		  }
	}
}
//PM : DOSXOB X Code for > 504MB Disk
void imageDisk::Set_Geometry(Bit32u setHeads, Bit32u setCyl, Bit32u setSect, Bit32u setSectSize) {
    Bitu bigdisk_shift = 0;

  //  if(setCyl > 16384) {}// LOG_MSG("Warning: This disk image is too big.");
  //    else 
    if(setCyl > 8192) bigdisk_shift = 4;
      else if(setCyl > 4096) bigdisk_shift = 3; // 128 "Heads" > Maximum of 4.2Gb
      else if(setCyl > 2048) bigdisk_shift = 2; // 64  "Heads"
      else if(setCyl > 1024) bigdisk_shift = 1; // 32  "Heads"

    heads = setHeads << bigdisk_shift;
    cylinders = setCyl >> bigdisk_shift;
    sectors = setSect;
    sector_size = setSectSize;
    active = true;

	PM_INFO("Disk Set Geometry C%d H%d S%d\n",cylinders,heads,sectors);
    updateDPT(); // Added in PicoMEM
}

void imageDisk::Get_Geometry(Bit32u * getHeads, Bit32u *getCyl, Bit32u *getSect, Bit32u *getSectSize) {
	*getHeads = heads;
	*getCyl = cylinders;
	*getSect = sectors;
	*getSectSize = sector_size;
}

Bit8u imageDisk::GetBiosType(void) {
	if(!hardDrive) {
		return (Bit8u)DiskGeometryList[floppytype].biosval;
	} else return 0;
}

Bit32u imageDisk::getSectSize(void) {
	return sector_size;
}

static Bitu GetDosDriveNumber(Bitu biosNum) {
	switch(biosNum) {
		case 0x0:
			return 0x0;
		case 0x1:
			return 0x1;
		case 0x80:
			return 0x2;
		case 0x81:
			return 0x3;
		case 0x82:
			return 0x4;
		case 0x83:
			return 0x5;
		default:
			return 0x7f;
	}
}

static bool driveInactive(Bitu driveNum) {
	if(driveNum>=(2 + MAX_HDD_IMAGES)) {
	//	PM_INFO("Disk %d non-existant", driveNum);

		PM_INFO("di1 %d",driveNum);
		last_status = 0x01;
		reg_SetCF();
		return true;
	}
	if(imageDiskList[driveNum] == NULL) {
	//	PM_INFO("Disk %d not active", driveNum);
		last_status = 0x01;

		PM_INFO("di2 %d",driveNum);	
		reg_SetCF();
		return true;
	}
	if(!imageDiskList[driveNum]->active) {
	//	PM_INFO("Disk %d not active", driveNum);
		last_status = 0x01;
						
		PM_INFO("di3 %d",driveNum);
		reg_SetCF();
		return true;
	}
	return false;
}


Bitu INT13_DiskHandler(void) {
    Bit32u sect_lba;
	Bit16u segat, bufptr;
	Bit8u sectbuf[512];
	Bitu  drivenum;
//    uint8_t sn;	
	Bitu  i,t;
    uint32_t BOffset;	
    uint32_t WOffset;

	last_drive = reg_dl;
	drivenum = GetDosDriveNumber(reg_dl);
#ifdef HDD_DISPLAY
	PM_INFO("Int13h, drive: %x Fnt: %x AL: %x \n",reg_dl,reg_ah,reg_al);
#endif
	//LOG_MSG("INT13: Function %x called on drive %x (dos drive %d)", reg_ah,  reg_dl, drivenum);
	
	switch(reg_ah) {
	case 0x0: /* Reset disk */
/*INT 13 - DISK - RESET DISK SYSTEM
	AH = 00h
	DL = drive (if bit 7 is set both hard disks and floppy disks reset)
Return: AH = status (see #00234)
	CF clear if successful (returned AH=00h)
	CF set on error	*/
		{
			/* if there aren't any diskimages (so only localdrives and virtual drives)
			 * always succeed on reset disk. If there are diskimages then and only then
			 * do real checks
			 */
	//		if (any_images && driveInactive(drivenum)) {
				/* driveInactive sets carry flag if the specified drive is not available */
	//			if ((machine==MCH_CGA) || (machine==MCH_PCJR)) {
					/* those bioses call floppy drive reset for invalid drive values */
	//				if (((imageDiskList[0]) && (imageDiskList[0]->active)) || ((imageDiskList[1]) && (imageDiskList[1]->active))) {
	//					last_status = 0x00;
	//					CALLBACK_SCF(false);
	//				}
	//			}
	//			return CBRET_NONE;
	//		}
		 last_status = 0x00;
		 reg_ClearCF();
		}
        break;
/*INT 13 - DISK - GET STATUS OF LAST OPERATION
	AH = 01h
	DL = drive (bit 7 set for hard disk)
Return: CF clear if successful (returned status 00h)
	CF set on error
	AH = status of previous operation (see #00234)
Only Last Status 0,1 and 7 are used */
	case 0x1: /* Get status of last operation */
		if(last_status != 0x00) {
			reg_ah = last_status;
			reg_SetCF();
		} else {
			reg_ah = 0x00;
			reg_ClearCF();
		}
		break;
/*INT 13 - DISK - READ SECTOR(S) INTO MEMORY
	AH = 02h
	AL = number of sectors to read (must be nonzero)
	CH = low eight bits of cylinder number
	CL = sector number 1-63 (bits 0-5)
	     high two bits of cylinder (bits 6-7, hard disk only)
	DH = head number
	DL = drive number (bit 7 set for hard disk)
	ES:BX -> data buffer */
	case 0x2: /* Read sectors */
		if (reg_al==0) {
			reg_ah = 0x01;
			reg_SetCF();
			return CBRET_NONE;
		}
		if (driveInactive(drivenum)) {		
			PM_INFO("Drive inactive");
			reg_ah = 0xff;
			reg_SetCF();
			return CBRET_NONE;
		}
	
		uint16_t mt_beg;
		uint16_t mt_end;
		uint32_t PCBuffer_Start;
		uint32_t PCBuffer_End;

        sect_lba = imageDiskList[drivenum]->CHS_to_LBA((Bit32u)reg_dh, (Bit32u)(reg_ch | ((reg_cl & 0xc0)<< 2)), (Bit32u)((reg_cl & 63)));

        PCBuffer_Start=(uint32_t)(reg_es<<4)+reg_bx; // PC_Buffer_Start is the current @ of the application disk buffer

#ifdef HDD_DISPLAY
#if PM_PRINTF
		PCBuffer_End=(PCBuffer_Start+reg_al*512)-1;  // PC_Buffer_End is the last @ of the application disk buffer
		mt_beg = GetMEMType(PCBuffer_Start);
		mt_end = GetMEMType(PCBuffer_End);   //-1 as we need the end, not the @ just after the end

		PM_INFO("C%d H%d S%d > LBA: %d ",(Bit32u)(reg_ch | ((reg_cl & 0xc0)<< 2)), (Bit32u)reg_dh, (Bit32u)((reg_cl & 63)),sect_lba);
		PM_INFO("B@ %x:%x>%x,T%x %x : ",reg_es,reg_bx,PCBuffer_Start,mt_beg,mt_end);
#endif
#endif

        BOffset=0;
  	    PCBuffer_End=PCBuffer_Start+512-1; //Current 512b PC Buffer End (-1 as we need the end, not the @ just after the end)

// **** Disk Read Loop ****
		for(uint8_t sn=0;sn<reg_al;sn++) {
#if PM_PRINTF
    //      PM_INFO(" O:%d ",BOffset);
#endif
// Current 512b Buffer Memory type (Begining and End)
 		  mt_beg = GetMEMType(PCBuffer_Start);
		  mt_end = GetMEMType(PCBuffer_End);
		  if (mt_beg!=mt_end)
		     { 
			  PM_ERROR("! Diff Mem Type %d %d > ",mt_beg,mt_end);
				if ((mt_beg<=MEM_RAM) && (mt_end<=MEM_RAM))
			      {						// MEM Begining and End not in PSRAM
			        mt_beg=MEM_NULL;  	// Force Standard BIOS Copy
				  }
				 else
			      {					// MEM Begining or End in PSRAM/EMS
			        mt_beg=0xFF;    // Force Slow MEM Copy with uSD Disable
				  }			
#ifdef HDD_DISPLAY
  		     PM_INFO("%d > ",mt_beg);		  
#endif			 
			 }

#ifdef PIMORONI_PICO_PLUS2_RP2350
if (mt_beg!=MEM_RAM) mt_beg=MEM_NULL;  // Can go back to use Copy via the CPU
#endif
/*
#if USE_PSRAM_DMA
if (mt_beg==MEM_PSRAM|mt_beg==MEM_EMS) 
  {
#if PM_PRINTF
   putchar('!');	 
#endif   
   mt_beg=0xFF;   // Do "Slow copy" with PSRAM Disabled
  }
#endif
*/
		  switch(mt_beg)
		    {
		    case MEM_NULL:  // ** Disk Read to the PC Ram (Disk Buffer)
#ifdef HDD_DISPLAY
#if PM_PRINTF
			    putchar('m');
#endif
#endif
//                dev_audiomix_pause();     // Need to pause audio during uSD Access  
		  		last_status = imageDiskList[drivenum]->Read_AbsoluteSector(sect_lba+sn, &PM_PTR_DISKBUFFER[BOffset]);
//                dev_audiomix_resume(); 			
		  		if ((last_status != 0x00) || (killRead)) 
				    {				
				 	 PM_ERROR("HDD Read Error");				 
					 killRead = false;
					 reg_ah = last_status;  // 4 if CF not accessible
					 reg_SetCF();
					 return CBRET_NONE;
		    		}	
                // ** Ask the BIOS to Copy the data to the application buffer (in Background)
		  		PC_WaitCMDCompleted();
                PC_MemCopyW_512b(PC_DB_Start+BOffset,PCBuffer_Start);
          		if (PCCR_PCSTATE==PCC_PCS_RESET) return CBRET_NONE;		// Quit if Reset asked
			   break;
		    case MEM_RAM:  // ** Disk Read to the PC RAM emulated in the Pico SRAM
#ifdef HDD_DISPLAY
#if PM_PRINTF
			    putchar('s');
#endif
#endif
//                dev_audiomix_pause();     // Need to pause audio during uSD Access  				
		  		last_status = imageDiskList[drivenum]->Read_AbsoluteSector(sect_lba+sn, &PM_Memory[(PCBuffer_Start - RAM_InitialAddress)]);
//                dev_audiomix_resume(); 					
		  		if ((last_status != 0x00) || (killRead)) 
				    {		
				 	 PM_ERROR("HDD Read Error");				 
					 killRead = false;
					 reg_ah = last_status;  // 4 if CF not accessible
					 reg_SetCF();
					 return CBRET_NONE;
		    		}
		       break;
		     case MEM_PSRAM:   // ** Disk Read to the PC RAM emulated in the Pico PSRAM
		     case MEM_EMS:     // ** Disk Read to the PC EMS emulated in the Pico PSRAM
#if USE_PSRAM
#if PM_PRINTF
			    putchar('p');
#endif				
//                dev_audiomix_pause();     // Need to pause audio during uSD Access  
		  		last_status = imageDiskList[drivenum]->Read_AbsoluteSector(sect_lba+sn, &PM_PTR_DISKBUFFER[BOffset]);
		  		if ((last_status != 0x00) || (killRead)) 
				    {
				 	 PM_ERROR("HDD Read Error");				 
					 killRead = false;
					 reg_ah = last_status;  // 4 if CF not accessible
					 reg_SetCF();
					 return CBRET_NONE;
		    		}

				// Copy to PSRAM
				PM_EnablePSRAM();
                // Compute the address to use in PSRAM
				uint32_t PSRAM_Addr;  			
                if (mt_beg==MEM_EMS) PSRAM_Addr=EMS_Base[(PCBuffer_Start>>14)&0x03]+(PCBuffer_Start & 0x3FFF);	
				   else PSRAM_Addr=PCBuffer_Start;
				// Copy the data to PSRAM
				if (PSRAM_Addr&0x03)
				 {  // Not 32Bit aligned, write in 8Bit to avoid problem
				 
#if PM_PRINTF
			    putchar('8');
#endif				 
			  	   for (int i=0;i<512;i++)	// ** Copy the Disk Data to the PSRAM  (Need to find a faster way)
				    { 
					psram_write8(&psram_spi,PSRAM_Addr+i,PM_PTR_DISKBUFFER[BOffset+i]);
				    }
				 }
				 else
				 {  // If 32Bit aligned, copy in 32Bit
			  	   for (int i=0;i<128;i++)	// ** Copy the Disk Data to the PSRAM  (Need to find a faster way)
				    { 
					uint32_t tmp=((uint32_t)PM_PTR_DISKBUFFER[BOffset+(i<<2)])+((uint32_t)PM_PTR_DISKBUFFER[BOffset+(i<<2)+1]<<8)+((uint32_t)PM_PTR_DISKBUFFER[BOffset+(i<<2)+2]<<16)+((uint32_t)PM_PTR_DISKBUFFER[BOffset+(i<<2)+3]<<24);
					psram_write32(&psram_spi,PSRAM_Addr+(i<<2),tmp);
				    }
				 }

				PM_EnableSD();
//                dev_audiomix_resume();				
		       break;
#endif			   
			 case 0xFF:  // "Slow" case : Copy via the PC CPU with PSRAM re enabled (when mixed RAM)
#if PM_PRINTF			 
				putchar('c');
#endif				
//                dev_audiomix_pause();     // Need to pause audio during uSD Access  
		  		last_status = imageDiskList[drivenum]->Read_AbsoluteSector(sect_lba+sn, &PM_PTR_DISKBUFFER[BOffset]);
//                dev_audiomix_resume();
		  		if ((last_status != 0x00) || (killRead)) 
				    {
				 	 PM_ERROR("HDD Read Error");
					 killRead = false;
					 reg_ah = last_status;  // 4 if CF not accessible
					 reg_SetCF();
					 return CBRET_NONE;
		    		}
				PM_EnablePSRAM();
		        PC_WaitCMDCompleted();
				PC_MemCopyW_512b(PC_DB_Start+BOffset,PCBuffer_Start);
		        PC_WaitCMDCompleted();
                if (PCCR_PCSTATE==PCC_PCS_RESET) return CBRET_NONE;						
				PM_EnableSD();				
			   break;
		    } //End switch(mt_beg)

         BOffset = (BOffset==0) ? 512 : 0;
         PCBuffer_Start+=512;
		 PCBuffer_End+=512;
#if PM_PRINTF
//          PM_INFO("E ");		  
#endif
		 }  // End For (Read Loop)

		PC_WaitCMDCompleted();
        if (PCCR_PCSTATE==PCC_PCS_RESET) return CBRET_NONE;
#ifdef HDD_DISPLAY        
		PM_INFO("\n");
#endif

		reg_ah = 0x00;
		reg_ClearCF();
		break;

/* INT 13 - DISK - WRITE DISK SECTOR(S)
	AH = 03h
	AL = number of sectors to write (must be nonzero)
	CH = low eight bits of cylinder number
	CL = sector number 1-63 (bits 0-5)
	     high two bits of cylinder (bits 6-7, hard disk only)
	DH = head number
	DL = drive number (bit 7 set for hard disk)
	ES:BX -> data buffer */
	case 0x3: /* Write sectors */

		if(driveInactive(drivenum)) {
			reg_ah = 0xff;
			reg_SetCF();
			return CBRET_NONE;
        }

        PCBuffer_Start=(uint32_t)(reg_es<<4)+reg_bx;  // Segment<<4+Offset
#if PM_PRINTF    
		PCBuffer_End=(PCBuffer_Start+reg_al*512)-1;	  // Add Buffer NB*5412 -1
		mt_beg = GetMEMType(PCBuffer_Start);
		mt_end = GetMEMType(PCBuffer_End);
	    PM_INFO("B@ %x:%x>%x Type %x, %x \n",reg_es,reg_bx,PCBuffer_Start,mt_beg,mt_end);
#endif
        BOffset=0;
  	    PCBuffer_End=PCBuffer_Start+512-1; //Current 512b PC Buffer End (-1 as we need the end, not the @ just after the end)

        sect_lba = imageDiskList[drivenum]->CHS_to_LBA((Bit32u)reg_dh, (Bit32u)(reg_ch | ((reg_cl & 0xc0)<< 2)), (Bit32u)((reg_cl & 63)));

	    PM_INFO("C%d H%d S%d > LBA: %d ",(Bit32u)(reg_ch | ((reg_cl & 0xc0)<< 2)), (Bit32u)reg_dh, (Bit32u)((reg_cl & 63)),sect_lba);

// ** The PC Copy the first Sector to the Buffer
        PM_EnablePSRAM();  // First sector Copy is done with no SD Access at the same time
        PC_WaitCMDCompleted();  // Wait until the PC finish the sector copy
		PC_MemCopyW_512b(PCBuffer_Start,PC_DB_Start+BOffset);
        PC_WaitCMDCompleted();  // Wait until the PC finish the sector copy
	    if (PCCR_PCSTATE==PCC_PCS_RESET) return CBRET_NONE;
		PM_EnableSD();

// **** Disk Write Loop ****
		for(uint8_t sn=0;sn<reg_al;sn++) 
		{
		   
		PM_INFO(".");

		 WOffset=BOffset;		// Save the Buffer Offset : The sector to copy to the Disk is the previous sector

// ** Prepare the next buffer Read (Even if there is not a next sector)
// Next 512b Buffer Memory type (Begining and End)
         BOffset = (BOffset==0) ? 512 : 0;
         PCBuffer_Start+=512;
		 PCBuffer_End+=512;
 		 mt_beg = GetMEMType(PCBuffer_Start);	// 0 for PC RAM, 1 for SRAM emulated RAM
		 mt_end = GetMEMType(PCBuffer_End);

		  if (mt_beg!=mt_end)
		     { 			
			  PM_INFO(" Diff T %d %d > ",mt_beg,mt_end);

				if ((mt_beg<=MEM_RAM) && (mt_end<=MEM_RAM))
			      {						// MEM Begining and End not in PSRAM
			        mt_beg=MEM_NULL;  	// Force Standard BIOS Copy
				  }
				 else
			      {					// MEM Begining or End in PSRAM/EMS
			        mt_beg=0xFF;    // Force Slow MEM Copy with uSD Disable
				  }		  
			
			  PM_INFO("%d > ",mt_beg);
			 }

	     PC_WaitCMDCompleted();  // Wait until the PC finish a sector copy
		 if (PCCR_PCSTATE==PCC_PCS_RESET) return CBRET_NONE;

#ifdef PIMORONI_PICO_PLUS2_RP2350
if (mt_beg!=MEM_RAM) mt_beg=MEM_NULL;  // Can go back to use Copy via the CPU
#endif

	     // ** The PC copy the NEXT sector in advance to the Buffer (If there is more to copy)
         if (sn!=(reg_al-1)) // If processing the last sector, no need to copy in advance.
             {	
               PC_WaitCMDCompleted();  // Wait until the PC finish the sector copy
			   if (mt_beg<=MEM_RAM) PC_MemCopyW_512b(PCBuffer_Start,PC_DB_Start+BOffset);	// PC RAM or 
			      else 
				   {  // Need to re enable the PSRAM and Wait
#if PM_PRINTF
//         			PM_INFO("!%x %x",PCBuffer_Start,PCBuffer_End);
                    PM_INFO("!");
#endif
			        PM_EnablePSRAM();  	// If target is not SRAM Emulated RAM Enable PSRAM
					PC_MemCopyW_512b(PCBuffer_Start,PC_DB_Start+BOffset);	
		            if (PCCR_PCSTATE==PCC_PCS_RESET) return CBRET_NONE;
//#if PM_PRINTF
//         			PM_INFO("w");
//#endif
	                PC_WaitCMDCompleted();  	// Wait until the PC finish a sector copy
					PM_EnableSD();  			// Re enable SD
				   }
       		  if (PCCR_PCSTATE==PCC_PCS_RESET) return CBRET_NONE;
			 }

	// ** Write the sector do the Disk (Previous sector or first if only 1 to write)
		     last_status = imageDiskList[drivenum]->Write_AbsoluteSector(sect_lba+sn, &PM_PTR_DISKBUFFER[WOffset]);
		 if(last_status != 0x00) {
		     PM_ERROR("HDD Write Error");
		     reg_SetCF();
		 	 return CBRET_NONE;
			}
  
        }  // End Sector Write Loop
			
//PM_INFO("EW");
		reg_ah = 0x00;
		reg_ClearCF();
        break;

	case 0x04: /* Verify sectors */
/*INT 13 - DISK - VERIFY DISK SECTOR(S)
	AH = 04h
	AL = number of sectors to verify (must be nonzero)
	CH = low eight bits of cylinder number
	CL = sector number 1-63 (bits 0-5)
	     high two bits of cylinder (bits 6-7, hard disk only)
	DH = head number
	DL = drive number (bit 7 set for hard disk)
	ES:BX -> data buffer (PC,XT,AT with BIOS prior to 1985/11/15)
Return: CF set on error
	CF clear if successful
	AH = status (see #00234)
	AL = number of sectors verified	 */
		if (reg_al==0) {
			reg_ah = 0x01;
			reg_SetCF();
			return CBRET_NONE;
		}
		if(driveInactive(drivenum)) return CBRET_NONE;

		/* TODO: Finish coding this section */
		/*
		segat = SegValue(es);
		bufptr = reg_bx;
		for(i=0;i<reg_al;i++) {
			last_status = imageDiskList[drivenum]->Read_Sector((Bit32u)reg_dh, (Bit32u)(reg_ch | ((reg_cl & 0xc0)<< 2)), (Bit32u)((reg_cl & 63)+i), sectbuf);
			if(last_status != 0x00) {
				LOG_MSG("Error in disk read");
				CALLBACK_SCF(true);
				return CBRET_NONE;
			}
			for(t=0;t<512;t++) {
				real_writeb(segat,bufptr,sectbuf[t]);
				bufptr++;
			}
		}*/
		//Qbix: The following codes don't match my specs. al should be number of sector verified
		//reg_al = 0x10; /* CRC verify failed */
		//reg_al = 0x00; /* CRC verify succeeded */
		reg_ClearCF();
		reg_ah = 0x00;          
		break;
    case 0x05: /* Format track */
    case 0x06: /* Format track set bad sector flags */
    case 0x07: /* Format track set bad sector flags */
        /* ignore it */
        //LOG_MSG("WARNING: Format track ignored\n");
        if (driveInactive(drivenum)) {
            reg_ah = 0xff;
			reg_SetCF();
            return CBRET_NONE;
        }
		reg_ClearCF();
        reg_ah = 0x00;
        break;
	case 0x08: /* Get drive parameters */
/* INT 13 - DISK - GET DRIVE PARAMETERS (PC,XT286,CONV,PS,ESDI,SCSI)
	AH = 08h
	DL = drive (bit 7 set for hard disk)
	ES:DI = 0000h:0000h to guard against BIOS bugs
Return: CF set on error
	    AH = status (07h) (see #00234)
	CF clear if successful
	    AH = 00h
	    AL = 00h on at least some BIOSes
	    BL = drive type (AT/PS2 floppies only) (see #00242)
	    CH = low eight bits of maximum cylinder number
	    CL = maximum sector number (bits 5-0)
		 high two bits of maximum cylinder number (bits 7-6)
	    DH = maximum head number
	    DL = number of drives     !!! PM : Error
	    ES:DI -> drive parameter table (floppies only)  */
		if(driveInactive(drivenum)) {
			last_status = 0x07;
			reg_ah = last_status;
			reg_SetCF();
			return CBRET_NONE;
		}
		PM_INFO("GetType: ");
		reg_ah = 0x00;
		reg_al = 0x00;
		reg_bl = imageDiskList[drivenum]->GetBiosType();
		Bit32u tmpheads, tmpcyl, tmpsect, tmpsize;
		imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
		if (tmpcyl!=0) //PM LOG(LOG_BIOS,LOG_ERROR)("INT13 DrivParm: cylinder count zero!");
		   tmpcyl--;		// cylinder count -> max cylinder
		if (tmpheads!=0) //PM LOG(LOG_BIOS,LOG_ERROR)("INT13 DrivParm: head count zero!");
		   tmpheads--;	// head count -> max head
        if (tmpheads>255) PM_INFO("Error: Head > 255\n");
		reg_ch = (Bit8u)(tmpcyl & 0xff);
		reg_cl = (Bit8u)(((tmpcyl >> 2) & 0xc0) | (tmpsect & 0x3f)); 
		reg_dh = (Bit8u)tmpheads;
		last_status = 0x00;
		if (reg_dl&0x80) {	// harddisks   !!! To correct 
		    reg_dl=New_DiskNB; // Send the total nb of disk (Real +Image)
			/*
			reg_dl = 0;
			if(imageDiskList[2] != NULL) reg_dl++;
			if(imageDiskList[3] != NULL) reg_dl++;
			if(imageDiskList[4] != NULL) reg_dl++;
			if(imageDiskList[5] != NULL) reg_dl++;			
			*/
		} else {		// floppy disks
		    reg_dl=New_FloppyNB; // Send the total nb of disk (Real +Image)	
			/*	
			reg_dl = 0;
			if(imageDiskList[0] != NULL) reg_dl++;
			if(imageDiskList[1] != NULL) reg_dl++;
			*/
		}
        PM_INFO("Type: %d Disk Nb: %d \n",reg_bl,reg_dl);
		PM_INFO("C%d H%d S%d \n",tmpcyl,tmpheads,tmpsect);
		reg_ClearCF();
		break;
	case 0x0C: /* SEEK TO CYLINDER (called by Checkit3 and Northon sysinfo) */
	case 0x10: /* CHECK IF DRIVE READY (called by Northon sysinfo) 	        */
	case 0x11: /* Recalibrate drive */
/* INT 13 - HARD DISK - RECALIBRATE DRIVE
	AH = 11h
	DL = drive number (80h = first, 81h = second hard disk)
Return: CF set on error
	CF clear if successful
	AH = status (see #00234 at AH=01h)	*/
	case 0x12: /* CONTROLLER RAM DIAGNOSTIC (XT,PS) */
	case 0x14: /* Controller Auto test              */
	case 0x16: /* Detect disk change (apparently added to XT BIOSes in 1986 according to RBIL) */
		reg_ah = 0x00;   // ah=6 if floppy changed
		reg_ClearCF();
		break;
	case 0x17: /* Set disk type for format */		
	/* Pirates! needs this to load */
		killRead = true;
		reg_ah = 0x00; 
		reg_ClearCF();
		break;
	default:
		//PM LOG(LOG_BIOS,LOG_ERROR)("INT13: Function %x called on drive %x (dos drive %d)", reg_ah,  reg_dl, drivenum);
		reg_ah=0xff;
		reg_SetCF();
	}
#if PM_PRINTF
	if (reg_ah!=0x00) PM_ERROR("Int13h Error %x\n",reg_ah);  
#endif	
	return CBRET_NONE;
}


void BIOS_SetupDisks(void) {  // Not called by PicoMEM Anyway, setup the BIOS Variables
/* TODO Start the time correctly */
/* PM 
	call_int13=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int13,&INT13_DiskHandler,CB_IRET,"Int 13 Bios disk");
	RealSetVec(0x13,CALLBACK_RealPointer(call_int13));
	int i; */
	
	for(int i=0;i<MAX_DISK_IMAGES;i++) {
		imageDiskList[i] = NULL;
	}
/*
	for(int i=0;i<MAX_SWAPPABLE_DISKS;i++) {
		diskSwap[i] = NULL;
	}
*/	
/*
	diskparm0 = CALLBACK_Allocate();
	diskparm1 = CALLBACK_Allocate();
	swapPosition = 0;

	RealSetVec(0x41,CALLBACK_RealPointer(diskparm0));
	RealSetVec(0x46,CALLBACK_RealPointer(diskparm1));

	PhysPt dp0physaddr=CALLBACK_PhysPointer(diskparm0);
	PhysPt dp1physaddr=CALLBACK_PhysPointer(diskparm1);
	for(i=0;i<16;i++) {
		phys_writeb(dp0physaddr+i,0);
		phys_writeb(dp1physaddr+i,0);
	}

	imgDTASeg = 0;
*/
/* Setup the Bios Area */
/* PM 
	mem_writeb(BIOS_HARDDISK_COUNT,2);

	MAPPER_AddHandler(swapInNextDisk,MK_f4,MMOD1,"swapimg","Swap Image");
	killRead = false;
	swapping_requested = false;
*/	
}
