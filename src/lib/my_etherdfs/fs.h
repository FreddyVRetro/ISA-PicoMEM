/*
 *
 */

#define ERR_FILE_NOTFOUND  0x02
#define ERR_PATH_NOTFOUND  0x03
#define ERR_NOHANDLE       0x04
#define ERR_ACC_DENIED     0x05
#define ERR_INVALID_HAND   0x06
#define ERR_NOMORE_FILES   0x12
#define ERR_INVALID_DRIVE  0x0B
#define ERR_DRIVE_NOTREADY 0x15

#ifndef FS_H_SENTINEL
#define FS_H_SENTINEL

struct fileprops {
  char fcbname[12];  /* FCB-style file name (FILE0001TXT) */
  unsigned long fsize;  // Size
  unsigned long ftime;  // Date + Time
  unsigned char fattr;
};

/*
Values for DOS extended error code:
---DOS 2.0+ ---
 00h (0)   no error
 01h (1)   function number invalid
 02h (2)   file not found
 03h (3)   path not found
 04h (4)   too many open files (no handles available)
 05h (5)   access denied
 06h (6)   invalid handle
 07h (7)   memory control block destroyed
 08h (8)   insufficient memory
 09h (9)   memory block address invalid
 0Ah (10)  environment invalid (usually >32K in length)
 0Bh (11)  format invalid
 0Ch (12)  access code invalid
 0Dh (13)  data invalid
 0Eh (14)  reserved
 0Eh (14)  (PTS-DOS 6.51+, S/DOS 1.0+) fixup overflow
 0Fh (15)  invalid drive
 10h (16)  attempted to remove current directory
 11h (17)  not same device
 12h (18)  no more files
---DOS 3.0+ (INT 24 errors)---
 13h (19)  disk write-protected
 14h (20)  unknown unit
 15h (21)  drive not ready
 16h (22)  unknown command
 17h (23)  data error (CRC)
 18h (24)  bad request structure length
 19h (25)  seek error
 1Ah (26)  unknown media type (non-DOS disk)
 1Bh (27)  sector not found
 1Ch (28)  printer out of paper
 1Dh (29)  write fault
 1Eh (30)  read fault
 1Fh (31)  general failure
 20h (32)  sharing violation
 21h (33)  lock violation
 22h (34)  disk change invalid (ES:DI -> media ID structure)(see #01681)
 23h (35)  FCB unavailable
 23h (35)  (PTS-DOS 6.51+, S/DOS 1.0+) bad FAT
 24h (36)  sharing buffer overflow
 25h (37)  (DOS 4.0+) code page mismatch
 26h (38)  (DOS 4.0+) cannot complete file operation (EOF / out of input)
 27h (39)  (DOS 4.0+) insufficient disk space
 28h-31h   reserved
*/

/* DOS/FAT attribs: 1=RO 2=HID 4=SYS 8=VOL 16=DIR 32=ARCH 64=DEVICE */
#define FAT_RO   1
#define FAT_HID  2
#define FAT_SYS  4
#define FAT_VOL  8
#define FAT_DIR 16
#define FAT_ARC 32
#define FAT_DEV 64

/* flags used with findfile() calls */
#define FFILE_ISROOT 1
#define FFILE_ISFAT  2

#define DIR_MAX 256

void init_fobj();

/* returns the "start sector" of a filesystem item (file or directory).
 * returns 0xffff on error */
// unsigned short getitemss(char *f);

// char *sstoitem(unsigned short ss);

/* turns a character c into its upper-case variant */
char upchar(char c);

/* translates a filename string into a fcb-style block ("FILE0001TXT") */
void filename2fcb(char *d, char *s);

/* provides DOS-like attributes for item i, as well as size, filling fprops
 * accordingly. returns item's attributes or 0xff on error.
 * DOS attr flags: 1=RO 2=HID 4=SYS 8=VOL 16=DIR 32=ARCH 64=DEVICE */
unsigned char getitemattr(char *i, struct fileprops *fprops);

/* set attributes fattr on file i. returns 0 on success, non-zero otherwise. */
int setitemattr(char *i, unsigned char fattr);

// Find file (true findfirst, false findnext)
int fs_find(bool Dofindfirst, char *path, char *pattern, struct fileprops *f, unsigned char attrm);

/* searches for file matching template tmpl in directory dss (dss is the starting sector of the directory, as obtained via getitemss) with attribute attr, fills 'out' with the nth match. returns 0 on success, non-zero otherwise. */
//int findfile(struct fileprops *f, unsigned short dss, char *tmpl, unsigned char attr, unsigned short *fpos, int flags);

/* creates or truncates a file f in directory d with attributes attr. returns 0 on success (and f filled), non-zero otherwise. */
int createfile(struct fileprops *f, char *d, char *fn, unsigned char attr);

/* returns disks total size, in bytes, or 0 on error. also sets dfree to the
 * amount of available bytes */
unsigned long long diskinfo(char *path, unsigned long long *dfree);

/* try to create directory, return 0 on success, non-zero otherwise */
int makedir(char *d);

/* try to remove directory, return 0 on success, non-zero otherwise */
int remdir(char *d);

/* change to directory d, return 0 if worked, non-zero otherwise (used
 * essentially to check whether the directory exists or not) */
int changedir(char *d);

int openfile(struct fileprops *f, char *filepath, unsigned char f_mode);

void closefile(unsigned char fid);

/* reads len bytes from file fname starting offset, writes to buff. returns
 * amount of bytes read or a negative value on error. */

int readfile(unsigned char *buff, unsigned char fid, uint32_t offset, uint16_t len);

/* writes len bytes from buff to file fname, starting at offset. returns
 * amount of bytes written or a negative value on error. */
long writefile(unsigned char *buff, unsigned short fid, unsigned long offset, unsigned short len);

/* remove all files matching the pattern, returns the number of removed files if any found,
 * or -1 on error or if no matching file found */
int delfiles(char *pattern);

/* rename fn1 into fn2 */
int renfile(char *fn1, char *fn2);

/* returns the size of an open file (or -1 on error) */
long getfopsize(unsigned short fid);

/* Converts a path full of lowercase 8.3 names to the host name, provided it exists */
int shorttolong(char *dst, char *src, const char *root);

#endif
