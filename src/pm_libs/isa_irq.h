#pragma once

#define  IRQ_Stat_REQUESTED     1   // Set by the Pico, IRQ Request in progress
#define  IRQ_Stat_INPROGRESS    2   // Set by the BIOS, IRQ Received
#define  IRQ_Stat_COMPLETED 	3   // Set by the BIOS, IRQ in progress (Accepted and Started)

#define  IRQ_Stat_WRONGSOURCE   0x11 // Set by the BIOS, IRQ Completed, Wrong Source
#define  IRQ_Stat_DISABLED      0x12 // This interrupt is disabled in the PIC
#define  IRQ_Stat_INVALIDARG    0x13 // Invalid Argument Error
#define  IRQ_Stat_SameIRQ       0x14 // Asked to call the same IRQ (As the Hardware PicoMEM one)

#define IRQ_None        0		// IRQ was fired, but not intentionally or nothing to do
#define IRQ_StartPCCMD  3
#define IRQ_USBMouse    4
#define IRQ_Keyboard    5
#define IRQ_IRQ3        6		// Directly call HW interrupt 3
#define IRQ_IRQ4        7		// Directly call HW interrupt 4
#define IRQ_IRQ5        8		// Directly call HW interrupt 5
#define IRQ_IRQ6        9		// Directly call HW interrupt 6
#define IRQ_IRQ7        10		// Directly call HW interrupt 7
#define IRQ_IRQ9        11		// Directly call HW interrupt 9
#define IRQ_IRQ10       12		// Directly call HW interrupt 10

#define IRQ_NE2000      20

typedef struct isa_irq_t {
    volatile bool     active;
} isa_irq_t;

#ifdef __cplusplus
extern "C" {  // To be usable from C code
#endif

extern bool isa_irq_Wait_End_Timeout(uint32_t delay);
extern void isa_irq_init();
extern uint8_t isa_irq_raise(uint8_t Source,uint8_t Arg,bool Force);
extern void isa_irq_lower();

#ifdef __cplusplus
}
#endif