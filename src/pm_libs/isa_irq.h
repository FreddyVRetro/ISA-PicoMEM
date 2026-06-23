#pragma once

#include "pm_board.h"
#include "../pm_gvars.h"
#include "pm_defines.h"

#include "pico/critical_section.h"
extern critical_section_t pm_irq_crit;
extern bool PM_IRQ_raised;

#define  IRQ_Stat_REQUESTED     1   // Set by the Pico, IRQ Request in progress
#define  IRQ_Stat_INPROGRESS    2   // Set by the BIOS, IRQ Received
#define  IRQ_Stat_COMPLETED 	3   // Set by the BIOS, IRQ in progress (Accepted and Started)

#define  IRQ_Stat_WRONGSOURCE   0x11 // Set by the BIOS, IRQ Completed, Wrong Source
#define  IRQ_Stat_DISABLED      0x12 // This interrupt is disabled in the PIC
#define  IRQ_Stat_INVALIDARG    0x13 // Invalid Argument Error
#define  IRQ_Stat_SameIRQ       0x14 // Asked to call the same IRQ (As the Hardware PicoMEM one)

// List the different possible IRQ Request
#define IRQ_R_TOTAL     6 // Total number of IRQ Request
#define IRQ_R_DMACOPY   0   // Ask the IRQ mux to send a DMA Copy
#define IRQ_R_AUDIO        1   // Will contain the requested HW IRQ NB[]
#define IRQ_R_NE2000    2   // Will contain the requested HW IRQ NB
#define IRQ_R_MOUSE     3   // Ask the IRQ mux to move the Mouse cursor
#define IRQ_R_KEYBOARD  4   // Ask the IRQ mux to send a Keyboard event
#define IRQ_R_PCCMD     5   // Ask the IRQ mux to send a PCCMD

typedef struct pm_irq_svar_t {    // shared variable for the Interrupt mux (Between the Pico and the PC)
  volatile uint8_t  IRR_list[IRQ_R_TOTAL];  // List of IRQ requested by the PicoMEM (Equivalent of the PIC IRR)
  volatile uint16_t ISR;                    // Interrupt Service Register, contains the IRQ currently in progress
  volatile uint8_t PM_IRR;                  // 1 if the PicoMEM IRQ is requested
  volatile uint8_t PM_ISR;                  // 1 if the PicoMEM IRQ is raised
  uint8_t  cnt;                             // PicoMEM IRQ Counter, used only be the PM IRQ itself
  volatile uint8_t  mouse_x;                // Mouse X position
  volatile uint8_t  mouse_y;                // Mouse Y position
  volatile uint8_t  mouse_b;                // Mouse Button state

} pm_irq_svar_t;

extern pm_irq_svar_t *SVAR_IRQ;

#ifdef __cplusplus
extern "C" {  // To be usable from C code
#endif

extern volatile uint64_t PM_IRQ_AckTime;
extern volatile uint64_t PM_IRQ_SendTime;



// check if a source IRQ is requested but there is no interrupt in progress
// IRQ_R_DMACOPY
// PM_INFO("DMACOPY IRQ Requested but no PM IRQ in progress");
__force_inline bool isa_pm_irq_source_notstarted(uint32_t source)
{
  if (!(SVAR_IRQ->PM_IRR+SVAR_IRQ->PM_ISR)&(SVAR_IRQ->IRR_list[source])) return true; // No PM IRQ in progress, but the source is requested
  return false; // PM IRQ in progress or no IRQ requested
}

extern void isa_irq_init();
extern bool isa_pm_irq_Wait_End_Timeout(uint32_t delay);
extern void isa_pm_irq_raise(uint8_t Source,uint8_t Arg,bool Force);
extern void isa_pm_irq_ack();

extern void isa_irq_raise(uint8_t Source,uint8_t irqnb, bool Force);
extern void isa_irq_lower(uint8_t Source,uint8_t irqnb);

#ifdef __cplusplus
}
#endif