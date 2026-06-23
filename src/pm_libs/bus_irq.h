#ifndef BUS_IRQ_H
#define BUS_IRQ_H

// interrupts API redirection to different codes via defines

#include "isa_irq.h"

#define bus_irq_init isa_irq_init   isa_irq_init
#define bus_pm_irq_Wait_End_Timeout isa_pm_irq_Wait_End_Timeout
#define bus_pm_irq_raise            isa_pm_irq_raise
#define bus_pm_irq_ack              isa_pm_irq_ack
#define bus_irq_raise               isa_irq_raise
#define bus_irq_lower               isa_irq_lower

#endif