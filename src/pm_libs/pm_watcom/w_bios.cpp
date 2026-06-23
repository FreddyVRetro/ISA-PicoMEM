#include "BIOS.H"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_pccmd.h"
#include "pm_defines.h"

/*
AH = Command
AL = Sectors count (0 is illegal) -- cannot cross ES page boundary, or a cylinder boundary, and must be < 128
CH = cylinder & 0xff
CL = Sector | ((cylinder >> 2) & 0xC0);
DH = Head -- may include two more cylinder bits
ES:BX -> buffer
DL = Drive number
*/
/*
AH = Command
AL = number of sectors (must be nonzero)
CH = low eight bits of cylinder number
CL = sector number 1-63 (bits 0-5)
	     high two bits of cylinder (bits 6-7, hard disk only)
DH = head number
DL = drive number (bit 7 set for hard disk)
ES:BX -> data buffer 
*/

unsigned short _bios_disk(unsigned __cmd, struct diskinfo_t *__diskinfo)
{
  reg_ah = (uint8_t) __cmd;
  reg_dl= (uint8_t) __diskinfo->drive;
  reg_al = (uint8_t) __diskinfo->sector;
  reg_ch = (uint8_t) __diskinfo->track;
  reg_cl = (uint8_t) __diskinfo->sector | (((uint8_t) __diskinfo->track >>2 ) & 0xC0);
  reg_dh = (uint8_t) __diskinfo->head; 
  reg_esl = (uint8_t) (((uint32_t) __diskinfo->buffer)>>16)&0xFF;
  reg_esh = (uint8_t) (((uint32_t) __diskinfo->buffer)>>(16+8))&0xFF;
 return 0;
}

unsigned short _bios_memsize(void)
{
 return 0;
}