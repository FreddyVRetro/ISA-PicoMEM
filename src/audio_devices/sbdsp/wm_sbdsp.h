#pragma once

#include "audio_fifo8.h"

//#define USE_HWDMA 1

// SB DSP command/event trace + FIFO-content probe (audio bring-up diagnostic).
// 1 = compile in the [SBTRACE]/[DMA][SBDSP] in=.. probes (Core-1 producer,
// Core-0 drain). 0 = fully compiled out (zero cost). Shared here so the
// sbdsp.cpp producer and the pm_cmd.cpp dump agree at compile time.
#define SB_DSP_WRITE_BUSY_TOGGLE 1   // 0x22C bit 7 busy on 8 of every 16 reads (DOSBox shape). REQUIRED: PoP waits for busy before ready (8570 end-of-level loop, fixed 2026-09-12)
#define SB_CMD_TRACE 0   // OFF (tags C/T/B/D/K + L/l/P/p/E/m/S; turn on only for low-command-rate hunts). Was 0: Cubic Player issues C14/S at a huge rate -> the SBTRACE UART drain on Core 0 starves pm_audio_domix -> I2S underruns -> silence. Turn back ON only for low-command-rate bring-up (PoP/DOOM detection).

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
    uint32_t sample_step;            /* 32-bit: rate_in << 32 / rate_out; uint16 overflowed at >=49716 Hz */
    uint32_t increment;             /* 32-bit phase accumulator; top 16 bits = interpolation fraction */
    bool     readsample;
    uint8_t  prev_sample8u;          // For the 8bit unsigned mode
    uint8_t  prev_sample8u_s0;       /* Linear interp: sample BEFORE prev_sample8u (s0 in s0→s1 pair) */
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
extern void sbdsp_trace_drain();
extern void wm10_eot_repulse(void);
extern bool sbdsp_in_dac_mode(void);
extern void sbdsp_dac_stats_print(void);
extern void sbdsp_e2_tick(void);          // drop an unanswered 0xE2 DMA byte after 100 ms (Core-0 main loop)   // [SB][DAC] servo stats, call from Core-0 main loop only   // direct-DAC (0x10) stream active: the guest polls 0x22C at 2x its sample rate, not a storm   // re-present an un-acked pulse-shaped single-cycle EOT (no-op in hold mode)   // SB cmd/event trace drain (no-op when SB_CMD_TRACE=0)
#if SB_CMD_TRACE
extern volatile uint8_t  sb_dbg_in_min;    // audio diag: raw FIFO byte range + out peak
extern volatile uint8_t  sb_dbg_in_max;
extern volatile int16_t  sb_dbg_out_peak;
extern volatile uint32_t sb_dbg_drained;
#endif

/*
486, IRQ shrared: 26/07/25
 Wold3D : Ok
 Mod Master XT : Crack at multiple frequencies
*/