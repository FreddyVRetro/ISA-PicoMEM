#pragma once

typedef struct sbdsp_t {
    uint8_t inbox;                   // command processing variables
    uint8_t outbox;
    uint8_t test_register;
    uint8_t current_command;
    uint8_t current_command_index;

    volatile uint8_t mainloop_action;         // Action to execute in the main loop

    uint8_t irq;

    // dma commands variables
    uint16_t dma_interval;           // Interval (in uS) between 2 samples is sent to the DAC
    uint16_t dma_block_size;         // Block size as sent to the command by the PC
    uint16_t dma_sample_count;
    uint16_t dma_sample_count_rx;
    uint16_t dma_pause_duration;
    uint8_t  dma_pause_duration_low;
    volatile bool autoinit;    
    volatile bool dma_enabled;

#ifdef BOARD_PICOMEM  // Added for DMA emulation
    uint32_t dma_time_to_irq;

// Variables for the buffers copy/convertion
    uint16_t sample_rate;
    uint16_t sample_step;
    uint16_t increment;
    bool  readsample;
    uint8_t  prev_sample8;

//    int16_t  previous_level;
//    uint8_t  samelevel_count;

#endif

    uint8_t time_constant;
                
    bool speaker_on;
        
    volatile bool dav_pc;       // Data available PC
    volatile bool dav_dsp;
    volatile bool dsp_busy;
    bool dac_resume_pending;

    uint8_t reset_state;  
   
    volatile int16_t cur_sample;
} sbdsp_t;

extern sbdsp_t sbdsp;

extern void sbdsp_ml_command();