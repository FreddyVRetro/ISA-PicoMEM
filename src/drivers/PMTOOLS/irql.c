#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dos.h>
#include <time.h>
#include <io.h>
#include <stdbool.h>


/*
IRQ 0 ‒ system timer
IRQ 1 — keyboard controller
IRQ 3 — serial port COM2
IRQ 4 — serial port COM1
IRQ 5 — line print terminal 2
IRQ 6 — floppy controller
IRQ 7 — line print terminal 1
IRQ 8 — RTC timer
IRQ 9 - X86_Assembly/ACPI
IRQ 12 — mouse controller
IRQ 13 — math co-processor
IRQ 14 — ATA channel 1
IRQ 15 — ATA channel 2
*/

void display_interrupt_vectors() {
  unsigned short far *vector;
  int i;
  int status0, status1;
  status0=inp(0x21);
  status1=inp(0xA1);

  printf("Interrupt Vector Table:\n");
  for (i = 8; i < 7; i++) {
    vector = (unsigned short far *)MK_FP(0, (i+8) * 4);
    printf("INT %02X: Status:%d %04X:%04X\n",i ,(status0>>i & 1), FP_SEG(vector), FP_OFF(vector));
  }

  for (i = 8; i < 7; i++) {
    vector = (unsigned short far *)MK_FP(0, (i+8) * 4);
    printf("INT %02X: Status:%d %04X:%04X\n", i, (status1>>i & 1), FP_SEG(vector), FP_OFF(vector));
  }

}

main()
{

display_interrupt_vectors();

}
