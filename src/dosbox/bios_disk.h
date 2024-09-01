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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef DOSBOX_BIOS_DISK_H
#define DOSBOX_BIOS_DISK_H

//#include <stdio.h>
#ifndef PMDOSBOX_H
#include "pmdosbox.h"  // Picomem version of dosbox.h
#endif
#ifndef DOSBOX_MEM_H
//PM #include "mem.h"
#endif
#ifndef DOSBOX_DOS_INC_H
//PM #include "dos_inc.h"
#endif
#ifndef DOSBOX_BIOS_H
//#include "bios.h"
#endif
#include "ff.h"

/* The Section handling Bios Disk Access */
#define BIOS_MAX_DISK 10

// Structure for the pre defined disk geometry list (Floppy)
#define MAX_SWAPPABLE_DISKS 20
struct diskGeo {
	Bit32u ksize;     /* Size in kilobytes */
	Bit16u secttrack; /* Sectors per track */
	Bit16u headscyl;  /* Heads per cylinder */
	Bit16u cylcount;  /* Cylinders per side */
	Bit16u biosval;   /* Type to return from BIOS */
};
extern const diskGeo DiskGeometryList[];

class imageDisk  {
public:
    Bit32u CHS_to_LBA(Bit32u head,Bit32u cylinder,Bit32u sector);
	Bit8u Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data);
	Bit8u Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data);
	Bit8u Read_AbsoluteSector(Bit32u sectnum, void * data);
	Bit8u Write_AbsoluteSector(Bit32u sectnum, void * data);

	void Set_Geometry(Bit32u setHeads, Bit32u setCyl, Bit32u setSect, Bit32u setSectSize);
	void Get_Geometry(Bit32u * getHeads, Bit32u *getCyl, Bit32u *getSect, Bit32u *getSectSize);
	Bit8u GetBiosType(void);
	Bit32u getSectSize(void);
	imageDisk(FIL *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk);
//PM Destructor ?
//	~imageDisk() { if(diskimg != NULL) { fclose(diskimg); }	};
	~imageDisk() {	};

	bool hardDrive;
	bool active;
	FIL *diskimg;
//	Bit8u diskname[512];
	Bit8u floppytype;

	Bit32u sector_size;
	Bit32u heads,cylinders,sectors;
};

// Master Boot Record (MBR)
struct partTable {
	uint8_t booter[446];
	struct partentry_t {
		uint8_t bootflag;
		uint8_t beginchs[3];
		uint8_t parttype;
		uint8_t endchs[3];
		uint32_t absSectStart;
		uint32_t partSize;
	} pentry[4];
	uint8_t  magic1; /* 0x55 */
	uint8_t  magic2; /* 0xaa */
#ifndef SECTOR_SIZE_MAX
# pragma warning SECTOR_SIZE_MAX not defined
#endif
#if SECTOR_SIZE_MAX > 512
    uint8_t  extra[SECTOR_SIZE_MAX - 512];
#endif
};

void updateDPT(void);

#define MAX_HDD_IMAGES 4
#define MAX_DISK_IMAGES (2 + MAX_HDD_IMAGES)

extern imageDisk *imageDiskList[MAX_DISK_IMAGES];
extern imageDisk *diskSwap[MAX_SWAPPABLE_DISKS];

extern Bits swapPosition;
extern Bit16u imgDTASeg; /* Real memory location of temporary DTA pointer for fat image disk access */
extern RealPt imgDTAPtr; /* Real memory location of temporary DTA pointer for fat image disk access */
//extern DOS_DTA *imgDTA;

void swapInDisks(void);
void swapInNextDisk(void);
bool getSwapRequest(void);

//Added for PM
Bitu INT13_DiskHandler(void);
extern void BIOS_SetupDisks(void);

#endif
