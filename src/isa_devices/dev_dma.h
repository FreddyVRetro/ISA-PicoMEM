#pragma once

typedef struct dma_regs_t {
    volatile uint16_t baseaddr;
    volatile uint16_t basecnt;
    volatile uint8_t page;
    volatile bool autoinit;
    volatile bool mask;
    volatile uint8_t changed;  // Set to one if a value of the channel was changed by the CPU
} dma_regs_t;

extern volatile bool dma_ignorechange;  // Inform the Virtual DMA that the PicoMEM is sending the update (then, ignore it)
extern dma_regs_t dmachregs[4];

extern uint8_t dev_dma_install();
extern void dev_dma_remove();
extern bool dev_dma_installed();
extern void dev_dma_update();
extern bool dev_dma_ior(uint32_t Addr,uint8_t *Data);
extern void dev_dma_iow(uint32_t Addr,uint8_t Data);