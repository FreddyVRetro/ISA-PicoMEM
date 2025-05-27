#pragma once

#include "../pm_defines.h"
#include "dev_dma.h"
// Inline code to simplify the emulated DMA use

// pm_defines must be included before
//#include "pm_defines.h"

#define PM_DMAB_Offset 6*1024
#define PM_DMAB_Size   1024  // 2*1024 max

#define DMA_CMD_NONE        0
#define DMA_CMD_START       1
#define DMA_CMD_CONTINUE    2
#define DMA_CMD_STOP        3
#define DMA_CMD_RESET       4

typedef struct pm_dma {            
    volatile uint8_t active;       // True if the dma lib is initialized
    volatile uint8_t channel;      // Channel of the current DMA transfer
    volatile uint8_t inprogress;   // DMA channel performing transfer 0: None (As DMA 0 can't be used)
    uint8_t  cmd;          // Command sent from the device
    uint8_t  page;
    uint16_t transfer_size;   // bytes to transfer (to reinit the remain value)
    uint16_t transfer_remain; // Remaining bytes to transfer
    uint16_t sizept;          // Size to transfer each time the transfer is called
// DMA bufer (PC side) values
    volatile uint16_t dma_remain;
    uint32_t dma_address;      // Initial PC address (To Loop)
    uint16_t dma_size;
    uint32_t dma_c_adress;     // Current transfer address
// FIFO (pico side) values
    uint16_t fifo_tail;
    uint16_t fifo_head;
    uint16_t fifo_end;         // variable fifo end to not transfer data block in 2 times
    uint16_t copy_cnt;
} pm_dma_t;

extern pm_dma_t isa_dma;

extern void isa_dma_init();

__force_inline bool isa_dma_fifo_empty()
{ 
  return (isa_dma.fifo_head==isa_dma.fifo_tail);
}

// ! Does not check if the fifo is empty, must be checked before (for speed reason)
__force_inline int8_t isa_dma_fifo_getb()
{
  // move to the next byte
  int8_t res=PM_DP_RAM[PM_DMAB_Offset+isa_dma.fifo_tail];
  if (isa_dma.fifo_tail==(isa_dma.fifo_end-1)) isa_dma.fifo_tail=0;
     else isa_dma.fifo_tail++;
  return res;
}

// ! Does not check if the fifo is empty, must be checked before (for speed reason)
__force_inline int16_t isa_dma_fifo_getw()
{
  // move to the next byte
  int16_t res=(int16_t) PM_DP_RAM[PM_DMAB_Offset+(isa_dma.fifo_tail>>1)];
  if (isa_dma.fifo_tail==isa_dma.fifo_end) isa_dma.fifo_tail=0;
     else isa_dma.fifo_tail+=2;
  return res;
}

//Call this fonction regularly to do DMA transfer (Multiple bytes are copied)
extern uint16_t isa_dma_transfer_fifo();

__force_inline void isa_dma_stop()
{
  if (isa_dma.cmd!=DMA_CMD_NONE) PM_INFO("DMA> Previous command not done\n");  
  isa_dma.cmd=DMA_CMD_STOP;
}

// Clean FIFO and Stop the DMA transfer
__force_inline void isa_dma_reset()
{
  if (isa_dma.cmd!=DMA_CMD_NONE) PM_INFO("DMA> Previous command not done\n");
  isa_dma.cmd=DMA_CMD_RESET;
}

// Do another transfer after the current one
__force_inline void isa_dma_continue_fifo(uint8_t channel, uint16_t size)
{
  if (isa_dma.channel!=channel) isa_dma.channel=channel;
  if (isa_dma.cmd!=DMA_CMD_NONE) PM_INFO("DMA> Previous command not done\n");
  isa_dma.cmd=DMA_CMD_CONTINUE;
  isa_dma.transfer_size=size;
}

// Start a DMA transfer to the FIFO
__force_inline void isa_dma_start_fifo(uint8_t channel, uint16_t size, uint16_t sizept)
{
  if (isa_dma.channel!=channel) isa_dma.channel=channel;
  if (isa_dma.cmd!=DMA_CMD_NONE) PM_INFO("DMA> Previous command not done\n");
  isa_dma.transfer_size=size;
  isa_dma.sizept=sizept;
  isa_dma.inprogress=channel;     // 
  isa_dma.cmd=DMA_CMD_CONTINUE;
//  isa_dma.cmd=DMA_CMD_START;
}