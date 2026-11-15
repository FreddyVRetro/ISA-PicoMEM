#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "ff.h"
#include "p_trace.h"
#include "pico/stdio.h"

p_trace_buffer_t p_trace;
bool p_trace_serial;

#if USE_P_TRACE

#define TRACE_FILE_PREFIX    "trace"
#define TRACE_FILE_EXT       ".bin"
#define TRACE_FILE_MAX_INDEX 999
#define TRACE_SAVE_THRESHOLD_BYTES 1024
#define TRACE_FILE_BUFFER_SIZE     1024
#define TRACE_EVENT_TYPE_MASK      0xF0000000
#define TRACE_EVENT_ADDRESS_MASK   0x03FFFF00
#define TRACE_EVENT_ID_MASK        (TRACE_EVENT_TYPE_MASK | TRACE_EVENT_ADDRESS_MASK)
#define TRACE_SAME_EVENT_FLAG      0x40000000

static uint8_t trace_file_buffer[TRACE_FILE_BUFFER_SIZE];
static uint32_t trace_file_buffer_size;
static uint32_t trace_last_event;
static bool trace_last_event_valid;

static bool p_trace_flush_file_buffer(void)
{
    UINT written;

    if (trace_file_buffer_size == 0) {
        return true;
    }

    if (f_write(&p_trace.file, trace_file_buffer, trace_file_buffer_size,
                &written) != FR_OK || written != trace_file_buffer_size) {
        return false;
    }

    trace_file_buffer_size = 0;
    return true;
}

static bool p_trace_append_file_data(const void *data, uint32_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;

    if (p_trace_serial) {
        while (size != 0) {
            putchar_raw(*bytes++);
            size--;
        }
        return true;
    }

    while (size != 0) {
        uint32_t space = TRACE_FILE_BUFFER_SIZE - trace_file_buffer_size;
        uint32_t copy_size = size < space ? size : space;

        memcpy(&trace_file_buffer[trace_file_buffer_size], bytes, copy_size);
        trace_file_buffer_size += copy_size;
        bytes += copy_size;
        size -= copy_size;

        if (trace_file_buffer_size == TRACE_FILE_BUFFER_SIZE &&
            !p_trace_flush_file_buffer()) {
            return false;
        }
    }

    return true;
}

static bool p_trace_append_event(uint32_t timestamp, uint32_t event)
{
    const uint32_t event_type = event & TRACE_EVENT_TYPE_MASK;
    const bool same_event = trace_last_event_valid &&
                            (trace_last_event & TRACE_EVENT_ID_MASK) ==
                            (event & TRACE_EVENT_ID_MASK) &&
                            (event_type == TREV_IOR ||
                             event_type == TREV_IOW);

    if (same_event) {
        const uint32_t data_only_event = event_type | TRACE_SAME_EVENT_FLAG |
                                         (event & 0xFF);
        if (!p_trace_append_file_data(&data_only_event,
                                      sizeof(data_only_event))) {
            return false;
        }
    } else {
        if (!p_trace_append_file_data(&timestamp, sizeof(timestamp)) ||
            !p_trace_append_file_data(&event, sizeof(event))) {
            return false;
        }
    }

    trace_last_event = event;
    trace_last_event_valid = true;
    return true;
}

// Open a new trace file: trace000.bin, trace001.bin, ...
// Index is incremented until a file name that does not exist yet is found.
static FRESULT p_trace_create_file(void)
{
    char filename[32];
    FILINFO fno;

    for (uint32_t i = 0; i <= TRACE_FILE_MAX_INDEX; i++) {
        snprintf(filename, sizeof(filename), "%s%03lu%s",
                 TRACE_FILE_PREFIX, (unsigned long)i, TRACE_FILE_EXT);

        if (f_stat(filename, &fno) == FR_NO_FILE) {
            return f_open(&p_trace.file, filename, FA_WRITE | FA_CREATE_NEW);
        }
    }
    return FR_DENIED;  // no free index left
}

void p_trace_init(void)
{
    p_trace.write_idx = 0;
    p_trace.read_idx  = 0;
    p_trace.in_fifo   = 0;
    p_trace.received  = 0;
    p_trace.sent      = 0;
    trace_file_buffer_size = 0;
    trace_last_event_valid = false;

    // NOTE: the filesystem must already be mounted (f_mount) before this call
    if (!p_trace_serial && p_trace_create_file() != FR_OK) {
        // TODO: error handling (FS not mounted / no free index / ...)
    }
}

// Write all pending buffer entries to the trace file.
// Returns the number of 32-bit words written.
uint32_t p_trace_save(void)
{
    uint32_t written = 0;

    while (p_trace.received - p_trace.sent >= 2) {
        uint32_t timestamp = p_trace.buffer[p_trace.read_idx];
        uint32_t event_index = (p_trace.read_idx + 1) & TR_INDEX_MASK;
        uint32_t event = p_trace.buffer[event_index];

        if (!p_trace_append_event(timestamp, event)) {
            break;
        }

        p_trace.read_idx = (p_trace.read_idx + 2) & TR_INDEX_MASK;
        p_trace.sent += 2;
        written += 2;
    }

    return written;
}

// Call this periodically (e.g., from the main loop on core1).
// Writes to the trace file only when at least 1024 bytes of data are pending.
// Returns the number of 32-bit words written (0 if below threshold).
uint32_t p_trace_worker(void)
{
    uint32_t pending = p_trace.received - p_trace.sent;

    if (pending * sizeof(uint32_t) < TRACE_SAVE_THRESHOLD_BYTES) {
        return 0;
    }
    return p_trace_save();
}

void p_trace_flush(void)
{
    p_trace_save();
    if (!p_trace_serial) {
        p_trace_flush_file_buffer();
        f_sync(&p_trace.file);
    }
}

void p_trace_close(void)
{
    p_trace_flush();
    if (!p_trace_serial) {
        f_close(&p_trace.file);
    }
}

#else

void p_trace_init(void)
{
}

uint32_t p_trace_save(void)
{
    return 0;
}

uint32_t p_trace_worker(void)
{
    return 0;
}

void p_trace_flush(void)
{
}

void p_trace_close(void)
{
}

#endif