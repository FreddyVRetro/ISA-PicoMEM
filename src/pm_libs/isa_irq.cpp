
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_defines.h"
#include "../pm_gvars.h"
#include "isa_irq.h"

//!!!!!  A Changer /Definir autrement
#if BOARD_PM15
#define PIN_IRQ 41
#else
#define PIN_IRQ 0
#endif

bool IRQ_raised;
bool IRQ_raise_inprogress;

void isa_irq_init()
{
 IRQ_raised=false;
 IRQ_raise_inprogress=false;
 BV_IRQ_Cnt=0;
 BV_IRQStatus=IRQ_Stat_COMPLETED;
}


// Wait that theere is no more IRQ in progress
// Timeout after the delay value (in uS)
bool isa_irq_Wait_End_Timeout(uint32_t delay)
{
  uint64_t initial_time;
  
  PCCR_PMSTATE=PCC_PMS_DOWAITCMD;  // Ask the PC to go in Wait CMD State
  // The PC Must be in Wait State before sending a CMD
  initial_time=get_absolute_time();
  do {
      if ((get_absolute_time()-initial_time)>delay) return false;
     } while (BV_IRQStatus==IRQ_Stat_INPROGRESS);
  
  return true;
}


//********************************************************
//*          IRQ Functions                               *
//********************************************************
// Source : Interrupt request source
// Arg    : Argument variable to send to the IRQ
// Force  : Start anyway even if running
uint8_t isa_irq_raise(uint8_t Source,uint8_t Arg,bool Force)
{

  uint8_t IRQSource=Source;

  if (IRQ_raise_inprogress)
     {
//      PM_ERROR("!!! IRQ Raise in Conflict !!");
      return (1);
     } else IRQ_raise_inprogress=true;

#if PM_PRINTF
   //if (Source==IRQ_IRQ5) PM_INFO("i%d,%d ",Source ,BV_IRQStatus);
/*
    switch(BV_IRQStatus)
     {
       case IRQ_Stat_REQUESTED:PM_ERROR(" Not Started ");
              break;     
       case IRQ_Stat_INPROGRESS: PM_ERROR(" In progress ");
              break;
       case IRQ_Stat_DISABLED: PM_ERROR(" Disabled ");
              break;
       case IRQ_Stat_INVALIDARG: PM_ERROR(" Invalid ");
              break; 
       case IRQ_Stat_SameIRQ: PM_ERROR(" Same IRQ ");
              break;
     }
*/              
#endif

/*
if (!Force && (BV_IRQStatus<IRQ_Stat_COMPLETED))
     {
      PM_ERROR(" ! Skip\n");
      IRQ_raise_inprogress=false;
      return (1);
     }
*/

  if (Source==IRQ_NE2000) 
      {
        //PM_INFO("w%d ",PM_Config->ne2000IRQ);
 //       PM_INFO("w");
       
        switch(PM_Config->ne2000IRQ) 
           {
          case 3:  IRQSource=IRQ_IRQ3;
               break;
          case 5:  IRQSource=IRQ_IRQ5;
               break;
          case 7:  IRQSource=IRQ_IRQ7;
               break;               
          case 9:  IRQSource=IRQ_None;
               break;
          default: IRQSource=IRQ_IRQ3;
               break;
           }
      }

#if UART_PIN0
#else
  if (IRQ_raised)
       {
  //      PM_ERROR(" IRQ still Up %d",BV_IRQStatus);
        gpio_put(PIN_IRQ,1);  // IRQ Down
        busy_wait_us(50);
       }

 // if (!Force)
  BV_IRQArg=Arg;
  if (BV_IRQStatus!=IRQ_Stat_INPROGRESS) BV_IRQStatus=IRQ_Stat_REQUESTED;

  BV_IRQSource=IRQSource;     // Change the source at the last "second"
  gpio_put(PIN_IRQ,0);        // IRQ Up
  IRQ_raised=true;
#endif

/*
// Debug display delay before Start
uint64_t initial_time; 
initial_time=get_absolute_time();
do {
    if ((get_absolute_time()-initial_time)>10000) break;;
   } while (BV_IRQStatus!=IRQ_Stat_INPROGRESS);
PM_INFO("d%x",get_absolute_time()-initial_time);
*/

  IRQ_raise_inprogress=false;
  return true;
}

void isa_irq_lower()
{
//PM_INFO("!Id ");
#if UART_PIN0
#else    
    gpio_put(PIN_IRQ,1);  // IRQ Down
    IRQ_raised=false;    
#endif    
}