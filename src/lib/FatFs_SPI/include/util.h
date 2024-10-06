/* util.h
Copyright 2021 Carl John Kugler III

Licensed under the Apache License, Version 2.0 (the License); you may not use 
this file except in compliance with the License. You may obtain a copy of the 
License at

   http://www.apache.org/licenses/LICENSE-2.0 
Unless required by applicable law or agreed to in writing, software distributed 
under the License is distributed on an AS IS BASIS, WITHOUT WARRANTIES OR 
CONDITIONS OF ANY KIND, either express or implied. See the License for the 
specific language governing permissions and limitations under the License.
*/

#pragma once

#include <stddef.h>    
#include <stdint.h>
#include <string.h>
//
#if PICO_RP2040
#include "RP2040.h"
#else
#include "RP2350.h"
#endif
//
#include "my_debug.h"

#ifdef __cplusplus
extern "C" {
#endif


// Greatest Common Divisor: Euclidian Algorithm
// https://www.freecodecamp.org/news/euclidian-gcd-algorithm-greatest-common-divisor/
int gcd(int a,int b);

typedef int (*printer_t)(const char* format, ...);

// works with negative index
static inline int wrap_ix(int index, int n)
{
    return ((index % n) + n) % n;
}
// Calculate arr indices with wrap around (+ and -)
static inline int mod_floor(int a, int n) {
    return ((a % n) + n) % n;
}

__attribute__((always_inline)) static inline uint32_t calculate_checksum(uint32_t const *p, size_t const size){
	uint32_t checksum = 0;
	for (uint32_t i = 0; i < (size/sizeof(uint32_t))-1; i++){
		checksum ^= *p;
		p++;
	}
	return checksum;
}

static inline void ext_str(size_t const data_sz,
                           uint8_t const data[],
                           size_t const msb,
                           size_t const lsb,
                           size_t const buf_sz,
                           char buf[]) {
    memset(buf, 0, buf_sz);
    size_t size = (1 + msb - lsb) / 8;  // bytes
    size_t byte = (data_sz - 1) - (msb / 8);
    for (uint32_t i = 0; i < size; i++) {
        myASSERT(i < buf_sz);
        myASSERT(byte < data_sz);
        buf[i] = data[byte++];
    }
}

static inline uint32_t ext_bits(size_t n_src_bytes, unsigned char const *data, int msb, int lsb) {
    uint32_t bits = 0;
    uint32_t size = 1 + msb - lsb;
    for (uint32_t i = 0; i < size; i++) {
        uint32_t position = lsb + i;
        uint32_t byte = (n_src_bytes - 1) - (position >> 3);
        uint32_t bit = position & 0x7;
        uint32_t value = (data[byte] >> bit) & 1;
        bits |= value << i;
    }
    return bits;
}
static inline uint32_t ext_bits16(unsigned char const *data, int msb, int lsb) {
    return ext_bits(16, data, msb, lsb);
}

char const* uint8_binary_str(uint8_t number);
char const* uint_binary_str(unsigned int number);

#ifdef __cplusplus
}
#endif
/* [] END OF FILE */
