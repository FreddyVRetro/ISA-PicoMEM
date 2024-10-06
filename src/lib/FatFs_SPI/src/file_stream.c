/*
 * file_stream.c
 *
 *  Created on: Jun 20, 2024
 *      Author: carlk
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//
#include "pico/stdlib.h"
//
#include "ff.h"
//
#include "f_util.h"
#include "my_debug.h"
//
#include "file_stream.h"

typedef struct {
    FIL file;
} cookie_t;

// functions.read should return -1 on failure, or else the number of bytes read (0 on EOF).
//  It is similar to read, except that cookie will be passed as the first argument.
static ssize_t cookie_read_function(void *vcookie_p, char *buf, size_t n) {
    cookie_t *cookie_p = vcookie_p;
    FIL *file_p = &cookie_p->file;
    UINT br;
    FRESULT fr = f_read(file_p, buf, n, &br);
    if (FR_OK != fr) {
        DBG_PRINTF("f_read error: %s\n", FRESULT_str(fr));
        return -1;
    }
    return br;
}

// functions.write should return -1 on failure, or else the number of bytes written.
//  It is similar to write, except that cookie will be passed as the first argument.
static ssize_t cookie_write_function(void *vcookie_p, const char *buf, size_t n) {
    cookie_t *cookie_p = vcookie_p;
    FIL *file_p = &cookie_p->file;
    UINT bw;
    FRESULT fr = f_write(file_p, buf, n, &bw);
    if (FR_OK != fr) {
        DBG_PRINTF("f_read error: %s\n", FRESULT_str(fr));
        return -1;
    }
    return bw;
}

// functions.seek should return -1 on failure, and 0 on success,
//  with off set to the current file position.
//  It is a cross between lseek and fseek, with the whence argument interpreted in the same
//  manner.
static int cookie_seek_function(void *vcookie_p, off_t *off, int whence) {
    cookie_t *cookie_p = vcookie_p;
    FIL *file_p = &cookie_p->file;

    FRESULT fr = FR_OK;
    switch (whence) {
        case SEEK_SET:
            fr = f_lseek(file_p, *off);
            break;
        case SEEK_CUR:
            fr = f_lseek(file_p, f_tell(file_p) + *off);
            break;
        case SEEK_END:
            fr = f_lseek(file_p, f_size(file_p) + *off);
            break;
        default:
            ASSERT_CASE_NOT(whence);
    }
    if (FR_OK != fr) {
        DBG_PRINTF("f_lseek error: %s\n", FRESULT_str(fr));
        return -1;
    } else {
        *off = f_tell(file_p);
        return 0;
    }
}

// functions.close should return -1 on failure, or 0 on success.
//  It is similar to close, except that cookie will be passed as the first argument.
//  A failed close will still invalidate the stream.
static int cookie_close_function(void *vcookie_p) {
    cookie_t *cookie_p = vcookie_p;
    FIL *file_p = &cookie_p->file;
    FRESULT fr = f_close(file_p);
    free(vcookie_p);
    if (FR_OK != fr) {
        DBG_PRINTF("f_close error: %s\n", FRESULT_str(fr));
        return -1;
    } else {
        return 0;
    }
}

FILE *open_file_stream(const char *pathname, const char *pcMode) {
    cookie_t *cookie_p = malloc(sizeof(cookie_t));
    if (!cookie_p) {
        return NULL;
    }

    BYTE mode = 0;

    // POSIX	FatFs
    if (0 == strcmp("r", pcMode))
        mode = FA_READ;
    else if (0 == strcmp("r+", pcMode))
        mode = FA_READ | FA_WRITE;
    else if (0 == strcmp("w", pcMode))
        mode = FA_CREATE_ALWAYS | FA_WRITE;
    else if (0 == strcmp("w+", pcMode))
        mode = FA_CREATE_ALWAYS | FA_WRITE | FA_READ;
    else if (0 == strcmp("a", pcMode))
        mode = FA_OPEN_APPEND | FA_WRITE;
    else if (0 == strcmp("a+", pcMode))
        mode = FA_OPEN_APPEND | FA_WRITE | FA_READ;
    else if (0 == strcmp("wx", pcMode))
        mode = FA_CREATE_NEW | FA_WRITE;
    else if (0 == strcmp("w+x", pcMode))
        mode = FA_CREATE_NEW | FA_WRITE | FA_READ;

    FRESULT fr = f_open(&cookie_p->file, pathname, mode);
    if (FR_OK != fr) {
        DBG_PRINTF("f_lseek error: %s\n", FRESULT_str(fr));
        return NULL;
    }
    cookie_io_functions_t iofs = {cookie_read_function, cookie_write_function,
                                  cookie_seek_function, cookie_close_function};

    /* create the stream */
    FILE *file = fopencookie(cookie_p, pcMode, iofs);
    if (!file) cookie_close_function(cookie_p);
    return (file);
}
