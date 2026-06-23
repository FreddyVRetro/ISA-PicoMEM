
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "pm_defines.h"
#include "../pm_gvars.h"
#include "isa_irq.h"
#include "pm_board.h"

// Manage the Interrupt generation to the PC:
// - If an IRQ is physically available, use it simply by raiding/lowering the pin
// - Otherwise, PicoMEM IRQ is used as multiplexed/"Virtual PIC" to generate other simulated HW IRQ
// - PicoMEM IRQ can also be used for PicoMEM specific devices/usage (commands)

// IRR_List : In Request Register, List of Requested IRQ, contains the Physical IRQ to generate or just 1 if it is an IRQ "Command"
// ISR      : In Service Register, contains the IRQ being serviced by the PM IRQ
// Interruption sequence :
// - PicoMEM : Set the IRR to request an IRQ
// - PicoMEM : Rize the PM IRQ Pin
// - PM IRQ  : Increment the counter at the Start
// - PM IRQ  : Search for the ISR <> 0 to initiate the IRQ or Command
// > Case of HW IRQ
// - PM IRQ  : Clear the selected ISR
// - PM IRQ  : If the HW IRQ Mask ignore the IRQ
// - PM IRQ  : Set the ISR bit and start the HW IRQ Handler
// - PC IRQ  : Acknowledge the device IRQ > The PicoMEM emulation code clear the ISR bit
// - PM IRQ  : When the IRQ is completed, clear the ISR bit as well
// - PM_IRQ  : Decrement the counter and Acknowledge the PM IRQ if the counter is 0 > The PicoMEM set 


// Important : When the PM IRQ is in progress the interruped are disabled
// Anyway, an IRQ being serviced can choose to re enable the interrupts and aknowledge itself, allowing other IRQ to be serviced

critical_section_t pm_irq_crit;

pm_irq_svar_t *SVAR_IRQ=(pm_irq_svar_t *) &PM_DP_RAM[IRQ_VAR_OFFS]; // IRQ Table, 6 Bytes

bool PM_IRQ_raised=false;         // Set to true when requested, and set to 0 when Acknowledged
bool IRQ_raise_inprogress=false;
bool IRQ_Crit_initialized=false;
volatile uint64_t PM_IRQ_AckTime;
volatile uint64_t PM_IRQ_SendTime;

// Lower all the IRQ and reset the variables
void isa_irq_init()
{
 if (!IRQ_Crit_initialized) {
     critical_section_init(&pm_irq_crit);
     IRQ_Crit_initialized=true;
    }
 PM_IRQ_raised=false;
 isa_pm_irq_ack();
#if USE_HWIRQ // Lower all the IRQ pins
 gpio_put(PIN_IRQ3,1);
 gpio_put(PIN_IRQ5,1);
 gpio_put(PIN_IRQ7,1);
 #ifdef PIN_IRQ6
 gpio_put(PIN_IRQ6,1);
 #endif
#else
// PicoMEM IRQ Down
 critical_section_enter_blocking(&pm_irq_crit);
 PM_IRQ_raised=false;
 gpio_put(PIN_PM_IRQ,1); // There is an inverter, 0 is Up
 critical_section_exit(&pm_irq_crit);
#endif
 SVAR_IRQ->ISR=0;             // Clear the ISR register
 SVAR_IRQ->PM_IRR=0;          // Clear the PM IRQ Request
 SVAR_IRQ->PM_ISR=0;          // Clear the PM IRQ In Service
}

//********************************************************
//*        Start the PicoMEM Multiplexed IRQ             *
//********************************************************
// Source : Interrupt request source or IRQ command
// HW IRQ number to use or 1    : Argument variable to send to the IRQ
// Force  : Start anyway even if running
void __not_in_flash_func(isa_pm_irq_raise)(uint8_t Source,uint8_t irqnb,bool Force)
{

  if (IRQ_raise_inprogress) {
      PM_ERROR("!!! IRQ Raise in Conflict !!");
      return;
     }

  IRQ_raise_inprogress=true;
  SVAR_IRQ->IRR_list[Source]=irqnb<<1;  // Set the IRQ requested by the PicoMEM (>>1 for direct call table index)

#if UART_PIN0
#else // DOOM Does not work if next line modified
  if ((!SVAR_IRQ->PM_IRR)|Force) {  // If the PicoMEM IRQ is not started don't ask again (Add in IRR List is sufficient)
  // PicoMEM IRQ Up
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

   } else if ((time_us_64()-PM_IRQ_SendTime)>10000) { // Try to add force here, in case another SB detect code redirect IRQ7
   } 

#endif
  IRQ_raise_inprogress=false;
}

// Acknowledge the PicoMEM IRQ, clear the ISR, Don't use it from somewhere else !!!!
// This is called by the BIOS (IOW command) when the IRQ is completed
// Executed from the core1 ONLY (No need to for critical section)
void __not_in_flash_func(isa_pm_irq_ack)()
{
  critical_section_enter_blocking(&pm_irq_crit);
  DBG_ON_INT_DMA();
  if (!SVAR_IRQ->PM_IRR)  // Don't acknowledge if another PM IRQ is requested and not served (Otherwise the IRQ time up will be too short)
  {
    // PicoMEM IRQ Down without Lock
    PM_IRQ_raised=false;
    gpio_put(PIN_PM_IRQ,1); // There is an inverter, 0 is Up 
    // Put IRQ Register to 0
    PM_IRQ_AckTime=time_us_64();  
  }
  SVAR_IRQ->PM_ISR=0;     // PM IRQ no more in service (Set to one in the BIOS IRQ)
  DBG_OFF_INT_DMA();
  critical_section_exit(&pm_irq_crit);    
}

// Clear the ISR register bit
void __not_in_flash_func(isa_pm_irq_lower)(uint8_t Source)
{
SVAR_IRQ->ISR &= ~(1 << Source);  // Clear the ISR bit for the Source   
}

// Raise an Hardware IRQ or multiplexed one if not available
void __not_in_flash_func(isa_irq_raise)(uint8_t Source,uint8_t irqnb, bool Force)
{
 //PM_INFO ("!IR%d ",irqnb);
#if USE_HWIRQ
 #define BV_HWIRQ PM_DP_RAM[12] // Detected Hardware IRQ Number (Bit field for multiple IRQ)
 uint8_t IRQpin=0;

 if ((BV_HWIRQ>>irqnb)&1)  // Hardware IRQ (Detected jumper) exist ? (Use PicoMEM IRQ instead)
   switch(irqnb)
    {
     case 3: IRQpin=PIN_IRQ3;
             break;
     case 5: IRQpin=PIN_IRQ5;
             break;
#ifdef PIN_IRQ6
     case 6: IRQpin=PIN_IRQ6;
             break;
#endif
     case 7: IRQpin=PIN_IRQ7;
             break;
     default: break;
    }
 if (!IRQpin) { // Not hardware IRQ, use the PicoMEM IRQ instead
        isa_pm_irq_raise(Source,irqnb,false);
        return;
       }
  //PM_INFO("!HwI %d",IRQpin);
 /*
 if (Force) {  // Force the IRQ even if it is already up, to avoid missing IRQ when the previous one was not acknowledge (IRQ masked)
    gpio_put(IRQpin,1);         // IRQ Down (Force if the IRQ was not acknowledge (IRQ masked))
    busy_wait_us(5);
    }
  */    
 gpio_put(IRQpin,0);        // Move the needed IRQ Up
#else
//PM_INFO("!dI %d",IRQCommand);
 if (irqnb==BV_IRQ) 
   { // Interrupt using the PicoMEM IRQ
    /*
    if (Force) {  // Force the IRQ even if it is already up, to avoid missing IRQ when the previous one was not acknowledge (IRQ masked)        
       gpio_put(PIN_PM_IRQ,1);        // IRQ Down (Force if the IRQ was not acknowledge (IRQ masked))
       busy_wait_us(5);
      }
    */       
    gpio_put(PIN_PM_IRQ,0);        // Move the needed IRQ Up
    PM_INFO("!");
   } else isa_pm_irq_raise(Source,irqnb,false);
#endif
}

// Lower an Hardware IRQ or multiplexed one if not available
void __not_in_flash_func(isa_irq_lower)(uint8_t Source,uint8_t irqnb)
{
//PM_INFO("!Id ");
#if USE_HWIRQ
uint8_t IRQpin=0;

 if ((BV_HWIRQ>>irqnb)&1)  // Hardware IRQ (Detected jumper) exist ? (Use PicoMEM IRQ instead)
   switch(irqnb)
    {
     case 3: IRQpin=PIN_IRQ3;
             break;
     case 5: IRQpin=PIN_IRQ5;
             break;
#ifdef PIN_IRQ6
     case 6: IRQpin=PIN_IRQ6;
             break;
#endif
     case 7: IRQpin=PIN_IRQ7;
             break;
    }

 if (!IRQpin) {
        isa_pm_irq_lower(Source);
        return;
       } 
 gpio_put(IRQpin,1);     // IRQ Down
 
#else // in multiplexed IRQ mode, Ack is done by the BIOS
 if (irqnb==BV_IRQ)
   {
    gpio_put(PIN_PM_IRQ,1);          // IRQ Down   
   } else isa_pm_irq_lower(Source);  // if not 3, 5 or 7, use the PicoMEM IR
#endif
}


void PM_IRQ_DMA_Copy(uint8_t channel, uint32_t src_addr, uint32_t dst_addr, uint32_t length)
{


}