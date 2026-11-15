#ifndef BUS_IRQ_H
#define BUS_IRQ_H

// interrupts API redirection to different codes via defines

#include "isa_irq.h"
#if BOPARD_WM10  // WonderMCA-specific direct MCA IRQ delivery. Used by the WM10 branches
#define bus_irq_init                wm_irq_init
#define bus_pm_irq_Wait_End_Timeout wm_pm_irq_Wait_End_Timeout
#define bus_pm_irq_raise            wm_pm_irq_raise
#define bus_pm_irq_ack              wm_pm_irq_ack
#define bus_irq_raise               wm_irq_raise
#define bus_irq_lower               wm_irq_lower
#else            // PicoMEM IRQ multiplexed delivery. Used by the PicoMEM branches
#define bus_irq_init                isa_irq_init
#define bus_pm_irq_Wait_End_Timeout isa_pm_irq_Wait_End_Timeout
#define bus_pm_irq_raise            isa_pm_irq_raise
#define bus_pm_irq_ack              isa_pm_irq_ack
#define bus_irq_raise               isa_irq_raise
#define bus_irq_lower               isa_irq_lower
#endif

#endif