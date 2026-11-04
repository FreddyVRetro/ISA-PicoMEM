#include <stdint.h>
#define __dmb() __asm__ volatile("" ::: "memory")
uint32_t time_us_32(void);
