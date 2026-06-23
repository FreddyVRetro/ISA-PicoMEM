/*
 * delays.h
 *
 *  Created on: Apr 25, 2022
 *      Author: carlk
 */
/* Using millis() or micros() for timeouts

For example, 

    uint32_t start = millis();
    do {
        // ...
       }
    } while (millis() - start < TIMEOUT);             

There is no problem if the millis() counter wraps, 
due to the properties of unsigned integer modulo arithmetic.

"A computation involving unsigned operands can never overflow, 
because a result that cannot be represented by the resulting 
unsigned integer type is reduced modulo the number that is 
one greater than the largest value that can be represented 
by the resulting type." 
    -- ISO/IEC 9899:1999 (E) §6.2.5/9

In other words, a uint32_t will wrap at 0 and UINT_MAX.

So, for example, 
    0x00000000 - 0xFFFFFFFF = 0x00000001

Remember that an unsigned integer will never be negative!

Be careful with comparisons. In the example above, 
if 0x00000000 is the result of the counter wrapping,
and 0xFFFFFFFF is the start timestamp, a comparison like

    millis() - start < TIMEOUT

is OK, but the following code is problematic if, say, the first call
to millis() returns 0xFFFFFFF0 and the second 
call to millis() returns 0xFFFFFFFF:

    uint32_t end = millis() + 100; // end = 0x00000054
    while (millis() < end)  // while (0xFFFFFFFF < 0x00000054)

*/

#pragma once

#include <stdint.h>
//
#include "pico/stdlib.h"
#if PICO_RP2040
#include "RP2040.h"
#else
#include "RP2350.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t millis() {
    __COMPILER_BARRIER();
    return time_us_64() / 1000;
    __COMPILER_BARRIER();
}

static inline void delay_ms(uint32_t ulTime_ms) {
    sleep_ms(ulTime_ms);
}

static inline uint64_t micros() {
    __COMPILER_BARRIER();
    return to_us_since_boot(get_absolute_time());
    __COMPILER_BARRIER();
}

#ifdef __cplusplus
}
#endif
/* [] END OF FILE */
