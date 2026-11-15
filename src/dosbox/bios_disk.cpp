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

#define HDD_DISPLAY 0

#include <stdio.h>
#include "pico/stdlib.h"

#include "pmdosbox.h"  // Picomem version of dosbox.h
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "pm_board.h"       // PicoMEM Board Definitions (GPIO, PSRAM...)

#if USE_SPI_PSRAM
#if USE_SPI_PSRAM_DMA
#include "psram_spi.h"
extern psram_spi_inst_t psram_spi;
#else
#include "../psram_spi2.h"
extern pio_spi_inst_t psram_spi;
#endif
#endif

#include "bios.h"
#include "bios_disk.h"

#include "pmdev.h"        // PicoMEM ressources (PSDAM, SD, USB) code
#include "dev_memory.h"   // Need access to the emulated RAM infos
#include "pm_pccmd.h"
#include "dev_audiomix.h"     // For Audio Pause / Resume

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
//static Bit8u last_drive;
//Bit16u imgDTASeg;
//RealPt imgDTAPtr;
//DOS_DTA *imgDTA;
bool killRead;
//static bool swapping_requested;

// PM void CMOS_SetRegister(Bitu regNr, Bit8u val); //For setting equipment word
uint32_t PC_DB_Start;  // Start of the Shared disk buffer in the Host (PC) RAM

// ---------------------------------------------------------------------------
// READ-AFTER-WRITE SD verify (diagnostic; OFF by default). After each sector is
// written, read it back from SD and compare to the source -- prints [disk_vrfy]
// lines on any SD round-trip mismatch. This is what localized the disk-write
// corruption to the SD SPI layer; root cause was the W5500 module's capacitive
// load on the shared 24 cm-ribbon SPI0 bus (fix: series R / shorter ribbon;
// firmware keeps SD_CRC + slow slew). Doubles write I/O when on. Set to 1 to
// re-verify SD round-trips if disk corruption ever reappears.
#define DISK_RAW_VERIFY 0
#if DISK_RAW_VERIFY
static uint8_t  disk_vrfy_buf[512];
static uint32_t disk_vrfy_bad_sectors = 0;
static uint32_t disk_vrfy_bad_bytes   = 0;
static uint32_t disk_vrfy_logged      = 0;
#endif
// ---------------------------------------------------------------------------

/* 2 floppys and 4 harddrives */ 
imageDisk *imageDiskList[MAX_DISK_IMAGES];

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
        PM_DP_RAM[DPT0_OFFS]=(Bit8u)tmpcyl;
        PM_DP_RAM[DPT0_OFFS+0x1]=(Bit8u)(tmpcyl>>8);		
        PM_DP_RAM[DPT0_OFFS+0x2]=(Bit8u)tmpheads;
        PM_DP_RAM[DPT0_OFFS+0x3]=0;
        PM_DP_RAM[DPT0_OFFS+0x4]=0;
        PM_DP_RAM[DPT0_OFFS+0x5]=0xFF;
        PM_DP_RAM[DPT0_OFFS+0x6]=0xFF;
        PM_DP_RAM[DPT0_OFFS+0x7]=0;
        PM_DP_RAM[DPT0_OFFS+0x8]=(0xc0 | (((imageDiskList[2]->heads) > 8) << 3));
        PM_DP_RAM[DPT0_OFFS+0x9]=0;
        PM_DP_RAM[DPT0_OFFS+0xA]=0;
        PM_DP_RAM[DPT0_OFFS+0xB]=0;
        PM_DP_RAM[DPT0_OFFS+0xC]=0;
        PM_DP_RAM[DPT0_OFFS+0xD]=0;
        PM_DP_RAM[DPT0_OFFS+0xE]=(Bit8u)tmpsect;
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
        PM_DP_RAM[DPT1_OFFS+0x0]=(Bit8u)tmpcyl;
        PM_DP_RAM[DPT1_OFFS+0x1]=(Bit8u)(tmpcyl>>8);
        PM_DP_RAM[DPT1_OFFS+0x2]=(Bit8u)tmpheads;
        PM_DP_RAM[DPT1_OFFS+0x3]=0;
        PM_DP_RAM[DPT1_OFFS+0x4]=0;
        PM_DP_RAM[DPT1_OFFS+0x5]=0xFF;
        PM_DP_RAM[DPT1_OFFS+0x6]=0xFF;
        PM_DP_RAM[DPT1_OFFS+0x7]=0;
        PM_DP_RAM[DPT1_OFFS+0x8]=(0xc0 | (((imageDiskList[2]->heads) > 8) << 3));
        PM_DP_RAM[DPT1_OFFS+0x9]=0;
        PM_DP_RAM[DPT1_OFFS+0xA]=0;
        PM_DP_RAM[DPT1_OFFS+0xB]=0;
        PM_DP_RAM[DPT1_OFFS+0xC]=0;
        PM_DP_RAM[DPT1_OFFS+0xD]=0;
        PM_DP_RAM[DPT1_OFFS+0xE]=(Bit8u)tmpsect;
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

#if DISK_RAW_VERIFY
	// Read the just-written sector back from SD and compare to the source.
	{
	    uint rb;
	    f_lseek(diskimg, bytenum);
	    if (f_read(diskimg, disk_vrfy_buf, sector_size, &rb) == FR_OK && rb == sector_size) {
	        const uint8_t *s = (const uint8_t *)data;
	        Bit32u i, nb = 0; int first = -1;
	        for (i = 0; i < sector_size; i++)
	            if (disk_vrfy_buf[i] != s[i]) { nb++; if (first < 0) first = (int)i; }
	        if (nb) {
	            disk_vrfy_bad_sectors++;
	            disk_vrfy_bad_bytes += nb;
	            if (disk_vrfy_logged < 24) {
	                disk_vrfy_logged++;
	                PM_INFO("[disk_vrfy] SD ROUND-TRIP BAD LBA=%lu %lu/512 +%d src=%02X sd=%02X (bad_sec=%lu)\n",
	                        (unsigned long)sectnum, (unsigned long)nb, first,
	                        s[first], disk_vrfy_buf[first], (unsigned long)disk_vrfy_bad_sectors);
	            }
	        }
	    } else {
	        PM_INFO("[disk_vrfy] LBA=%lu read-back FAILED\n", (unsigned long)sectnum);
	    }
	}
#endif

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
	PM_INFO("Set_Geometry C%d H%d S%d\n",setCyl,setHeads,setSect);
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

// "Media changed" flag, one byte per virtual floppy slot. Set by the screenpad
// mount path (pm_screenpad_host.cpp sph_mount) when a new image is mounted
// at runtime, cleared by INT 13h AH=16h on the first read. Lets DOS notice
// that a previously-empty drive now has media so DIR A: works after a
// mid-session mount. (2026-06-08)
// Floppy media-change flag for runtime (screenpad) mounts. Set by sph_mount;
// INT13 AH=16h reports "disk changed" (06h) while set and is NOT cleared by the
// poll (a background poll would otherwise consume it before DOS's own DIR
// media-check ran). It is cleared by the next READ of the drive (case 0x2),
// i.e. DOS rebuilding the BPB from the new image. Held-until-read (not a timed
// window) so it survives the seconds a user takes to type DIR A:. (2026-07-04)
volatile uint8_t g_fdd_media_changed[2] = {0, 0};

// True if drivenum is one of our virtual floppy slots (0 or 1) with no
// image currently mounted. Used to keep the slot visible to the chassis BIOS
// and DOS even when empty, so a mid-session screenpad mount can populate it.
static bool isEmptyFloppySlot(Bitu driveNum) {
    if (driveNum >= 2) return false;            // not a floppy slot
    if (imageDiskList[driveNum] != NULL &&
        imageDiskList[driveNum]->active) return false;  // image actually mounted
    return true;
}

static bool driveInactive(Bitu driveNum) {
	if(driveNum>=(2 + MAX_HDD_IMAGES)) {
		PM_INFO("Disk %d non-existant", driveNum);
		last_status = 0x01;
		reg_SetCF();
		return true;
	}
	if(imageDiskList[driveNum] == NULL) {
		PM_INFO("Disk %d not active", driveNum);
		last_status = 0x01;
		reg_SetCF();
		return true;
	}
	if(!imageDiskList[driveNum]->active) {
		PM_INFO("Disk %d not active", driveNum);
		last_status = 0x01;
		reg_SetCF();
		return true;
	}
	return false;
}

#if USE_CDROM
// ===========================================================================
// INT 13h CD-ROM drive -- El Torito no-emulation, drive 0xE0
// ===========================================================================
// EDD subset serving boot loaders (isolinux) and BIOS-backed DOS drivers
// (ELTORITO.SYS): AH=41h/42h/48h plus El Torito AH=4Bh. 2048-byte sectors,
// pure LBA, no CHS. Reads go through dev_cdr_read2048() -- stateless, so
// the MKE emulation (CDMKE) and CD audio can use the same disc in parallel.
// The DAP and result structures move between PC memory and the DPRAM disk
// buffer with PC_MemCopy (the PC executes copies in both directions; the
// buffer is PC-visible at PC_DB_Start).
#include "../isa_devices/dev_cdrom_mke.h"

#define CD_DRIVE_NB       0xE0
#define CD_DRIVE_WILDCARD 0x7F   // El Torito AH=4Bh "any drive" probe
#define INT13_CD_TRACE    0     // per-call log line; set 1 to debug CD INT 13h

// Floppy-emulation overlay (Tier 3, defined below INT13_CD)
static uint8_t INT13_CDFDD_Arm(uint8_t *scratch);
static Bitu    INT13_CDFDD(void);
void           INT13_CDFDD_Disarm(void);   // also called at CMD_DEV_Init

static Bitu INT13_CD(void) {
    uint8_t *DB = (uint8_t *)&PM_PTR_DISKBUFFER[0];   // 2KB DPRAM staging
    uint32_t dssi = ((uint32_t)reg_ds << 4) + reg_si;

#if INT13_CD_TRACE
    PM_INFO("[CD13] AH=%02X DL=%02X DS:SI=%04X:%04X\n",
            (unsigned)reg_ah, (unsigned)reg_dl, (unsigned)reg_ds, (unsigned)reg_si);
#endif

    // The wildcard drive only answers El Torito status calls.
    if (reg_dl == CD_DRIVE_WILDCARD && reg_ah != 0x4B) {
        reg_ah = 0x01; reg_SetCF(); return CBRET_NONE;
    }

    switch (reg_ah) {
    case 0x00:  // Reset
    case 0x01:  // Status of last operation
    case 0x44:  // EDD verify
    case 0x45:  // Lock/unlock (pretend success)
    case 0x47:  // EDD seek
        reg_ah = 0; reg_ClearCF(); break;

    case 0x15:  // Get disk type: removable with change line
        reg_ah = 0x02; reg_ClearCF(); break;

    case 0x41:  // EDD installation check
        if (reg_bx != 0x55AA) { reg_ah = 0x01; reg_SetCF(); break; }
        reg_ah = 0x21;                   // EDD 1.1
        reg_bh = 0xAA; reg_bl = 0x55;    // BX = AA55h: extensions present
        reg_ch = 0x00; reg_cl = 0x01;    // subset 1: extended disk access
        reg_ClearCF(); break;

    case 0x42: { // EDD extended read (2048-byte sectors)
        if (dev_cdr_total_sectors() == 0) { reg_ah = 0x31; reg_SetCF(); break; } // no media
        // Fetch the 16-byte DAP from DS:SI into the DPRAM staging buffer.
        PC_MemCopy(dssi, PC_DB_Start, 16);
        if (!PC_Wait_CMD_End_Disk()) return CBRET_NONE;
        uint8_t  dap_size = DB[0];
        uint16_t count = (uint16_t)DB[2] | ((uint16_t)DB[3] << 8);
        uint16_t boff  = (uint16_t)DB[4] | ((uint16_t)DB[5] << 8);
        uint16_t bseg  = (uint16_t)DB[6] | ((uint16_t)DB[7] << 8);
        uint32_t lba   = (uint32_t)DB[8] | ((uint32_t)DB[9] << 8) |
                         ((uint32_t)DB[10] << 16) | ((uint32_t)DB[11] << 24);
        if (dap_size < 0x10 || (DB[12] | DB[13] | DB[14] | DB[15])) {
            reg_ah = 0x01; reg_SetCF(); break;   // bad DAP / LBA > 32 bit
        }
        uint32_t target = ((uint32_t)bseg << 4) + boff;
        uint16_t done = 0;
        uint8_t  err  = 0;
        while (done < count) {
            if (!dev_cdr_read2048(lba + done, DB)) { err = 0x04; break; } // read fault
            PC_MemCopy(PC_DB_Start, target, 2048);
            if (PCCR_PCSTATE == PCC_PCS_RESET) return CBRET_NONE;
#if BOARD_WM10||BOARD_WM20||BOARD_PM15
				pm_audio_domix(true);   // Insert Audio Mix during the wait time
#else
				pm_audio_domix(false);  // Insert Audio Mix during the wait time
#endif
            if (!PC_Wait_CMD_End_Disk()) return CBRET_NONE;
            target += 2048;
            done++;
        }
        // Write the transferred count back into the caller's DAP (offset 2).
        DB[0] = (uint8_t)done; DB[1] = (uint8_t)(done >> 8);
        PC_MemCopy(PC_DB_Start, dssi + 2, 2);
        if (!PC_Wait_CMD_End_Disk()) return CBRET_NONE;
        if (err) { reg_ah = err; reg_SetCF(); }
        else     { reg_ah = 0;   reg_ClearCF(); }
        break; }

    case 0x48: { // EDD get drive parameters
        // First word at DS:SI = caller's result buffer size.
        PC_MemCopy(dssi, PC_DB_Start, 2);
        if (!PC_Wait_CMD_End_Disk()) return CBRET_NONE;
        uint16_t bufsize = (uint16_t)DB[0] | ((uint16_t)DB[1] << 8);
        if (bufsize < 0x1A) { reg_ah = 0x01; reg_SetCF(); break; }
        uint32_t total = dev_cdr_total_sectors();
        memset(DB, 0, 0x1A);
        DB[0] = 0x1A; DB[1] = 0;         // v1.x structure, 26 bytes
        DB[2] = 0x14; DB[3] = 0;         // flags: removable + change line, CHS invalid
        DB[0x10] = (uint8_t)total;       // total sectors (64-bit field, high = 0)
        DB[0x11] = (uint8_t)(total >> 8);
        DB[0x12] = (uint8_t)(total >> 16);
        DB[0x13] = (uint8_t)(total >> 24);
        DB[0x18] = 0x00; DB[0x19] = 0x08; // 2048 bytes per sector
        PC_MemCopy(PC_DB_Start, dssi, 0x1A);
        if (!PC_Wait_CMD_End_Disk()) return CBRET_NONE;
        reg_ah = 0; reg_ClearCF(); break; }

    case 0xB0: { // INTERNAL (PMBIOS BOOT_CD): arm the floppy-emulation
        // overlay on drive 00h. Returns AL = media type (1-3) on success.
        uint8_t m = INT13_CDFDD_Arm(DB);
        if (m == 0) { reg_ah = 0x01; reg_SetCF(); break; }
        reg_al = m; reg_ah = 0; reg_ClearCF(); break; }

    case 0x4B: { // El Torito: terminate emulation (AL=0) / get status (AL=1)
        if (reg_al > 1) { reg_ah = 0x01; reg_SetCF(); break; }
        if (reg_al == 0) INT13_CDFDD_Disarm();   // spec: 4B00h ends emulation
        dev_cdr_eltorito_t et;
        uint8_t r = dev_cdr_eltorito(&et, DB);
        if (r == 1) { reg_ah = 0x31; reg_SetCF(); break; }   // no media
        if (r == 2) {
            // Media present but no boot catalog (plain data ISO). Still
            // answer with a valid packet (media type 0, boot fields zero):
            // ELTORITO.SYS accepts any drive whose 4B01h succeeds, which
            // makes it a universal BIOS-backed DOS driver for us -- not
            // only for discs that were actually booted.
            memset(&et, 0, sizeof(et));
        }
        memset(DB, 0, 0x13);
        DB[0] = 0x13;                    // packet size
        DB[1] = et.media_type;           // 0 = no emulation
        DB[2] = CD_DRIVE_NB;             // drive number
        DB[3] = 0;                       // controller index
        DB[4] = (uint8_t)et.image_lba;   // boot image LBA
        DB[5] = (uint8_t)(et.image_lba >> 8);
        DB[6] = (uint8_t)(et.image_lba >> 16);
        DB[7] = (uint8_t)(et.image_lba >> 24);
        DB[12] = (uint8_t)et.load_seg;   // load segment
        DB[13] = (uint8_t)(et.load_seg >> 8);
        DB[14] = (uint8_t)et.sector_count; // virtual (512-byte) sector count
        DB[15] = (uint8_t)(et.sector_count >> 8);
        PC_MemCopy(PC_DB_Start, dssi, 0x13);
        if (!PC_Wait_CMD_End_Disk()) return CBRET_NONE;
        reg_ah = 0; reg_ClearCF(); break; }

    case 0x03:  // Write -- read-only device
        reg_ah = 0x03; reg_SetCF(); break;

    default:    // incl. 02h/08h legacy CHS -- no CHS on a no-emulation CD
        reg_ah = 0x01; reg_SetCF(); break;
    }
    return CBRET_NONE;
}

// ===========================================================================
// El Torito FLOPPY-EMULATION boot (Tier 3) -- drive 00h overlay
// ===========================================================================
// Discs like Win95 OSR2 carry a boot-floppy image (media type 1-3) instead
// of a no-emulation loader. Arming the overlay points INT 13h drive 00h at
// that image: CHS reads become 512-byte slices of 2048-byte CD sectors at
// image_lba. The PM_Int13h claim is one DPRAM byte -- FDD_Attribute[0] >=
// 0x80 routes drive 00h to the firmware (same byte the FDD mount sets); the
// pre-arm value is restored on disarm. Read-only; disarmed at CMD_DEV_Init
// (next boot) and by El Torito AH=4B00h "terminate emulation".
static struct {
    bool     active;
    uint8_t  media;          // 1=1.2M  2=1.44M  3=2.88M
    uint8_t  saved_attr;     // FDD_Attribute[0] before the overlay claimed it
    uint8_t  spt, heads;
    uint16_t cyls;
    uint32_t image_lba;      // boot image start (2048-byte CD sectors)
    uint32_t total512;       // virtual 512-byte sector count (geometry size)
} cdfdd;

void INT13_CDFDD_Disarm(void) {
    if (!cdfdd.active) return;
    PM_Config->FDD_Attribute[0] = cdfdd.saved_attr;
    cdfdd.active = false;
    PM_INFO("[CD13] floppy-emulation overlay disarmed\n");
}

// Parse the catalog and arm the overlay. Returns the media type (1-3) on
// success, 0 when the disc has no floppy-emulation boot entry.
static uint8_t INT13_CDFDD_Arm(uint8_t *scratch) {
    static const struct { uint8_t spt, heads, cmos; uint16_t cyls; } geo[3] = {
        { 15, 2, 0x02, 80 },   // media 1: 1.2M  5.25"
        { 18, 2, 0x04, 80 },   // media 2: 1.44M 3.5"
        { 36, 2, 0x05, 80 },   // media 3: 2.88M 3.5"
    };
    dev_cdr_eltorito_t et;
    if (dev_cdr_eltorito(&et, scratch) != 0) return 0;
    if (et.media_type < 1 || et.media_type > 3) return 0;
    if (!cdfdd.active) cdfdd.saved_attr = PM_Config->FDD_Attribute[0];
    cdfdd.media     = et.media_type;
    cdfdd.spt       = geo[et.media_type - 1].spt;
    cdfdd.heads     = geo[et.media_type - 1].heads;
    cdfdd.cyls      = geo[et.media_type - 1].cyls;
    cdfdd.total512  = (uint32_t)cdfdd.spt * cdfdd.heads * cdfdd.cyls;
    cdfdd.image_lba = et.image_lba;
    cdfdd.active    = true;
    PM_Config->FDD_Attribute[0] = 0x80;    // PM_Int13h claims drive 00h now
    PM_INFO("[CD13] floppy-emulation overlay armed: media %u (%u/%u/%u) img LBA %lu\n",
            cdfdd.media, cdfdd.cyls, cdfdd.heads, cdfdd.spt,
            (unsigned long)cdfdd.image_lba);
    return et.media_type;
}

static uint8_t cdfdd_cmos_type(void) {
    return (cdfdd.media == 1) ? 0x02 : (cdfdd.media == 3) ? 0x05 : 0x04;
}

// INT 13h services for the overlaid drive 00h. Mirrors the image-floppy
// path's behavior (incl. AH=08h without ES:DI) for the calls a DOS /
// Win95 boot exercises.
static Bitu INT13_CDFDD(void) {
    uint8_t *DB = (uint8_t *)&PM_PTR_DISKBUFFER[0];   // 2KB DPRAM staging

#if INT13_CD_TRACE
    PM_INFO("[CDFD] AH=%02X AL=%02X CHS=%u/%u/%u ES:BX=%04X:%04X\n",
            (unsigned)reg_ah, (unsigned)reg_al,
            (unsigned)(reg_ch | ((reg_cl & 0xC0) << 2)), (unsigned)reg_dh,
            (unsigned)(reg_cl & 0x3F), (unsigned)reg_es, (unsigned)reg_bx);
#endif

    switch (reg_ah) {
    case 0x00:  // reset
    case 0x01:  // status of last op (always report clean)
    case 0x0C: case 0x10: case 0x11: case 0x12: case 0x14: case 0x17:
    case 0x18:  // set media type calls: accept
        reg_ah = 0; last_status = 0; reg_ClearCF(); break;

    case 0x04:  // verify: report AL sectors verified
        reg_ah = 0; last_status = 0; reg_ClearCF(); break;

    case 0x02: { // read sectors (CHS)
        if (reg_al == 0) { reg_ah = 0x01; last_status = 0x01; reg_SetCF(); break; }
        uint32_t cyl  = (uint32_t)(reg_ch | ((reg_cl & 0xC0) << 2));
        uint32_t sect = (uint32_t)(reg_cl & 0x3F);
        uint32_t head = reg_dh;
        if (sect == 0 || sect > cdfdd.spt || head >= cdfdd.heads) {
            reg_ah = 0x04; last_status = 0x04; reg_SetCF(); break;
        }
        uint32_t lba512 = (cyl * cdfdd.heads + head) * cdfdd.spt + sect - 1;
        uint32_t target = ((uint32_t)reg_es << 4) + reg_bx;
        uint32_t cached = 0xFFFFFFFFu;
        uint8_t  done = 0, err = 0;
        while (done < reg_al) {
            uint32_t l = lba512 + done;
            if (l >= cdfdd.total512) { err = 0x04; break; }
            uint32_t cd = cdfdd.image_lba + (l >> 2);
            if (cd != cached) {
                if (!dev_cdr_read2048(cd, DB)) { err = 0x04; break; }
                cached = cd;
            }
            // Copy the 512-byte slice straight out of the staging buffer.
            PC_MemCopy(PC_DB_Start + ((l & 3) << 9), target, 512);
            if (PCCR_PCSTATE == PCC_PCS_RESET) return CBRET_NONE;
#if BOARD_WM10||BOARD_WM20||BOARD_PM15
				pm_audio_domix(true);   // Insert Audio Mix during the wait time
#else
				pm_audio_domix(false);   // Insert Audio Mix during the wait time
#endif
            if (!PC_Wait_CMD_End_Disk()) return CBRET_NONE;
            target += 512;
            done++;
        }
        reg_al = done;
        if (err) { reg_ah = err; last_status = err; reg_SetCF(); }
        else     { reg_ah = 0;   last_status = 0;   reg_ClearCF(); }
        break; }

    case 0x03:  // write: read-only medium -> write protected
        reg_ah = 0x03; last_status = 0x03; reg_SetCF(); break;

    case 0x08:  // drive parameters (no ES:DI, same as the image-floppy path)
        reg_ah = 0x00; reg_al = 0x00;
        reg_bl = cdfdd_cmos_type();
        reg_ch = (Bit8u)(cdfdd.cyls - 1);
        reg_cl = (Bit8u)((((cdfdd.cyls - 1) >> 2) & 0xC0) | cdfdd.spt);
        reg_dh = (Bit8u)(cdfdd.heads - 1);
        reg_dl = 1;
        last_status = 0; reg_ClearCF(); break;

    case 0x15:  // disk type: floppy with change-line support
        reg_ah = 0x02; reg_ClearCF(); break;

    case 0x16:  // change line: no change
        reg_ah = 0x00; last_status = 0; reg_ClearCF(); break;

    default:
        reg_ah = 0x01; last_status = 0x01; reg_SetCF(); break;
    }
    return CBRET_NONE;
}
#endif // USE_CDROM

Bitu INT13_DiskHandler(void) {
    Bit32u sect_lba;
	Bit16u segat, bufptr;
	Bit8u sectbuf[512];
	Bitu  drivenum;
	Bitu  i,t;
    uint32_t BOffset;
    uint32_t WOffset;

//	last_drive = reg_dl;
//if (reg_ah==2) printf("R %d Sect :",reg_al);

//printf("Int13h %x ",reg_ah);
#if USE_CDROM
	// El Torito CD drive (0xE0) + AH=4Bh wildcard (0x7F): fully handled by
	// the CD-specific EDD handler; never enters the imageDisk paths.
	if (reg_dl == CD_DRIVE_NB || reg_dl == CD_DRIVE_WILDCARD)
		return INT13_CD();
	// Floppy-emulation boot overlay: while armed, drive 00h is the CD's
	// embedded boot-floppy image, whatever FDD0 is otherwise configured.
	if (cdfdd.active && reg_dl == 0x00)
		return INT13_CDFDD();
#endif

	drivenum = GetDosDriveNumber(reg_dl);

// ==========================================================================
// INT13_TRACE — full per-call diagnostic (set to 0 to disable)
// Logs every Int13h dispatch with all relevant register state to the Pico
// serial. Enable when debugging boot hangs / disk read problems. The cost
// is one PM_INFO line per disk call; insignificant vs SD I/O time.
// ==========================================================================
#define INT13_TRACE 0  // Set back to 1 to re-enable Int13h trace logs
#if INT13_TRACE
    PM_INFO("INT13> AH=%02X AL=%02X CH=%02X CL=%02X DH=%02X DL=%02X ES:BX=%04X:%04X drv=%u\n",
            (unsigned)reg_ah, (unsigned)reg_al,
            (unsigned)reg_ch, (unsigned)reg_cl,
            (unsigned)reg_dh, (unsigned)reg_dl,
            (unsigned)reg_es, (unsigned)reg_bx,
            (unsigned)drivenum);
#endif

#if HDD_DISPLAY
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
		  //
		  PM_INFO("Reset Disk %x\n",reg_dl);
 		  last_status = 0x00;
		  reg_ClearCF();
//		  busy_wait_us(100);
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
	    //busy_wait_us(100);
	    
		if (reg_al==0) {
			reg_ah = 0x01;
			reg_SetCF();
			return CBRET_NONE;
		}
		if (isEmptyFloppySlot(drivenum)) {
			// Empty floppy slot -- "Not ready" so DOS prompts the user to
			// insert a disk; once they screenpad-mount, AH=16h will signal
			// "changed" and the retry succeeds. (2026-06-08)
			reg_ah = 0x80;
			last_status = 0x80;
			reg_SetCF();
			return CBRET_NONE;
		}
		if (driveInactive(drivenum)) {
			PM_INFO("Drive inactive");
			reg_ah = 0xff;
			reg_SetCF();
			return CBRET_NONE;
		}

		// Swap-over handshake, step 1 (state 2): return 80h "not ready" once,
		// then drop to state 1. MS-DOS 6.22 on PS/2 only drops its cached
		// FAT/directory when the drive first goes NOT-READY (a physical eject);
		// a bare 06h change line makes it rebuild the BPB but keep the dir
		// buffers, so it still shows the OLD disk ("wrong disk") -- confirmed on
		// hardware, the change line alone is not enough. This step reproduces
		// the working manual eject->retry->mount automatically (DOS auto-retries
		// a not-ready read, so the sequence is transparent). (2026-07-14)
		if (drivenum < 2 && g_fdd_media_changed[drivenum] == 2) {
			g_fdd_media_changed[drivenum] = 1;
			reg_ah = 0x80;
			last_status = 0x80;
			reg_SetCF();
			return CBRET_NONE;
		}

		// Swap-over handshake, step 2 (state 1) -- also the whole story for a
		// fresh mount into an empty slot. Return 06h "media changed" (CF set)
		// instead of serving the sector, forcing DOS to FLUSH all buffers for
		// the drive and retry; the retry (flag now cleared) serves the new
		// image. Clear the flag here so the retry succeeds and we don't loop.
		// (2026-07-04)
		if (drivenum < 2 && g_fdd_media_changed[drivenum]) {
			g_fdd_media_changed[drivenum] = 0;
			reg_ah = 0x06;
			last_status = 0x06;
			reg_SetCF();
			return CBRET_NONE;
		}

		uint16_t mt_beg;
		uint16_t mt_end;
		uint32_t PCBuffer_Start;
		uint32_t PCBuffer_End;

        sect_lba = imageDiskList[drivenum]->CHS_to_LBA((Bit32u)reg_dh, (Bit32u)(reg_ch | ((reg_cl & 0xc0)<< 2)), (Bit32u)((reg_cl & 63)));

        PCBuffer_Start=(uint32_t)(reg_es<<4)+reg_bx; // PC_Buffer_Start is the current @ of the application disk buffer

#if HDD_DISPLAY

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
		for(uint32_t sn=0;sn<reg_al;sn++) {
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
#if HDD_DISPLAY
  		     PM_INFO("%d > ",mt_beg);		  
#endif			 
			 }

#ifdef PIMORONI_PICO_PLUS2_RP2350
if (mt_beg!=MEM_RAM) mt_beg=MEM_NULL;  // Can go back to use Copy via the CPU
#endif

if (mt_beg==MEM_PSRAM) mt_beg=0xFF;  // EMS > Slow Copy via CPU (For Tandy 1000, With PSRAM in DMA)
if (mt_beg==MEM_EMS) mt_beg=0xFF;    // EMS > Slow Copy via CPU

		   // PM_INFO("MT %d ",mt_beg);
		   // PM_INFO("LBA %d ",sect_lba+sn);

		  switch(mt_beg)
		    {
		    case MEM_NULL:  // ** Disk Read to the PC Ram (Disk Buffer)
#if HDD_DISPLAY
#if PM_PRINTF
			    putchar('m');
#endif
#endif
		  		last_status = imageDiskList[drivenum]->Read_AbsoluteSector(sect_lba+sn, &PM_PTR_DISKBUFFER[BOffset]);
#if INT13_TRACE
                // Dump the loaded sector when it looks like a boot sector
                // (CHS=0,0,1 -> sect_lba 0 = MBR; or first sector of any
                // active partition is also interesting). Show first 32 bytes
                // + the 0x55AA signature so we can verify it's a sane boot
                // sector before the host CPU jumps into it.
                if ((sect_lba + sn) == 0 ||
                    (sect_lba + sn) <= 64) {  // MBR + first ~32 KB of partitions
                    volatile uint8_t *p = (volatile uint8_t *)&PM_PTR_DISKBUFFER[BOffset];
                    PM_INFO("INT13: SectRead drv=%u LBA=%u status=%02X\n",
                            (unsigned)drivenum, (unsigned)(sect_lba + sn),
                            (unsigned)last_status);
                    PM_INFO("  [0x000]:");
                    for (int z = 0; z < 16; z++) PM_INFO(" %02X", p[z]);
                    PM_INFO("\n  [0x010]:");
                    for (int z = 16; z < 32; z++) PM_INFO(" %02X", p[z]);
                    PM_INFO("\n  Signature [0x1FE]: %02X %02X (expect 55 AA)\n",
                            p[510], p[511]);
                }
#endif
		  		if ((last_status != 0x00) || (killRead))
				    {
				 	 PM_ERROR("HDD Read Error");
					 killRead = false;
					 reg_ah = last_status;  // 4 if CF not accessible
					 reg_SetCF();
					 return CBRET_NONE;
		    		}
                // ** Ask the BIOS to Copy the data to the application buffer (in Background)

                PC_MemCopyW_512b(PC_DB_Start+BOffset,PCBuffer_Start);
          		if (PCCR_PCSTATE==PCC_PCS_RESET) return CBRET_NONE;		// Quit if Reset asked
#if BOARD_WM10||BOARD_WM20||BOARD_PM15
				pm_audio_domix(true);   // Insert Audio Mix during the wait time
#else
				pm_audio_domix(false);   // Insert Audio Mix during the wait time
#endif
				if (!PC_Wait_CMD_End_Disk()) return CBRET_NONE; // Wait until the PC finish a sector copy				

			   break;
		    case MEM_RAM:  // ** Disk Read to the PC RAM emulated in the Pico SRAM
#if HDD_DISPLAY
#if PM_PRINTF
			    putchar('s');
#endif
#endif
		  		last_status = imageDiskList[drivenum]->Read_AbsoluteSector(sect_lba+sn, &PM_Memory[(PCBuffer_Start - RAM_InitialAddress)]);				
		  		if ((last_status != 0x00) || (killRead)) 
				    {		
				 	 PM_ERROR("HDD Read Error");				 
					 killRead = false;
					 reg_ah = last_status;  // 4 if CF not accessible
					 reg_SetCF();
					 return CBRET_NONE;
		    		}
		       break;
#if USE_SPI_PSRAM
   		     case MEM_PSRAM:   // ** Disk Read to the PC RAM emulated in the Pico PSRAM
		     case MEM_EMS:     // ** Disk Read to the PC EMS emulated in the Pico PSRAM
#if PM_PRINTF
			    putchar('p');
#endif
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
				PM_PICO_EnablePSRAM();
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
		       break;
#endif			   
			 default:  // "Slow" case : Copy via the PC CPU with PSRAM re enabled (when mixed RAM)
#if PM_PRINTF
				putchar('c');
#endif
		  		last_status = imageDiskList[drivenum]->Read_AbsoluteSector(sect_lba+sn, &PM_PTR_DISKBUFFER[BOffset]);
		  		if ((last_status != 0x00) || (killRead)) 
				    {
				 	 PM_ERROR("HDD Read Error");
					 killRead = false;
					 reg_ah = last_status;  // 4 if CF not accessible
					 reg_SetCF();
					 return CBRET_NONE;
		    		}

				PM_PICO_EnablePSRAM();
				PC_MemCopyW_512b(PC_DB_Start+BOffset,PCBuffer_Start);

#if BOARD_WM10||BOARD_WM20||BOARD_PM15
				pm_audio_domix(true);   // Insert Audio Mix during the wait time
#else
				pm_audio_domix(false);   // Insert Audio Mix during the wait time
#endif

		        if (!PC_Wait_CMD_End_Disk()) return CBRET_NONE; // Wait until the PC finish a sector copy
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

		 if (!PC_Wait_CMD_End_Disk()) return CBRET_NONE; // Wait until the PC finish a sector copy

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

//	    PM_INFO("W%d Sect:",reg_al,reg_ah,reg_dl);

		if (isEmptyFloppySlot(drivenum)) {
			reg_ah = 0x80;       // "Not ready" for empty floppy slot
			last_status = 0x80;
			reg_SetCF();
			return CBRET_NONE;
		}
		if(driveInactive(drivenum)) {
			reg_ah = 0xff;
			reg_SetCF();
			return CBRET_NONE;
        }

        PCBuffer_Start=(uint32_t)(reg_es<<4)+reg_bx;  // Segment<<4+Offset

#if PM_PRINTF  
#if HDD_DISPLAY
		PCBuffer_End=(PCBuffer_Start+reg_al*512)-1;	  // Add Buffer NB*5412 -1
		mt_beg = GetMEMType(PCBuffer_Start);
		mt_end = GetMEMType(PCBuffer_End);
	    PM_INFO("B@ %x:%x>%x Type %x, %x \n",reg_es,reg_bx,PCBuffer_Start,mt_beg,mt_end);
#endif
#endif

        BOffset=0;
  	    PCBuffer_End=PCBuffer_Start+512-1; //Current 512b PC Buffer End (-1 as we need the end, not the @ just after the end)

        sect_lba = imageDiskList[drivenum]->CHS_to_LBA((Bit32u)reg_dh, (Bit32u)(reg_ch | ((reg_cl & 0xc0)<< 2)), (Bit32u)((reg_cl & 63)));

	  //  PM_INFO("C%d H%d S%d > LBA: %d ",(Bit32u)(reg_ch | ((reg_cl & 0xc0)<< 2)), (Bit32u)reg_dh, (Bit32u)((reg_cl & 63)),sect_lba);

// ** The PC Copy the first Sector to the Buffer
        PM_PICO_EnablePSRAM();  // First sector Copy is done with no SD Access at the same time
		PC_MemCopyW_512b(PCBuffer_Start,PC_DB_Start+BOffset);
        if (!PC_Wait_CMD_End_Disk()) return CBRET_NONE; // Wait until the PC finish a sector copy
        PM_EnableSD();

// **** Disk Write Loop ****
		for(uint32_t sn=0;sn<reg_al;sn++) 
		{
#if HDD_DISPLAY		   
		PM_INFO(".");
#endif
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
			      {					// MEM Begining or end in PSRAM/EMS
			        mt_beg=0xFF;    // Force Slow MEM Copy with uSD Disable
				  }		  
			
			  PM_INFO("%d > ",mt_beg);
			 }
 
		 if (!PC_Wait_CMD_End_Disk()) return CBRET_NONE; // Wait until the PC finish a sector copy

#ifdef PIMORONI_PICO_PLUS2_RP2350
if (mt_beg!=MEM_RAM) mt_beg=MEM_NULL;  // Can go back to use Copy via the CPU
#endif

	     // ** The PC copy the NEXT sector in advance to the Buffer (If there is more to copy)
         if (sn!=(reg_al-1)) // If processing the last sector, no need to copy in advance.
             {	
			   if (mt_beg<=MEM_RAM) PC_MemCopyW_512b(PCBuffer_Start,PC_DB_Start+BOffset);	// PC RAM or Pico SRAM emulated RAM
			      else
				   {  // Using PSRAM : Need to re enable the PSRAM and Wait
                     PM_PICO_EnablePSRAM();
#if PM_PRINTF
//         			 PM_INFO("!%x %x",PCBuffer_Start,PCBuffer_End);
                     PM_INFO("!");
#endif
					 PC_MemCopyW_512b(PCBuffer_Start,PC_DB_Start+BOffset);
		             if (PCCR_PCSTATE==PCC_PCS_RESET) return CBRET_NONE;
//#if PM_PRINTF
//         			 PM_INFO("w");
//#endif
		             if (!PC_Wait_CMD_End_Disk()) return CBRET_NONE; // Wait until the PC finish a sector copy

					 PM_EnableSD();  	// Re enable SD
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
    case 0x04: /* Verify sectors */
	if (reg_al==0) {
			reg_ah = 0x01;
			reg_SetCF();
			return CBRET_NONE;
		}
        if(driveInactive(drivenum)) {
            reg_ah = last_status;
            return CBRET_NONE;
        }
		//Qbix: The following codes don't match my specs. al should be number of sector verified
		//reg_al = 0x10; /* CRC verify failed */
		//reg_al = 0x00; /* CRC verify succeeded */
		reg_ClearCF();
		reg_ah = 0x00;          
		break;
    case 0x05: /* Format track */
    case 0x06: /* Format track set bad sector flags */
    case 0x07: /* Format track set bad sector flags */
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
	    DL = number of drives
	    ES:DI -> drive parameter table (floppies only)  */
		if (isEmptyFloppySlot(drivenum)) {
			// Report a generic 1.44 MB 3.5" drive (CMOS type 4) for the
			// empty slot so DOS keeps the drive in its table. Geometry =
			// 80 cyl, 2 heads, 18 sectors/track. (2026-06-08)
			reg_ah = 0x00;
			reg_al = 0x00;
			reg_bl = 0x04;            // 1.44 MB 3.5"
			reg_ch = 79;              // max cyl = 80-1
			reg_cl = 18;              // max sector
			reg_dh = 1;               // max head = 2-1
			reg_dl = New_FloppyNB;
			last_status = 0x00;
			reg_ClearCF();
			break;
		}
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
        uint32_t tmpheads, tmpcyl, tmpsect, tmpsize;
		imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
		if (tmpcyl!=0) tmpcyl--;		// cylinder count -> max cylinder
		if (tmpheads!=0) tmpheads--;	// head count -> max head
      //  if (tmpheads>255) PM_INFO("Error: Head > 255\n");
		reg_ch = (Bit8u)(tmpcyl & 0xff);
		reg_cl = (Bit8u)(((tmpcyl >> 2) & 0xc0) | (tmpsect & 0x3f)); 
		reg_dh = (Bit8u)tmpheads;
		last_status = 0x00;
		if (reg_dl&0x80) {	// harddisks
		    reg_dl=New_DiskNB; // Send the total nb of disk (Real +Image)
		} else {		// floppy disks
		    reg_dl=New_FloppyNB; // Send the total nb of disk (Real +Image)	

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
	case 0x19: /* Park Heads*/
	    reg_ah = 0x00;
		reg_ClearCF();
		break;
	case 0x16: /* Detect disk change line (RBIL) */
		// 06h "disk changed" while the flag is set -- HELD, not cleared by the
		// poll (else a background poll consumes it before DOS's DIR media-check
		// runs). Cleared by the next READ (case 0x2) = DOS rebuilding the BPB.
		// On a PS/2 DOS treats the 1.44M drive as having a change line and
		// trusts this over any re-read heuristic, so this is the only channel
		// that makes a live screenpad swap visible here. (2026-07-04)
		if (drivenum < 2 && g_fdd_media_changed[drivenum])
			reg_ah = 0x06;
		else
			reg_ah = 0x00;
		reg_ClearCF();
		break;
		case 0x15: /* Get Disk Type */
/*	 Read DASD Type (XT BIOS from 1/10/86 & newer)
	AH = 15h
	DL = drive number (0=A:, 1=2nd floppy, 80h=drive 0, 81h=drive 1)
	on return:
	AH = 00 drive not present
	   = 01 diskette, no change detection present
	   = 02 diskette, change detection present
	   = 03 fixed disk present
	CX:DX = number of fixed disk sectors; if 3 is returned in AH
	CF = 0 if successful
	   = 1 if error
*/
		if (isEmptyFloppySlot(drivenum)) {
			// Slot configured but no media: claim a 1.44 MB drive with
			// change-detection so DOS keeps the drive in its table and
			// will poll AH=16h after a future screenpad mount. (2026-06-08)
			reg_ah = 0x02;  // diskette, change detection present
			reg_ch = 0;
			reg_cl = 0;
			reg_dh = 0;
			reg_dl = 0;
			last_status = 0x00;
			reg_ClearCF();
			PM_INFO("Get Disk Type Drive %d: empty floppy slot (type 02)\n", drivenum);
			break;
		}
		if(driveInactive(drivenum)) {
			last_status = 0x07;
			reg_ah = last_status;
			reg_SetCF();
			return CBRET_NONE;
		}
		if (imageDiskList[drivenum]->hardDrive)
		   {
		    reg_ah=0x03;  // Fixed Disk present
			Bit32u totalsectors=(imageDiskList[drivenum]->cylinders)*(imageDiskList[drivenum]->heads)*(imageDiskList[drivenum]->sectors);
			reg_ch=(Bit8u)((totalsectors >>24) & 0xFF);
			reg_cl=(Bit8u)((totalsectors >>16) & 0xFF);
			reg_dh=(Bit8u)(totalsectors >>8 & 0xFF);
			reg_dl=(Bit8u)(totalsectors & 0xFF);
		   }
		  else
		   {
		    // 0x02 = change-line present, so DOS consults AH=16h for media
		    // change instead of its unreliable volume-serial heuristic (which
		    // fails when two images share a serial). Paired with the held-until-
		    // read flag in case 0x16. (2026-07-04)
		    reg_ah=0x02;  // Floppy Disk present, change detection present
			reg_ch=0;
			reg_cl=0;
			reg_dh=0;
			reg_dl=0;
		   }
		PM_INFO("Get Disk Type Drive %d Type %d \n",drivenum,reg_ah);
		reg_ClearCF();
		break;
	case 0x17: /* Set disk type for format */		
	/* Pirates! needs this to load */
		killRead = true;
		reg_ah = 0x00; 
		reg_ClearCF();
		break;		
	default:
		// Unsupported function: refuse silently (CF=1, AH=FF). Drivers
		// probe liberally -- ELTORITO.SYS walks AH=4Bh across every HDD
		// looking for the CD, OSes poke capability calls like AH=54h --
		// and each refusal is normal, not console-worthy.
#if INT13_TRACE
		PM_INFO("INT13: Function %x called on drive %x (dos drive %d)", reg_ah,  reg_dl, drivenum);
#endif
		reg_ah=0xff;
		reg_SetCF();
	}
#if PM_PRINTF
	// BIOS Int13h convention: CF=1 means error, CF=0 means success.
	// Non-zero AH with CF=0 is a valid status (e.g. AH=15h returns
	// AH=01/03 to report floppy/fixed-disk type). Only log when CF is
	// set, otherwise the chassis's boot-time drive enumeration spams
	// "Int13h Error 1" for every floppy detected. AH=FF (unsupported
	// function / inactive drive) is excluded too: probe refusals are
	// routine and already visible via INT13_TRACE when debugging.
	if ((reg_flagl & 1) && reg_ah != 0x00 && reg_ah != 0xff) PM_ERROR("Int13h Error %x\n",reg_ah);
#endif

#if INT13_TRACE
    PM_INFO("INT13< AH_ret=%02X AL_ret=%02X CF=%u  (status=%02X)\n",
            (unsigned)reg_ah, (unsigned)reg_al,
            (unsigned)((reg_flagl & 1) ? 1 : 0),
            (unsigned)last_status);
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
