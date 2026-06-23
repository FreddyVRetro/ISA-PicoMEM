#include <conio.h>
#include <dos.h> 
#include <malloc.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <i86.h>

#include "pm_lib.h"

// * Status and Commands definition
#define STAT_READY         0x00  // Ready to receive a command
#define STAT_CMDINPROGRESS 0x01
#define STAT_CMDERROR      0x02
#define STAT_CMDNOTFOUND   0x03
#define STAT_INIT          0x04  // Init in Progress
#define STAT_WAITCOM       0x05  // Wait for the USB Serial to be connected

#define DEFAULT_BASE 0x2A0

// Gravis Gamepad definitions
#define GRIP_LENGTH_GPP		24
#define GRIP_STROBE_GPP		200	/* 200 us */


typedef struct ROM_Header {
  uint16_t id;
  uint8_t size;
} ROM_Header_t;

bios_var_t far *BIOSVAR = (bios_var_t far *) MK_FP(0x0040,0x0000);

uint16_t PM_Base=0;
uint16_t PM_DataL=0;
uint16_t PM_DataH=0;
uint16_t BIOS_Segment=0;
uint16_t PM_DeviceMask=0;

uint8_t Ext_ROM_NB;

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

// Read the port number "port" "LoopNb" times

void IO_ReadLoop(uint16_t port, uint16_t LoopNb)
{
_asm {
  MOV DX,port
  MOV CX,LoopNb
@ReadLoop:
  IN AL,DX
  LOOP @ReadLoop
  };
}


// Test the PicoMEM Test port (2x3)
// Should answer previous value +1 at every read
// Return the number of errors

uint16_t IO_Test(uint16_t LoopNb)
{
 uint16_t r;
_asm {
  MOV DX,PM_Base
  ADD DX,3
  MOV CX,LoopNb
  MOV BX,0

  CLI
  IN AL,DX
  MOV AH,AL
@TestReadLoop:
  IN AL,DX
  INC AH
  CMP AL,AH
  JNE @TestRead_Fail
  LOOP @TestReadLoop
  JMP @TestReadEnd
@TestRead_Fail:
  INC BX
  OUT DX,AL 		      // { Generate a signal }
  LOOP @TestReadLoop  
@TestReadEnd:
  STI

  MOV r,BX
  };
 return r;
}


// Perform AA 55 Write / Read in loop and tell the Nb of error
// To use with the Data Low and Data High ports
uint16_t IO_Data_Test(uint16_t port, uint16_t LoopNb)
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
  je IOTAAOk
  inc bx
IOTAAOk:
  mov al,0x55
  out dx,al
  in al,dx
  cmp al,0x55
  je IOT55Ok
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
 uint16_t r;
_asm {
  PUSH ES
  PUSH DI
  MOV AX,Segm
  MOV ES,AX
  MOV DI,Ofs
  MOV DX,PM_Base
  ADD DX,3
  
  MOV CX,LoopNb
@TestMemWLoop:
  MOV AX,0xAA55
  MOV ES:[DI],AX
  CMP AX,ES:[DI]
  JNE @TestMemW_Fail
  MOV AX,0x55AA
  MOV ES:[DI],AX
  CMP AX,ES:[DI]
  JNE @TestMemW_Fail
  LOOP @TestMemWLoop
 JMP @PM_RWMEMLoopbEnd

@TestMemW_Fail:
  MOV AL,0          //{To detect where is the problem in the scope}
  OUT DX,AL
  LOOP @TestMemWLoop
  
@PM_RWMEMLoopbEnd:   
  POP DI
  POP ES  
  };
 return r;
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
  mov r,al
};
return r;
}

bool BIOS_detect(uint16_t Segm)
{
  ROM_Header_t far *ROM;
  uint8_t checksum;

  ROM=(ROM_Header_t far *) MK_FP(Segm,0);
  uint16_t id=ROM->id;
  if (id==0xAA55)
   {
    checksum=BIOS_Checksum(Segm,(ROM->size)*512);
    printf("- %FP Size:%d (%dKB) Checksum:%x\n",ROM,ROM->size,(ROM->size)/2,checksum);
    return true;
   } else return false;

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
     case STAT_CMDERROR     : 
                              printf("Command Status: Error (%x)\n",inp(PM_Base+1));
                              outp(PM_Base,0);  // Error : Reset and go check again the status
                              break;
     case STAT_CMDNOTFOUND  : printf("CMD not found (Old Firmware ?)\n");
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
 // printf("PM CMD %X Arg %X\n",cmd,arg);
   _asm {sti};
  if (pm_wait_cmd_end())
   {
    outpw(PM_Base+1,arg);   // Send the parameters
    outp(PM_Base,cmd);      // Send the command
    pm_wait_cmd_end();
    return inpw(PM_Base+1);
   } else return 0xFF;
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
  printf(" > Reported Port: %X - Address: %X\n",PM_Base,BIOS_Segment);

  PM_BIOS_IRQ_Present=true;
  if (!pm_detectport(100)) 
    {
      PM_Port_present=false;
     //PM_Port_present=false; 
     //printf(" > I/O Port Failure\n");
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

//https://github.com/torvalds/linux/blob/master/drivers/input/joystick/grip.c

// Gravis GamePAD test
static int grip_gpp_read_packet(uint16_t gameport, int shift, uint32_t *data)
{
	unsigned long flags;
	int i,u;
  uint32_t v,w;
	unsigned int t;

	*data = 0;
	t = 500;  // Timeout value (Nb of loop) (around 500us)
	i = 0;

  _asm { cli };
	v = inp(gameport) >> shift;

	do {
		t--;
		u = v; 
    v = (inp(gameport) >> shift) & 3;
		if (~v & u & 1) {   
			*data |=(uint32_t) (((uint32_t) v >> 1) << i++);
			t = 500 ;
	  	}
	} while (i < GRIP_LENGTH_GPP && t > 0);
  _asm { sti };

  printf("GamePad Data: %024lb, i:%d, %X > ",*data, i, inp(gameport) );

	if (i < GRIP_LENGTH_GPP) return -1;
  
	for (i = 0; i < GRIP_LENGTH_GPP && (*data & 0xfe4210) ^ 0x7c0000; i++)
  //for (i = 0; i < GRIP_LENGTH_GPP && (*data & 0x00FE1084) ^ 0x7C0000; i++)
		*data = *data >> 1 | (*data & 1) << (GRIP_LENGTH_GPP - 1);

	return -(i == GRIP_LENGTH_GPP);
}


bool pm_get_yn()
{
  char canswer;
  return (scanf(" %c", &canswer) == 1 && ((canswer == 'Y')||(canswer == 'y')));
}

void test_print_result(uint16_t total, uint16_t res)
{
if (res==0) printf(" Ok (x%d)\n",total);
   else printf("Fail (%d/%d)\n",res/total);
}     

void pm_diag()
{
 uint16_t res16;
 bool TestPort_result;    // Test Port Ok ?
 bool DataL_port_result;  // Data Low port Ok ?
 bool DataH_port_result;  // Data High port Ok ?

 printf(" ***     Diagnostic MODE     ***\n");
 printf(" *** Not fully implemented ! ***\n");

 if (!PM_Port_present)
  { printf("** PicoMEM Port Not detected **\n"); }

  printf("** Gravis GamePad test ? (y/n)");
  if (pm_get_yn())
   {
    for (int i=0;i<5;i++)
     {  
      uint32_t data;
      int res=grip_gpp_read_packet(0x201, 4, &data);  // 4 for player 1, 6 for player 2
      if (res < 0) printf("GamePad 1 Fail: %lX\n",(uint32_t) data);
         else printf("GamePad 1 Ok: %lX\n",(uint32_t) data);
      res=grip_gpp_read_packet(0x201, 6, &data);  // 4 for player 1, 6 for player 2
      if (res < 0) printf("GamePad 2 Fail: %lX\n",(uint32_t) data);
         else printf("GamePad 2 Ok: %lX\n",(uint32_t) data);
      sleep(2);
     }
   }

  printf(" > Start Port test (2Ax) ? (y/n)");
  if (pm_get_yn())
   {
// Port test with C Test port code
     printf("- Test Port (2A3, code 1): %d\n",pm_detectport(10000));
// Port test with ASM Test port code
     printf("- Test Port (2A3, code 2):");
     res16=IO_Test(10000);
     TestPort_result=(res16==0);
     test_print_result(10000,res16);

     printf("- Data Port L (2A1):");
     res16=IO_Data_Test(PM_Base+1,10000);
     DataL_port_result=(res16==0);    
     test_print_result(10000,res16);
 
     printf("- Data Port H (2A2):");
     res16=IO_Data_Test(PM_Base+1,10000);
     DataH_port_result=(res16==0);
     test_print_result(10000,res16);
   } 

  printf(" > ROM detection: \n");
 for (uint16_t addr=0xC000;addr<0xF800;addr+=0x0400)
     {
      BIOS_detect(addr);
     }

// printf("\nPress a key to finish.\n");
// getch();

}

// Code for DMA Test
// Macros return upper or lower byte of a word.
#define HIGH(val) ((val) >> 8)
#define LOW(val) ((val) & 0xFF)


// Macros calculate the linear address and page of a far pointer.
#define LINEAR_ADDRESS(val) (((unsigned long)FP_SEG(val) << 4) + (unsigned long)FP_OFF(val))
#define PAGE(val) (LINEAR_ADDRESS(val) >> 16)

// Programmable Interrupt Controller registers and command.
#define PIC1_COMMAND 0x20
#define PIC1_MASK 0x21
#define PIC_EOI 0x20

// Offset between physical interrupts and logical interrupts.
#define INTERRUPT_OFFSET 0x8

// DMA controller macros to calculate address and length for a channel.
#define DMA_ADDRESS(channel) ((channel) * 2)
#define DMA_LENGTH(channel) ((channel) * 2 + 1)

// DMA controller registers and register bits.
#define DMA_MASK 0x0A
#define  DMA_MASK_SET 0x04
#define  DMA_MASK_CLEAR 0x00
#define DMA_MODE 0x0B
#define  DMA_MODE_READ 0x08
#define  DMA_MODE_WRITE 0x04
#define  DMA_MODE_SINGLE 0x40
#define  DMA_MODE_AUTO 0x10
#define DMA_CLEAR 0x0C

// DMA page register I/O ports in DMA channel order (0, 1, 2, 3).
const int dma_page_register[] = {0x87, 0x83, 0x81, 0x82};

int sb_dma = 0x1;

// Configures the DMA controller for playing back a sound.
void setup_dma_transfer(unsigned char far *buffer, int length, int autoinit)
{
        // Note that the input buffer cannot cross a page boundary or
        // DMA will loop back to the beginning of the page.
        unsigned int pointer = LINEAR_ADDRESS(buffer);
        unsigned int page = PAGE(buffer);
        printf("DMA: %x, Length: %d, Page: %x, Pointer: %x\n", sb_dma, length, page, pointer);

        // Mask off DMA while we set it up.
        outp(DMA_MASK, DMA_MASK_SET | sb_dma);
        outp(DMA_CLEAR, 0);
        outp(DMA_MODE, (autoinit ? DMA_MODE_AUTO : DMA_MODE_SINGLE) | DMA_MODE_READ | sb_dma);
        outp(DMA_ADDRESS(sb_dma), LOW((long)pointer));
        outp(DMA_ADDRESS(sb_dma), HIGH((long)pointer));
        outp(dma_page_register[sb_dma], page);
        outp(DMA_LENGTH(sb_dma), LOW(length - 1));
        outp(DMA_LENGTH(sb_dma), HIGH(length - 1));

        // Unmask DMA since we are ready to go.
        outp(DMA_MASK, DMA_MASK_CLEAR | sb_dma);
}

// Stops and clears the DMA controller
void end_dma_transfer()
{
        outp(DMA_MASK, DMA_MASK_SET | sb_dma);
        outp(DMA_CLEAR, 0);
}

// Allocate a block of memory that doesn't cross a page boundary.
void far *farmalloc_page(unsigned long nbytes)
{
        void far *start;
        void far *temp = NULL;
        unsigned int page_offset;
        // Find out where the next available memory is being allocated.
        start = _fmalloc(1);
        if (start == NULL) {
                return NULL;
        }
        page_offset = LINEAR_ADDRESS(start);
#if 0
        printf("farmalloc_page: requested length %ld\n", nbytes);
        printf("farmalloc_page: ptr %Fp\n", start);
        printf("farmalloc_page: page offset %X\n", page_offset);
#endif

        // If we wouldn't have enough room for the buffer in the current
        // page, allocate a temporary buffer to fill up that space, so
        // that our real buffer will start at the beginning of the next
        // page. 
        if ((unsigned long)page_offset + nbytes - 1 > 0xFFFF) {
                temp = _fmalloc(0xFFFF - page_offset + 1);
                if (temp == NULL) {
                        printf("farmalloc_page: Could not allocate temp buffer\n");
                       // printf("farmalloc_page: farcore left: %ld\n", farcoreleft());
                        _ffree(start);
                        return NULL;
                }
        }

        // Don't need the temporary buffer anymore. 
        _ffree(start);

        // Allocate the real buffer.
        start = _fmalloc(nbytes);
        page_offset = LINEAR_ADDRESS(start);
        if (temp != NULL) {
                _ffree(temp);
        }
        return start;
}


void pm_dmadiag()
{
        unsigned char far *buffer;
        unsigned long size;
        unsigned int sample_rate;

        size=1024;

        buffer = (unsigned char *far)farmalloc_page(size);
        if (buffer == NULL) {
                printf("Could not allocate buffer.\n");
                printf("Press any key to go back to the menu.\n");
                getch();
                return;
        }

        for (unsigned long i = 0; i < size; i++) {
                buffer[i] = (unsigned char)(i & 0xFF);
        }
        
        // Test 64 bytes
        setup_dma_transfer(buffer, 64, 0);
        pm_io_cmd(CMD_DMA_TEST,0);

        setup_dma_transfer(buffer, 64, 1);       
        pm_io_cmd(CMD_DMA_TEST,1);        

        setup_dma_transfer(buffer, 10, 0);
        pm_io_cmd(CMD_DMA_TEST,2); 

        setup_dma_transfer(buffer, 10, 1);
        pm_io_cmd(CMD_DMA_TEST,2);         

        setup_dma_transfer(buffer+13, 10, 0);
        pm_io_cmd(CMD_DMA_TEST,2); 


}
