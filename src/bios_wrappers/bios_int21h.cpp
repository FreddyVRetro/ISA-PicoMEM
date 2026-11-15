#include "bios_int21h.h"

#include "pm_debug.h"
#include "pm_defines.h"
#include "pm_gvars.h"

extern volatile uint8_t PM_Status;

/*
 * Legacy INT 21h tracing switch retained for reference when adding DOS calls:
 *
 *       switch (reg_ah)
 *       {
 *       case 0x0D: PM_INFO(". DISK RESET (AH:%x)\n",reg_ah); break;
 *       case 0x0E: PM_INFO(". SELECT DEFAULT DRIVE (AH:%x)\n",reg_ah); break;
 *       case 0x0F: PM_INFO(". OPEN FILE USING FCB (AH:%x)\n",reg_ah); break;
 *       case 0x10: PM_INFO(". CLOSE FILE USING FCB (AH:%x)\n",reg_ah); break;
 *       case 0x11: PM_INFO(". FIND FIRST MATCHING FILE USING FCB (AH:%x)\n",reg_ah); break;
 *       case 0x12: PM_INFO(". FIND NEXT MATCHING FILE USING FCB (AH:%x)\n",reg_ah); break;
 *       case 0x13: PM_INFO(". DELETE FILE USING FCB (AH:%x)\n",reg_ah); break;
 *       case 0x14: PM_INFO(". SEQUENTIAL READ FROM FCB FILE (AH:%x)\n",reg_ah); break;
 *       case 0x15: PM_INFO(". SEQUENTIAL WRITE TO FCB FILE (AH:%X)\n",reg_ah); break;
 *       case 0x16: PM_INFO(". CREATE OR TRUNCATE FILE (AH:%X)\n",reg_ah); break;
 *       case 0x17: PM_INFO(". EXTENDED RENAME FILE (AH:%x)\n",reg_ah); break;
 *       case 0x18: PM_INFO(". EXTENDED RENAME FILE (AH:%x)\n",reg_ah); break;
 *       case 0x19: PM_INFO(". GET CURRENT DEFAULT DRIVE (AH:%x)\n",reg_ah); break;
 *       case 0x1A: PM_INFO(". SET DISK TRANSFER AREA ADDRESS (AH:%x)\n",reg_ah); break;
 *       case 0x23: PM_INFO(". GET FILE SIZE (AH:%x)\n",reg_ah); break;
 *       case 0x25: PM_INFO(". SET INTERRUPT VECTOR (AH:%x)\n",reg_ah); break;
 *       case 0x29: PM_INFO(". PARSE FILENAME INTO FCB (AH:%x)\n",reg_ah); break;
 *       case 0x2A: PM_INFO(". GET SYSTEM DATE (AH:%x)\n",reg_ah); break;
 *       case 0x30: PM_INFO(". GET DOS VERSION (AH:%x)\n",reg_ah); break;
 *       case 0x2C: PM_INFO(". GET SYSTEM TIME (AH:%x)\n",reg_ah); break;
 *       case 0x32: PM_INFO(". GET DOS DRIVE PARAMETER BLOCK FOR SPECIFIC DRIVE (AH:%x) DL:%X\n",reg_ah,reg_dl); break;
 *       case 0x36: PM_INFO(". GET FREE DISK SPACE (AH:%x) Drive:%d\n",reg_ah,reg_dl); break;
 *       case 0x39: PM_INFO(". MKDIR - CREATE SUBDIRECTORY (AH:%x)\n",reg_ah); break;
 *       case 0x3A: PM_INFO(". RMDIR - REMOVE SUBDIRECTORY (AH:%x)\n",reg_ah); break;
 *       case 0x3B: PM_INFO(". CHDIR - SET CURRENT DIRECTORY (AH:%x)\n",reg_ah); break;
 *       case 0x3C: PM_INFO(". CREAT - CREATE OR TRUNCATE FILE (AH:%x)\n",reg_ah); break;
 *       case 0x3D: PM_INFO(". OPEN - OPEN EXISTING FILE (AH:%x)\n",reg_ah); break;
 *       case 0x3E: PM_INFO(". CLOSE FILE (AH:%x) Handle:%X\n",reg_ah,(uint16_t) reg_bl); break;
 *       case 0x3F: PM_INFO(". READ - READ FROM FILE OR DEVICE (AH:%x) Handle:%X\n",reg_ah,(uint16_t) reg_bl); break;
 *       case 0x40: PM_INFO(". WRITE - WRITE TO FILE OR DEVICE (AH:%x) Handle:%x\n",reg_ah,(uint16_t) reg_bl); break;
 *       case 0x41: PM_INFO(". UNLINK - DELETE FILE (AH:%x) Handle:%xn\n",reg_ah,(uint16_t) reg_bl); break;
 *       case 0x42: PM_INFO(". LSEEK - SET FILE POINTER (AH:%x) Handle:%x\n",reg_ah,(uint16_t) reg_bl); break;
 *       case 0x43: PM_INFO(". GET FILE ATTRIBUTES (AH:%x)\n",reg_ah); break;
 *       case 0x47: PM_INFO(". CWD - GET CURRENT DIRECTORY (AH:%x)\n",reg_ah); break;
 *       case 0x48: PM_INFO(". ALLOCATE MEMORY (AH:%x) Paragraph:%x\n",reg_ah,(uint16_t) reg_bl); break;
 *       case 0x49: PM_INFO(". FREE MEMORY Segment:%x\n",reg_ah,(uint16_t) reg_esl); break;
 *       case 0x4A: PM_INFO(". RESIZE MEMORY BLOCK (AH:%x)\n",reg_ah); break;
 *       case 0x4B: PM_INFO(". EXEC - LOAD AND/OR EXECUTE PROGRAM %x\n",reg_ah); break;
 *       case 0x4C: PM_INFO(". EXIT - TERMINATE WITH RETURN CODE (AH:%x) AL:%x\n",reg_ah,reg_al); break;
 *       case 0x4D: PM_INFO(". GET RETURN CODE (ERRORLEVEL) (AH:%x)\n",reg_ah); break;
 *       case 0x4E: PM_INFO(". FINDFIRST - FIND FIRST MATCHING FILE (AH:%x)\n",reg_ah); break;
 *       case 0x4F: PM_INFO(". FINDNEXT - FIND NEXT MATCHING FILE (AH:%x)\n",reg_ah); break;
 *       case 0x50: PM_INFO(". SET CURRENT PSP (AH:%x)\n",reg_ah); break;
 *       case 0x63: PM_INFO(". GET EXTENDED ERROR INFORMATION (AH:%x)\n",reg_ah); break;
 *       default: PM_INFO(". AH:%x AL:%X BX:%X, CX:%X, DX:%X\n",reg_ah, reg_al, (uint16_t) reg_bl, (uint16_t) reg_cl, (uint16_t) reg_dl); break;
 *       }
 */

// DOS INT 21h tracing observes the BIOS register image and does not change
// DOS state; the PMBIOS performs the actual DOS interrupt work on the PC.
// AH=40h/BX=1 is a screen write and is intentionally skipped to reduce logging.
void bios_int21h_execute()
{
  if ((reg_ah == 0x40) & (reg_bx == 1)) {
    PM_Status = STAT_READY;
    return;
  }

  PM_INFO(".DD........... Int21h DOS ............DD.\n");

  switch (reg_ah) {
    case 0x0D: PM_INFO(". DISK RESET (AH:%x)\n", reg_ah); break;
    case 0x0E: PM_INFO(". SELECT DEFAULT DRIVE (AH:%x)\n", reg_ah); break;
    case 0x0F: PM_INFO(". OPEN FILE USING FCB (AH:%x)\n", reg_ah); break;
    case 0x10: PM_INFO(". CLOSE FILE USING FCB (AH:%x)\n", reg_ah); break;
    case 0x11: PM_INFO(". FIND FIRST MATCHING FILE USING FCB (AH:%x)\n", reg_ah); break;
    case 0x12: PM_INFO(". FIND NEXT MATCHING FILE USING FCB (AH:%x)\n", reg_ah); break;
    case 0x13: PM_INFO(". DELETE FILE USING FCB (AH:%x)\n", reg_ah); break;
    case 0x14: PM_INFO(". SEQUENTIAL READ FROM FCB FILE (AH:%x)\n", reg_ah); break;
    case 0x15: PM_INFO(". SEQUENTIAL WRITE TO FCB FILE (AH:%X)\n", reg_ah); break;
    case 0x16: PM_INFO(". CREATE OR TRUNCATE FILE (AH:%X)\n", reg_ah); break;
    case 0x17: PM_INFO(". EXTENDED RENAME FILE (AH:%x)\n", reg_ah); break;
    case 0x18: PM_INFO(". EXTENDED RENAME FILE (AH:%x)\n", reg_ah); break;
    case 0x19: PM_INFO(". GET CURRENT DEFAULT DRIVE (AH:%x)\n", reg_ah); break;
    case 0x1A: PM_INFO(". SET DISK TRANSFER AREA ADDRESS (AH:%x)\n", reg_ah); break;
    case 0x23: PM_INFO(". GET FILE SIZE (AH:%x)\n", reg_ah); break;
    case 0x25: PM_INFO(". SET INTERRUPT VECTOR (AH:%x)\n", reg_ah); break;
    case 0x29: PM_INFO(". PARSE FILENAME INTO FCB (AH:%x)\n", reg_ah); break;
    case 0x2A: PM_INFO(". GET SYSTEM DATE (AH:%x)\n", reg_ah); break;
    case 0x30: PM_INFO(". GET DOS VERSION (AH:%x)\n", reg_ah); break;
    case 0x2C: PM_INFO(". GET SYSTEM TIME (AH:%x)\n", reg_ah); break;
    case 0x32: PM_INFO(". GET DOS DRIVE PARAMETER BLOCK FOR SPECIFIC DRIVE (AH:%x) DL:%X\n", reg_ah, reg_dl); break;
    case 0x36: PM_INFO(". GET FREE DISK SPACE (AH:%x) Drive:%d\n", reg_ah, reg_dl); break;
    case 0x39: PM_INFO(". MKDIR - CREATE SUBDIRECTORY (AH:%x)\n", reg_ah); break;
    case 0x3A: PM_INFO(". RMDIR - REMOVE SUBDIRECTORY (AH:%x)\n", reg_ah); break;
    case 0x3B: PM_INFO(". CHDIR - SET CURRENT DIRECTORY (AH:%x)\n", reg_ah); break;
    case 0x3C: PM_INFO(". CREAT - CREATE OR TRUNCATE FILE (AH:%x)\n", reg_ah); break;
    case 0x3D: PM_INFO(". OPEN - OPEN EXISTING FILE (AH:%x)\n", reg_ah); break;
    case 0x3E: PM_INFO(". CLOSE FILE (AH:%x) Handle:%X\n", reg_ah, (uint16_t)reg_bl); break;
    case 0x3F: PM_INFO(". READ - READ FROM FILE OR DEVICE (AH:%x) Handle:%X\n", reg_ah, (uint16_t)reg_bl); break;
    case 0x40: PM_INFO(". WRITE - WRITE TO FILE OR DEVICE (AH:%x) Handle:%x\n", reg_ah, (uint16_t)reg_bl); break;
    case 0x41: PM_INFO(". UNLINK - DELETE FILE (AH:%x) Handle:%xn", reg_ah, (uint16_t)reg_bl); break;
    case 0x42: PM_INFO(". LSEEK - SET FILE POINTER (AH:%x) Handle:%x\n", reg_ah, (uint16_t)reg_bl); break;
    case 0x43: PM_INFO(". GET FILE ATTRIBUTES (AH:%x)\n", reg_ah); break;
    case 0x47: PM_INFO(". CWD - GET CURRENT DIRECTORY (AH:%x)\n", reg_ah); break;
    case 0x48: PM_INFO(". ALLOCATE MEMORY (AH:%x) Paragraph:%x\n", reg_ah, (uint16_t)reg_bl); break;
    case 0x49: PM_INFO(". FREE MEMORY Segment:%x\n", reg_ah, (uint16_t)reg_esl); break;
    case 0x4A: PM_INFO(". RESIZE MEMORY BLOCK (AH:%x)\n", reg_ah); break;
    case 0x4B: PM_INFO(". EXEC - LOAD AND/OR EXECUTE PROGRAM %x\n", reg_ah); break;
    case 0x4C: PM_INFO(". EXIT - TERMINATE WITH RETURN CODE (AH:%x) AL:%x\n", reg_ah, reg_al); break;
    case 0x4D: PM_INFO(". GET RETURN CODE (ERRORLEVEL) (AH:%x)\n", reg_ah); break;
    case 0x4E: PM_INFO(". FINDFIRST - FIND FIRST MATCHING FILE (AH:%x)\n", reg_ah); break;
    case 0x4F: PM_INFO(". FINDNEXT - FIND NEXT MATCHING FILE (AH:%x)\n", reg_ah); break;
    case 0x50: PM_INFO(". SET CURRENT PSP (AH:%x)\n", reg_ah); break;
    case 0x63: PM_INFO(". GET EXTENDED ERROR INFORMATION (AH:%x)\n", reg_ah); break;
    default: PM_INFO(". AH:%x AL:%X BX:%X, CX:%X, DX:%X\n", reg_ah, reg_al, (uint16_t)reg_bl, (uint16_t)reg_cl, (uint16_t)reg_dl); break;
  }

  PM_Status = STAT_READY;
}

void bios_int21h_end()
{
  // The BIOS sends this marker after the traced INT 21h sequence completes.
  PM_INFO(".DD........... Int21h END ............DD.\n");
  PM_INFO(". AH:%x AL:%X BX:%X, CX:%X, DX:%X\n", reg_ah, reg_al, (uint16_t)reg_bl, (uint16_t)reg_cl, (uint16_t)reg_dl);
  PM_Status = STAT_READY;
}
