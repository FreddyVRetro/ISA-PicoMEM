#pragma once

typedef struct sbdsp_t {
    uint8_t inbox;
    uint8_t outbox;
    uint8_t test_register;
    uint8_t current_command;
    uint8_t current_command_index;

    uint16_t dma_interval;     
    // int16_t dma_interval_trim;
    uint8_t dma_transfer_size;
    uint8_t  dma_buffer[DSP_DMA_FIFO_SIZE];
    volatile uint16_t dma_buffer_tail;
    volatile uint16_t dma_buffer_head;

    uint16_t dma_pause_duration;
    uint8_t dma_pause_duration_low;

    uint16_t dma_block_size;
    uint32_t dma_sample_count;
    uint32_t dma_sample_count_rx;

#ifdef BOARD_PICOMEM  // Added for DMA emulation
// DMA emulation is done by multiple copy of 128 bytes and the remaining added to the last one
// DMA event (timer ISR) is launched for every data copy, the data read is pushed to the dma buffer fifo at the next ISR.
// ( Moved from buffer in shared emulated RAM to dma_buffer)
    uint32_t e_dma_time_to_irq;
    uint32_t e_dma_samples_read;    // Nb of bytes read since the previous event
    bool e_dma_firsttransfer;

// Variables for the buffers copy/convertion
    uint16_t sample_rate;
    uint32_t sample_step;
    int16_t  minimum_fifo_level;    // Minimum fifo level to start the Audio output
    int16_t  previous_level;
    uint8_t  samelevel_count;

#endif

    uint8_t time_constant;
    // uint16_t sample_rate;
    // uint32_t sample_step;
    // uint64_t cycle_us;

    // uint64_t sample_offset;  
    // uint8_t sample_factor;
                
    bool autoinit;    
    bool dma_enabled;

    bool speaker_on;
        
    volatile bool dav_pc;
    volatile bool dav_dsp;
    volatile bool dsp_busy;
    bool dac_resume_pending;

    uint8_t reset_state;  
   
    volatile int16_t cur_sample;
} sbdsp_t;