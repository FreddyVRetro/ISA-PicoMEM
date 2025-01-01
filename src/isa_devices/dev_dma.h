#pragma once

extern uint8_t dev_dma_install();
extern void dev_dma_remove();
extern bool dev_dma_installed();
extern void dev_dma_update();
extern bool dev_dma_ior(uint32_t CTRL_AL8,uint8_t *Data );
extern void dev_dma_iow(uint32_t CTRL_AL8,uint8_t Data);