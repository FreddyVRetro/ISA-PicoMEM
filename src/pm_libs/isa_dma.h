#pragma once

// PicoMEM DMA from PC can be direct or sent to the DMA FIFO
// The DMA fifo is loacted in the emulated RAM ( PM_DP_RAM[PM_DMAB_Offset] ) and is 2Kb max

#include "pm_defines.h"
#include "dev_dma.h"
#include "hardware/pio.h"

// Virtual DMA definitions
#define PM_DMAB_Offset 6*1024
#define PM_DMAB_Size   1024  // 2*1024 max
#define DMA_BLOC_SIZE 128    // Size of the DMA transfer block (128 bytes)

// Real DMA definitions
#define DMA_PIO pio0
#define DMA_SM 3

#define DMA_DEV_SB 1
#define DMA_DEV_GUS 2

#if USE_HWDMA
extern volatile irq_handler_t DMA_isr_ptr;    // Pointer to the current DMA ISR
#endif

typedef struct pm_dma {
    volatile uint8_t active;        // True if the dma lib is initialized
//    volatile uint8_t channel;      // Channel of the current DMA transfer

// DMA bufer (PC side) values
    volatile uint16_t dma_remain;
    uint32_t dma_address;           // Initial PC address (To Loop)
    uint16_t dma_size;
    uint32_t dma_c_adress;          // Current transfer address
    uint32_t to_transfer;
    volatile uint32_t buffer_level; // Bytes in the DMA buffer
    volatile uint32_t buffer_readindex;   // Read index in the DMA buffer
    volatile bool vdma_wait_irq;   // True when a DMA copy request was sent
    uint32_t irq_timeout_cnt;

} pm_dma_t;

extern uint isa_dma_w_offset;
extern pm_dma_t isa_dma;

extern void isa_dma_init();
extern void isa_dma_install();
//extern void isa_dma_set_isr(uint8_t dev_nb,irq_handler_t dma_isr);

__force_inline  void isa_dma_set_isr(irq_handler_t dma_isr)
{
#if USE_HWDMA
 // PM_INFO("isa_dma_set_isr %p\n",dma_isr); DMA_isr_ptr=dma_isr;
  DMA_isr_ptr=dma_isr;
#endif  
}

__force_inline bool isa_dma_isr_active(irq_handler_t dma_isr)
{
#if USE_HWDMA
  return (DMA_isr_ptr==dma_isr);
#endif
  return true;  // If no HW DMA, always return true
}

__force_inline extern void isa_dma_start_write()
{
#if USE_HWDMA    
  pio_sm_put_blocking(DMA_PIO, DMA_SM, 0xffffffffu);
#else
//  sbdsp.vdma_to_send++;      // Ask one more byte to transfer
#endif
}

__force_inline extern uint32_t isa_dma_complete_write()
{
  uint32_t dma_data = pio_sm_get(DMA_PIO, DMA_SM);
  return dma_data;
}

__force_inline extern uint32_t isa_dma_complete_write_blocking()
{
  uint32_t dma_data = pio_sm_get_blocking(DMA_PIO, DMA_SM);
  return dma_data;
}

// Clean the DMA transfer buffer
__force_inline void isa_dma_buffer_free()
{
  isa_dma.buffer_level=0;
  isa_dma.vdma_wait_irq=false;
}

__force_inline bool isa_dma_buffer_empty()
{
  return (isa_dma.buffer_level==0);
}

__force_inline extern void isa_dma_stop_write() {
    pio_sm_exec(DMA_PIO, DMA_SM, pio_encode_jmp(isa_dma_w_offset));
}


// Get one byte from the DMA buffer
__force_inline int8_t isa_dma_buffer_get()
{
  uint8_t res;
  isa_dma.buffer_level--;
  res=PM_DP_RAM[PM_DMAB_Offset+isa_dma.buffer_readindex];
  isa_dma.buffer_readindex++;
  return res;
}

// Get a pointer to the DMA buffer, and update the read index / level
__force_inline int8_t *isa_dma_buffer_get_ptr(uint16_t size)
{
  int8_t *res;
  res=(int8_t *) &PM_DP_RAM[PM_DMAB_Offset+isa_dma.buffer_readindex];
  isa_dma.buffer_level-=size;
  isa_dma.buffer_readindex+=size;
  return res;
}

void isa_dmacpy_ack();
// Transfer DATA from the current DMA Channel to the DMA buffer
void isa_dma_transfer(uint8_t channel);