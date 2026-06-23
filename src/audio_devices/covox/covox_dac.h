#pragma once
#include "audio_fifo.h"

#define CVX_TYPE_NONE       0
#define CVX_TYPE_DAC        1  // Covox DAC
#define CVX_TYPE_DAC_STEREO 2  // Covox DAC Stereo in one
#define CVX_TYPE_DAC_DUAL   3  // Covox DAC Dual (LPT1+LPT2)
#define CVX_TYPE_DISNEY     4

typedef struct cvx {
    audio_fifo_8_t  *fifo;        // FIFO for the DAC samples
    audio_fifo_8s_t *fifo_stereo; // FIFO for the DAC samples (Stereo)

    uint8_t type=CVX_TYPE_NONE;    // Type of the Covox DAC (0=off, 1=covox, 2=Disney Sound Source)
    uint8_t channel;           // 
    uint8_t sample;
    uint8_t sampleleft;
    uint8_t ctrlcount;
    bool   isleft;             // For stereo DAC, is it left channel next?

    uint16_t dac_interval;
    uint16_t sample_step;
    uint16_t increment;
    bool     readsample;
    uint8_t  prev_sample8;
    uint8_t  prev_sample8left;    
    uint8_t  control;       // Copy of the control register

    uint8_t dss_fifo[16]; // Disney Sound Source FIFO
    uint8_t dss_fifo_head; // Head of the Disney Sound Source FIFO
    uint8_t dss_fifo_tail; // Tail of the Disney Sound Source FIFO
} cvx_t;

extern cvx_t cvx;

uint8_t cvx_enable(uint8_t type, uint16_t baseport, uint16_t baseport2);
void cvx_disable();
void cvx_start_dac();
void cvx_stop_dac();
bool cvx_inp(uint16_t address, uint8_t *data);
void cvx_outp(uint16_t address, uint8_t data);
void cvx_updatebuffer(int16_t* buff,uint32_t samples);
void cvx_test();