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
#define AUDIO_FIFO_BITS (AUDIO_FIFO_SIZE - 1)
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
} afifo_state_t;

// 8Bit Mono FIFO structure
typedef struct {
    int8_t buffer[AUDIO_FIFO_SIZE];
    volatile uint32_t write_idx;
    volatile uint32_t read_idx;
    volatile afifo_state_t state;
    // mutex_t mutex;
    // semaphore_t data_available;
    volatile uint32_t samples_in_fifo;
    volatile uint32_t samples_received;
    volatile uint32_t samples_sent;    
} audio_fifo_8_t;

// 8Bit Stereo FIFO structure
typedef struct {
    int8_t buffer[AUDIO_FIFO_SIZE*2];
    volatile uint32_t write_idx;
    volatile uint32_t read_idx;
    volatile afifo_state_t state;
    // mutex_t mutex;
    // semaphore_t data_available;
    volatile uint32_t samples_in_fifo;
    volatile uint32_t samples_received;
    volatile uint32_t samples_sent;    
} audio_fifo_8s_t;

// FIFO management functions (implemented in audio_fifo.c)
// No more get_audio_fifo() - caller owns the fifo instance(s)
void afifo_init8(audio_fifo_8_t *fifo);
uint32_t afifo_level8(audio_fifo_8_t *fifo);
void afifo_reset8(audio_fifo_8_t *fifo);
bool afifo_add_sample8(audio_fifo_8_t *fifo, int8_t sample);
bool afifo_add_samples8(audio_fifo_8_t *fifo, const int8_t *samples_buffer, uint32_t num_samples_to_add);
uint32_t afifo_take_samples8(audio_fifo_8_t *fifo, uint32_t num_samples);
bool  afifo_take_sample8(audio_fifo_8_t *fifo, int8_t *sample);

void afifo_init8s(audio_fifo_8s_t *fifo);
void afifo_reset8s(audio_fifo_8s_t *fifo);
bool afifo_add_sample8s(audio_fifo_8s_t *fifo, int8_t sample_r,int8_t sample_l);
bool afifo_take_sample8s(audio_fifo_8s_t *fifo,  int8_t *sample_r,int8_t *sample_l);

#ifdef __cplusplus
}  // extern "C"
#endif
