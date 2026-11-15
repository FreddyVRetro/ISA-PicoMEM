#pragma once

#include "pm_board.h"
#include "../pm_gvars.h"
#include "pm_defines.h"

#include "pico/critical_section.h"

#if BOARD_WM10
// WonderMCA-specific direct MCA IRQ delivery. Used by the WM10 branches
// of pm_irq_up / pm_irq_down_nolock below -- on WM10 the legacy
// PIN_PM_IRQ (= GPIO 0 default) isn't wired to the bus, so the
// multiplexed PicoMEM IRQ must drive a real MCA IRQ pin via
// mca_irq_raise / mca_irq_lower. mca_irq.h is C; guard the linkage
// spec so this header stays usable from both C and C++ TUs.
#ifdef __cplusplus
extern "C" {
#endif
#include "mca_irq.h"
#ifdef __cplusplus
}
#endif
#endif
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
#define IRQ_R_AUDIO     1   // Will contain the requested HW IRQ NB
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

// Check if a multiplexed/PicoMEM interrupt is requested
__force_inline void wm_pm_irq_up()
{
#if BOARD_WM10
  // No critical section on WM10. Confirmed by bisect 2026-06-07:
  // the legacy section produced a Core 0 vs Core 1 deadlock the
  // moment Core 0 raised the MCA IRQ -- chassis ISR responds within
  // microseconds and writes CMD_PMIRQ_ACK to 0x2A0, which makes
  // Core 1's dev_pmio_iow enter isa_pm_irq_ack -> try to acquire
  // pm_irq_crit which Core 0 is still holding. Core 1 stuck =
  // CHRDY held = chassis stuck.
  // Section isn't needed: shared state is single-byte volatile
  // (atomic on M33), GPIO uses atomic SIO writes, and dedicated
  // MCA IRQ pins (no shared PIN_PM_IRQ to serialize on).
  //
  // 2026-06-07: PULSE the IRQ line instead of holding LOW. If we
  // held the line LOW for the duration of the chassis ISR, the
  // 8259 would re-fire IRQ 3 every time INT 33h (PMMOUSE) did STI
  // mid-handler -- nested PM_Int frames pile up until DOS prints
  // "Internal stack overflow, system halted". A short pulse lets
  // the 8259 latch the falling edge, then the line goes HIGH and
  // no further IRQ fires until we explicitly pulse again.
  // PM_IRR/PM_ISR protocol still drives the source dispatch and ack
  // -- only the line discipline changes from level-asserted to
  // edge-pulsed.
  //
  // PM_IRR=1 BEFORE the pulse: chassis PM_Int clears PM_IRR=0 at its
  // line 1525 entry. If we set PM_IRR=1 AFTER the pulse, we race the
  // chassis ISR -- chassis sometimes clears first, then Pico overwrites
  // back to 1, leaving stale PM_IRR=1 that blocks the NEXT raise via
  // wm_pm_irq_raise's `if (!PM_IRR) | Force` guard. Setting it before
  // the pulse makes the chassis's clear the LAST write -- stale state
  // race gone.
  SVAR_IRQ->PM_IRR = 1;
  PM_IRQ_SendTime  = time_us_64();
  PM_IRQ_raised    = true;
  if (BV_IRQ != 0) {
    mca_irq_raise(BV_IRQ, true);    // 20us HIGH lead-in, then ASSERT (falling edge latched)
    busy_wait_us(200);               // hold LOW for 200us -- plenty for any 8259 sample window
    mca_irq_lower(BV_IRQ);          // RELEASE -- chassis services from edge memory
  }
#else
  critical_section_enter_blocking(&pm_irq_crit);
  if (PM_IRQ_raised)
   {
    gpio_put(PIN_PM_IRQ,1);     // If already UP, need to go down and wait
    busy_wait_us(5);
   }
  SVAR_IRQ->PM_IRR=1;
  gpio_put(PIN_PM_IRQ,0); // There is an inverter, 0 is Up
  PM_IRQ_SendTime=time_us_64();
  PM_IRQ_raised=true;
  critical_section_exit(&pm_irq_crit);
#endif
}

 __force_inline void wm_pm_irq_down()
{
#if BOARD_WM10
  // No critical section on WM10 -- see pm_irq_up comment.
  PM_IRQ_raised = false;
  if (BV_IRQ != 0) mca_irq_lower(BV_IRQ);
#else
  critical_section_enter_blocking(&pm_irq_crit);
  PM_IRQ_raised=false;
  gpio_put(PIN_PM_IRQ,1); // There is an inverter, 0 is Up
  critical_section_exit(&pm_irq_crit);
#endif
}

 __force_inline void wm_pm_irq_down_nolock()
{
  PM_IRQ_raised=false;
#if BOARD_WM10
  if (BV_IRQ != 0) mca_irq_lower(BV_IRQ);
#else
  gpio_put(PIN_PM_IRQ,1); // There is an inverter, 0 is Up
#endif
}

// check if a source IRQ is requested but there is no interrupt in progress
// IRQ_R_DMACOPY
// PM_INFO("DMACOPY IRQ Requested but no PM IRQ in progress");
__force_inline bool wm_pm_irq_source_notstarted(uint32_t source)
{
  if (!(SVAR_IRQ->PM_IRR+SVAR_IRQ->PM_ISR)&(SVAR_IRQ->IRR_list[source])) return true; // No PM IRQ in progress, but the source is requested
  return false; // PM IRQ in progress or no IRQ requested
}

extern void wm_irq_init();
extern bool wm_pm_irq_Wait_End_Timeout(uint32_t delay);
extern void wm_pm_irq_raise(uint8_t Source,uint8_t Arg,bool Force);
extern void wm_pm_irq_ack();

extern void wm_irq_raise(uint8_t Source,uint8_t irqnb, bool Force);
extern void wm_irq_lower(uint8_t Source,uint8_t irqnb);

#ifdef __cplusplus
}
#endif