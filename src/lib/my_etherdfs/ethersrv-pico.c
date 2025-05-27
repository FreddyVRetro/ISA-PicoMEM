/*
 * ethersrv-linux is serving files through the EtherDFS protocol. Runs on
 * Linux.
 *
 * http://etherdfs.sourceforge.net
 *
 * ethersrv-linux is distributed under the terms of the MIT License, as listed
 * below.
 *
 * Copyright (C) 2017, 2018 Mateusz Viste
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

//#include <arpa/inet.h>       /* htons() */
//#include <errno.h>
//#include <endian.h>          /* le16toh(), le32toh() */
//#include <linux/if_ether.h>
//#include <linux/if_packet.h>
#include <limits.h>          /* PATH_MAX and such */
//#include <net/ethernet.h>
//#include <net/if.h>
//#include <signal.h>
#include <stdio.h>
#include <string.h>          /* mempcy() */
//#include <sys/ioctl.h>
//#include <sys/socket.h>
#include <stdint.h>          /* uint16_t, uint32_t */
//#include <stdlib.h>          /* realpath() */
//#include <time.h>            /* time() */
//#include <unistd.h>          /* close(), getopt(), optind */

#include "debug.h"
#include "lock.h"

/* program version */
#define PVER "20170415"

/* protocol version (single byte, must be in sync with etherdfs) */
#define PROTOVER 2

#ifdef BOARD_PICOMEM

extern volatile uint8_t PM_DP_RAM[16*1024];   // "Dual Port" RAM for Disk I/O Buffer

#include "pico/stdlib.h"
#include "../pm_debug.h"
//#include "../pm_gvars.h"   // Has an extern "C" C++ code so can't add it
#include "../pm_defines.h"
#include "ff.h"

char sd_rootdir[]="0:";
char usb_rootdir[]="1:";

/* DOS to FatFS open mode convert
   Bit 0-2 access mode
           000=read
           001=write
           010=read/write
*/           
const unsigned char F_AM_CONVERT[4] = {FA_READ,FA_WRITE,FA_READ+FA_WRITE,FA_READ+FA_WRITE};

#endif

#include "fs.h"

/* answer cache - last answers sent to clients - used if said client didn't
 * receive my answer, and re-sends his requests so I don't process this
 * request again (which might be dangerous in case of write requests, like
 * write to file, delete file, rename file, etc. For every client that ever
 * sent me a query, there is exactly one entry in the cache. */

#ifndef BOARD_PICOMEM  //No chache, no miss in the Pico
#define ANSWCACHESZ 16
//#define ANSWCACHESZ 1  // Only one for picoMEM
static struct struct_answcache {
  unsigned char frame[1520]; /* entire frame that was sent (first 6 bytes is the client's mac) */
  time_t timestamp; /* time of answer (so if cache full I can drop oldest) */
  unsigned short len;  /* frame's length */
} answcache[ANSWCACHESZ];
#endif

/* all the calls I support are in the range AL=0..2Eh - the list below serves
 * as a convenience to compare AL (subfunction) values */
enum AL_SUBFUNCTIONS {
  AL_INSTALLCHK = 0x00,
  AL_RMDIR      = 0x01,
  AL_MKDIR      = 0x03,
  AL_CHDIR      = 0x05,
  AL_CLSFIL     = 0x06,
  AL_CMMTFIL    = 0x07,
  AL_READFIL    = 0x08,
  AL_WRITEFIL   = 0x09,
  AL_LOCKFIL    = 0x0A,
  AL_UNLOCKFIL  = 0x0B,
  AL_DISKSPACE  = 0x0C,
  AL_SETATTR    = 0x0E,
  AL_GETATTR    = 0x0F,
  AL_RENAME     = 0x11,
  AL_DELETE     = 0x13,
  AL_OPEN       = 0x16,
  AL_CREATE     = 0x17,
  AL_FINDFIRST  = 0x1B,
  AL_FINDNEXT   = 0x1C,
  AL_SKFMEND    = 0x21,
  AL_UNKNOWN_2D = 0x2D,
  AL_SPOPNFIL   = 0x2E,
  AL_UNKNOWN    = 0xFF
};


/* an array with flags indicating whether given drive is FAT-based or not */
static unsigned char drivesfat[26]; /* 0 if not, non-zero otherwise */

/* the flag is set when ethersrv is expected to terminate */

#ifndef BOARD_PICOMEM
static sig_atomic_t volatile terminationflag = 0;

void sigcatcher(int sig) {
  switch (sig) {
    case SIGTERM:
    case SIGQUIT:
    case SIGINT:
      terminationflag = 1;
      break;
    default:
      break;
  }
}
#endif

/* returns a printable version of a FCB block (ie. with added null terminator), this is used only by debug routines */
//#if DEBUG > 0
static char *pfcb(char *s) {
  static char r[12] = "FILENAMEEXT";
  memcpy(r, s, 11);
  return(r);
}
//#endif

/* turns a character c into its low-case variant */
static char lochar(char c) {
  if ((c >= 'A') && (c <= 'Z')) c += ('a' - 'A');
  return(c);
}

/* turns a string into all-lower-case characters, up to n chars max */
static void lostring(char *s, int n) {
  while ((n-- != 0) && (*s != 0)) {
    *s = lochar(*s);
    s++;
  }
}

/* checks wheter dir is belonging to the root directory. returns 0 if so, 1
 * otherwise */
static int isroot(char *root, char *dir) {
  /* fast-forward to the 'virtual directory' part */
  while ((*root != 0) && (*dir != 0)) {
    root++;
    dir++;
  }
  /* skip any leading / */
  while (*dir == '/') dir++;
  /* is there any subsequent '/' ? if so, then it's not root */
  while (*dir != 0) {
    if (*dir == '/') return(0);
    dir++;
  }
  /* otherwise it's root */
  return(1);
}


/* explode a full X:\DIR\FILE????.??? search path into directory and mask */
static void explodepath(char *dir, char *file, char *source, int sourcelen) {
  int i, lastbackslash;
  /* if drive present, skip it */
  if (source[1] == ':') {
    source += 2;
    sourcelen -= 2;
  }
  /* find last slash or backslash and copy source into dir up to this last backslash */
  lastbackslash = 0;
  for (i = 0; i < sourcelen; i++) {
    if ((source[i] == '\\') || (source[i] == '/')) lastbackslash = i;
  }
  memcpy(dir, source, lastbackslash + 1);
  dir[lastbackslash + 1] = 0;
  /* copy file/mask into file */
  memcpy(file, source+lastbackslash+1, sourcelen - (lastbackslash + 1));
  file[sourcelen - (lastbackslash + 1)] = 0;
}


/* replaces all rep chars in string s by repby */
static void charreplace(char *s, char rep, char repby) {
  while (*s != 0) {
    if (*s == rep) *s = repby;
    s++;
  }
}

/* copies everything after last slash into dst */
static void copy_after_last_slash(char *dst, const char *src) {
    const char *last_slash = strrchr(src, '/');

    if (last_slash)
        strcpy(dst, last_slash + 1);
}

static int process(unsigned char *reqbuff) {
  int query, reqdrv, reqflags;
  int reslen = 0;
  unsigned short reqbufflen;
  unsigned short *ax;     /* pointer to store the value of AX after the query */
  unsigned char *answ;    /* convenience pointer to answer->frame */
  unsigned short *wreqbuff; /* same as query, but word-based (16 bits) */
  unsigned short *wansw;  /* same as answer->frame, but word-based (16 bits) */
  char *root;
  answ = reqbuff;

#ifndef BOARD_PICOMEM
  /* does it match the cache entry (same seq and same mac and len > 0)? if so, just re-send it again */
  if ((answ[57] == reqbuff[57]) && (memcmp(answ, reqbuff + 6, 6) == 0) && (answer->len > 0)) {
  #if SIMLOSS > 0
    // PM_ERROR("Cache HIT (seq %u)\n", answ[57]);
  #endif
    return(answer->len);
  }
#endif

#ifndef BOARD_PICOMEM   // Answer address == Request address in Pico, No MAC Address
  /* copy all headers as-is */
  memcpy(answ, reqbuff, 60);

  /* switch src and dst addresses so the reply header is ready */
  memcpy(answ, answ + 6, 6);  /* copy source mac into dst field */
  memcpy(answ + 6, mymac, 6); /* copy my mac into source field */
#endif

  /* remember the pointer to the AX result, and fetch reqdrv and AL query */
  reqbufflen = ((uint16_t *)reqbuff)[26];   
  //if (reqbufflen < 60) return(-1);

  reqdrv = reqbuff[58] & 31;   /* 5 lowest -> drive */
  reqflags = reqbuff[58] >> 5; /* 3 highest bits -> flags */
  reqflags = reqflags;         /* just so the compiler won't complain (I don't use flags yet) */
  query = reqbuff[59];

  ax = (uint16_t *)answ + 29;  /* answer AX at offset 29*2 (58) */

  // skip eth headers now, as well as padding, seq, reqdrv and AL
  reqbuff += 60;
  answ += 60;
  reqbufflen -= 60;
  reslen = 0;

  // set up wansw and wreqbuff pointers
  wansw = (uint16_t *)answ;
  wreqbuff = (uint16_t *)reqbuff;

  // is the drive valid? (C: - Z:)
  if ((reqdrv < 2) || (reqdrv > 25)) { // 0=A, 1=B, 2=C, etc
    PM_ERROR("invalid drive value: 0x%02Xh\n", reqdrv);
    return(-3);
  }
  if (reqdrv==('S'-'A')) root=sd_rootdir;         // S: SD Drive
   else if (reqdrv==('U'-'A')) root=usb_rootdir;  // U: USB
    else {
          PM_ERROR("unknown drive: %c: (%02Xh)\n", 'A' + reqdrv, reqdrv);
          return(-3);
         }
  // assume success (hence AX == 0 most of the time)
  *ax = 0;
  // let's look at the exact query
  PM_INFO("Got query: %02Xh, root %s len%d\n", query, root,reqbufflen);

  if (query == AL_DISKSPACE) {
    unsigned long long diskspace, freespace;
    PM_INFO("DISKSPACE for drive '%c:\n", 'A' + reqdrv);
    diskspace = diskinfo(root, &freespace);
    // limit results to slightly under 2 GiB (otherwise MS-DOS is confused)
    if (diskspace >= 2lu*1024*1024*1024) diskspace = 2lu*1024*1024*1024 - 1;
    if (freespace >= 2lu*1024*1024*1024) freespace = 2lu*1024*1024*1024 - 1;
    PM_INFO("TOTAL: %llu KiB ; FREE: %llu KiB\n", diskspace >> 10, freespace >> 10);
    *ax = 1;              // AX: media id (8 bits) | sectors per cluster (8 bits) -- MSDOS tolerates only 1 here! */
    wansw[1] = 32768;     // CX: bytes per sector
    diskspace >>= 15;     // space to number of 32K clusters
    freespace >>= 15;     // space to number of 32K clusters
    wansw[0] = diskspace; // BX: total clusters
    wansw[2] = freespace; // DX: available clusters
    reslen += 6;
  } else if ((query == AL_READFIL) && (reqbufflen == 8)) {  // AL=08h
    uint16_t len, fileid;
    uint32_t offset;
    long readlen;
    offset = ((uint32_t *)reqbuff)[0];
    fileid = wreqbuff[2];
    len = wreqbuff[3];
    PM_INFO(" Reading %u bytes: File #%u, Offset %u, total:%d , copied: %d\n", len, fileid, offset,wreqbuff[4],wreqbuff[5]);
    readlen = readfile(answ, fileid, offset, len);
    if (readlen < 0) {
      PM_ERROR("Error : %d\n",-readlen);
      *ax = -readlen; // Return the error code
    } else {
      reslen += readlen;
    }
  } else if ((query == AL_WRITEFIL) && (reqbufflen >= 6)) {  // AL=09h
    uint16_t fileid;
    uint32_t offset;
    long writelen;
    offset = ((uint32_t *)reqbuff)[0];
    fileid = wreqbuff[2];
    PM_INFO("Writing %u bytes into file #%u, starting offset %u\n", reqbufflen - 6, fileid, offset);
    writelen = writefile(reqbuff + 6, fileid, offset, reqbufflen - 6);
    if (writelen < 0) {
      PM_ERROR("ERROR: Access denied");
      *ax = 5; // "access denied"
    } else {
      wansw[0] = writelen;
      reslen += 2;
    }
  } else if ((query == AL_LOCKFIL) || (query == AL_UNLOCKFIL)) { // AL=0x0A
    // I do nothing, except lying that lock/unlock succeeded
  } else if (query == AL_FINDFIRST) { // 0x1B
    struct fileprops fprops;
    char directory[DIR_MAX];
    unsigned short dirss;
    char filemask[16];
    char filemaskfcb[11];
    int offset;
    unsigned fattr;
    unsigned short fpos = 0;
    int flags;
    fattr = reqbuff[0];

    offset = sprintf(directory, "%s/", root);
    // explode the full "\DIR\FILE????.???" search path into directory and mask
    explodepath(directory + offset, filemask, (char *)reqbuff + 1, reqbufflen - 1);
    lostring(directory + offset, -1);
    lostring(filemask, -1);
    charreplace(directory, '\\', '/');

    filename2fcb(filemaskfcb, filemask);
    PM_INFO("FindFirst in '%s'\filemaskfcb: '%s' \nattribs: 0x%2X\n", directory, pfcb(filemaskfcb), fattr);
    flags = 0;

      if (fs_find(true, (char *) &directory, (char *) &filemaskfcb, &fprops, fattr) != 0) {
      PM_INFO("No matching file found\n");
      *ax = ERR_NOMORE_FILES; // 0x12 is "no more files" -- one would assume 0x02 "file not found" would be better, but that's not what MS-DOS 5.x does, some applications rely on a failing FFirst to return 0x12 (for example LapLink 5)
    } else { // found a file
      PM_INFO("found file: FCB '%s' (attr %02Xh)\n", pfcb(fprops.fcbname), fprops.fattr);
      answ[0] = fprops.fattr; // fattr (1=RO 2=HID 4=SYS 8=VOL 16=DIR 32=ARCH 64=DEVICE)
      memcpy(answ + 1, fprops.fcbname, 11);
      answ[12] = fprops.ftime & 0xff;
      answ[13] = (fprops.ftime >> 8) & 0xff;
      answ[14] = (fprops.ftime >> 16) & 0xff;
      answ[15] = (fprops.ftime >> 24) & 0xff;
      answ[16] = fprops.fsize & 0xff;         // fsize
      answ[17] = (fprops.fsize >> 8) & 0xff;  // fsize
      answ[18] = (fprops.fsize >> 16) & 0xff; // fsize
      answ[19] = (fprops.fsize >> 24) & 0xff; // fsize
      wansw[10] = 1;     // dir id     PM : Send 1 as Dir ID
      wansw[11] = fpos;  // file position in dir
      reslen = 24;
    }
  } else if (query == AL_FINDNEXT) { // 0x1C
    unsigned short fpos;
    struct fileprops fprops;
    char *fcbmask;
    char filemaskfcb[11];    
    unsigned char fattr;
    unsigned short dirss;
    int flags;
//    dirss = le16toh(wreqbuff[0]);
//    fpos = le16toh(wreqbuff[1]);
    dirss = wreqbuff[0];
    fpos = wreqbuff[1];
    fattr = reqbuff[4];
    fcbmask = (char *)reqbuff + 5;
    //

    PM_INFO("FindNext looks for nth file %u in dir #%u\nfcbmask: '%s'\nattribs: 0x%2X\n", fpos, dirss, pfcb(fcbmask), fattr);
    flags = 0;
    
    // if (isroot(root, sstoitem(dirss)) != 0) flags |= FFILE_ISROOT;  !! A Changer
    // if (drivesfat[reqdrv] != 0) flags |= FFILE_ISFAT;

    if (fs_find(false, (char *) NULL, fcbmask, &fprops, fattr)) {
      PM_INFO("No more matching files found\n");
      *ax = ERR_NOMORE_FILES; // "no more files"
    } else { // found a file
      PM_INFO("> Found file: '%s' (attr %02Xh)\n", pfcb(fprops.fcbname), fprops.fattr);
      answ[0] = fprops.fattr; // fattr (1=RO 2=HID 4=SYS 8=VOL 16=DIR 32=ARCH 64=DEVICE) */
      memcpy(answ + 1, fprops.fcbname, 11);
      answ[12] = fprops.ftime & 0xff;
      answ[13] = (fprops.ftime >> 8) & 0xff;
      answ[14] = (fprops.ftime >> 16) & 0xff;
      answ[15] = (fprops.ftime >> 24) & 0xff;
      answ[16] = fprops.fsize & 0xff;         // fsize
      answ[17] = (fprops.fsize >> 8) & 0xff;  // fsize
      answ[18] = (fprops.fsize >> 16) & 0xff; // fsize
      answ[19] = (fprops.fsize >> 24) & 0xff; // fsize
//      wansw[10] = htole16(dirss); // dir id
//      wansw[11] = htole16(fpos);  // file position in dir
      wansw[10] = dirss; // Pos 20 dir id
      wansw[11] = fpos;  // Pos 22 file position in dir
      reslen = 24;
    }
  } else if ((query == AL_MKDIR) || (query == AL_RMDIR)) { // MKDIR or RMDIR
    char directory[DIR_MAX];
    int offset;
    offset = sprintf(&directory[0], "%s/", root);
    // explode the full "\DIR\FILE????.???" search path into directory and mask
    memcpy(directory + offset, (char *)reqbuff, reqbufflen);
    directory[offset + reqbufflen] = 0;
    lostring(directory + offset, -1);
    charreplace(directory, '\\', '/');

    if (query == AL_MKDIR) {
      PM_INFO("MKDIR '%s'\n", directory);
      if (makedir(directory) != 0) {
        *ax = 29;
        PM_ERROR("MKDIR Error\n");
      }
    } else {
      PM_INFO("RMDIR '%s'\n", directory);
      if (remdir(directory) != 0) {
        *ax = 29;
        PM_ERROR("RMDIR Error\n");
      }
    }
  } else if (query == AL_CHDIR) { /* check if dir exist, return ax=0 if so, ax=3 otherwise */
    char directory[DIR_MAX];
    int offset;
    offset = sprintf(directory, "%s/", root);
    memcpy(directory + offset, (char *)reqbuff, reqbufflen);
    directory[offset + reqbufflen] = 0;
    lostring(directory + offset, -1);
    charreplace(directory, '\\', '/');    // Not needed for fatfs
    PM_INFO("CHDIR '%s'\n", directory);

    if (changedir(directory) != 0) {
      PM_ERROR("CHDIR Error (%s)\n", directory);
      *ax = ERR_PATH_NOTFOUND;
    }
  } else if (query == AL_CLSFIL) { // AL_CLSFIL (0x06)
    // I do nothing, since I do not keep any open files around anyway.
    // just say 'ok' by sending back AX=0
    unsigned short fid;
    fid = wreqbuff[0];

    PM_INFO("CLOSE FILE\n");
    closefile(fid);
    *ax = 0;
  } else if ((query == AL_SETATTR) && (reqbufflen > 1)) { // AL_SETATTR (0x0E)
    char fullpathname[DIR_MAX];
    int offset;
    unsigned char fattr;
    fattr = reqbuff[0];
    // get full file path
    offset = sprintf(fullpathname, "%s/", root);
    memcpy(fullpathname + offset, (char *)reqbuff + 1, reqbufflen - 1);
    fullpathname[offset + reqbufflen - 1] = 0;
    lostring(fullpathname + offset, -1);
    charreplace(fullpathname, '\\', '/');

    PM_INFO("SETATTR [file: '%s', attr: 0x%02X]\n", fullpathname, fattr);
    if (setitemattr(fullpathname, fattr) != 0) *ax = 2;
  } else if ((query == AL_GETATTR) && (reqbufflen > 0)) { /// AL_GETATTR (0x0F)
    char fullpathname[DIR_MAX];
    int offset;
    struct fileprops fprops;

    // get full file path
    offset = sprintf(fullpathname, "%s/", root);
    memcpy(fullpathname + offset, (char *)reqbuff, reqbufflen);
    fullpathname[offset + reqbufflen] = 0;
    lostring(fullpathname + offset, -1);
    charreplace(fullpathname, '\\', '/');

    PM_INFO("GETATTR on file: '%s' (fatflag=%d)\n", fullpathname);
    if (getitemattr(fullpathname, &fprops) == 0xFF) {
      PM_INFO("no file found\n");
      *ax = 2;
    } else {
      PM_INFO("> Found it (%d bytes, attr 0x%02X)\n", (uint32_t) fprops.fsize, fprops.fattr);
      answ[reslen++] = fprops.ftime & 0xff;
      answ[reslen++] = (fprops.ftime >> 8) & 0xff;
      answ[reslen++] = (fprops.ftime >> 16) & 0xff;
      answ[reslen++] = (fprops.ftime >> 24) & 0xff;
      answ[reslen++] = fprops.fsize & 0xff;
      answ[reslen++] = (fprops.fsize >> 8) & 0xff;
      answ[reslen++] = (fprops.fsize >> 16) & 0xff;
      answ[reslen++] = (fprops.fsize >> 24) & 0xff;
      answ[reslen++] = fprops.fattr;
    }
  } else if ((query == AL_RENAME) && (reqbufflen > 2)) { // AL_RENAME (0x11)
    // query is LSSS...DDD...
    struct fileprops fprops;
    char fn1[DIR_MAX], fn2[DIR_MAX];
    int fn1len, fn2len, offset, res;

    offset = sprintf(fn1, "%s/", root);
    sprintf(fn2, "%s/", root);
    fn1len = reqbuff[0];
    fn2len = reqbufflen - (1 + fn1len);
    if (reqbufflen > fn1len) {
      memcpy(fn1 + offset, reqbuff + 1, fn1len);
      fn1[fn1len + offset] = 0;
      lostring(fn1 + offset, -1);
      charreplace(fn1, '\\', '/');

      memcpy(fn2 + offset, reqbuff + 1 + fn1len, fn2len);
      fn2[fn2len + offset] = 0;
      lostring(fn2 + offset, -1);
      charreplace(fn2, '\\', '/');

      PM_INFO("RENAME src='%s' dst='%s'\n", fn1, fn2);
      res=renfile(fn1, fn2);
      if (res< 0)
        {
          PM_ERROR("RENAME Error (%d)\n",-res);
          *ax = -res;
        }
    } else {
      *ax = 2;
    }
  } else if (query == AL_DELETE) {
    char fullpathname[DIR_MAX];
    int offset,res;
    // compute full path/file first
    offset = sprintf(fullpathname, "%s/", root);
    memcpy(fullpathname + offset, reqbuff, reqbufflen);
    fullpathname[reqbufflen + offset] = 0;
    lostring(fullpathname + offset, -1);
    charreplace(fullpathname, '\\', '/');
    PM_INFO("DELETE '%s'\n", fullpathname);

    res= delfiles(fullpathname);  // !! To correct: DEL only one and del directory as well
    if (res < 0) {
      PM_ERROR("DELETE Error (%d)\n",-res);
      *ax = -res;
      } 
  } else if ((query == AL_OPEN) || (query == AL_CREATE) || (query == AL_SPOPNFIL)) { 
    // OPEN is only about "does this file exist", and CREATE "please create or truncate this file", while SPOPNFIL is a combination of both with extra flags
    struct fileprops fprops;
    char directory[DIR_MAX];
    char fname[16];
    char fnamefcb[12];
    char fullpathname[DIR_MAX];
    int offset;
    int fileres,fileid;  // ! fileid will have the opened/created/truncated file id or a negative value in case of error
    int ofmode;  // FatFS File Acces Mode
    unsigned short stackattr, actioncode, spopen_openmode, spopres = 0;
    unsigned char resopenmode,attr;
    // fetch args
    stackattr = wreqbuff[0];
    actioncode = wreqbuff[1];
    spopen_openmode = wreqbuff[2];

    // compute full path/file
    offset = sprintf(fullpathname, "%s/", root);
    memcpy(fullpathname + offset, reqbuff + 6, reqbufflen - 6);
    fullpathname[reqbufflen + offset - 6] = 0;
    lostring(fullpathname + offset, -1);
    charreplace(fullpathname, '\\', '/');

    // compute directory and 'search mask'
    offset = sprintf(directory, "%s/", root);
    explodepath(directory + offset, fname, (char *)reqbuff+6, reqbufflen-6);
    lostring(directory + offset, -1);
    charreplace(directory, '\\', '/');

    if (query==AL_CREATE) stackattr=2;    // Force to create as Read/Write (PicoMEM, for Disktest)
    ofmode=F_AM_CONVERT[stackattr&0x03];  // convert DOS to FatFS open mode

    // compute a FCB-style version of the filename
    // filename2fcb(fnamefcb, fname);
    
    spopres=0;
    if (query==AL_CREATE) // If Create, open with Create directly
       {
         PM_INFO("CREATEFIL / stackattr (attribs)=%02Xh ofmode=%x  '%s'\n", stackattr,FA_CREATE_ALWAYS+ofmode, fullpathname);
         fileid = openfile(&fprops, fullpathname,FA_CREATE_ALWAYS+ofmode);  // Create, fail if exist
         // change attr ofmode
         if (fileid<0) // fileid contains the error code ?
           {
            PM_INFO("Create fail, code=%x\n",-fileid);
           } else spopres=2;  // File Created
         resopenmode = stackattr & 0xff;
       }

    if (query==AL_OPEN) // If Open, open with Open directly
       {
         PM_INFO("OPEN FILE / stackattr (attribs)=%02Xh ofmode=%x '%s'\n", stackattr,ofmode, fullpathname);
         fileid = openfile(&fprops, fullpathname, ofmode);
         if (fileid<0) // fileid contains the error code ?
            {
             PM_INFO("Open fail, code=%x\n",-fileid);
            } else spopres=1;  // File Opened
         resopenmode = stackattr & 0xff;
       }

    if (query==AL_SPOPNFIL) 
       {
        PM_INFO("SPOPNFIL / stackattr=%02Xh / action=%02Xh / openmode=%02Xh '%s'\n", stackattr, actioncode, spopen_openmode, fullpathname); 

        attr = getitemattr(fullpathname, &fprops);
        resopenmode = spopen_openmode & 0x7f; /* that's what PHANTOM.C does */
        if (attr == 0xff) { /* file not found - look at high nibble of action code */
          PM_INFO("file doesn't exist -> ");
          if ((actioncode & 0xf0) == 16) { /* create */
            PM_INFO("create file\n");
            fileid = openfile(&fprops, fullpathname,FA_CREATE_NEW + FA_WRITE + FA_READ);  // Create, force read/write mode (Is it really correct ?)
            // change attr ofmode
            if (fileid<0) // fileid contains the error code ?
               {
                PM_INFO("Create fail, code=%x\n",-fileid);
               } else spopres=2;  // File Created
          } else { /* fail */
            PM_INFO("fail\n");
            fileid = -ERR_ACC_DENIED;
           }
        } else if ((attr & (FAT_VOL | FAT_DIR)) != 0) { /* item is a DIR or a VOL */
          PM_INFO("fail: item '%s' is either a DIR or a VOL\n", fullpathname);
          fileid = -ERR_ACC_DENIED;
        } else { /* file found (not a VOL, not a dir) - look at low nibble of action code */
          PM_INFO("file exists already (attr %02Xh) -> ", attr);
          if ((actioncode & 0x0f) == 1) { /* open */
            PM_INFO("open file\n");
            fileid = openfile(&fprops, fullpathname, ofmode);
            if (fileid<0) // fileid contains the error code ?
               {
                PM_INFO("Open fail, code=%x\n",-fileid);
               } else spopres=1;  // File Opened            
            spopres = 1; /* spopres == 1 means 'file opened' */
          } else if ((actioncode & 0x0f) == 2) { /* truncate */
            PM_INFO("truncate file\n");
            fileid = -ERR_ACC_DENIED;  // !!! To be done (Truncate)
            if (fileid>0) spopres = 3; /* spopres == 3 means 'file truncated' */
          } else { /* fail */
            PM_INFO("fail\n");
            fileid = -ERR_ACC_DENIED;
          }
        }
        }

    if (fileid<0)
       {
        PM_INFO("Error: %d\n",-fileid);
        *ax = -fileid;
       } else
      {
        // success (found a file, created it or truncated it)
        PM_INFO("Opened file: FCB '%s' (id %04X)\n", pfcb(fprops.fcbname), fileid);
        PM_INFO("     fsize: %d\n", (uint32_t) fprops.fsize);
        PM_INFO("     fattr: %02Xh\n", fprops.fattr);
        PM_INFO("     ftime: %04lX\n", fprops.ftime);
        PM_INFO("     resopenmode = %02X\n", resopenmode);
        answ[reslen++] = fprops.fattr; // fattr (1=RO 2=HID 4=SYS 8=VOL 16=DIR 32=ARCH 64=DEVICE)
        memcpy(answ + reslen, fprops.fcbname, 11);
        reslen += 11;
        answ[reslen++] = fprops.ftime & 0xff; // time: YYYYYYYM MMMDDDDD hhhhhmmm mmmsssss
        answ[reslen++] = (fprops.ftime >> 8) & 0xff;
        answ[reslen++] = (fprops.ftime >> 16) & 0xff;
        answ[reslen++] = (fprops.ftime >> 24) & 0xff;
        answ[reslen++] = fprops.fsize & 0xff;         // fsize
        answ[reslen++] = (fprops.fsize >> 8) & 0xff;  // fsize
        answ[reslen++] = (fprops.fsize >> 16) & 0xff; // fsize
        answ[reslen++] = (fprops.fsize >> 24) & 0xff; // fsize
        answ[reslen++] = fileid & 0xff;
        answ[reslen++] = fileid >> 8;
        // CX result (only relevant for SPOPNFIL)
        answ[reslen++] = spopres & 0xff;
        answ[reslen++] = spopres >> 8;
        answ[reslen++] = resopenmode;
      }
    
  } else if ((query == AL_SKFMEND) && (reqbufflen == 6)) { // SKFMEND (0x21)
    // translate a 'seek from end' offset into an 'seek from start' offset
//    int32_t offs = le32toh(((uint32_t *)reqbuff)[0]);
    int32_t offs = ((uint32_t *)reqbuff)[0];
    long fsize;
//    unsigned short fss = le16toh(((unsigned short *)reqbuff)[2]);
    unsigned short fss = ((unsigned short *)reqbuff)[2];
    PM_INFO("SKFMEND on file #%u at offset %d\n", fss, offs);
    // if arg is positive, zero it out
    if (offs > 0) offs = 0;
    //
    fsize = getfopsize(fss);
    if (fsize < 0) {
      PM_INFO("ERROR: file not found or other error\n");
      *ax = 2;
    } else { // compute new offset and send it back
      PM_INFO("file #%u is %lu bytes long\n", fss, fsize);
      offs += fsize;
      if (offs < 0) offs = 0;
      PM_INFO("new offset: %d\n", offs);
//      ((uint32_t *)answ)[0] = htole32(offs);
      ((uint32_t *)answ)[0] = offs;
      reslen = 4;
    }
  } else { // unknown query - ignore
    return(-1);
  }
  return(reslen + 60);
}

extern void ethdfs_init()
{
PM_INFO ("PicoMEM DFS Init\n");
init_fobj();
PM_ETHDFS_BUFF=0x12;
PM_DP_RAM[PM_DB_Offset-60+1]=0x34;
PM_DP_RAM[PM_DB_Offset-60+2]=0xAA;
}

// Process the EtherDFS querry (From a Fixed address command/answer buffer)
void ethdfs_docmd(uint8_t *buffer)
{
  int16_t ret, len, ax;

  PM_INFO("----------- DFS Command ----------------\n");

  //for (int i=52;i<60;i++) printf("%x,",buffer[i]);

  ret=process(buffer);
 
  ((uint16_t *)buffer)[26]=ret;   // Return the response size or error (negative value)
  ax = ((uint16_t *)buffer)[29];

  if (ret<0) PM_INFO("Error");

  PM_INFO(" > Packet Length %d, AX : %d\n",ret,ax);
}

void ethdfs_test()
{
  char directory[]="0:/";
  char filemask[]="???????????";
  struct fileprops fprops;

  char fbuff[512];
  int fileid;
  int fileid1;
  int fileid2;    
  int res;
  int res1;
  int res2;

  PM_INFO("Test findfirst\n");
  // Find Volume
  fs_find(true,(char *) &directory, (char *) &filemask, &fprops,0x08);
  // Real FindFirst
  fs_find(true,(char *) &directory, (char *) &filemask, &fprops,0x12);


  fs_find(false,(char *) &directory, (char *) &filemask, &fprops,0x12);
  fs_find(false,(char *) &directory, (char *) &filemask, &fprops,0x12);
  fs_find(false,(char *) &directory, (char *) &filemask, &fprops,0x12);
  fs_find(false,(char *) &directory, (char *) &filemask, &fprops,0x12);  
  fs_find(false,(char *) &directory, (char *) &filemask, &fprops,0x12);
  fs_find(false,(char *) &directory, (char *) &filemask, &fprops,0x12);
  fs_find(false,(char *) &directory, (char *) &filemask, &fprops,0x12);


  fileid = openfile(&fprops, "0:/CONFIG.TXT",FA_READ);
  PM_INFO("Open 0:/CONFIG.TXT result:%d\n",fileid);

  fileid2 = openfile(&fprops, "0:/WIFI.TXT",FA_READ);
  PM_INFO("Open 0:/WIFI.TXT result:%d\n",fileid2);

  res = readfile(fbuff, fileid, 0, 512); 
  PM_INFO("Read result:%d\n",res);

  PM_INFO("File Data: %s",fbuff);

  res = readfile(fbuff, fileid2, 0, 512); 
  PM_INFO("Read result:%d\n",res);
  
  PM_INFO("File Data: %s",fbuff);

  closefile(fileid);
  closefile(fileid2);

/*
  fs_findnext((char *) &filemask,&fprops,0);
  fs_findnext((char *) &filemask,&fprops,0);
  fs_findnext((char *) &filemask,&fprops,0);
  fs_findnext((char *) &filemask,&fprops,0);
  fs_findnext((char *) &filemask,&fprops,0);
  fs_findnext((char *) &filemask,&fprops,0);
  fs_findnext((char *) &filemask,&fprops,0);
  fs_findnext((char *) &filemask,&fprops,0);
*/

}

/* used for debug output of frames on screen */
#if DEBUG > 0
static void dumpframe(unsigned char *frame, int len) {
  int i, b;
  int lines;
  const int LINEWIDTH=16;
  lines = (len + LINEWIDTH - 1) / LINEWIDTH; /* compute the number of lines */
  /* display line by line */
  for (i = 0; i < lines; i++) {
    /* read the line and output hex data */
    for (b = 0; b < LINEWIDTH; b++) {
      int offset = (i * LINEWIDTH) + b;
      if (b == LINEWIDTH / 2) printf(" ");
      if (offset < len) {
        printf(" %02X", frame[offset]);
      } else {
        printf("   ");
      }
    }
    printf(" | "); /* delimiter between hex and ascii */
    /* now output ascii data */
    for (b = 0; b < LINEWIDTH; b++) {
      int offset = (i * LINEWIDTH) + b;
      if (b == LINEWIDTH / 2) printf(" ");
      if (offset >= len) {
        printf(" ");
        continue;
      }
      if ((frame[offset] >= ' ') && (frame[offset] <= '~')) {
        printf("%c", frame[offset]);
      } else {
        printf(".");
      }
    }
    /* newline and loop */
    printf("\n");
  }
}
#endif