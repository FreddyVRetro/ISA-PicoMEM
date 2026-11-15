#pragma once

#include <stdint.h>

// GUS DMA-chain instrumentation. Counters are bumped on Core 1 (the MCA
// handler / DMA ISR) and dumped from the Core 0 main loop -- never printf
// from Core 1. Reading each field tells you where the patch-upload chain
// breaks (see the [gus_dma] console dump in pm_cmd.cpp):
//   starts=0            -> guest never wrote GF1 reg 0x41 enable (I/O decode /
//                          GUS not selected / wrong ULTRASND port)
//   starts>0, events=0  -> StartDMA ran but the DMA timer event never fired
//   events>0, isr=0     -> REQ asserted but no CDMA byte arrived (arb/grant/
//                          CDMA not responding -- cross-check [wm_dma] req/grant)
//   isr>0, tc=0         -> bytes flowing but /TC never seen (count/TC issue)
//   tc>0, irq>0         -> completed + IRQ raised (if PLAYMIDI still stuck, the
//                          guest-side IRQ routing / ack is the suspect)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile uint32_t starts;      // GUS_StartDMA entered (reg 0x41 bit0 = 1)
    volatile uint32_t events;      // GUS_DMA_EventHandler fired (REQ asserted)
    volatile uint32_t isr_calls;   // GUS_DMA_isr fired (byte captured from CDMA)
    volatile uint32_t bytes;       // bytes written into GUS DRAM
    volatile uint32_t tc;          // terminal-count reached
    volatile uint32_t irq;         // DMA-TC IRQ raised to the guest
    volatile uint32_t start_addr;  // dmaAddr at transfer start
    volatile uint32_t last_addr;   // dmaAddr at last byte
    volatile uint8_t  last_ctrl;   // last DMAControl (GF1 reg 0x41) programmed
    volatile uint8_t  irq_num;     // myGUS.irq at the TC IRQ (should match ULTRASND)
    volatile uint32_t irqstat_rd;  // guest reads of the 2X6 IRQ-status reg (ISR ran?)
    volatile uint8_t  eff_irq;     // last GUS_EffectiveIRQStatus() the 2X6 poll saw
    volatile uint8_t  raw_irqstat; // raw myGUS.IRQStatus at that read
    volatile uint32_t reg41_rd;    // guest reads of GF1 reg 0x41 (DMA-TC ack path)

    // DMA byte capture: the first GUS_DMA_CAP_N bytes of the FIRST patch upload,
    // both as delivered by the CDMA (raw) and as stored into GUS DRAM (post
    // MSB-invert). Lets us eyeball whether 16-bit samples arrive intact or the
    // byte pairing / invert is wrong. Dumped once from Core 0.
    #define GUS_DMA_CAP_N 64
    volatile uint8_t  cap_raw[GUS_DMA_CAP_N];  // byte from CDMA (pre-invert)
    volatile uint8_t  cap[GUS_DMA_CAP_N];      // byte stored to DRAM (post-invert)
    volatile uint32_t cap_n;                   // count captured
    volatile uint32_t cap_addr0;               // dmaAddr of the first captured byte
    volatile uint8_t  cap_ctrl;                // DMAControl at capture (16-bit/invert bits)
} gus_dma_dbg_t;

extern gus_dma_dbg_t gus_dma_dbg;

#ifdef __cplusplus
}
#endif
