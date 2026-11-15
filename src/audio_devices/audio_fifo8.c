/**
 * audio_fifo.c - Implementation of shared FIFO functionality
 * 8Bit Version for SBDSP audio output.
 * 
 * Implements the internal functions declared in audio_fifo8.h that are
 * needed by both producer and consumer modules.
 */

#include "audio_fifo8.h"
/* #include "pico/stdlib.h" */
#include <string.h>
#include <stdio.h>

#include "../pm_debug.h"  // For PM_INFO, PM_ERROR, etc.

#define AUDIO_SINE    0
#define SINE_8B       0
#define SINE_UNSIGNED 0

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

// Initialize the FIFO (called by producer_init or directly by user)
void afifo_init8(audio_fifo_8_t *fifo) {
    // Initialize FIFO structure
    if (fifo==NULL) PM_ERROR("FIFO Ptr is NULL");
    memset(fifo, 0, sizeof(audio_fifo_8_t)); // Use the passed pointer
    fifo->state = FIFO_STATE_STOPPED;
    fifo->write_idx = 0;
    fifo->read_idx = 0;
//    fifo->samples_in_fifo = 0;
    fifo->samples_received = 0;
    fifo->samples_sent = 0;  
    fifo->start_threshold = AFIFO8_START_THRESHOLD;
}


// FREE SPACE in the fifo (historical name -- see afifo_count8 for the level)
uint32_t afifo_level8(audio_fifo_8_t *fifo) {
  return AFIFO8_SIZE - (fifo->samples_received-fifo->samples_sent);
}
// Samples currently buffered
uint32_t afifo_count8(audio_fifo_8_t *fifo) {
  return fifo->samples_received-fifo->samples_sent;
}
// Start threshold for the current stream: DMA keeps the half-full default
// (absorbs chassis grant jitter); direct DAC (0x10) wants a few ms, so that
// running dry costs ~3 ms of silence, not 51 ms (2026-09-12).
void afifo_set_start8(audio_fifo_8_t *fifo, uint32_t n) {
    if (n == 0) n = 1;
    if (n > AFIFO8_SIZE / 2) n = AFIFO8_SIZE / 2;
    fifo->start_threshold = n;
}
// Reset the FIFO to empty state (and the default start threshold)
void afifo_reset8(audio_fifo_8_t *fifo) {
    fifo->state = FIFO_STATE_STOPPED;
    fifo->write_idx = fifo->read_idx;
    
    fifo->samples_received = 0;
    fifo->samples_sent = 0;    
    fifo->start_threshold = AFIFO8_START_THRESHOLD;
}

// Add a sample to the FIFO 8Bit Mono (called by producer).
// __not_in_flash_func: this is called from Core 1's wm_dma_consume_byte
// (via the SBDSP completion_isr) during the DMA IOW data phase. Keeping
// it in main SRAM avoids XIP cache misses that would otherwise extend
// the bus cycle by up to several us -- causing CACP arb-time pressure
// and POST 113 on sustained sbdma transfers (observed 2026-06-03).
bool __not_in_flash_func(afifo_add_sample8)(audio_fifo_8_t *fifo, int8_t sample) {
#if AUDIO_SINE
    return true;
#else
//    if (fifo->samples_in_fifo == AFIFO8_SIZE) {
    if ((fifo->samples_received-fifo->samples_sent) == AFIFO8_SIZE) {
    return false;
    }
    fifo->buffer[fifo->write_idx] = sample;
    fifo->write_idx = (fifo->write_idx + 1) & (AFIFO8_BITS);
    fifo->samples_received++;

    if ((fifo->samples_received-fifo->samples_sent) == fifo->start_threshold) {
        /* No PM_INFO here -- this runs on Core 1 inside the DMA IOW
           window. A printf (~50 us UART) would hold the bus cycle
           and trip POST 113. Silent state transition is fine. */
        fifo->state = FIFO_STATE_RUNNING;
    }
    return true;
#endif
}

// Add multiple samples to the FIFO 8Bit Mono (called by producer)
bool afifo_add_samples8(audio_fifo_8_t *fifo, const int8_t *samples_buffer, uint32_t num_samples_to_add) {

    // Check if there's enough space in the FIFO
      uint32_t available_space = AFIFO8_SIZE - (fifo->samples_received-fifo->samples_sent);

    if (num_samples_to_add > available_space) {
        return false; // Not enough space for all requested samples
    }

    // Check basic conditions
    if (num_samples_to_add == 0) {
        return true; // Nothing to add, operation considered successful
    }

    // Determine how many samples can be written before wrap-around
    // AFIFO8_SIZE is the total capacity. write_idx is 0 to AFIFO8_SIZE-1.
    uint32_t samples_to_end = AFIFO8_SIZE - fifo->write_idx;

    if (num_samples_to_add <= samples_to_end) {
        // All samples fit without wrapping around the physical end of the buffer
        memcpy(&fifo->buffer[fifo->write_idx], samples_buffer, num_samples_to_add * sizeof(audio_sample_t));
    } else {
        // Samples will wrap around the physical end of the buffer
        // Part 1: Copy samples up to the end of the physical buffer
        memcpy(&fifo->buffer[fifo->write_idx], samples_buffer, samples_to_end * sizeof(audio_sample_t));

        // Part 2: Copy remaining samples from the beginning of the physical buffer
        uint32_t remaining_samples = num_samples_to_add - samples_to_end;
        memcpy(&fifo->buffer[0], samples_buffer + samples_to_end, remaining_samples * sizeof(audio_sample_t));
    }

    // Update FIFO state
    fifo->write_idx = (fifo->write_idx + num_samples_to_add) & (AFIFO8_BITS);
    fifo->samples_received += num_samples_to_add;

    if ((fifo->samples_received-fifo->samples_sent) >= fifo->start_threshold) {        
        if (fifo->state == FIFO_STATE_STOPPED) PM_INFO("Start FIFO\n");
        fifo->state = FIFO_STATE_RUNNING;
    }
    return true;
}

// Take num_samples from the FIFO. Note that it is up to the consumer to actually
// access the bits in the FIFO directly via its instance of audio_fifo_8_t
uint32_t afifo_take_samples8(audio_fifo_8_t *fifo, uint32_t num_samples) {
    if (fifo->state == FIFO_STATE_STOPPED) {
        return 0;
    }
    uint32_t samples_returned = MIN((fifo->samples_received-fifo->samples_sent), num_samples);
    fifo->read_idx = (fifo->read_idx + samples_returned) & (AFIFO8_BITS);
    fifo->samples_sent += samples_returned;

    // Only go back to stopped once fifo is fully exhausted
    if ((fifo->samples_received-fifo->samples_sent)  == 0) {        
        fifo->state = FIFO_STATE_STOPPED;
    }
    return samples_returned;
}

// Take one sample from the FIFO.
// Returns false if empty, true if sample was taken
bool afifo_take_sample8(audio_fifo_8_t *fifo, int8_t *sample) {
    if (fifo->state == FIFO_STATE_STOPPED) {
        return false;
    }
    *sample=fifo->buffer[fifo->read_idx];
    fifo->read_idx = (fifo->read_idx + 1) & (AFIFO8_BITS);
    fifo->samples_sent++;
    // Only go back to stopped once fifo is fully exhausted
    if ((fifo->samples_received-fifo->samples_sent)  == 0) {        
        //if (fifo->state == FIFO_STATE_RUNNING) PM_INFO("Stop FIFO\n");
        fifo->state = FIFO_STATE_STOPPED;
        
    }
    return true; 
}

// 8Bit Stereo FIFO functions

// Initialize the FIFO (called by producer_init or directly by user)
void afifo_init8s(audio_fifo_8s_t *fifo) {
    // Initialize FIFO structure
    if (fifo==NULL) PM_ERROR("FIFO Ptr is NULL");
    memset(fifo, 0, sizeof(audio_fifo_8_t)); // Use the passed pointer
    fifo->state = FIFO_STATE_STOPPED;
    fifo->write_idx = 0;
    fifo->read_idx = 0;
    fifo->samples_received = 0;
    fifo->samples_sent = 0;  
}

// Current samples in fifo
uint32_t afifo_level8s(audio_fifo_8s_t *fifo) {
  return AFIFO8_SIZE - (fifo->samples_received-fifo->samples_sent);
}

// Reset the FIFO to empty state
void afifo_reset8s(audio_fifo_8s_t *fifo) {
    fifo->state = FIFO_STATE_STOPPED;
    fifo->write_idx = fifo->read_idx;
    
    fifo->samples_received = 0;
    fifo->samples_sent = 0;    
}

// Add a sample to the FIFO 8Bit Stereo (called by producer)
bool afifo_add_sample8s(audio_fifo_8s_t *fifo, int8_t sample_r,int8_t sample_l) {
    if ((fifo->samples_received-fifo->samples_sent) == AFIFO8_SIZE) {
    return false;
    }
    fifo->buffer[fifo->write_idx*2] = sample_r;
    fifo->buffer[fifo->write_idx*2+1] = sample_l;
    fifo->write_idx = (fifo->write_idx + 1) & (AFIFO8_BITS);
    fifo->samples_received++;

    if ((fifo->samples_received-fifo->samples_sent) == AFIFO8_START_THRESHOLD) {
        if (fifo->state == FIFO_STATE_STOPPED) PM_INFO("Start FIFO\n");
        fifo->state = FIFO_STATE_RUNNING;
    }
    return true;
}

// Take one sample from the FIFO.
// Returns false if empty, true if sample was taken
bool afifo_take_sample8s(audio_fifo_8s_t *fifo,  int8_t *sample_r,int8_t *sample_l) {
    if (fifo->state == FIFO_STATE_STOPPED) {
        return false;
    }
    *sample_r=fifo->buffer[fifo->read_idx*2];
    *sample_l=fifo->buffer[fifo->read_idx*2+1];
    fifo->read_idx = (fifo->read_idx + 1) & (AFIFO8_BITS);
    fifo->samples_sent++;
    // Only go back to stopped once fifo is fully exhausted
    if ((fifo->samples_received-fifo->samples_sent)  == 0) {        
        //if (fifo->state == FIFO_STATE_RUNNING) 
        //    PM_INFO("Stop FIFO\n");
        fifo->state = FIFO_STATE_STOPPED;
        
    }
    return true;
}