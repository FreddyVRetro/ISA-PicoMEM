#pragma once
// Inline code to simplify the emulated DMA use

// pm_defines must be included before
//#include "pm_defines.h"

#define ED_STAT_NONE       0  // No DMA transfer requested
#define ED_STAT_REQUESTED  1  // DMA Transfer requested (IRQ Started, or not...)
#define ED_STAT_INPROGRESS 2 
#define ED_STAT_COMPLETED  3
#define ED_STAT_ERROR      4

// Link to the DMA "emulation" device
typedef struct dma_regs_t {
    volatile uint16_t baseaddr;
    volatile uint16_t basecnt;
    volatile uint8_t page;
} dma_regs_t;

extern dma_regs_t dmaregs[4];

__force_inline void e_dma_start(uint8_t channel, bool first_transfer, bool autoinit, uint8_t size)
{

// Transmit the parameters to the emulated DMA code
  PM_DP_RAM[OFFS_IRQPARAM]=channel;
  PM_DP_RAM[OFFS_IRQPARAM+1]=dmaregs[channel].page;
  PM_DP_RAM[OFFS_IRQPARAM+2]=first_transfer;
  PM_DP_RAM[OFFS_IRQPARAM+3]=size;

// Start the IRQ
  PM_IRQ_Raise(IRQ_E_DMA,autoinit);
}

__force_inline uint8_t e_dma_wait_blocking()
{
  do {} while (BV_DMAStatus=ED_STAT_INPROGRESS);
  return  BV_DMAStatus;
}

__force_inline uint8_t e_dma_status()
{
  return BV_DMAStatus;
}