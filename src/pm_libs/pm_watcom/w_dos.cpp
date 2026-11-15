/* w_dos.cpp : Watcom C runtime DOS (INT 21h) call shims for the PicoMEM.
 *
 * See w_bios.cpp for the shared PC-command mechanism (PC_Int13/PC_Int21).
 * These functions have the exact signatures of the Open Watcom DOS.H /
 * DOSFUNC.H runtime calls; they fill in the shared register block
 * (reg_ax, reg_bl, ... see pm_defines.h) and forward the call to the real
 * PC via PC_Int21() -> PCC_IRQ21 (the PMBIOS ROM on the PC runs "INT 21h",
 * see pmbios/pm_cmd.asm : PCC_Do_IRQ21).
 *
 * Buffers/strings that a DOS call needs to see at a real DS:DX (path
 * names, read/write chunks, the Find-First DTA, ...) are staged in the
 * "Disk Buffer" area of the dual-port RAM (PM_PTR_DISKBUFFER / PM_DP_RAM
 * @ PM_DB_Offset). That RAM window is also mapped into the PC's address
 * space starting at segment (PM_BIOSADDRESS + 0x400) -- see the "BIOS "
 * config line handling in pm_disk.cpp / dev_memory.cpp, which maps the
 * PMBIOS ROM at PM_BIOSADDRESS:0000 and this RAM right after it at
 * +0x4000 bytes (= +0x400 paragraphs). So a byte written to
 * PM_PTR_DISKBUFFER[n] is immediately visible to the PC at
 * (PM_BIOSADDRESS+0x400):(PM_DB_Offset+n), with no extra copy command.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "DOS.H"
#include "DOSFUNC.H"

#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "../pm_pccmd.h"
#include "../../isa_devices/dev_memory.h"   // PM_BIOSADDRESS

// ===========================================================================
// Shared buffer helpers
// ===========================================================================

// Segment, as seen from the PC, of the shared Disk-Buffer RAM window.
#define W_BUF_SEG      ((uint16_t)(PM_BIOSADDRESS + 0x400))

// Sub-offsets inside that window (PM_DB_Size = 2KB, see pm_defines.h)
#define W_BUF_PATHOFF  0     // ASCIIZ path / short string parameters (<=256B)
#define W_BUF_DTAOFF   256   // Find-First/Next DTA (43 bytes, classic find_t)
#define W_BUF_IOOFF    512   // chunk buffer used by _dos_read / _dos_write

// Carry flag helper (INT 21h error indicator, low bit of the saved FLAGS)
static inline bool w_GetCF() { return (reg_flagl & 1) != 0; }

// Point DS:DX at an offset inside the shared buffer
static inline void w_SetDSDX(uint16_t off)
{
  reg_dsl = (uint8_t)(W_BUF_SEG & 0xFF);
  reg_dsh = (uint8_t)(W_BUF_SEG >> 8);
  reg_dl  = (uint8_t)(off & 0xFF);
  reg_dh  = (uint8_t)(off >> 8);
}

// Copy a null-terminated path into the shared buffer (path sub-area)
static void w_CopyPath(const char *path)
{
  uint16_t i = 0;
  while (path[i] && i < (W_BUF_DTAOFF - 1)) { PM_PTR_DISKBUFFER[W_BUF_PATHOFF + i] = path[i]; i++; }
  PM_PTR_DISKBUFFER[W_BUF_PATHOFF + i] = 0;
}

// ===========================================================================
// DOS.H / DOSFUNC.H
// ===========================================================================

// Generic "raw" DOS call: AH=dosfn, DX=dx, AL=al -> returns AX
int bdos(int __dosfn, unsigned int __dx, unsigned int __al)
{
  reg_ah = (uint8_t) __dosfn;
  reg_dh = (uint8_t)(__dx >> 8);
  reg_dl = (uint8_t)__dx;
  reg_al = (uint8_t) __al;
  PC_Int21();
  return reg_ax;
}

// --- Drives ----------------------------------------------------------------

void _dos_getdrive(unsigned *__drive)
{
  reg_ah = DOS_CUR_DISK;
  PC_Int21();
  *__drive = reg_al + 1;   // AL is 0-based (0=A:), the API is 1-based
}

unsigned _getdrive(void)
{
  unsigned drive;
  _dos_getdrive(&drive);
  return drive;
}

void _dos_setdrive(unsigned __drivenum, unsigned *__drives)
{
  reg_dl = (uint8_t)(__drivenum - 1);
  reg_ah = 0x0E;   // Select Disk (not in DOSFUNC.H)
  PC_Int21();
  *__drives = reg_al;
}

unsigned _dos_getdiskfree(unsigned __drive, struct _diskfree_t *__diskspace)
{
  reg_dl = (uint8_t)__drive;
  reg_ah = 0x36;   // Get Disk Free Space (not in DOSFUNC.H)
  PC_Int21();

  uint16_t spc = reg_ax;             // AX = sectors per cluster, 0xFFFF = invalid drive
  if (spc == 0xFFFF) return 0xFFFF;

  __diskspace->sectors_per_cluster = spc;
  __diskspace->avail_clusters      = ((uint16_t)reg_bh << 8) | reg_bl;
  __diskspace->bytes_per_sector    = ((uint16_t)reg_ch << 8) | reg_cl;
  __diskspace->total_clusters      = ((uint16_t)reg_dh << 8) | reg_dl;
  return 0;
}

unsigned _getdiskfree(unsigned __drive, struct _diskfree_t *__diskspace)
{
  return _dos_getdiskfree(__drive, __diskspace);
}

// --- Date / Time -------------------------------------------------------

void _dos_getdate(struct dosdate_t *__date)
{
  reg_ah = DOS_GET_DATE;
  PC_Int21();
  __date->year      = ((uint16_t)reg_ch << 8) | reg_cl;
  __date->month     = reg_dh;
  __date->day       = reg_dl;
  __date->dayofweek = reg_al;
}

unsigned _dos_setdate(struct dosdate_t *__date)
{
  reg_ch = (uint8_t)(__date->year >> 8);
  reg_cl = (uint8_t) __date->year;
  reg_dh = __date->month;
  reg_dl = __date->day;
  reg_ah = 0x2B;   // Set Date (not in DOSFUNC.H)
  PC_Int21();
  return reg_al;   // 0 = Ok, 0xFF = invalid
}

void _dos_gettime(struct dostime_t *__time)
{
  reg_ah = DOS_GET_TIME;
  PC_Int21();
  __time->hour    = reg_ch;
  __time->minute  = reg_cl;
  __time->second  = reg_dh;
  __time->hsecond = reg_dl;
}

unsigned _dos_settime(struct dostime_t *__time)
{
  reg_ch = __time->hour;
  reg_cl = __time->minute;
  reg_dh = __time->second;
  reg_dl = __time->hsecond;
  reg_ah = 0x2D;   // Set Time (not in DOSFUNC.H)
  PC_Int21();
  return reg_al;   // 0 = Ok, 0xFF = invalid
}

unsigned _dos_getftime(int __handle, unsigned *__date, unsigned *__time)
{
  reg_bh = (uint8_t)(__handle >> 8);
  reg_bl = (uint8_t)__handle;
  reg_al = 0x00;
  reg_ah = DOS_FILE_DATE;
  PC_Int21();
  if (w_GetCF()) return reg_ax;
  *__time = ((uint16_t)reg_ch << 8) | reg_cl;
  *__date = ((uint16_t)reg_dh << 8) | reg_dl;
  return 0;
}

unsigned _dos_setftime(int __handle, unsigned __date, unsigned __time)
{
  reg_bh = (uint8_t)(__handle >> 8);
  reg_bl = (uint8_t)__handle;
  reg_ch = (uint8_t)(__time >> 8);
  reg_cl = (uint8_t)__time;
  reg_dh = (uint8_t)(__date >> 8);
  reg_dl = (uint8_t)__date;
  reg_al = 0x01;
  reg_ah = DOS_FILE_DATE;
  PC_Int21();
  return w_GetCF() ? reg_ax : 0;
}

// --- Files ---------------------------------------------------------------

unsigned _dos_open(const char *__path, unsigned __mode, int *__handle)
{
  w_CopyPath(__path);
  w_SetDSDX(W_BUF_PATHOFF);
  reg_al = (uint8_t)__mode;
  reg_ah = DOS_OPEN;
  PC_Int21();
  if (w_GetCF()) { *__handle = -1; return reg_ax; }
  *__handle = (int)reg_ax;
  return 0;
}

unsigned _dos_creat(const char *__path, unsigned __attr, int *__handle)
{
  w_CopyPath(__path);
  w_SetDSDX(W_BUF_PATHOFF);
  reg_ch = (uint8_t)(__attr >> 8);
  reg_cl = (uint8_t)__attr;
  reg_ah = DOS_CREAT;
  PC_Int21();
  if (w_GetCF()) { *__handle = -1; return reg_ax; }
  *__handle = (int)reg_ax;
  return 0;
}

unsigned _dos_creatnew(const char *__path, unsigned __attr, int *__handle)
{
  w_CopyPath(__path);
  w_SetDSDX(W_BUF_PATHOFF);
  reg_ch = (uint8_t)(__attr >> 8);
  reg_cl = (uint8_t)__attr;
  reg_ah = 0x5B;   // Create New File (fails if it exists) -- not in DOSFUNC.H
  PC_Int21();
  if (w_GetCF()) { *__handle = -1; return reg_ax; }
  *__handle = (int)reg_ax;
  return 0;
}

unsigned _dos_close(int __handle)
{
  reg_bh = (uint8_t)(__handle >> 8);
  reg_bl = (uint8_t)__handle;
  reg_ah = DOS_CLOSE;
  PC_Int21();
  return w_GetCF() ? reg_ax : 0;
}

unsigned _dos_commit(int __handle)
{
  reg_bh = (uint8_t)(__handle >> 8);
  reg_bl = (uint8_t)__handle;
  reg_ah = DOS_COMMIT_FILE;
  PC_Int21();
  return w_GetCF() ? reg_ax : 0;
}

// Transferred through the shared I/O chunk (W_BUF_IOOFF..) in
// (PM_DB_Size - W_BUF_IOOFF) byte pieces, since __buf is a Pico-local
// pointer that the PC's DOS can't dereference directly.
unsigned _dos_read(int __handle, void *__buf, unsigned __count, unsigned *__bytes)
{
  uint8_t *dst = (uint8_t *)__buf;
  const uint16_t chunkmax = PM_DB_Size - W_BUF_IOOFF;
  *__bytes = 0;

  while (__count != 0)
  {
    uint16_t chunk = (__count > chunkmax) ? chunkmax : (uint16_t)__count;

    reg_bh = (uint8_t)(__handle >> 8);
    reg_bl = (uint8_t)__handle;
    reg_ch = (uint8_t)(chunk >> 8);
    reg_cl = (uint8_t)chunk;
    w_SetDSDX(W_BUF_IOOFF);
    reg_ah = DOS_READ;
    PC_Int21();
    if (w_GetCF()) return reg_ax;

    uint16_t got = reg_ax;
    memcpy(dst, (const void *)&PM_PTR_DISKBUFFER[W_BUF_IOOFF], got);
    dst += got;
    *__bytes += got;
    __count  -= chunk;
    if (got < chunk) break;   // short read -> EOF
  }
  return 0;
}

unsigned _dos_write(int __handle, void const *__buf, unsigned __count, unsigned *__bytes)
{
  const uint8_t *src = (const uint8_t *)__buf;
  const uint16_t chunkmax = PM_DB_Size - W_BUF_IOOFF;
  *__bytes = 0;

  while (__count != 0)
  {
    uint16_t chunk = (__count > chunkmax) ? chunkmax : (uint16_t)__count;
    memcpy((void *)&PM_PTR_DISKBUFFER[W_BUF_IOOFF], src, chunk);

    reg_bh = (uint8_t)(__handle >> 8);
    reg_bl = (uint8_t)__handle;
    reg_ch = (uint8_t)(chunk >> 8);
    reg_cl = (uint8_t)chunk;
    w_SetDSDX(W_BUF_IOOFF);
    reg_ah = DOS_WRITE;
    PC_Int21();
    if (w_GetCF()) return reg_ax;

    uint16_t wrote = reg_ax;
    src += wrote;
    *__bytes += wrote;
    __count  -= chunk;
    if (wrote < chunk) break;   // disk full
  }
  return 0;
}

unsigned _dos_getfileattr(const char *__path, unsigned *__attr)
{
  w_CopyPath(__path);
  w_SetDSDX(W_BUF_PATHOFF);
  reg_al = 0x00;
  reg_ah = DOS_CHMOD;
  PC_Int21();
  if (w_GetCF()) return reg_ax;
  *__attr = ((uint16_t)reg_ch << 8) | reg_cl;
  return 0;
}

unsigned _dos_setfileattr(const char *__path, unsigned __attr)
{
  w_CopyPath(__path);
  w_SetDSDX(W_BUF_PATHOFF);
  reg_ch = (uint8_t)(__attr >> 8);
  reg_cl = (uint8_t)__attr;
  reg_al = 0x01;
  reg_ah = DOS_CHMOD;
  PC_Int21();
  return w_GetCF() ? reg_ax : 0;
}

// find_t (43 bytes: 21 reserved + attrib + wr_time + wr_date + size + name)
// is copied verbatim to/from the shared DTA sub-area, which is pointed at
// by DS:DX via DOS_SET_DTA before each find call.
unsigned _dos_findfirst(const char *__path, unsigned __attr, struct find_t *__buf)
{
  w_CopyPath(__path);

  w_SetDSDX(W_BUF_DTAOFF);
  reg_ah = DOS_SET_DTA;
  PC_Int21();

  w_SetDSDX(W_BUF_PATHOFF);
  reg_ch = (uint8_t)(__attr >> 8);
  reg_cl = (uint8_t)__attr;
  reg_ah = DOS_FIND_FIRST;
  PC_Int21();
  if (w_GetCF()) return reg_ax;

  memcpy(__buf, (const void *)&PM_PTR_DISKBUFFER[W_BUF_DTAOFF], sizeof(struct find_t));
  return 0;
}

unsigned _dos_findnext(struct find_t *__buf)
{
  // DOS keeps its search state in find_t's 21 reserved bytes -- restore
  // the caller's copy into the shared DTA so the PC-side search can resume.
  memcpy((void *)&PM_PTR_DISKBUFFER[W_BUF_DTAOFF], __buf, sizeof(struct find_t));

  w_SetDSDX(W_BUF_DTAOFF);
  reg_ah = DOS_SET_DTA;
  PC_Int21();

  reg_ah = DOS_FIND_NEXT;
  PC_Int21();
  if (w_GetCF()) return reg_ax;

  memcpy(__buf, (const void *)&PM_PTR_DISKBUFFER[W_BUF_DTAOFF], sizeof(struct find_t));
  return 0;
}

unsigned _dos_findclose(struct find_t *__buf)
{
  (void)__buf;
  return 0;   // Nothing to release for the classic (non-LFN) find_t API
}

// --- Memory ----------------------------------------------------------------

unsigned _dos_allocmem(unsigned __size, unsigned *__seg)
{
  reg_bh = (uint8_t)(__size >> 8);
  reg_bl = (uint8_t)__size;
  reg_ah = DOS_ALLOC_SEG;
  PC_Int21();
  if (w_GetCF()) { *__seg = ((uint16_t)reg_bh << 8) | reg_bl; return reg_ax; }  // BX = max available
  *__seg = reg_ax;
  return 0;
}

unsigned _dos_freemem(unsigned __seg)
{
  reg_esl = (uint8_t)__seg;
  reg_esh = (uint8_t)(__seg >> 8);
  reg_ah = DOS_FREE_SEG;
  PC_Int21();
  return w_GetCF() ? reg_ax : 0;
}

unsigned _dos_setblock(unsigned __size, unsigned __seg, unsigned *__maxsize)
{
  reg_esl = (uint8_t)__seg;
  reg_esh = (uint8_t)(__seg >> 8);
  reg_bh  = (uint8_t)(__size >> 8);
  reg_bl  = (uint8_t)__size;
  reg_ah  = DOS_MODIFY_SEG;
  PC_Int21();
  if (w_GetCF()) { *__maxsize = ((uint16_t)reg_bh << 8) | reg_bl; return reg_ax; }
  *__maxsize = __size;
  return 0;
}

// --- Interrupt vectors -------------------------------------------------
// Packed as a 32bit value: high 16 bits = segment, low 16 bits = offset.
// There is no native "PC far pointer" type on the Pico side, so callers
// must pack/unpack with that same convention.

void (*_dos_getvect(int __intnum))()
{
  reg_al = (uint8_t)__intnum;
  reg_ah = DOS_GET_INT;
  PC_Int21();
  uint32_t packed = ((uint32_t)reg_es << 16) | (((uint16_t)reg_bh << 8) | reg_bl);
  return (void (*)())(uintptr_t)packed;
}

void _dos_setvect(int __intnum, void (*__handler)())
{
  uint32_t packed = (uint32_t)(uintptr_t)__handler;
  reg_dsh = (uint8_t)(packed >> 24);
  reg_dsl = (uint8_t)(packed >> 16);
  reg_dh  = (uint8_t)(packed >> 8);
  reg_dl  = (uint8_t)packed;
  reg_al  = (uint8_t)__intnum;
  reg_ah  = DOS_SET_INT;
  PC_Int21();
}

void _dos_keep(unsigned __retcode, unsigned __memsize)
{
  reg_al = (uint8_t)__retcode;
  reg_dh = (uint8_t)(__memsize >> 8);
  reg_dl = (uint8_t)__memsize;
  reg_ah = 0x31;   // TSR (not in DOSFUNC.H)
  PC_Int21();
}

// --- Extended error info ----------------------------------------------

int dosexterr(struct _DOSERROR *__info)
{
  reg_bh = 0; reg_bl = 0;
  reg_ah = 0x59;   // Get Extended Error Info (not in DOSFUNC.H)
  PC_Int21();
  if (__info)
  {
    __info->exterror = reg_ax;
    __info->errclass = reg_bh;
    __info->action   = reg_bl;
    __info->locus    = reg_ch;
  }
  return reg_ax;
}

// --- Not supported by this shim -----------------------------------------
// Critical-error handling and interrupt chaining would need to run local
// code on the emulated PC's own CPU when INT 24h / the chained vector
// fires there -- there is no such hook in the PC-command protocol, so
// these are documented no-ops rather than a silent partial emulation.

void _harderr(int (*__func)(unsigned, unsigned, unsigned *))
{
  (void)__func;
  PM_WARNING("_harderr: not supported by the PicoMEM DOS call shim\n");
}

void _hardresume(int __result) { (void)__result; }
void _hardretn(int __error)    { (void)__error; }

void _chain_intr(void (*__handler)())
{
  (void)__handler;
  PM_WARNING("_chain_intr: not supported by the PicoMEM DOS call shim\n");
}

unsigned sleep(unsigned __seconds)
{
  sleep_ms((uint32_t)__seconds * 1000);
  return 0;
}

// LFN (long file name) support needs the DOS LFN API (INT 21h AX=71xxh)
// and wide-char paths, neither of which the classic find_t-based calls
// above use. Stubbed as "no LFN available" rather than implemented.
int _islfn(const char *__path)
{
  (void)__path;
  return 0;
}

char *_lfntosfn(char *__orgname, char *__shortname)
{
  strcpy(__shortname, __orgname);
  return __shortname;
}

unsigned _wdos_findfirst(const wchar_t *__path, unsigned __attr, struct _wfind_t *__buf)
{
  (void)__path; (void)__attr; (void)__buf;
  PM_WARNING("_wdos_findfirst: LFN wide-char find not supported\n");
  return 2;    // "file not found" -- closest classic DOS error code
}

unsigned _wdos_findnext(struct _wfind_t *__buf)
{
  (void)__buf;
  return 18;   // "no more files"
}

unsigned _wdos_findclose(struct _wfind_t *__buf)
{
  (void)__buf;
  return 0;
}
