/*************************************************************************
*
*  FILE : DMAW.C  ver 1.01  (Aug 15, 94)
*
*  Copyright (C) 1994-96 Creative Technology.
*
*  DMA DEMO PROGRAM FOR PLAYING WAVE FILES
*
*  PURPOSE:     This program demonstrates how to play a .wav file
*               using DMA auto-init mode.
*
*  LIMITATION : This program does not support 8 bit STEREO for SBPro.
*
*               16 bit files must use the SB16.
*
*  DISCLAIMER : Although this program has been tested with
*               standard 8/16 bit PCM WAVE files, there could
*               exist some unknown bugs.
*
*  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
*  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
*  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
*  PURPOSE.
*
*  You have a royalty-free right to use, modify, reproduce and
*  distribute the Sample Files (and/or any modified version) in
*  any way you find useful, provided that you agree that
*  Creative has no warranty obligations or liability for any Samples Files.
*
**************************************************************************/
#include <conio.h>
#include <dos.h>
#include <mem.h>
#include <stdio.h>
#include <stdlib.h>


#define DMA_BUF_SIZE    8192
#define DMA8_FF_REG      0xC
#define DMA8_MASK_REG    0xA
#define DMA8_MODE_REG    0xB
#define DMA16_FF_REG    0xD8
#define DMA16_MASK_REG  0xD4
#define DMA16_MODE_REG  0xD6

#define DMA0_ADDR        0
#define DMA0_COUNT       1
#define DMA0_PAGE     0x87
#define DMA1_ADDR        2
#define DMA1_COUNT       3
#define DMA1_PAGE     0x83
#define DMA3_ADDR        6
#define DMA3_COUNT       7
#define DMA3_PAGE     0x82
#define DMA5_ADDR     0xC4
#define DMA5_COUNT    0xC6
#define DMA5_PAGE     0x8B
#define DMA6_ADDR     0xC8
#define DMA6_COUNT    0xCA
#define DMA6_PAGE     0x89
#define DMA7_ADDR     0xCC
#define DMA7_COUNT    0xCE
#define DMA7_PAGE     0x8A

#define DSP_BLOCK_SIZE            0x0048
#define DSP_DATA_AVAIL               0xE
#define DSP_HALT_SINGLE_CYCLE_DMA 0x00D0
#define DSP_READ_PORT                0xA
#define DSP_READY                   0xAA
#define DSP_RESET                    0x6
#define DSP_TIME_CONSTANT         0x0040
#define DSP_WRITE_PORT               0xC
#define DSP_VERSION                    0xE1

#define AUTO_INIT                   1
#define FAIL                        0
#define FALSE                       0
#define MASTER_VOLUME            0x22
#define MIC_VOLUME               0x0A
#define MIXER_ADDR                0x4
#define MIXER_DATA                0x5
#define MONO                        0
#define PIC_END_OF_INT           0x20
#define PIC_MASK                 0x21
#define PIC_MODE                 0x20
#define SUCCESS                     1
#define SINGLE_CYCLE                0
#define STEREO                      1
#define TRUE                        1
#define VOICE_VOLUME             0x04

struct WAVEHDR{
    char             format[4];      // RIFF
    unsigned long     f_len;          // filelength
    char            wave_fmt[8];    // WAVEfmt_
    unsigned long    fmt_len;    // format lenght
    unsigned short  fmt_tag;    // format Tag
    unsigned short  channel;    // Mono/Stereo
    unsigned long   samples_per_sec;
    unsigned long   avg_bytes_per_sec;
    unsigned short  blk_align;
    unsigned short  bits_per_sample;
    char            data[4];        // data
    unsigned long   data_len;    // data size
    } wavehdr;


/*---------  FUNCTION PROTOTYPES  --------------------------------*/
/*----------------------------------------------------------------*/
char           GetBlasterEnv(int *, int *, int *),
               InitDMADSP(unsigned long, int, int),
               ResetDSP(int);

unsigned int   FillHalfOfBuffer(int *, FILE *, unsigned char *);

unsigned long  AllocateDMABuffer(unsigned char **),
               OnSamePage(unsigned char *);

void           Play(unsigned int, char),
               DSPOut(int, int),
	       Fill_play_buf(unsigned char *, int *, FILE *),
               SetMixer(void);

void interrupt DMAOutputISR(void);   // Interrupt Service Routine

int            Chk_hdr(FILE *);
/*----------------------------------------------------------------*/

/*---------  GLOBAL DECLARATIONS  --------------------------------*/
/*----------------------------------------------------------------*/
char  gBufNowPlaying,
      gEndOfFile,
      gLastBufferDonePlaying,
      Mode,      // indicates MONO or STEREO
      g16BitDMA;

int   Base,
      DSP_Ver;

unsigned long gNoOfBytesLeftInFile;

/*----------------------------------------------------------------*/


/*--- BEGIN main() -----------------------------------------------*/
/*----------------------------------------------------------------*/
int main(int argv, char *argc[])
{
  char  Filename[80],
    Filetype[44],
    RetValue;

  FILE *FileToPlay;

  int BufToFill,
      DMAChan8Bit,
      DMAChan16Bit,
      IRQNumber,
      IRQMask,
      MaskSave,
      Offset;

  unsigned char *DMABuffer;
  unsigned int   BytesLeftToPlay;
  unsigned long  BufPhysAddr;

  void interrupt (*IRQSave)(void);

  /*--- OPEN FILE TO BE PLAYED -------------------------------*/
  /*----------------------------------------------------------*/
  if (argv > 1)
  {

      if ((FileToPlay = fopen(argc[1], "rb")) == NULL)
      {
      printf("%s Error opening file--PROGRAM TERMINATED!\n",
        argc[1]);
      exit(0);
      }

  }
  else
  {
     printf("\nDMAW version 1.01");
     printf("\nUsage : DMAW WAV-file\n");
     exit(0);
  }

   //clrscr();
  printf("\tDMA program for playing .WAV files");
  printf("\n\t----------------------------------\n\n");
  /*--- VERIFY FILE IS .WAV FORMAT---------------------------*/
  /*---------------------------------------------------------*/
  if(Chk_hdr(FileToPlay))
  {
     printf("Header check error - PROGRAM ABORTED");
     exit(0);
  }

  Mode = (wavehdr.channel == 1) ? MONO : STEREO;

  /*--- ALLOCATE BUFFERS -----------------------------------------*/
  /*--------------------------------------------------------------*/
  BufPhysAddr = AllocateDMABuffer(&DMABuffer);
  if (BufPhysAddr == FAIL)
  {
    puts("DMA Buffer allocation failed!--PROGRAM ABORTED");
    fclose(FileToPlay);
    exit(0);
  }


  /*--- GET ENVIRONMENT VALUES -----------------------------------*/
  /*--------------------------------------------------------------*/
  RetValue = GetBlasterEnv(&DMAChan8Bit, &DMAChan16Bit, &IRQNumber);

  /*--- PRINT OUT INFO -------------------------------------------*/
  /*--------------------------------------------------------------*/
  printf("    DMA Buffer Address     = %4x:%-4x (SEG:OFF) (hex)\n",
     FP_SEG(DMABuffer), FP_OFF(DMABuffer));
  printf("    DMA Buffer Phys. Addr. = %-7lu   (decimal)\n",  BufPhysAddr);
  printf("    8-bit DMA channel      = %-5d     (decimal)\n",   DMAChan8Bit);
  printf("    16-bit DMA channel     = %-5d     (decimal)\n",   DMAChan16Bit);
  printf("    I/O port address       = %-3x       (hex)\n",       Base);
  printf("    IRQ number             = %-2d        (decimal)\n\n", IRQNumber);


  /*--- ARE ENVIRONMENT VALUES VALID? --------------------------*/
  /*------------------------------------------------------------*/
  if (RetValue == FAIL)
  {
    puts("BLASTER env. string or parameter(s) missing--PROGRAM ABORTED!");
    free(DMABuffer);
    fclose(FileToPlay);
    exit(0);
  }

  /*--- RESET THE DSP ----------------------------------------*/
  /*----------------------------------------------------------*/
  if(ResetDSP(Base) == FAIL)
  {
    puts("Unable to reset DSP chip--PROGRAM TERMINATED!");
    free(DMABuffer);
    fclose(FileToPlay);
    exit(0);
  }

  if((DSP_Ver < 4) && (wavehdr.bits_per_sample == 16))
  {
    printf("\n** DSP version %d.xx does not support 16bit files.\n", DSP_Ver);
    free(DMABuffer);
    fclose(FileToPlay);
    exit(0);
  }


  /*--- SAVE CURRENT ISR FOR IRQNumber, THEN GIVE IRQNumber NEW ISR ---*/
  /*-------------------------------------------------------------------*/
  IRQSave = _dos_getvect(IRQNumber + 8);
  _dos_setvect(IRQNumber + 8, DMAOutputISR);


  /*--- SAVE CURRENT INTERRUPT MASK AND SET NEW INTERRUPT MASK -------*/
  /*------------------------------------------------------------------*/
  MaskSave = inp((int) PIC_MASK);
  IRQMask = ((int) 1 << IRQNumber); // Shift a 1 left IRQNumber of bits
  outp(PIC_MASK, (MaskSave & ~IRQMask)); // Enable previous AND new interrupts


  /*--- PROGRAM THE DMA, DSP CHIPS -----------------------------------*/
  /*------------------------------------------------------------------*/
  if (InitDMADSP(BufPhysAddr, DMAChan8Bit, DMAChan16Bit) == FAIL)
  {
    puts("InitDMADSP() fails--PROGRAM ABORTED!");
    free(DMABuffer);
    fclose(FileToPlay);
    exit(0);
  }


  /*--- FILL THE FIRST 1/2 OF DMA BUFFER BEFORE PLAYING BEGINS -------*/
  /*------------------------------------------------------------------*/
  BufToFill              = 0;      // Altered by FillHalfOfBuffer()
  gEndOfFile             = FALSE;  // Altered by FillHalfOfBuffer()
  gBufNowPlaying         = 0;      // Altered by ISR
  gLastBufferDonePlaying = FALSE;  // Set in ISR
  gNoOfBytesLeftInFile     = wavehdr.data_len;
  SetMixer();

  BytesLeftToPlay = FillHalfOfBuffer(&BufToFill, FileToPlay, DMABuffer);

  /*--- BEGIN PLAYING THE FILE ---------------------------------------*/
  /*------------------------------------------------------------------*/
  if (wavehdr.data_len < DMA_BUF_SIZE / 2)  // File size is < 1/2 buffer size.
  {
    Play(BytesLeftToPlay, SINGLE_CYCLE);
    while (gBufNowPlaying == 0);  // Wait for playing to finish (ISR called)
  }
  else  // File size >= 1/2 buffer size
  {
    Play(BytesLeftToPlay, AUTO_INIT);
    Fill_play_buf(DMABuffer, &BufToFill, FileToPlay);
  }

  printf("DSP_HALT_SINGLE_CYCLE_DMA\n");
  DSPOut(Base, DSP_HALT_SINGLE_CYCLE_DMA);  // Done playing, halt DMA


  /*--- RESTORE ISR AND ORIGINAL IRQ VECTOR -------------------------*/
  /*-----------------------------------------------------------------*/
  printf("Restore IRQ vector\n");
  outp(PIC_MASK, MaskSave);
  _dos_setvect(IRQNumber + 8, IRQSave);

  free(DMABuffer);
  fclose(FileToPlay);
  return(0);
}


/**********************************************************************
*
* FUNCTION : Chk_hdr()
*
* DESCRIPTION : check for validity of the wave file header
*
************************************************************************/

int Chk_hdr(FILE * FileToPlay)
{
  char * dummy[80];

  memset (&wavehdr,0,sizeof(wavehdr));  //init to 0
  fread(&wavehdr, 44, 1, FileToPlay);  // Get file type description.

  if (memcmp(wavehdr.format, "RIFF", 4))
  {
    puts("File is not a valid .WAV file--PROGRAM ABORTED");
    return 1;
  }

  if (memcmp(wavehdr.wave_fmt, "WAVEfmt ", 8))
  {
    puts("File is not a valid .WAV file");
    return 1;
  }

  if (!((wavehdr.channel == 1) || (wavehdr.channel == 2)))
  {
     printf("Unknown number of channels -> %d", wavehdr.channel);
     return 1;
  }

  if (memcmp(wavehdr.data, "data", 4))
  {
    if (memcmp(wavehdr.data, "fact", 4))
    {
      puts("Unknown file format");
      return 1;
    }
    while(wavehdr.data_len)
    {
      fread(dummy, (wavehdr.data_len%80), 1, FileToPlay);  // Get file type description.
      wavehdr.data_len -= wavehdr.data_len%80;
    }
    fread(wavehdr.data, 8, 1, FileToPlay);
    if (memcmp(wavehdr.data, "data", 4))
    {
      puts("Unknown file format");
      return 1;
    }
  }

  return 0;
} /* chk_hdr() */


/*************************************************************************
*
*  FUNCTION: Play()
*
*  DESCRIPTION : Sets up playing of the wave file depending on number
*         of bits per sample, MONO/STEREO and DMAMode
*
*************************************************************************/
void Play(unsigned int BytesLeftToPlay, char DMAMode)
{
  int Command;

  /*--- IF BytesLeftToPlay IS 0 OR 1, MAKE SURE THAT WHEN DSPOut() IS ---*/
  /*--- CALLED, THE COUNT DOESN'T WRAP AROUND TO A + NUMBER WHEN 1 IS ---*/
  /*    SUBTRACTED! -----------------------------------------------------*/
  if(BytesLeftToPlay <= 1 && g16BitDMA)
    BytesLeftToPlay = 2;
  else if (BytesLeftToPlay == 0 && !g16BitDMA)
    BytesLeftToPlay = 1;

  if(DSP_Ver < 4) // SBPro (DSP ver 3.xx)
  {
    if(wavehdr.bits_per_sample == 8)
    {
      if (DMAMode == AUTO_INIT)
      {
        DSPOut(Base, DSP_BLOCK_SIZE);
        DSPOut(Base, (int) ((BytesLeftToPlay - 1) & 0x00FF));
        DSPOut(Base, (int) ((BytesLeftToPlay - 1) >> 8));
        DSPOut(Base, 0x001C);  // AUTO INIT 8bit PCM
      }
      else
      {
        DSPOut(Base, 0x0014);  // SINGLE CYCLE 8bit PCM
        DSPOut(Base, (BytesLeftToPlay - 1) & 0x00FF);  // LO byte size
        DSPOut(Base, (BytesLeftToPlay - 1) >> 8);       // HI byte size
      }
    }
    else if (wavehdr.bits_per_sample == 16) // 16Bit
    {
      DSPOut(Base, 0x0041);
      DSPOut(Base, (int) ((wavehdr.samples_per_sec & 0x0000FF00) >> 8));
      DSPOut(Base, (int) (wavehdr.samples_per_sec & 0x000000FF));
      DSPOut(Base, (DMAMode == AUTO_INIT) ? 0x00B4 : 0x00B0); // AUTO INIT/SINGLE CYCLE
      DSPOut(Base, (Mode == MONO) ? 0x0010 : 0x0030);  // MONO/STEREO
      DSPOut(Base, (BytesLeftToPlay/2 - 1) & 0x00FF);     // LO byte size
      DSPOut(Base, (BytesLeftToPlay/2 - 1) >> 8);         // HI byte size
    }
  }
  else if(DSP_Ver == 4)// SB16 (DSP ver 4.xx)
  {
    DSPOut(Base, 0x0041);  // DSP output transfer rate
    DSPOut(Base, (int) ((wavehdr.samples_per_sec & 0x0000FF00) >> 8)); // Hi byte
    DSPOut(Base, (int) (wavehdr.samples_per_sec & 0x000000FF));        // Lo byte

    if (DMAMode == AUTO_INIT)
      DSPOut(Base, (wavehdr.bits_per_sample == 8) ? 0x00C6 : 0x00B6);  // AUTO INIT 8/16 bit
    else
      DSPOut(Base, (wavehdr.bits_per_sample == 8) ? 0x00C0 : 0x00B0);  // SINGLE CYCLE 8/16 bit

    if (wavehdr.bits_per_sample == 8)
      DSPOut(Base, (Mode == MONO) ? 0x0000 : 0x0020); // 8bit MONO/STEREO
    else
      DSPOut(Base, (Mode == MONO) ? 0x0010 : 0x0030); // 16bit MONO/STEREO

  /*--- Program number of samples to play -------------------------------*/
    DSPOut(Base, (int) ((BytesLeftToPlay/(wavehdr.bits_per_sample/8) - 1) & 0x00FF)); // LO byte
    DSPOut(Base, (int) ((BytesLeftToPlay/(wavehdr.bits_per_sample/8) - 1) >> 8));     // HI byte
  }

  return;
}

/*************************************************************************
*
* FUNCTION: Fill_play_buf()
*
* DESCRIPTION : Keeps the DMA buffers filled with new data until end of
*        file.
*
*************************************************************************/
void Fill_play_buf(unsigned char *DMABuffer, int *BufToFill, FILE *FileToPlay)
{
  unsigned int NumberOfAudioBytesInBuffer, Count;

  do
  {
    while (*BufToFill == gBufNowPlaying); // Wait for buffer to finish playing

    NumberOfAudioBytesInBuffer = FillHalfOfBuffer(BufToFill, FileToPlay,
                          DMABuffer);
    if (NumberOfAudioBytesInBuffer < DMA_BUF_SIZE / 2)
      Play(NumberOfAudioBytesInBuffer, SINGLE_CYCLE);

  } while (!gEndOfFile);  // gEndOfFile set in FillHalfOfBuffer()

//  while (gLastBufferDonePlaying == FALSE);  // Wait until done playing
  sleep(5);
  return;
}

/*************************************************************************
*
* FUNCTION: FillHalfOfBuffer()
*
* DESCRIPTION : Fill each half of the DMA buffer.
*
*************************************************************************/
unsigned int FillHalfOfBuffer(int *BufToFill, FILE *FileToPlay,
                  unsigned char *DMABuffer)
{
  unsigned int Count;

  if (*BufToFill == 1)  // Fill top 1/2 of DMA buffer
    DMABuffer += DMA_BUF_SIZE / 2;

  if(gNoOfBytesLeftInFile < DMA_BUF_SIZE/2)
  {
     fread(DMABuffer, gNoOfBytesLeftInFile, 1, FileToPlay);
     Count = gNoOfBytesLeftInFile;
     gNoOfBytesLeftInFile = 0;
     gEndOfFile = TRUE;
  }
  else
  {
     fread(DMABuffer, DMA_BUF_SIZE/2, 1, FileToPlay);
     Count = DMA_BUF_SIZE/2;
     gNoOfBytesLeftInFile -= DMA_BUF_SIZE/2;
  }


  *BufToFill ^= 1;  // Toggle to fill other 1/2 of buffer next time.

  return(Count);
}

/*************************************************************************
*
* FUNCTION: DMAOutputISR()
*
* DESCRIPTION:  Interrupt service routine.  Every time the DSP chip finishes
*               playing half of the DMA buffer in auto-init mode, an
*               interrupt is generated, which invokes this routine.
*
*************************************************************************/
void interrupt DMAOutputISR(void)
{
  static char SecondToLastBufferPlayed = FALSE;
  int IntStatus;

  if (g16BitDMA == TRUE)
  {
    outp(Base + 4, 0x82);       // Select interrupt status reg. in mixer
    IntStatus = inp(Base + 5);  // Read interrupt status reg.

    if (IntStatus & 2)
      inp(Base + 0xF);   // Acknowledge interrupt (16-bit)
  }
  else
    inp(Base + (int) DSP_DATA_AVAIL);  // Acknowledge interrupt (8-bit)

  gBufNowPlaying ^= 1;

  outp(PIC_MODE, (int) PIC_END_OF_INT); // End of interrupt

  if (SecondToLastBufferPlayed)
    gLastBufferDonePlaying = TRUE;

  if (gEndOfFile)
    SecondToLastBufferPlayed = TRUE;

  return;
}


/*************************************************************************
*
* FUNCTION: InitDMADSP()
*
* DESCRIPTION: This function reads the first data block of the file and
*              from it obtains information that is needed to program the
*              DMA and DSP chips.  After reading the data block, the file
*              pointer points to the first byte of the voice data.
*
*              NOTE: The DMA chip is ALWAYS programmed for auto-init mode
*                    (command 0x58)!  The DSP chip will be programmed for
*                    auto-init or single-cycle mode depending upon
*                    conditions--see Play() for details.
*
*************************************************************************/
char InitDMADSP(unsigned long BufPhysAddr, int DMAChan8Bit, int DMAChan16Bit)
{
  char BitsPerSample,
       BlockType,
       Pack;

  int  DMAAddr,
       DMACount,
       DMAPage,
       Offset,
       Page,
       Temp;

  unsigned char ByteTimeConstant;
  unsigned int  WordTimeConstant;


  /*--- GET DMA ADDR., COUNT, AND PAGE FOR THE DMA CHANNEL USED ----------*/
  /*----------------------------------------------------------------------*/
  if (wavehdr.bits_per_sample == 8)
  {
    g16BitDMA = FALSE; // DMA is not 16-bit (it's 8-bit).

    switch(DMAChan8Bit)   // File is 8-bit.  Program DMA 8-bit DMA channel
    {
      case 0:
    DMAAddr  = DMA0_ADDR;
    DMACount = DMA0_COUNT;
    DMAPage  = DMA0_PAGE;
      break;

      case 1:
    DMAAddr  = DMA1_ADDR;
    DMACount = DMA1_COUNT;
    DMAPage  = DMA1_PAGE;
      break;

      case 3:
    DMAAddr  = DMA3_ADDR;
    DMACount = DMA3_COUNT;
    DMAPage  = DMA3_PAGE;
      break;

      default:
    puts("File is 8-bit--invalid 8-bit DMA channel");
      return(FAIL);
    }
  }
  else
  {
    g16BitDMA = TRUE;     // DMA is 16-bit (not 8-bit).

    switch(DMAChan16Bit)  // File is 16-bit.  Program DMA 16-bit DMA channel
    {
      case 5:
    DMAAddr  = DMA5_ADDR;
    DMACount = DMA5_COUNT;
    DMAPage  = DMA5_PAGE;
      break;

      case 6:
    DMAAddr  = DMA6_ADDR;
    DMACount = DMA6_COUNT;
    DMAPage  = DMA6_PAGE;
      break;

      case 7:
    DMAAddr  = DMA7_ADDR;
    DMACount = DMA7_COUNT;
    DMAPage  = DMA7_PAGE;
      break;

      default:
    puts("File is 16-bit--invalid 16-bit DMA channel");
      return(FAIL);
    }

    DMAChan16Bit -= 4; // Convert
  }


  /*--- PROGRAM THE DMA CHIP ---------------------------------------------*/
  /*----------------------------------------------------------------------*/
  Page   = (int) (BufPhysAddr >> 16);
  Offset = (int) (BufPhysAddr & 0xFFFF);

  if (wavehdr.bits_per_sample == 8) // 8-bit file--Program 8-bit DMA controller
  {
    outp(DMA8_MASK_REG, (int) (DMAChan8Bit | 4));     // Disable DMA while prog.
    outp(DMA8_FF_REG,   (int) 0);                     // Clear the flip-flop

    outp(DMA8_MODE_REG, (int) (DMAChan8Bit  | 0x58));  // 8-bit auto-init
    outp(DMACount, (int) ((DMA_BUF_SIZE - 1) & 0xFF)); // LO byte of count
    outp(DMACount, (int) ((DMA_BUF_SIZE - 1) >> 8));   // HI byte of count
  }
  else    // 16-bit file--Program 16-bit DMA controller
  {
    // Offset for 16-bit DMA channel must be calculated differently...
    // Shift Offset 1 bit right, then copy LSB of Page to MSB of Offset.
    Temp = Page & 0x0001;  // Get LSB of Page and...
    Temp <<= 15;           // ...move it to MSB of Temp.
    Offset >>= 1;          // Divide Offset by 2
    Offset &= 0x7FFF;      // Clear MSB of Offset
    Offset |= Temp;        // Put LSB of Page into MSB of Offset

    outp(DMA16_MASK_REG, (int) (DMAChan16Bit | 4));    // Disable DMA while prog.
    outp(DMA16_FF_REG,   (int) 0);                     // Clear the flip-flop

    outp(DMA16_MODE_REG, (int) (DMAChan16Bit  | 0x58));  // 16-bit auto-init
    outp(DMACount, (int) ((DMA_BUF_SIZE/2 - 1) & 0xFF)); // LO byte of count
    outp(DMACount, (int) ((DMA_BUF_SIZE/2 - 1) >> 8));   // HI byte of count
  }


  outp(DMAPage, Page);                   // Physical page number
  outp(DMAAddr, (int) (Offset & 0xFF));  // LO byte address of buffer
  outp(DMAAddr, (int) (Offset >> 8));    // HI byte address of buffer


  // Done programming the DMA, enable it
  if (wavehdr.bits_per_sample == 8)
    outp(DMA8_MASK_REG, DMAChan8Bit);
  else
    outp(DMA16_MASK_REG, DMAChan16Bit);


  /*--- PROGRAM THE DSP CHIP ------------------------------------------*/
  /*-------------------------------------------------------------------*/
  if(DSP_Ver < 4)
  {
    ByteTimeConstant = (unsigned char) (256 - 1000000/wavehdr.samples_per_sec);
    DSPOut(Base, (int) DSP_TIME_CONSTANT);
    DSPOut(Base, (int) ByteTimeConstant);
  }

  DSPOut(Base, 0x00D1);  // Must turn speaker ON before doing D/A conv.

  return(SUCCESS);
}


/*************************************************************************
*
* FUNCTION: AllocateDMABuffer()
*
* DESCRIPTION : Allocate memory for the DMA buffer.  After memory is
*               allocated for the buffer, call OnSamePage() to verify
*               that the entire buffer is located on the same page.
*               If the buffer crosses a page boundary, allocate another
*               buffer. Continue this process until the DMA buffer resides
*               entirely within the same page.
*
* ENTRY: **DMABuffer is the address of the pointer that will point to
*        the memory allocated.
*
* EXIT: If a buffer is succesfully allocated, *DMABuffer will point to
*       the buffer and the physical address of the buffer pointer will
*       be returned.
*
*       If a buffer is NOT successfully allocated, return FAIL.
*
*************************************************************************/
unsigned long AllocateDMABuffer(unsigned char **DMABuffer)
{
  unsigned char  BufferNotAllocated = TRUE,
         Done = FALSE,
        *PtrAllocated[100];

  int            i,
         Index = 0;

  unsigned long  PhysAddress;

  do
  {
    *DMABuffer = (unsigned char *) malloc(DMA_BUF_SIZE);

    if (*DMABuffer != NULL)
    {
      /*--- Save the ptr for every malloc() performed ---*/
      PtrAllocated[Index] = *DMABuffer;
      Index++;

      /*--- If entire buffer is within one page, we're out of here! ---*/
      PhysAddress = OnSamePage(*DMABuffer);
      if (PhysAddress != FAIL)
      {
    BufferNotAllocated = FALSE;
    Done = TRUE;
      }
    }
    else
      Done = TRUE;  // malloc() couldn't supply requested memory

  } while (!Done);


  if (BufferNotAllocated)
  {
    Index++;             // Incr. Index so most recent malloc() gets free()d
    PhysAddress = FAIL;  // return FAIL
  }

  /*--- Deallocate all memory blocks crossing a page boundary ---*/
  for (i= 0; i < Index - 1; i++)
    free(PtrAllocated[i]);

  return(PhysAddress);
}


/**************************************************************************
*
* FUNCTION: OnSamePage()
*
* DESCRIPTION: Check the memory block pointed to by the parameter
*              passed to make sure the entire block of memory is on the
*              same page.  If a buffer DOES cross a page boundary,
*              return FAIL. Otherwise, return the physical address
*              of the beginning of the DMA buffer.
*
* ENTRY: *DMABuffer - Points to beginning of DMA buffer.
*
* EXIT: If the buffer is located entirely within one page, return the
*       physical address of the buffer pointer.  Otherwise return FAIL.
*
**************************************************************************/
unsigned long OnSamePage(unsigned char *DMABuffer)
{
  unsigned long BegBuffer,
        EndBuffer,
        PhysAddress;

  /*----- Obtain the physical address of DMABuffer -----*/
  BegBuffer = ((unsigned long) (FP_SEG(DMABuffer)) << 4) +
           (unsigned long) FP_OFF(DMABuffer);
  EndBuffer   = BegBuffer + DMA_BUF_SIZE - 1;
  PhysAddress = BegBuffer;

  /*-- Get page numbers for start and end of DMA buffer. --*/
  BegBuffer >>= 16;
  EndBuffer >>= 16;

  if (BegBuffer == EndBuffer)
    return(PhysAddress);  // Entire buffer IS on same page!
  return(FAIL); // Entire buffer NOT on same page.  Thanks Intel!
}


/**************************************************************************
*
* FUNCTION: GetBlasterEnv()
*
* DESCRIPTION : Get the BLASTER environment variable and search its
*               string for the DMA channel, I/O address port, and
*               IRQ number.  Assign these values to the parameters passed
*               by the caller.
*
* ENTRY: All parameters passed are pointers to integers.  They will be
*        assigned the values found in the environment string.
*
* EXIT:  If DMA channel, I/O address, and IRQ number are found, return
*        PASS, otherwise return FAIL.
*
*
**************************************************************************/
char GetBlasterEnv(int *DMAChan8Bit, int *DMAChan16Bit, int *IRQNumber)
{
  char  Buffer[5],
    DMAChannelNotFound = TRUE,
       *EnvString,
    IOPortNotFound     = TRUE,
    IRQNotFound        = TRUE,
    SaveChar;

  int   digit,
    i,
    multiplier;


  EnvString = getenv("BLASTER");

  if (EnvString == NULL)
    return(FAIL);

  do
  {
    switch(*EnvString)
    {
      case 'A':  // I/O base port address found
      case 'a':
    EnvString++;
    for (i = 0; i < 3; i++)  // Grab the digits
    {
      Buffer[i] = *EnvString;
      EnvString++;
    }

    // The string is in HEX, convert it to decimal
    multiplier = 1;
    Base = 0;
    for (i -= 1; i >= 0; i--)
    {
      // Convert to HEX
      if (Buffer[i] >= '0' && Buffer[i] <= '9')
        digit = Buffer[i] - '0';
      else if (Buffer[i] >= 'A' && Buffer[i] <= 'F')
        digit = Buffer[i] - 'A' + 10;
      else if (Buffer[i] >= 'a' && Buffer[i] <= 'f')
        digit = Buffer[i] - 'a' + 10;

      Base = Base + digit * multiplier;
      multiplier *= 16;
    }

    IOPortNotFound = FALSE;
      break;


      case 'D': // 8-bit DMA channel
      case 'd':
      case 'H': // 16-bit DMA channel
      case 'h':
    SaveChar = *EnvString;
    EnvString++;
    Buffer[0] = *EnvString;
    EnvString++;

    if (*EnvString >= '0' && *EnvString <= '9')
    {
      Buffer[1] = *EnvString; // DMA Channel No. is 2 digits
      Buffer[2] = NULL;
      EnvString++;
    }
    else
      Buffer[1] = NULL;       // DMA Channel No. is 1 digit

    if (SaveChar == 'D' || SaveChar == 'd')
      *DMAChan8Bit  = atoi(Buffer);  // 8-Bit DMA channel
    else
      *DMAChan16Bit = atoi(Buffer);  // 16-bit DMA channel

    DMAChannelNotFound = FALSE;
      break;

      case 'I':  // IRQ number
      case 'i':
    EnvString++;
    Buffer[0] = *EnvString;
    EnvString++;

    if (*EnvString >= '0' && *EnvString <= '9')
    {
      Buffer[1] = *EnvString; // IRQ No. is 2 digits
      Buffer[2] = NULL;
      EnvString++;
    }
    else
      Buffer[1] = NULL;       // IRQ No. is 1 digit

    *IRQNumber  = atoi(Buffer);
    IRQNotFound = FALSE;
      break;

      default:
    EnvString++;
      break;
    }

  } while (*EnvString != NULL);

  if (DMAChannelNotFound || IOPortNotFound || IRQNotFound)
    return(FAIL);

  return(SUCCESS);
}


/*************************************************************************
*
* FUNCTION: DSPOut()
*
* DESCRIPTION: Writes the value passed to this function to the DSP chip.
*
*************************************************************************/
void DSPOut(int IOBasePort, int WriteValue)
{
  // Wait until DSP is ready before writing the command
  while ((inp(IOBasePort + DSP_WRITE_PORT) & 0x80) != 0);

  outp(IOBasePort + DSP_WRITE_PORT, WriteValue);
  return;
}


/*************************************************************************
*
* FUNCTION: ResetDSP()
*
* DESCRIPTION: Self explanatory
*
*************************************************************************/
char ResetDSP(int IOBasePort)
{
  unsigned long i;

  outp(IOBasePort + DSP_RESET, (int) 1);
  delay(10); // wait 10 mS
  outp(IOBasePort + DSP_RESET, (int) 0);

  // Wait until data is available
  while ((inp(IOBasePort + DSP_DATA_AVAIL) & 0x80) == 0);

  if (inp(IOBasePort + DSP_READ_PORT) == DSP_READY)
  {
    outp(IOBasePort + DSP_WRITE_PORT, DSP_VERSION);
    while ((inp(IOBasePort + DSP_DATA_AVAIL) & 0x80) == 0);
    DSP_Ver = inp(IOBasePort + DSP_READ_PORT);
    inp(IOBasePort + DSP_READ_PORT);
    return(SUCCESS);
  }

  return(FAIL);

}



/**************************************************************************
*
* FUNCTION: SetMixer()
*
* DESCRIPTION: Self explanatory
*
**************************************************************************/
void SetMixer(void)
{
  outp(Base + MIXER_ADDR, (int) MIC_VOLUME);
  outp(Base + MIXER_DATA, (int) 0x00);

  outp(Base + MIXER_ADDR, (int) VOICE_VOLUME);
  outp(Base + MIXER_DATA, (int) 0xFF);

  outp(Base + MIXER_ADDR, (int) MASTER_VOLUME);
  outp(Base + MIXER_DATA, (int) 0xFF);

  return;
}
