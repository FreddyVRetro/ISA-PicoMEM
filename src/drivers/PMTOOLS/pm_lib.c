#include <conio.h>
#include <dos.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <i86.h>

// * Status and Commands definition
#define STAT_READY         0x00  // Ready to receive a command
#define STAT_CMDINPROGRESS 0x01
#define STAT_CMDERROR      0x02
#define STAT_CMDNOTFOUND   0x03
#define STAT_INIT          0x04  // Init in Progress
#define STAT_WAITCOM       0x05  // Wait for the USB Serial to be connected

#define DEFAULT_BASE 0x2A0

typedef struct ROM_Header
{
  uint16_t id;
  uint8_t size;
} ROM_Header_t;

uint16_t PM_Base=0;
uint16_t PM_DataL=0;
uint16_t PM_DataH=0;
uint16_t BIOS_Segment=0;
uint16_t PM_DeviceMask=0;

bool PM_BIOS_IRQ_Present=false;  // BIOS Interrupt detected
bool PM_BIOS_Present=false;      // BIOS Code detected and Checksum OK
bool PM_Port_present=false;      // IO Port detected
bool PM_CMD_Busy=false;

// Use the PicoMEM test port (answer previous value +1 at every read)
bool pm_detectport(uint16_t LoopNb)
{
  uint8_t res,res1;

//  outp(PM_Base,0);  // Command Reset
  res=inp(PM_Base+3);
  for (uint16_t i=1;i<LoopNb;i++)
    {
      res1=inp(PM_Base+3);
      if ((res+1)!=res1) return false;
      res=res1;
    }
 return true;
}

// Perform AA 55 Write / Read in loop and tell the Nb of error
// To use with the Data Low and Data High ports
uint16_t IO_Test(uint16_t port, uint16_t LoopNb)
{
 uint16_t r;
_asm {
  push ds
  mov cx,LoopNb
  mov dx,port
  xor bx,bx
IOTLoop:
  mov al,0xAA
  out dx,al
  in al,dx
  cmp al,0xAA
  JE IOTAAOk
  inc bx
IOTAAOk:
  mov al,0x55
  out dx,al
  in al,dx
  cmp al,0x55
  JE IOT55Ok
  inc bx
IOT55Ok:
  loop IOTLoop
  mov r,bx
  };
 return r;
}

// Perform AA 55 Write / Read in loop and tell the Nb of error
uint16_t RAM_Test_w(uint16_t Segm, uint16_t Ofs, uint16_t LoopNb)
{
 return 0;
}

// Perform AA Write, XOR, 55 Read in loop and tell the Nb of error
uint16_t RAM_Testxor_w(uint16_t Segm, uint16_t Ofs, uint16_t LoopNb)
{
 return 0;
}

// Test rapidly an 8kb RAM block
bool RAM_FastTest_8k(uint16_t Segm, uint16_t Ofs, uint16_t Offs)
{
 char *ram;
 return true;
}

// Display size bytes of memory (Ascii 32 to 127)
// 16 bytes per line
void RAM_Display(uint16_t Segm, uint16_t size )
{


}

uint8_t BIOS_detect(uint16_t Segm)
{
  ROM_Header_t *ROM;
  ROM=(ROM_Header_t *) MK_FP(Segm,0);
  uint16_t id=ROM->id;
  printf("@ %X ID %X Size %d",Segm,id,ROM->size);
  return true;
}

// Return the checksum value at this Segment (Max 64Kb size)
uint8_t BIOS_Checksum(uint16_t Segm,uint16_t Size)
{
uint8_t r;
_asm {
  push ds
  mov cx,Size
  mov ax,Segm
  push ax
  pop ds
  mov si,0
  xor ax,ax
CSLoop:
  lodsb
  add ah,al
  loop CSLoop
  xchg ah,al      // Return the sum
  pop ds
  // No need for return as value put in al directly.
  mov r,al
};
return r;
}

/*
;PM BIOS Function 0 : Detect the PicoMEM BIOS and return config  > To use by PMEMM, PMMOUSE ...
;                     Also redirect the Picomem Hardware IRQ if not done
; Input : AH=60h AL=00h
;         DX=1234h
; Return: AX : Base Port
;         BX : BIOS Segment
;         CX : Available devices Bit Mask
;             * Bit 0 : PSRAM Available
;			  * Bit 1 : uSD Available
;			  * Bit 2 : USB Host Enabled
;			  * Bit 3 : Wifi Enabled
;         DX : AA55h (Means Ok)
*/
extern bool pm_irq_detect()
{
bool r;
_asm {
mov ah,0x60
mov al,0
mov dx,0x1234
int 0x13
mov PM_Base,ax
inc ax
mov PM_DataL,ax
inc ax
mov PM_DataH,ax
mov BIOS_Segment,bx
mov PM_DeviceMask,cx
mov al,1              // Return true
cmp dx,0xAA55
je @@end
mov al,0              // Return false
@@end:
mov r,al
};
return r;
}

/* Run a PicoMEM BIOS Command and return ax
  10h : Enable the Mouse
  11h : Disable the Mouse
  12h : Reinstall the Keyboard interrupt
*/
uint16_t pm_irq_cmd(uint8_t cmd)
{
uint16_t r;
 _asm {
  mov ah,0x60
  mov al,cmd
  mov dx,0x1234
  int 0x13
  mov r,ax
 };
 return r;
}

bool pm_cmdport_ok()
{ 
if (PM_Port_present)
  {
   uint8_t res=inp(PM_Base); 
   if (res!=0)
      {
       printf("Error : CMD not Ready (%d)",res);
       return false;
      }
    return true;
   }
 return false;   
}

bool pm_wait_cmd_end()
{
  while(true)
  {
    uint8_t res=inp(PM_Base);
    switch (res)
        {
     case STAT_READY        : return true;
     case STAT_CMDINPROGRESS: break;   // In progress, Loop
     case STAT_CMDERROR     :  // Status not used for the moment
     case STAT_CMDNOTFOUND  : printf("CMD Error\n");
                              outp(PM_Base,0);  // Error : Reset and go check again the status
                              break;
     case STAT_INIT         :
     case STAT_WAITCOM      : printf("Err: PicoMEM Init/Wait\n");
                              return false;
     default                : printf("Err: Invalid CMD Status (%X)\n",res);
                              return false;
        }
  }
}

// Send a command via I/O with argument and return a word
uint16_t pm_io_cmd(uint8_t cmd,uint16_t arg)
{
  if (pm_wait_cmd_end())
   {
    outpw(PM_Base+1,arg);   // Send the parameters
    outp(PM_Base,cmd);      // Send the command
    pm_wait_cmd_end();
    return inpw(PM_Base+1);
   }     
  return 0;
}

bool pm_Search_Port()
{
  return false;
}

bool pm_detect()
{
if (pm_irq_detect())
{ 
  printf(" - PicoMEM BIOS Interrupt Detected\n");
  printf(" > Reported Port: %X - Address %X\n",PM_Base,BIOS_Segment);

  PM_BIOS_IRQ_Present=true;
  if (!pm_detectport(100)) 
    {
     PM_Port_present=false; 
     printf(" > I/O Port Failure\n");
    }
    else
    {
     PM_Port_present=true; 
     uint8_t res=inp(PM_Base);
     if (res!=0)
        {
         printf(" > Warning I/O detected but Command Status <>0 (%X)",res);
         PM_CMD_Busy=true;
        }      
    }
 return true;
}
else 
{
  printf(" > BIOS Interrupt not available :(\n");
  PM_BIOS_IRQ_Present=false;
  PM_Base=DEFAULT_BASE;
  printf(" - I/O Port %X: ",PM_Base);
  if (pm_detectport(100)) 
    { 
      printf("Ok\n"); 
      PM_Port_present=true;
      return true;
    }
      else
       {
         printf("Failure\n");
         PM_Port_present=false;         
       }
 return false;
}
}

bool pm_get_yn()
{
  char canswer;
  return (scanf(" %c", &canswer) == 1 && ((canswer == 'Y')||(canswer == 'y')));
}

void pm_diag()
{
 printf(" *** Diagnostic MODE ***\n");

 if (!PM_Port_present)
  { printf("** PicoMEM Port Not detected **\n"); }

  printf(" > Start Port test ?");
  if (pm_get_yn())
   {
     printf("Starting Port test (2A0)\n");
   } 

 for (uint16_t addr=0xC800;addr<0xF000;addr+=0x1000)
  {
   BIOS_detect(addr);
  }

 printf("\nPress a key to continue.\n");
 getch();

}
