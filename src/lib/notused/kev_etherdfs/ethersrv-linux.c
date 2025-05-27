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


#include <sys/types.h>
#include <errno.h>
#include <limits.h>          /* PATH_MAX and such */
#include <signal.h>
#include <stdio.h>
#include <string.h>          /* mempcy() */
#include <stdint.h>          /* uint16_t, uint32_t */
#include <stdlib.h>          /* realpath() */
#include <time.h>            /* time() */
#include <unistd.h>          /* close(), getopt(), optind */

#include "debug.h"
#include "fs.h"

#include "lwip/def.h"

extern uint16_t ne2000_rx(void *buf,int len);

uint16_t htole16(uint16_t x) {
  return x;
}
uint32_t htole32(uint32_t x) {
  return x;
}
uint32_t le32toh(uint32_t x) {
  return x;
}
uint16_t le16toh(uint16_t x) {
  return x;
}






/* program version */
#define PVER "20170415"

/* protocol version (single byte, must be in sync with etherdfs) */
#define PROTOVER 2


/* answer cache - last answers sent to clients - used if said client didn't
 * receive my answer, and re-sends his requests so I don't process this
 * request again (which might be dangerous in case of write requests, like
 * write to file, delete file, rename file, etc. For every client that ever
 * sent me a query, there is exactly one entry in the cache. */
#define ANSWCACHESZ 4
static struct struct_answcache {
  unsigned char frame[1520]; /* entire frame that was sent (first 6 bytes is the client's mac) */
  time_t timestamp; /* time of answer (so if cache full I can drop oldest) */
  unsigned short len;  /* frame's length */
} answcache[ANSWCACHESZ];


unsigned char etherdfs_mac[6] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
char *root[26];
  


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

/* returns a printable version of a FCB block (ie. with added null terminator), this is used only by debug routines */
#if DEBUG > 0
static char *pfcb(char *s) {
  static char r[12] = "FILENAMEEXT";
  memcpy(r, s, 11);
  return(r);
}
#endif

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

/* finds the cache entry related to given client */
static struct struct_answcache *findcacheentry(unsigned char *clientmac) {
  int i, oldest = 0;
  /* iterate through cache entries until matching mac is found */
  for (i = 0; i < ANSWCACHESZ; i++) {
    if (memcmp(answcache[i].frame, clientmac, 6) == 0) {
      return(&(answcache[i])); /* found! */
    }
    /* is this the oldest entry? remember it. */
    if (answcache[i].timestamp < answcache[oldest].timestamp) oldest = i;
  }
  /* if nothing found, over-write the oldest entry */
  return(&(answcache[oldest]));
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


static int process(struct struct_answcache *answer, unsigned char *reqbuff, int reqbufflen, unsigned char *mymac, char **rootarray) {
  int query, reqdrv, reqflags;
  int reslen = 0;
  unsigned short *ax;     /* pointer to store the value of AX after the query */
  unsigned char *answ;    /* convenience pointer to answer->frame */
  unsigned short *wreqbuff; /* same as query, but word-based (16 bits) */
  unsigned short *wansw;  /* same as answer->frame, but word-based (16 bits) */
  char *root;
  answ = answer->frame;
  /* must be at least 60 bytes long */
  if (reqbufflen < 60) return(-1);
  /* does it match the cache entry (same seq and same mac and len > 0)? if so, just re-send it again */
  if ((answ[57] == reqbuff[57]) && (memcmp(answ, reqbuff + 6, 6) == 0) && (answer->len > 0)) {
  #if SIMLOSS > 0
    fprintf(stderr, "Cache HIT (seq %u)\n", answ[57]);
  #endif
    return(answer->len);
  }

  /* copy all headers as-is */
  memcpy(answ, reqbuff, 60);

  /* switch src and dst addresses so the reply header is ready */
  memcpy(answ, answ + 6, 6);  /* copy source mac into dst field */
  memcpy(answ + 6, mymac, 6); /* copy my mac into source field */
  /* remember the pointer to the AX result, and fetch reqdrv and AL query */
  ax = (uint16_t *)answ + 29;
  reqdrv = reqbuff[58] & 31; /* 5 lowest -> drive */
  reqflags = reqbuff[58] >> 5; /* 3 highest bits -> flags */
  reqflags = reqflags; /* just so the compiler won't complain (I don't use flags yet) */
  query = reqbuff[59];
  /* skip eth headers now, as well as padding, seq, reqdrv and AL */
  reqbuff += 60;
  answ += 60;
  reqbufflen -= 60;
  reslen = 0;
  /* set up wansw and wreqbuff */
  wansw = (uint16_t *)answ;
  wreqbuff = (uint16_t *)reqbuff;

  /* is the drive valid? (C: - Z:) */
  if ((reqdrv < 2) || (reqdrv > 25)) { /* 0=A, 1=B, 2=C, etc */
    fprintf(stderr, "invalid drive value: 0x%02Xh\n", reqdrv);
    return(-3);
  }
  /* do I know this drive? */
  root = rootarray[reqdrv];
  if (root == NULL) {
    fprintf(stderr, "unknown drive: %c: (%02Xh)\n", 'A' + reqdrv, reqdrv);
    return(-3);
  }
  /* assume success (hence AX == 0 most of the time) */
  *ax = 0;
  /* let's look at the exact query */
  DBG("Got query: %02Xh [%02X %02X %02X %02X]\n", query, reqbuff[0], reqbuff[1], reqbuff[2], reqbuff[3]);
  if (query == AL_DISKSPACE) {
    unsigned long long diskspace, freespace;
    DBG("DISKSPACE for drive '%c:'\n", 'A' + reqdrv);
    diskspace = diskinfo(root, &freespace);
    /* limit results to slightly under 2 GiB (otherwise MS-DOS is confused) */
    if (diskspace >= 2lu*1024*1024*1024) diskspace = 2lu*1024*1024*1024 - 1;
    if (freespace >= 2lu*1024*1024*1024) freespace = 2lu*1024*1024*1024 - 1;
    DBG("TOTAL: %llu KiB ; FREE: %llu KiB\n", diskspace >> 10, freespace >> 10);
    *ax = 1; /* AX: media id (8 bits) | sectors per cluster (8 bits) -- MSDOS tolerates only 1 here! */
    wansw[1] = htole16(32768);  /* CX: bytes per sector */
    diskspace >>= 15; /* space to number of 32K clusters */
    freespace >>= 15; /* space to number of 32K clusters */
    wansw[0] = htole16(diskspace); /* BX: total clusters */
    wansw[2] = htole16(freespace); /* DX: available clusters */
    reslen += 6;
  } else if ((query == AL_READFIL) && (reqbufflen == 8)) {  /* AL=08h */
    uint16_t len, fileid;
    uint32_t offset;
    long readlen;
    offset = le32toh(((uint32_t *)reqbuff)[0]);
    fileid = le16toh(wreqbuff[2]);
    len = le16toh(wreqbuff[3]);
    DBG("Asking for %u bytes of the file #%u, starting offset %u\n", len, fileid, offset);
    readlen = readfile(answ, fileid, offset, len);
    if (readlen < 0) {
      fprintf(stderr, "ERROR: invalid handle\n");
      *ax = 5; /* "access denied" */
    } else {
      reslen += readlen;
    }
  } else if ((query == AL_WRITEFIL) && (reqbufflen >= 6)) {  /* AL=09h */
    uint16_t fileid;
    uint32_t offset;
    long writelen;
    offset = le32toh(((uint32_t *)reqbuff)[0]);
    fileid = le16toh(wreqbuff[2]);
    DBG("Writing %u bytes into file #%u, starting offset %u\n", reqbufflen - 6, fileid, offset);
    writelen = writefile(reqbuff + 6, fileid, offset, reqbufflen - 6);
    if (writelen < 0) {
      *ax = 5; /* "access denied" */
    } else {
      wansw[0] = htole16(writelen);
      reslen += 2;
    }
  } else if ((query == AL_LOCKFIL) || (query == AL_UNLOCKFIL)) { /* 0x0A / 0x0B */
    /* I do nothing, except lying that lock/unlock succeeded */
  } else if (query == AL_FINDFIRST) { /* 0x1B */
    struct fileprops fprops;
    char directory[256];
    unsigned short dirss;
    char filemask[16], filemaskfcb[12];
    int offset;
    unsigned fattr;
    unsigned short fpos = 0;
    int flags;
    fattr = reqbuff[0];
    offset = sprintf(directory, "%s/", root);
    /* */
    /* explode the full "\DIR\FILE????.???" search path into directory and mask */
    explodepath(directory + offset, filemask, (char *)reqbuff + 1, reqbufflen - 1);
    lostring(directory + offset, -1);
    lostring(filemask, -1);
    charreplace(directory, '\\', '/');
    /* */
    filename2fcb(filemaskfcb, filemask);
    DBG("FindFirst in '%s'\nfilemask: '%s' (FCB '%s')\nattribs: 0x%2X\n", directory, filemask, pfcb(filemaskfcb), fattr);
    flags = 0;
    if (isroot(root, directory) != 0) flags |= FFILE_ISROOT;
    if (drivesfat[reqdrv] != 0) flags |= FFILE_ISFAT;    
    dirss = getitemss(directory);    
    if ((dirss == 0xffffu) || (findfile(&fprops, dirss, filemaskfcb, fattr, &fpos, flags) != 0)) {
      DBG("No matching file found\n");
      *ax = 0x12; /* 0x12 is "no more files" -- one would assume 0x02 "file not found" would be better, but that's not what MS-DOS 5.x does, some applications rely on a failing FFirst to return 0x12 (for example LapLink 5) */
    } else { /* found a file */
      DBG("found file: FCB '%s' (attr %02Xh)\n", pfcb(fprops.fcbname), fprops.fattr);
      answ[0] = fprops.fattr; /* fattr (1=RO 2=HID 4=SYS 8=VOL 16=DIR 32=ARCH 64=DEVICE) */
      memcpy(answ + 1, fprops.fcbname, 11);
      answ[12] = fprops.ftime & 0xff;
      answ[13] = (fprops.ftime >> 8) & 0xff;
      answ[14] = (fprops.ftime >> 16) & 0xff;
      answ[15] = (fprops.ftime >> 24) & 0xff;
      answ[16] = fprops.fsize & 0xff;         /* fsize */
      answ[17] = (fprops.fsize >> 8) & 0xff;  /* fsize */
      answ[18] = (fprops.fsize >> 16) & 0xff; /* fsize */
      answ[19] = (fprops.fsize >> 24) & 0xff; /* fsize */
      wansw[10] = htole16(dirss); /* dir id */
      wansw[11] = htole16(fpos); /* file position in dir */
      reslen = 24;
    }
  } else if (query == AL_FINDNEXT) { /* 0x1C */
    unsigned short fpos;
    struct fileprops fprops;
    char *fcbmask;
    unsigned char fattr;
    unsigned short dirss;
    int flags;
    dirss = le16toh(wreqbuff[0]);
    fpos = le16toh(wreqbuff[1]);
    fattr = reqbuff[4];
    fcbmask = (char *)reqbuff + 5;
    /* */
    DBG("FindNext looks for nth file %u in dir #%u\nfcbmask: '%s'\nattribs: 0x%2X\n", fpos, dirss, pfcb(fcbmask), fattr);
    flags = 0;
    if (isroot(root, sstoitem(dirss)) != 0) flags |= FFILE_ISROOT;
    if (drivesfat[reqdrv] != 0) flags |= FFILE_ISFAT;
    if (findfile(&fprops, dirss, fcbmask, fattr, &fpos, flags)) {
      DBG("No more matching files found\n");
      *ax = 0x12; /* "no more files" */
    } else { /* found a file */
      DBG("found file: FCB '%s' (attr %02Xh)\n", pfcb(fprops.fcbname), fprops.fattr);
      answ[0] = fprops.fattr; /* fattr (1=RO 2=HID 4=SYS 8=VOL 16=DIR 32=ARCH 64=DEVICE) */
      memcpy(answ + 1, fprops.fcbname, 11);
      answ[12] = fprops.ftime & 0xff;
      answ[13] = (fprops.ftime >> 8) & 0xff;
      answ[14] = (fprops.ftime >> 16) & 0xff;
      answ[15] = (fprops.ftime >> 24) & 0xff;
      answ[16] = fprops.fsize & 0xff;         /* fsize */
      answ[17] = (fprops.fsize >> 8) & 0xff;  /* fsize */
      answ[18] = (fprops.fsize >> 16) & 0xff; /* fsize */
      answ[19] = (fprops.fsize >> 24) & 0xff; /* fsize */
      wansw[10] = htole16(dirss); /* dir id */
      wansw[11] = htole16(fpos);  /* file position in dir */
      reslen = 24;
    }
  } else if ((query == AL_MKDIR) || (query == AL_RMDIR)) { /* MKDIR or RMDIR */
    char directory[256];
    int offset;
    offset = sprintf(directory, "%s/", root);
    /* explode the full "\DIR\FILE????.???" search path into directory and mask */
    memcpy(directory + offset, (char *)reqbuff, reqbufflen);
    directory[offset + reqbufflen] = 0;
    lostring(directory + offset, -1);
    charreplace(directory, '\\', '/');
    if (query == AL_MKDIR) {
      DBG("MKDIR '%s'\n", directory);
      if (makedir(directory) != 0) {
        *ax = 29;
        printf("MKDIR Error: %s\n", strerror(errno));
      }
    } else {
      DBG("RMDIR '%s'\n", directory);
      if (remdir(directory) != 0) {
        *ax = 29;
        printf("RMDIR Error: %s\n", strerror(errno));
      }
    }
  } else if (query == AL_CHDIR) { /* check if dir exist, return ax=0 if so, ax=3 otherwise */
    char directory[256];
    int offset;
    offset = sprintf(directory, "%s/", root);
    memcpy(directory + offset, (char *)reqbuff, reqbufflen);
    directory[offset + reqbufflen] = 0;
    lostring(directory + offset, -1);
    charreplace(directory, '\\', '/');
    DBG("CHDIR '%s'\n", directory);
    /* try to chdir to this dir - if works, then we're good */
    if (changedir(directory) != 0) {
      printf("CHDIR Error (%s): %s\n", directory, strerror(errno));
      *ax = 3;
    }
  } else if (query == AL_CLSFIL) { /* AL_CLSFIL (0x06) */
    /* I do nothing, since I do not keep any open files around anyway.
     * just say 'ok' by sending back AX=0 */
    DBG("CLOSE FILE\n");
    *ax = 0;
  } else if ((query == AL_SETATTR) && (reqbufflen > 1)) { /* AL_SETATTR (0x0E) */
    char fname[512];
    int offset;
    unsigned char fattr;
    fattr = reqbuff[0];
    /* get full file path */
    offset = sprintf(fname, "%s/", root);
    memcpy(fname + offset, (char *)reqbuff + 1, reqbufflen - 1);
    fname[offset + reqbufflen - 1] = 0;
    lostring(fname + offset, -1);
    charreplace(fname, '\\', '/');
    DBG("SETATTR [file: '%s', attr: 0x%02X]\n", fname, fattr);
    
      if (setitemattr(fname, fattr) != 0) *ax = 2;
    
  } else if ((query == AL_GETATTR) && (reqbufflen > 0)) { /* AL_GETATTR (0x0F) */
    char fname[512];
    int offset;
    struct fileprops fprops;
    /* get full file path */
    offset = sprintf(fname, "%s/", root);
    memcpy(fname + offset, (char *)reqbuff, reqbufflen);
    fname[offset + reqbufflen] = 0;
    lostring(fname + offset, -1);
    charreplace(fname, '\\', '/');
    DBG("GETATTR on file: '%s'\n", fname);
    /* */
    if (getitemattr(fname, &fprops) == 0xFF) {
      DBG("no file found\n");
      *ax = 2;
    } else {
      DBG("found it (%lu bytes, attr 0x%02X)\n", fprops.fsize, fprops.fattr);
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
  } else if ((query == AL_RENAME) && (reqbufflen > 2)) { /* AL_RENAME (0x11) */
    /* query is LSSS...DDD... */
    char fn1[1024], fn2[1024];
    int fn1len, fn2len, offset;
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
      DBG("RENAME src='%s' dst='%s'\n", fn1, fn2);
      /* if fn2 destination exists, abort with errcode=5 (as does MS-DOS 5) */
      if (getitemattr(fn2, NULL) != 0xff) {
        DBG("ERROR: '%s' exists already\n", fn2);
        *ax = 5;
      } else {
        DBG("'%s' doesn't exist -> proceed with renaming\n", fn2);
        if (renfile(fn1, fn2) != 0) *ax = 5;
      }
    } else {
      *ax = 2;
    }
  } else if (query == AL_DELETE) {
    char fullpathname[512];
    int offset;
    struct fileprops fprops;
    /* compute full path/file first */
    offset = sprintf(fullpathname, "%s/", root);
    memcpy(fullpathname + offset, reqbuff, reqbufflen);
    fullpathname[reqbufflen + offset] = 0;
    lostring(fullpathname + offset, -1);
    charreplace(fullpathname, '\\', '/');
    DBG("DELETE '%s'\n", fullpathname);
    /* is it read-only? */    
    getitemattr(fullpathname,&fprops);
    if ((fprops.fattr & FAT_RO) || (fprops.fattr & FAT_DIR)) {
      *ax = 5; /* "access denied" */
    } else if (delfiles(fullpathname) < 0) {
      *ax = 2;
    }
  } else if ((query == AL_OPEN) || (query == AL_CREATE) || (query == AL_SPOPNFIL)) { /* OPEN is only about "does this file exist", and CREATE "please create or truncate this file", while SPOPNFIL is a combination of both with extra flags */
    struct fileprops fprops;
    char directory[256];
    char fname[16], fnamefcb[12];
    char fullpathname[512];
    int offset;
    int fileres;
    unsigned short stackattr, actioncode, spopen_openmode, spopres = 0;
    unsigned char resopenmode;
    /* fetch args */
    stackattr = le16toh(wreqbuff[0]);
    actioncode = le16toh(wreqbuff[1]);
    spopen_openmode = le16toh(wreqbuff[2]);
    /* compute full path/file */
    offset = sprintf(fullpathname, "%s/", root);
    memcpy(fullpathname + offset, reqbuff + 6, reqbufflen - 6);
    fullpathname[reqbufflen + offset - 6] = 0;
    lostring(fullpathname + offset, -1);
    charreplace(fullpathname, '\\', '/');
    /* compute directory and 'search mask' */
    offset = sprintf(directory, "%s/", root);
    explodepath(directory + offset, fname, (char *)reqbuff+6, reqbufflen-6);
    lostring(directory + offset, -1);
    lostring(fname, -1);
    charreplace(directory, '\\', '/');
    /* does the directory exist? */
    if (changedir(directory) != 0) {
      DBG("open/create/spop failed because directory does not exist\n");
      *ax = 3; /* "path not found" */
    } else {
      /* compute a FCB-style version of the filename */
      filename2fcb(fnamefcb, fname);
      /* */
      DBG("stack word: %04X\n", stackattr);
      DBG("looking for file '%s' (FCB '%s') in '%s'\n", fname, pfcb(fnamefcb), directory);
      /* open or create file, depending on exact subfunction */
      if (query == AL_CREATE) {
        DBG("CREATEFIL / stackattr (attribs)=%04Xh / fn='%s'\n", stackattr, fullpathname);
        fileres = createfile(&fprops, directory, fname, stackattr & 0xff);
        resopenmode = 2; /* read/write */
      } else if (query == AL_SPOPNFIL) {
        /* actioncode contains instructions about how to behave...
         *   high nibble = action if file does NOT exist:
         *     0000 fail
         *     0001 create
         *   low nibble = action if file DOES exist
         *     0000 fail
         *     0001 open
         *     0010 replace/open */
        int attr;
        DBG("SPOPNFIL / stackattr=%04Xh / action=%04Xh / openmode=%04Xh / fn='%s'\n", stackattr, actioncode, spopen_openmode, fullpathname);
        /* see if file exists (and is a file) */
        attr = getitemattr(fullpathname, &fprops);
        resopenmode = spopen_openmode & 0x7f; /* that's what PHANTOM.C does */
        if (attr == 0xff) { /* file not found - look at high nibble of action code */
          DBG("file doesn't exist -> ");
          if ((actioncode & 0xf0) == 16) { /* create */
            DBG("create file\n");
            fileres = createfile(&fprops, directory, fname, stackattr & 0xff);
            if (fileres == 0) spopres = 2; /* spopres == 2 means 'file created' */
          } else { /* fail */
            DBG("fail\n");
            fileres = 1;
          }
        } else if ((attr & (FAT_VOL | FAT_DIR)) != 0) { /* item is a DIR or a VOL */
          DBG("fail: item '%s' is either a DIR or a VOL\n", fullpathname);
          fileres = 1;
        } else { /* file found (not a VOL, not a dir) - look at low nibble of action code */
          DBG("file exists already (attr %02Xh) -> ", attr);
          if ((actioncode & 0x0f) == 1) { /* open */
            DBG("open file\n");
            fileres = 0;
            spopres = 1; /* spopres == 1 means 'file opened' */
          } else if ((actioncode & 0x0f) == 2) { /* truncate */
            DBG("truncate file\n");
            fileres = createfile(&fprops, directory, fname, stackattr & 0xff);
            if (fileres == 0) spopres = 3; /* spopres == 3 means 'file truncated' */
          } else { /* fail */
            DBG("fail\n");
            fileres = 1;
          }
        }
      } else { /* simple 'OPEN' */
        int attr;
        DBG("OPENFIL / stackattr (open modes)=%04Xh / fn='%s'\n", stackattr, fullpathname);
        resopenmode = stackattr & 0xff;
        attr = getitemattr(fullpathname, &fprops);
        /* check that item exists, and is neither a volume nor a directory */
        if ((attr != 0xff) && ((attr & (FAT_VOL | FAT_DIR)) == 0)) {
          fileres = 0;
        } else {
          fileres = 1;
        }
      }
      if (fileres != 0) {
        DBG("open/create/spop failed with fileres = %d\n", fileres);
        *ax = 2;
      } else { /* success (found a file, created it or truncated it) */
        unsigned short fileid;
        fileid = getitemss(fullpathname);
        DBG("found file: FCB '%s' (id %04X)\n", pfcb(fprops.fcbname), fileid);
        DBG("     fsize: %lu\n", fprops.fsize);
        DBG("     fattr: %02Xh\n", fprops.fattr);
        DBG("     ftime: %04lX\n", fprops.ftime);
        if (fileid == 0xffffu) {
          printf("ERROR: failed to get a proper fileid!\n");
          return(-1);
        }
        answ[reslen++] = fprops.fattr; /* fattr (1=RO 2=HID 4=SYS 8=VOL 16=DIR 32=ARCH 64=DEVICE) */
        memcpy(answ + reslen, fprops.fcbname, 11);
        reslen += 11;
        answ[reslen++] = fprops.ftime & 0xff; /* time: YYYYYYYM MMMDDDDD hhhhhmmm mmmsssss */
        answ[reslen++] = (fprops.ftime >> 8) & 0xff;
        answ[reslen++] = (fprops.ftime >> 16) & 0xff;
        answ[reslen++] = (fprops.ftime >> 24) & 0xff;
        answ[reslen++] = fprops.fsize & 0xff;         /* fsize */
        answ[reslen++] = (fprops.fsize >> 8) & 0xff;  /* fsize */
        answ[reslen++] = (fprops.fsize >> 16) & 0xff; /* fsize */
        answ[reslen++] = (fprops.fsize >> 24) & 0xff; /* fsize */
        answ[reslen++] = fileid & 0xff;
        answ[reslen++] = fileid >> 8;
        /* CX result (only relevant for SPOPNFIL) */
        answ[reslen++] = spopres & 0xff;
        answ[reslen++] = spopres >> 8;
        answ[reslen++] = resopenmode;
      }
    }
  } else if ((query == AL_SKFMEND) && (reqbufflen == 6)) { /* SKFMEND (0x21) */
    /* translate a 'seek from end' offset into an 'seek from start' offset */
    int32_t offs = le32toh(((uint32_t *)reqbuff)[0]);
    long fsize;
    unsigned short fss = le16toh(((unsigned short *)reqbuff)[2]);
    DBG("SKFMEND on file #%u at offset %d\n", fss, offs);
    /* if arg is positive, zero it out */
    if (offs > 0) offs = 0;
    /* */
    fsize = getfopsize(fss);
    if (fsize < 0) {
      DBG("ERROR: file not found or other error\n");
      *ax = 2;
    } else { /* compute new offset and send it back */
      DBG("file #%u is %lu bytes long\n", fss, fsize);
      offs += fsize;
      if (offs < 0) offs = 0;
      DBG("new offset: %d\n", offs);
      ((uint32_t *)answ)[0] = htole32(offs);
      reslen = 4;
    }
  } else { /* unknown query - ignore */
    return(-1);
  }
  return(reslen + 60);
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

/* compare two chunks of data, returns 0 if data is the same, non-zero otherwise */
static int cmpdata(unsigned char *d1, unsigned char *d2, int len) {
  while (len-- > 0) {
    if (*d1 != *d2) return(1);
    d1++;
    d2++;
  }
  return(0);
}

/* compute the BSD checksum of l bytes starting at ptr */
static unsigned short bsdsum(unsigned char *ptr, unsigned short l) {
  unsigned short res = 0;
  for (; l > 0; l--) {
    res = (res << 15) | (res >> 1);
    res += *ptr;
    ptr++;
  }
  return(res);
}


/* daemonize the process, return 0 on success, non-zero otherwise */
static int daemonize(void) {
  pid_t mypid;

  /* I don't want to get notified about SIGHUP */
  signal(SIGHUP, SIG_IGN);

  /* fork off */
  mypid = fork();
  if (mypid == 0) { /* I'm the child, do nothing */
    /* nothing to do - just continue */
  } else if (mypid > 0) { /* I'm the parent - quit now */
    exit(0);
  } else {  /* error condition */
    return(-2);
  }
  return(0);
}

/* generates a formatted MAC address printout and returns a static buffer */
static char *printmac(unsigned char *b) {
  static char macbuf[18];
  sprintf(macbuf, "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
  return(macbuf);
}


void etherdfs_init() {
  printf("EtherDFS Initialized.\n");
  root[2]="/";
}

void etherdfs_process(char *buff,uint16_t len) {
  int i;
  unsigned char cksumflag;
  unsigned short edf5framelen;  
  struct struct_answcache *cacheptr;
    
    if (len < 60) return;
    /* validate this is for me (or broadcast) */
    //if ((cmpdata(mymac, buff, 6) != 0) && (cmpdata((unsigned char *)"\xff\xff\xff\xff\xff\xff", buff, 6) != 0)) continue; /* skip anything that is not for me */
    /* is this EDF5 ?*/
    if (((unsigned short *)buff)[6] != lwip_htons(0xEDF5)) {
      printf("Error: Received non-EDF5 frame\n");
      return;
    }
    /* validate protocol version matches what I expect */
    if ((buff[56] & 127) != PROTOVER) {
      printf("Error: unsupported protocol version from %s\n", buff + 6);
      return;
    }
    cksumflag = buff[56] >> 7;
    /* trim of padding, if any, or reject frame if it came truncated */
    edf5framelen = le16toh(((unsigned short *)buff)[26]);
    if (edf5framelen == 0) {
      /* nothing to do, edf5framelen is not provided */
    } else if (edf5framelen > len) { /* frame seems truncated */
      printf("Error: received a truncated frame from %s\n", printmac(buff + 6));
      return;
    } else if (edf5framelen < 60) { /* obvious error */
      printf("Error: received a malformed frame from %s\n", printmac(buff + 6));
      return;
    } else { /* edf5framelen seems sane, use it instead of the Ethernet lenght */
      #if DEBUG > 0
        if (len != edf5framelen) {
          DBG("Note: Received frame with padding from %s (edf5len = %u, ethernet len = %u)\n", printmac(buff + 6), edf5framelen, len);
        }
      #endif
      len = edf5framelen;
    }
    /* */
  #if DEBUG > 0
    DBG("Received frame of %d bytes (cksum = %s)\n", len, (cksumflag != 0)?"ENABLED":"DISABLED");
    dumpframe(buff, len);
  #endif
   #if SIMLOSS > 0
    /* simulated frame LOSS (input) */
    if ((rand() & 31) == 0) {
      fprintf(stderr, "INPUT LOSS!\n");
      continue;
    }
   #endif
    /* validate the CKSUM, if any */
    if (cksumflag != 0) {
      unsigned short cksum_remote, cksum_mine;
      cksum_mine = bsdsum(buff + 56, len - 56);
      cksum_remote = le16toh(((unsigned short *)buff)[27]);
      if (cksum_mine != cksum_remote) {
        printf("CHECKSUM MISMATCH! Computed: 0x%02Xh Received: 0x%02Xh\n", cksum_mine, cksum_remote);
        return;
      }
    }

  cacheptr = findcacheentry(buff + 6);
  /* process frame */
  len = process(cacheptr, buff, len, etherdfs_mac, root);
  /* update cache entry */
  if (len >= 0) {
    cacheptr->len = len;
    cacheptr->timestamp = time(NULL);
  } else {
    cacheptr->len = 0;
  }
  /* */
  #if SIMLOSS > 0
  /* simulated frame LOSS (output) */
  if ((rand() & 31) == 0) {
    fprintf(stderr, "OUTPUT LOSS!\n");
    continue;
  }
  #endif
  DBG("---------------------------------\n");
  if (len > 0) {
    /* fill in frame's length */
    cacheptr->frame[52] = len & 0xff;
    cacheptr->frame[53] = (len >> 8) & 0xff;
    /* fill in checksum into the answer */
    if (cksumflag != 0) {
      unsigned short newcksum = bsdsum(cacheptr->frame + 56, len - 56);
      cacheptr->frame[54] = newcksum & 0xff;
      cacheptr->frame[55] = (newcksum >> 8) & 0xff;
      cacheptr->frame[56] |= 128; /* make sure to set the CKS bit */
    } else {
      cacheptr->frame[54] = 0;
      cacheptr->frame[55] = 0;
      cacheptr->frame[56] &= 127; /* make sure to reset the CKS bit */
    }
#if DEBUG > 0
    DBG("Sending back an answer of %d bytes\n", len);
    dumpframe(cacheptr->frame, len);
#endif
  i=ne2000_rx(cacheptr->frame,len);  
      //ne2000_rx_frame(nic, buf, len);             
//KEVIN      i = send(sock, cacheptr->frame, len, 0);

    if (i < 0) {
      fprintf(stderr, "ERROR: send() returned %d (%s)\n", i, strerror(errno));
    } else if (i != len) {
      fprintf(stderr, "ERROR: send() sent less than expected (%d != %d)\n", i, len);
    }
  } else {
    fprintf(stderr, "Query ignored (result: %d)\n", len);
  }
  DBG("---------------------------------\n");  
}





int zmain(int argc, char **argv) {
  int sock, len, i;
  unsigned char buff[2048];
  unsigned char cksumflag;
  unsigned short edf5framelen;
  unsigned char mymac[6];
  char *intname, *root[26];
  struct struct_answcache *cacheptr;
  int opt;
  
  /* load all "virtual drive" paths */
  for (i = 0; i < 26; i++) root[i] = NULL;
  for (i = 0; i < (argc - optind); i++) {
    char tmppath[PATH_MAX];
    if (realpath(argv[i + optind], tmppath) == NULL) {
      fprintf(stderr, "ERROR: failed to resolve path '%s'\n", argv[i + optind]);
      return(1);
    }
    //root[i + 2] = strdup(tmppath);
//    if (isfat(root[i + 2]) == 0) {
//      drivesfat[i + 2] = 1;
//    } else {
//      drivesfat[i + 2] = 0;
//      fprintf(stderr, "WARNING: the path '%s' doesn't seem to be stored on a FAT filesystem! DOS attributes won't be supported.\n\n", root[i + 2]);
//    }
  }

  printf("Listening on '%s' [%s]\n", intname, printmac(mymac));
  for (i = 2; i < 26; i++) {
    if (root[i] == NULL) break;
    printf("Drive %c: mapped to %s\n", 'A' + i, root[i]);
  }
  /* main loop */
  while (terminationflag == 0) {
    struct timeval stimeout = {10, 0}; /* set timeout to 10s */
    /* prepare the set of descriptors to be monitored later through select() */
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    /* wait for something to happen on my socket */
    select(sock + 1, &fdset, NULL, &fdset, &stimeout);
//KEVIN    len = recv(sock, buff, sizeof(buff), MSG_DONTWAIT);
    if (len < 60) continue; /* restart if less than 60 bytes or negative */
    /* validate this is for me (or broadcast) */
    if ((cmpdata(mymac, buff, 6) != 0) && (cmpdata((unsigned char *)"\xff\xff\xff\xff\xff\xff", buff, 6) != 0)) continue; /* skip anything that is not for me */
    /* is this EDF5 ?*/
    if (((unsigned short *)buff)[6] != lwip_htons(0xEDF5)) {
      fprintf(stderr, "Error: Received non-EDF5 frame\n");
      continue;
    }
    /* validate protocol version matches what I expect */
    if ((buff[56] & 127) != PROTOVER) {
      fprintf(stderr, "Error: unsupported protocol version from %s\n", buff + 6);
      continue;
    }
    cksumflag = buff[56] >> 7;
    /* trim of padding, if any, or reject frame if it came truncated */
    edf5framelen = le16toh(((unsigned short *)buff)[26]);
    if (edf5framelen == 0) {
      /* nothing to do, edf5framelen is not provided */
    } else if (edf5framelen > len) { /* frame seems truncated */
      fprintf(stderr, "Error: received a truncated frame from %s\n", printmac(buff + 6));
      continue;
    } else if (edf5framelen < 60) { /* obvious error */
      fprintf(stderr, "Error: received a malformed frame from %s\n", printmac(buff + 6));
      continue;
    } else { /* edf5framelen seems sane, use it instead of the Ethernet lenght */
      #if DEBUG > 0
        if (len != edf5framelen) {
          DBG("Note: Received frame with padding from %s (edf5len = %u, ethernet len = %u)\n", printmac(buff + 6), edf5framelen, len);
        }
      #endif
      len = edf5framelen;
    }
    /* */
  #if DEBUG > 0
    DBG("Received frame of %d bytes (cksum = %s)\n", len, (cksumflag != 0)?"ENABLED":"DISABLED");
    dumpframe(buff, len);
  #endif
   #if SIMLOSS > 0
    /* simulated frame LOSS (input) */
    if ((rand() & 31) == 0) {
      fprintf(stderr, "INPUT LOSS!\n");
      continue;
    }
   #endif
    /* validate the CKSUM, if any */
    if (cksumflag != 0) {
      unsigned short cksum_remote, cksum_mine;
      cksum_mine = bsdsum(buff + 56, len - 56);
      cksum_remote = le16toh(((unsigned short *)buff)[27]);
      if (cksum_mine != cksum_remote) {
        fprintf(stderr, "CHECKSUM MISMATCH! Computed: 0x%02Xh Received: 0x%02Xh\n", cksum_mine, cksum_remote);
        continue;
      }
    }
    /* */
    cacheptr = findcacheentry(buff + 6);
    /* process frame */
    len = process(cacheptr, buff, len, mymac, root);
    /* update cache entry */
    if (len >= 0) {
      cacheptr->len = len;
      cacheptr->timestamp = time(NULL);
    } else {
      cacheptr->len = 0;
    }
    /* */
   #if SIMLOSS > 0
    /* simulated frame LOSS (output) */
    if ((rand() & 31) == 0) {
      fprintf(stderr, "OUTPUT LOSS!\n");
      continue;
    }
   #endif
    DBG("---------------------------------\n");
    if (len > 0) {
      /* fill in frame's length */
      cacheptr->frame[52] = len & 0xff;
      cacheptr->frame[53] = (len >> 8) & 0xff;
      /* fill in checksum into the answer */
      if (cksumflag != 0) {
        unsigned short newcksum = bsdsum(cacheptr->frame + 56, len - 56);
        cacheptr->frame[54] = newcksum & 0xff;
        cacheptr->frame[55] = (newcksum >> 8) & 0xff;
        cacheptr->frame[56] |= 128; /* make sure to set the CKS bit */
      } else {
        cacheptr->frame[54] = 0;
        cacheptr->frame[55] = 0;
        cacheptr->frame[56] &= 127; /* make sure to reset the CKS bit */
      }
  #if DEBUG > 0
      DBG("Sending back an answer of %d bytes\n", len);
      dumpframe(cacheptr->frame, len);
  #endif
  printf("ETHERDFS SEND\n");
        //ne2000_rx_frame(nic, buf, len);             
//KEVIN      i = send(sock, cacheptr->frame, len, 0);
      if (i < 0) {
        fprintf(stderr, "ERROR: send() returned %d (%s)\n", i, strerror(errno));
      } else if (i != len) {
        fprintf(stderr, "ERROR: send() sent less than expected (%d != %d)\n", i, len);
      }
    } else {
      fprintf(stderr, "Query ignored (result: %d)\n", len);
    }
    DBG("---------------------------------\n");
  }
    
}
