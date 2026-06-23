/**
 * audio_fifo.h - Shared header for audio producer and consumer
 *
 * Implements a thread-safe FIFO for audio data between cores on RP2040
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
// #include "pico/mutex.h"
// #include "pico/sem.h"

// Configuration
#define AUDIO_FIFO_SIZE 1024  // Must be power of 2
#define AUDIO_FIFO_START_THRESHOLD (AUDIO_FIFO_SIZE >> 1)  // Start playback when half full
//#define AUDIO_FIFO_START_THRESHOLD 5

#ifdef __cplusplus
extern "C" {
#endif

// Audio sample type - adjust as needed for your audio format
typedef int8_t audio_sample_t;

// FIFO state
typedef enum {
    FIFO_STATE_STOPPED,
    FIFO_STATE_RUNNING,
} fifo_state_t;

// FIFO structure
typedef struct {
    int8_t buffer[AUDIO_FIFO_SIZE];
    volatile uint32_t write_idx;
    volatile uint32_t read_idx;
    volatile fifo_state_t state;
    // mutex_t mutex;
    // semaphore_t data_available;
    volatile uint32_t bytes_in_fifo;
} audio_fifo_8_t;

// FIFO management functions (implemented in audio_fifo.c)
// No more get_audio_fifo() - caller owns the fifo instance(s)
void fifo_init(audio_fifo_8_t *fifo);
uint32_t fifo_level(audio_fifo_8_t *fifo);
void fifo_reset(audio_fifo_8_t *fifo);
bool fifo_tx_b(audio_fifo_8_t *fifo, int8_t sample);
bool fifo_tx_n(audio_fifo_8_t *fifo, const int8_t *bytes_buffer, uint32_t num_bytes_to_add);

uint32_t fifo_rx_n(audio_fifo_8_t *fifo, uint32_t num_bytes);
// Helper for consumer to get one sample, this is a more fundamental FIFO op

bool  fifo_rx(audio_fifo_8_t *fifo, int8_t *sample);

#ifdef __cplusplus
}  // extern "C"
#endif
