#pragma once

#include "audio_fifo.h"

//#define USE_HWDMA 1

enum SB_TYPES {SBT_NONE=0,SBT_2=1,SBT_PRO2=2,SBT_16=3};


typedef struct sbdsp_t {

    uint8_t inbox;                   // command processing variables
    uint8_t outbox;
    uint8_t test_register;
    uint8_t current_command;
    uint8_t current_command_index;
    uint8_t mode;                    // DSP mode (DMA, DAC ...)

    uint8_t type;                    // Sound Blaster Type
    uint8_t irq;
    uint8_t dma8;

    volatile uint8_t mainloop_action;  // Action to execute in the main loop

    uint8_t  dma_mode;               // 2, 3, 4, 8 or 16 bits
    bool adpcm_ref;    

    // dma commands variables
    uint16_t dma_interval;           // Interval (in uS) between 2 samples read by the DMA
    uint16_t dma_block_size;         // Block size as sent to the command by the PC
    uint16_t dma_sample_count;       // Number of samples to transfer sent by the PC
    uint16_t dma_sample_count_rx;    // Count of samples to transfer (When = dma_sample_count, send an interrupt and clear)
    uint16_t dma_xfer_count;         // Number of transfers to do (for 16bit/stereo DMA)
    uint16_t dma_xfer_count_left;    // Number of transfers left to do (for 16bit/stereo DMA)
    
    bool autoinit;
    bool dma_enabled;
    bool dma_16bit;
    bool dma_stereo;
    bool dma_signed;
    uint8_t dma_rx_width;            // 1,2 or 4 bytes per transfer
    uint8_t dma_rx_index;            // Current DMA transfer width index

  //  volatile bool vdma_flushdata;    // Force the Samples copy from DMA to the FIFO
    uint32_t vdma_to_send;           // Count of bytes to transfer for the Virtual DMA
    uint32_t vdma_sent;              // Different variable, one for the main loop, one for the IRQ to avoid conflict

    uint16_t dac_interval;           // Interval (in uS) between 2 samples is sent to the DAC
    int32_t timer_delta;             // Timer delta for the DAC timer event
    

// Variables for the buffers copy/convertion
    uint16_t sample_rate;
    uint16_t sample_step;
    uint16_t increment;
    bool     readsample;
    uint8_t  prev_sample8u;          // For the 8bit unsigned mode
    uint8_t  prev_sample8u_left;     // For the 8bit unsigned mode
    uint16_t  prev_sample16u;          // For the 8bit unsigned mode
    uint16_t  prev_sample16u_left;     // For the 8bit unsigned mode

    uint16_t dac_pause_duration;
    uint8_t time_constant;          // Time constant send via the command 0x40

    bool speaker_on;
    volatile bool dav_pc;           // Data available PC
    volatile bool dav_dsp;
    volatile bool dsp_busy;
    bool dac_resume_pending;
    volatile bool irq_8_pending;
    volatile bool irq_16_pending;

    uint8_t reset_state;    
    uint8_t cur_sample8;            // Current sample for the DAC
    uint8_t silence8;               // Value used for silence in 8bit mode
    
    uint8_t mixer_command;  // To remove later

    uint8_t ident_e2[2];    // Buffer for the E2 command, to be able to return 2 bytes of data    
    //    volatile int16_t cur_sample;  // PicoGUS bufferless mode
} sbdsp_t;

extern sbdsp_t sbdsp;
extern audio_fifo_8_t sbdsp_fifo; // FIFO for the DSP samples

extern void sbdsp_init();
extern bool sbdsp_write(uint8_t address, uint8_t value);
extern uint8_t sbdsp_read(uint8_t address);
extern void sbdsp_process();
extern void sbdsp_dma_worker();

/*
486, IRQ shrared: 26/07/25
 Wold3D : Ok
 Mod Master XT : Crack at multiple frequencies
*/