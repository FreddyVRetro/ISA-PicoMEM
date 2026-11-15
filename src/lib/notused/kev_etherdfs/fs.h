/*
 *
 */

#ifndef FS_H_SENTINEL
#define FS_H_SENTINEL

struct fileprops {
  char fcbname[12];  /* FCB-style file name (FILE0001TXT) */
  unsigned long fsize;
  unsigned long ftime;
  unsigned char fattr;
};

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

/* returns the "start sector" of a filesystem item (file or directory).
 * returns 0xffff on error */
unsigned short getitemss(char *f);

char *sstoitem(unsigned short ss);

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

/* searches for file matching template tmpl in directory dss (dss is the starting sector of the directory, as obtained via getitemss) with attribute attr, fills 'out' with the nth match. returns 0 on success, non-zero otherwise. */
int findfile(struct fileprops *f, unsigned short dss, char *tmpl, unsigned char attr, unsigned short *fpos, int flags);

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

/* reads len bytes from file fname starting offset, writes to buff. returns
 * amount of bytes read or a negative value on error. */
long readfile(unsigned char *buff, unsigned short fss, unsigned long offset, unsigned short len);

/* writes len bytes from buff to file fname, starting at offset. returns
 * amount of bytes written or a negative value on error. */
long writefile(unsigned char *buff, unsigned short fss, unsigned long offset, unsigned short len);

/* remove all files matching the pattern, returns the number of removed files if any found,
 * or -1 on error or if no matching file found */
int delfiles(char *pattern);

/* rename fn1 into fn2 */
int renfile(char *fn1, char *fn2);

/* returns the size of an open file (or -1 on error) */
long getfopsize(unsigned short fss);

#endif
