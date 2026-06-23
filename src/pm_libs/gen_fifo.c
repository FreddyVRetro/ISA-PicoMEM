/**
 * gen_fifo.c - Generic fifo with fast word/dword... tx
 *
 */

#include "gen_fifo.h"
/* #include "pico/stdlib.h" */
#include <string.h>
#include <stdio.h>

#include "../pm_debug.h"  // For PM_INFO, PM_ERROR, etc.

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

// Current bytes in fifo
uint32_t g_fifo_level(audio_fifo_8_t *fifo) {
    return fifo->bytes_in_fifo;
}

// Initialize the FIFO (called by producer_init or directly by user)
void g_fifo_init(audio_fifo_8_t *fifo) {
    // Initialize FIFO structure
    memset(fifo, 0, sizeof(audio_fifo_8_t)); // Use the passed pointer
    fifo->state = FIFO_STATE_STOPPED;
    fifo->write_idx = 0;
    fifo->read_idx = 0;
    fifo->bytes_in_fifo = 0;
}

// Reset the FIFO to empty state
void g_fifo_reset(audio_fifo_8_t *fifo) {
    fifo->state = FIFO_STATE_STOPPED;
    fifo->write_idx = fifo->read_idx;
    fifo->bytes_in_fifo = 0;
}

// Add a sample to the FIFO (called by producer)
bool g_fifo_tx(audio_fifo_8_t *fifo, int8_t data) {
    if (fifo->bytes_in_fifo == GENERIC_FIFO_SIZE) {
        return false;
    }
    fifo->buffer[fifo->write_idx] = data;
    fifo->write_idx = (fifo->write_idx + 1) & (AUDIO_FIFO_BITS);
    fifo->bytes_in_fifo++;
    return true;
}

// Add multiple bytes to the FIFO
bool g_fifo_tx_n(audio_fifo_8_t *fifo, const int8_t *bytes_buffer, uint32_t num_bytes_to_add) {
    // Check basic conditions
    if (num_bytes_to_add == 0) {
        return true; // Nothing to add, operation considered successful
    }

    // Check if there's enough space in the FIFO
    uint32_t available_space = GENERIC_FIFO_SIZE - fifo->bytes_in_fifo;
    if (num_bytes_to_add > available_space) {
        return false; // Not enough space for all requested bytes
    }

    // Determine how many bytes can be written before wrap-around
    // GENERIC_FIFO_SIZE is the total capacity. write_idx is 0 to GENERIC_FIFO_SIZE-1.
    uint32_t bytes_to_end = GENERIC_FIFO_SIZE - fifo->write_idx;

    if (num_bytes_to_add <= bytes_to_end) {
        // All bytes fit without wrapping around the physical end of the buffer
        memcpy(&fifo->buffer[fifo->write_idx], bytes_buffer, num_bytes_to_add);
    } else {
        // bytes will wrap around the physical end of the buffer
        // Part 1: Copy bytes up to the end of the physical buffer
        memcpy(&fifo->buffer[fifo->write_idx], bytes_buffer, bytes_to_end);

        // Part 2: Copy remaining bytes from the beginning of the physical buffer
        uint32_t remaining_bytes = num_bytes_to_add - bytes_to_end;
        memcpy(&fifo->buffer[0], bytes_buffer + bytes_to_end, remaining_bytes);
    }

    return true;
}

// Take num_bytes from the FIFO. Note that it is up to the consumer to actually
// access the bits in the FIFO directly via its instance of audio_fifo_8_t
uint32_t g_fifo_rx_n(audio_fifo_8_t *fifo, uint32_t num_bytes) {
    if (fifo->state == FIFO_STATE_STOPPED) {
        return 0;
    }
    uint32_t bytes_returned = MIN(fifo->bytes_in_fifo, num_bytes);
    fifo->read_idx = (fifo->read_idx + bytes_returned) & (AUDIO_FIFO_BITS);
    fifo->bytes_in_fifo -= bytes_returned;
    // Only go back to stopped once fifo is fully exhausted
    if (fifo->bytes_in_fifo == 0) {
        fifo->state = FIFO_STATE_STOPPED;
    }
    return bytes_returned;
}

// Take one sample from the FIFO.
// Returns false if empty, true if sample was taken
bool g_fifo_rx(audio_fifo_8_t *fifo, int8_t *data) {
    if (fifo->bytes_in_fifo==0) return false;
    *data=fifo->buffer[fifo->read_idx];
    fifo->read_idx = (fifo->read_idx + 1) & (AUDIO_FIFO_BITS);
    fifo->bytes_in_fifo --;
    return true;
}