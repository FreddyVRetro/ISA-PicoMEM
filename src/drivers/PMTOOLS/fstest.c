#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dos.h>
#include <time.h>
#include <io.h>
#include <stdbool.h>


#define ERR_FILE_NOTFOUND 0x02
#define ERR_PATH_NOTFOUND 0x03
#define ERR_NOHANDLE      0x04
#define ERR_ACC_DENIED    0x05
#define ERR_INVALID_HAND  0x06
#define ERR_NOMORE_FILES  0x12
#define ERR_INVALID_DRIVE 0x15


#define YEAR(t)   (((t & 0xFE00) >> 9) + 1980)
#define MONTH(t)  ((t & 0x01E0) >> 5)
#define DAY(t)    (t & 0x001F)
#define HOUR(t)   ((t & 0xF800) >> 11)
#define MINUTE(t) ((t & 0x07E0) >> 5)
#define SECOND(t) ((t & 0x001F) << 1)


bool test_fileio()
{
  int handle, err;
  unsigned int written, readcnt;
  struct stat buf;
  char test_txt[] = "D:/TEST.TXT";
  char readbuff[256] = {0}; // Initialize buffer to zero

  unsigned date, time, attribute;
  struct dosdate_t ddate;
  struct dostime_t dtime;

  printf("File functions test\n");

  printf("Open D:/TEST.TXT (O_RDWR): ");
  err = _dos_open(test_txt, O_RDWR, &handle);

  if (err != 0) 
  {
    printf("Error :%x\n", err);

    if (err == ERR_FILE_NOTFOUND) // no file
    {
      printf("File not found\n");
      printf("Create D:/TEST.TXT (O_CREAT): ");
      err = _dos_creat(test_txt, _A_NORMAL, &handle);
      if (err == 0) 
        printf("OK\n");
      else 
      {
        printf("Error :%x\n", err);
        return false;
      }
    } 
    else 
      return false;
  } 
  else 
    printf("OK\n");

  printf("Add text to the file ");
  err = _dos_write(handle, "Hello World\n", 12, &written);
  if (err == 0) 
    printf("OK\n");
  else 
  {
    printf("Error :%x\n", err);
    return false;
  }
  _dos_write(handle, "2eme phrase\n", 13, &written);

  // Move file pointer to the beginning
  lseek(handle, 0, SEEK_SET);

  printf("Read the file ");
  err = _dos_read(handle, readbuff, 256, &readcnt);
  if (err == 0) 
  {
    printf("OK\n");
    printf("Read %d bytes\n", readcnt);
    printf("Content: %s\n", readbuff);
  } 
  else 
  {
    printf("Error :%x\n", err);
    _dos_close(handle);
    return false;
  }

  printf("D:/TEST.TXT Infos:\n");
  _dos_getftime( handle, &date, &time );
  printf( "The file was last modified on %d/%d/%d",
           MONTH(date), DAY(date), YEAR(date) );
  printf( " at %.2d:%.2d:%.2d\n",
           HOUR(time), MINUTE(time), SECOND(time) );


  _dos_getfileattr(test_txt, &attribute );
  printf("File attributes: %c%c%c%c%c\n",
         (attribute & _A_SUBDIR) ? 'D' : '-',
         (attribute & _A_RDONLY) ? 'R' : '-',
         (attribute & _A_HIDDEN) ? 'H' : '-',
         (attribute & _A_SYSTEM) ? 'S' : '-',
         (attribute & _A_ARCH  ) ? 'A' : '-');

    /* Get and display the current date and time */
/*
    _dos_getdate( &ddate );
    _dos_gettime( &dtime );
    printf( "The date (MM-DD-YYYY) is: %d-%d-%d\n",
                  ddate.month, ddate.day, ddate.year );
    printf( "The time (HH:MM:SS) is: %.2d:%.2d:%.2d\n",
                  dtime.hour, dtime.minute, dtime.second );
*/

  printf("Close the file ");
  _dos_close(handle);

  return true;
}

bool list_files(char *search,int mode,bool dototal)
{
  struct find_t fileinfo;
  int err,handle,count;

  err=_dos_findfirst(search, _A_NORMAL | _A_SYSTEM | _A_HIDDEN | _A_SUBDIR, &fileinfo);
  
  count=0;
  if (err==0) {
     while (err==0)
      {
        count++;
        printf("%-20s",fileinfo.name);

        printf(" %.2d/%.2d/%d, %.2d:%.2d:%.2d",
               MONTH(fileinfo.wr_date ), DAY(fileinfo.wr_date), YEAR(fileinfo.wr_date),
               HOUR(fileinfo.wr_time) , MINUTE(fileinfo.wr_time), SECOND(fileinfo.wr_time));
        printf(" %c%c%c%c%c\n",
               (fileinfo.attrib & _A_SUBDIR) ? 'D' : '-',
               (fileinfo.attrib & _A_RDONLY) ? 'R' : '-',
               (fileinfo.attrib & _A_HIDDEN) ? 'H' : '-',
               (fileinfo.attrib & _A_SYSTEM) ? 'S' : '-',
               (fileinfo.attrib & _A_ARCH  ) ? 'A' : '-');
        err=_dos_findnext(&fileinfo);
      }
  }
if (dototal) 
  {
    printf("Total files: %d\n",count);
    disksize(5);  // 5, E:
  }
return true;
}

 void disksize(unsigned char drv)
 {
 struct diskfree_t disk_data;
 /* get information about drive 5 (the E drive) */
 if( _dos_getdiskfree( drv, &disk_data ) == 0 ) {
 printf( "total clusters: %u\n",
 disk_data.total_clusters );
 printf( "available clusters: %u\n",
 disk_data.avail_clusters );
 printf( "sectors/cluster: %u\n",
 disk_data.sectors_per_cluster );
 printf( "bytes per sector: %u\n",
 disk_data.bytes_per_sector );
 } else {
 printf( "Invalid drive specified\n" );
 }
 }

void test_files()
{
    int handle, err;
    struct stat buf;
    char test_txt[]="D:/TEST.TXT";
    char test_dir[]="D:/TESTDIR";

    printf("File fonctions test");

    printf("Open D:/TEST.TXT (O_RDONLY): ");
    err = _dos_open(test_txt,O_RDONLY,&handle);
    if (err != 0) printf("Error :%x\n",err);
     else printf("OK\n");
}


main()
{

//printf("E: size\n");
//disksize();

printf("Run dir E:/*.*\n");
list_files("D:/*.*",_A_NORMAL | _A_SYSTEM | _A_HIDDEN | _A_SUBDIR,true);
printf("Run dir E:/*.TXT\n");
list_files("D:/*.TXT",_A_NORMAL | _A_SYSTEM | _A_HIDDEN | _A_SUBDIR,true);


test_fileio();

}
