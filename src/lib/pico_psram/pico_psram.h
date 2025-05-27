/**
    @file sfe_psram.c

    @brief This file contains a function that is used to detect and initialize PSRAM on
    SparkFun rp2350 boards.
*/

/*
    The MIT License (MIT)

    Copyright (c) 2024 SparkFun Electronics

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions: The
    above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED
    "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
    NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
    PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
    ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#pragma once

//#define __psram_start__ 0x11000000  // XIP Cache
//#define __psram_start__ 0x15000000  // XIP Cache Translate
#define __psram_start__ 0x1D000000    // XIP No Cache No translate

extern size_t ps_size;
extern size_t ps_heap_size;

extern void __no_inline_not_in_flash_func(setup_psram)(bool useheap,uint32_t heapstart);
extern void psram_reinit_timing();
extern void *ps_malloc(size_t size);
extern void ps_free(void *ptr);
extern void *ps_realloc(void *ptr, size_t size);
extern void *ps_calloc(size_t num, size_t size);
extern size_t ps_largest_free_block();
extern size_t ps_total_space();
extern size_t ps_total_used();
