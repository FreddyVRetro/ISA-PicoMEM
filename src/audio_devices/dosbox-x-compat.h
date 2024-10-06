#pragma once

#include "pico/platform.h"

typedef uint32_t	Bitu;

//#define INLINE inline __attribute__((always_inline))
#define INLINE __force_inline

typedef uint8_t const *       ConstHostPt;        /* host (virtual) memory address aka ptr */
static INLINE uint16_t host_readw(ConstHostPt off) {
    return __builtin_bswap16(*(uint16_t *)off);
}

#ifdef DEBUG
#define LOG_MSG(msg, ...) printf(msg "\n", ##__VA_ARGS__);
#define DEBUG_LOG_MSG(msg, ...) printf(msg "\n", ##__VA_ARGS__);
#else
#define LOG_MSG(...) (void)0
#define DEBUG_LOG_MSG(...) (void)0
#endif
