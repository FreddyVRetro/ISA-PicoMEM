#include <conio.h>
#include <dos.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <i86.h>

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

// Use the PicoMEM test port (answer previous value +1 at every read)
bool pm_detectport(uint16_t LoopNb)
{
  uint8_t res,res1;

  outp(PM_Base,0);
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
RAM_Display(uint16_t Segm, uint16_t size )
{

}

uint8_t BIOS_detect(uint16_t Segm)
{
  ROM_Header_t *ROM;
  ROM=MK_FP(Segm,0);
  printf("ID %x Size %d",ROM->id,ROM->size);
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
 extern uint16_t pm_irq_cmd(uint8_t cmd)
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

bool pm_Search_Port()
{
  return false;
}

bool pm_detect()
{
if (pm_irq_detect())
{ 
  printf("PicoMEM BIOS Interrupt Detected\n");
  printf(" Reported Port: %X - Address %X\n",PM_Base,BIOS_Segment);

  PM_BIOS_IRQ_Present=true;
  if (!pm_detectport(100)) {
     printf("I/O Port Failure\n");
    }
 return true;
}
else 
{
  printf("PicoMEM BIOS Interrupt not available :(\n");
  PM_BIOS_IRQ_Present=false;
  PM_Base=DEFAULT_BASE;
  printf("Try I/O Port (2A0): ");  
  if (pm_detectport(100)) {
     printf("Ok\n");
    } else printf("Failure\n");
 return false;
}
}

void pm_diag()
{
printf("Start the Diagnostic Code \n");

for (uint16_t addr=0xC800;addr<0xF000;addr+=0x100)
  {
   BIOS_detect(addr);
  }

}