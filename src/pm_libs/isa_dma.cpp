// Code to support the Hardware and emulated DMA
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "pm_pccmd.h"
#include "dev_memory.h"
#include "dev_audiomix.h"
#include "isa_dma.h"
#include "isa_irq.h"

#if USE_HWDMA
#if BOARD_PM15 || BOARD_PM20
#include "isa_iomem_m2_v15.pio.h"
#endif
#endif

uint isa_dma_w_offset;

// DMA transfer will be done through a buffer in the BIOS RAM (last 2Kb)
pm_dma_t isa_dma;
uint32_t DMA_IRQ_Sent;   // To count the NB of DMA IRQ sent and acknowledge (For send/Not acked detection)
uint32_t DMA_IRQ_Acked;

uint64_t DMA_IRQ_AckTime;
uint64_t DMA_Start_Time;

#define DMA_IRQ_START_WAIT 500 // Need 500us between the Ack and next Request

#if USE_HWDMA
volatile irq_handler_t DMA_isr_ptr;    // Pointer to the current DMA ISR
volatile irq_handler_t New_DMA_isr_ptr; // Pointer to the new DMA ISR
#endif

uint8_t pm_dma_isr_dev=0; // Currently configured ISR for the Hardware DMA (0 if no HW DMA, 1 for SB DSP, 2 for GUS, etc...)


// Default Blank ISR for the HW DMA
#if USE_HWDMA

static void __not_in_flash_func(isa_dma_isr)(void) {
    DMA_isr_ptr();
  }

static void __not_in_flash_func(isa_dma_isr_blank)(void) {
    isa_dma_complete_write();
  }
#endif


void isa_dma_init()
{
  PM_INFO("isa_dma_init\n");
  isa_dma.buffer_level=0;
  isa_dma.dma_remain=0;
  isa_dma.vdma_wait_irq=false;   // True when a DMA copy request was sent
  DBG_OFF_2();  
  DMA_IRQ_AckTime=time_us_64();
}

// To be started early in the code, at Pico startup
void isa_dma_install()
{
  PM_INFO("isa_dma_install\n");
#if USE_HWDMA
if (pio_sm_is_claimed(DMA_PIO, DMA_SM)) PM_ERROR("pio/sm %s/%d already claimed\n",DMA_PIO, DMA_SM);
   else PM_INFO("Claiming PIO %s SM %d\n",DMA_PIO, DMA_SM); 
#if BOARD_PM15
PM_INFO(" > Add ISA DMA Write SM\n");
  pio_sm_claim(DMA_PIO,DMA_SM);
  isa_dma_w_offset = pio_add_program(DMA_PIO, &isa_dma_w_program);
  isa_dma_w_program_init(DMA_PIO, DMA_SM, isa_dma_w_offset);
#elif BOARD_PM20
PM_INFO(" > Add ISA DMA Write SM with TC\n");
  pio_sm_claim(DMA_PIO,DMA_SM);
  isa_dma_w_offset = pio_add_program(DMA_PIO, &isa_dma_w_tc_program);
  isa_dma_w_tc_program_init(DMA_PIO, DMA_SM, isa_dma_w_offset);
#endif

  irq_set_enabled(PIO0_IRQ_0, false);
  pio_set_irq0_source_enabled(DMA_PIO, pis_sm3_rx_fifo_not_empty, true);  // !! To change if the DMA SM value change
  irq_set_priority(PIO0_IRQ_0, PICO_HIGHEST_IRQ_PRIORITY);
  irq_set_exclusive_handler(PIO0_IRQ_0, isa_dma_isr);
  irq_set_enabled(PIO0_IRQ_0, true);

#endif
  
  isa_dma_init();
}

// Executed when the DMA Copy interrupt is completed (run from core1)
void isa_dmacpy_ack()
{
  isa_dma.dma_remain-=isa_dma.to_transfer;   // Update the remaining bytes to transfer
  isa_dma.dma_c_adress+=isa_dma.to_transfer; 
  isa_dma.buffer_readindex=0;                // Reset the DMA buffer read index  
  isa_dma.buffer_level=isa_dma.to_transfer;  // Update the bytes in the DMA buffer
  isa_dma.vdma_wait_irq=false;
  DMA_IRQ_Acked++;
  DMA_IRQ_AckTime=time_us_64();
  DBG_OFF_2();
}

void isa_dma_start_copy(uint8_t channel, uint32_t Src, uint32_t Dest, uint32_t Len, uint16_t NewAddr, uint16_t NewSize)
{
 bool force_irq=false;
// Copy the parameters for the IRQ command
if (DMA_IRQ_Sent!=DMA_IRQ_Acked) { // If the previous DMA Copy command was not acknowledged
   PM_INFO("DMA not acked %d!=%d\n",DMA_IRQ_Sent,DMA_IRQ_Acked);
   DMA_IRQ_Sent=0;
   DMA_IRQ_Acked=0;
   force_irq=true;
//   return; // Do not send a new DMA Copy command
   }

   PM_DP_RAM[OFFS_IRQ_PARAM]=0xFF;  // Reset the IRQ parameters
   PM_DP_RAM[OFFS_IRQ_PARAM+1]=0xFF;
  // Source Offset
   PM_DP_RAM[OFFS_IRQ_PARAM]=(uint8_t)Src&0xF;
   PM_DP_RAM[OFFS_IRQ_PARAM+1]=0;
   // Source Segment
   PM_DP_RAM[OFFS_IRQ_PARAM+2]=(uint8_t)(Src>>4);
   PM_DP_RAM[OFFS_IRQ_PARAM+3]=(uint8_t)(Src>>12);
   // Destination Offset
   PM_DP_RAM[OFFS_IRQ_PARAM+4]=(uint8_t)Dest&0xF;
   PM_DP_RAM[OFFS_IRQ_PARAM+5]=0;
   // Destination Segment
   PM_DP_RAM[OFFS_IRQ_PARAM+6]=(uint8_t)(Dest>>4);
   PM_DP_RAM[OFFS_IRQ_PARAM+7]=(uint8_t)(Dest>>12);  
   // Length
   PM_DP_RAM[OFFS_IRQ_PARAM+8]=(uint8_t)Len;
   PM_DP_RAM[OFFS_IRQ_PARAM+9]=(uint8_t)(Len>>8);   

   // New DMA regs values for after the copy (Address and Size)
   PM_DP_RAM[OFFS_IRQ_PARAM+10]=(uint8_t)NewAddr&0xFF;
   PM_DP_RAM[OFFS_IRQ_PARAM+11]=(uint8_t)(NewAddr>>8);
   PM_DP_RAM[OFFS_IRQ_PARAM+12]=(uint8_t)NewSize&0xFF;
   PM_DP_RAM[OFFS_IRQ_PARAM+13]=(uint8_t)(NewSize>>8);

   // DMA Channel
   PM_DP_RAM[OFFS_IRQ_PARAM+14]=channel;

   isa_pm_irq_raise(IRQ_R_DMACOPY,1,force_irq);  // Send a DMA Copy interrupt request
   DMA_IRQ_Sent++;
//   PM_INFO("S%dA%d",DMA_IRQ_Sent, DMA_IRQ_Acked);
}

// Perform a DMA Transfer (Emulated DMA Code)
// Transfer a 128 byte block from the PC to the PicoMEM RAM using a Single buffer (No FIFO)
void isa_dma_transfer(uint8_t channel)
{
 // PM_INFO("!BL%d ",isa_dma.buffer_level);

  if (isa_dma.vdma_wait_irq) 
      {
  /*      if ((SVAR_IRQ->PM_IRR+SVAR_IRQ->PM_ISR)==0){
           DBG_ON_2();
           busy_wait_us(5);
           DBG_OFF_2();
           busy_wait_us(5);
          }          */
/*        {
              PM_INFO ("!TO ");
              isa_dma.vdma_wait_irq=false;  // Give a new chance to be started
            } */
        return;
      }

  if ((time_us_64()-DMA_IRQ_AckTime)<100) return; // Too fast, wait for the defined time
   //if ((time_us_64()-PM_IRQ_AckTime)<200) return; // Too fast, wait for the defined time
  
   // !! Check again the DMA Buffer send, as may change since the last check
  if (isa_dma.buffer_level!=0) return;  // DMA Copy finished and buffer empty, Don't fill again

// 1) Check if the DMA registers were modified

  if (dmachregs[channel].changed)  // !! Risk if only one change is captured when arriving here, Nobody should do that
    {
      dmachregs[channel].changed=false;
      isa_dma.dma_address=((uint32_t) dmachregs[channel].page<<16)+dmachregs[channel].baseaddr;
      isa_dma.dma_c_adress=isa_dma.dma_address;
      isa_dma.dma_size=dmachregs[channel].basecnt+1;  // DMA register is the size -1
      isa_dma.dma_remain=isa_dma.dma_size;

      PM_INFO("DMA> A:%x S:%d ",isa_dma.dma_c_adress,isa_dma.dma_size);
      if (dmachregs[channel].autoinit) PM_INFO("Autoinit enabled\n");
    }

  if (isa_dma.dma_size==0) {
      PM_INFO("DMA> No DMA size set, can't transfer\n");
      return; // No DMA size set
    }

  if (isa_dma.dma_remain==0) {
      if (dmachregs[channel].autoinit) // Autoinit mode, so reinit the DMA size
        {
          //PM_INFO("DMA>Cont");
          isa_dma.dma_remain=isa_dma.dma_size;
          isa_dma.dma_c_adress=isa_dma.dma_address;
        } else
         {
          return; // No DMA size set
         }
    }


  // Minimum size to transfer is DMA_BLOC_SIZE/2
  isa_dma.to_transfer = (isa_dma.dma_remain<(DMA_BLOC_SIZE+DMA_BLOC_SIZE/2)) ? isa_dma.dma_remain : DMA_BLOC_SIZE;

  uint32_t DMA_Buffer_Addr=((uint32_t) PM_BIOSADDRESS<<4)+16*1024+PM_DMAB_Offset;    // "Dual Port RAM" is at BIOS Address + 16Kb
//  PM_INFO("DMA> TX %x>%x r%d tt%d \n",isa_dma.dma_c_adress, DMA_Buffer_Addr, isa_dma.dma_remain, isa_dma.to_transfer);
//  PM_INFO("DMA> TX %x tt%d na %x ns %x\n",isa_dma.dma_c_adress, isa_dma.to_transfer,(uint16_t) (isa_dma.dma_c_adress+isa_dma.to_transfer),
//          (uint16_t) (isa_dma.dma_remain-isa_dma.to_transfer-1));
  
  isa_dma.vdma_wait_irq=true;
  DMA_Start_Time=time_us_64();  
  DBG_ON_2();
  isa_dma_start_copy(channel, isa_dma.dma_c_adress, DMA_Buffer_Addr, isa_dma.to_transfer,
                  (uint16_t) (isa_dma.dma_c_adress+isa_dma.to_transfer),(int16_t) (isa_dma.dma_remain-isa_dma.to_transfer-1));
  
}