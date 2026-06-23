/* my_debug.c
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

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
//
#if PICO_RP2040
#include "RP2040.h"
#else
#include "RP2350.h"
#endif
#include "pico/stdlib.h"
//
#include "crash.h"
//
#include "my_debug.h"

/* Function Attribute ((weak))
The weak attribute causes a declaration of an external symbol to be emitted as a weak symbol
rather than a global. This is primarily useful in defining library functions that can be
overridden in user code, though it can also be used with non-function declarations. The
overriding symbol must have the same type as the weak symbol.
https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html

You can override these functions in your application to redirect "stdout"-type messages.
*/

/* Single string output callbacks */

void __attribute__((weak)) put_out_error_message(const char *s) { (void)s; }
void __attribute__((weak)) put_out_info_message(const char *s) { (void)s; }
void __attribute__((weak)) put_out_debug_message(const char *s) { (void)s; }

/* "printf"-style output callbacks */

#if defined(USE_PRINTF) && USE_PRINTF

int __attribute__((weak)) 
error_message_printf(const char *func, int line, 
        const char *fmt, ...) 
{
    printf("%s:%d: ", func, line);
    va_list args;
    va_start(args, fmt);
    int cw = vprintf(fmt, args);
    va_end(args);
    stdio_flush();
    return cw;
}
int __attribute__((weak)) error_message_printf_plain(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int cw = vprintf(fmt, args);
    va_end(args);
    stdio_flush();
    return cw;
}
int __attribute__((weak)) info_message_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int cw = vprintf(fmt, args);
    va_end(args);
    return cw;
}
int __attribute__((weak)) 
debug_message_printf(const char *func, int line, 
        const char *fmt, ...) 
{
#if defined(NDEBUG) || !USE_DBG_PRINTF
    (void) func;
    (void) line;
#endif
    va_list args;
    va_start(args, fmt);
    int cw = vprintf(fmt, args);
    va_end(args);
    stdio_flush();
    return cw;
}

#else

/* These will truncate at 256 bytes. You can tell by checking the return code. */

int __attribute__((weak)) 
error_message_printf(const char *func, int line, 
        const char *fmt, ...) 
{
    char buf[256] = {0};
    va_list args;
    va_start(args, fmt);
    int cw = vsnprintf(buf, sizeof(buf), fmt, args);
    put_out_error_message(buf);
    va_end(args);
    return cw;
}
int __attribute__((weak)) error_message_printf_plain(const char *fmt, ...) {
    char buf[256] = {0};
    va_list args;
    va_start(args, fmt);
    int cw = vsnprintf(buf, sizeof(buf), fmt, args);
    put_out_info_message(buf);
    va_end(args);
    return cw;
}
int __attribute__((weak)) info_message_printf(const char *fmt, ...) {
    char buf[256] = {0};
    va_list args;
    va_start(args, fmt);
    int cw = vsnprintf(buf, sizeof(buf), fmt, args);
    put_out_info_message(buf);
    va_end(args);
    return cw;
}
int __attribute__((weak)) 
debug_message_printf(const char *func, int line, 
        const char *fmt, ...) 
{
    char buf[256] = {0};
    va_list args;
    va_start(args, fmt);
    int cw = vsnprintf(buf, sizeof(buf), fmt, args);
    put_out_debug_message(buf);
    va_end(args);
    return cw;
}

#endif

void __attribute__((weak)) my_assert_func(const char *file, int line, const char *func,
                                          const char *pred) {
    error_message_printf_plain("assertion \"%s\" failed: file \"%s\", line %d, function: %s\n",
                               pred, file, line, func);
    __disable_irq(); /* Disable global interrupts. */
    capture_assert(file, line, func, pred);
}

void assert_case_not_func(const char *file, int line, const char *func, int v) {
    char pred[128];
    snprintf(pred, sizeof pred, "case not %d", v);
    my_assert_func(file, line, func, pred);
}

void assert_case_is(const char *file, int line, const char *func, int v, int expected) {
    char pred[128];
    snprintf(pred, sizeof pred, "%d is %d", v, expected);
    my_assert_func(file, line, func, pred);
}

void dump8buf(char *buf, size_t buf_sz, uint8_t *pbytes, size_t nbytes) {
    int n = 0;
    for (size_t byte_ix = 0; byte_ix < nbytes; ++byte_ix) {
        for (size_t col = 0; col < 32 && byte_ix < nbytes; ++col, ++byte_ix) {
            n += snprintf(buf + n, buf_sz - n, "%02hhx ", pbytes[byte_ix]);
            myASSERT(0 < n && n < (int)buf_sz);
        }
        n += snprintf(buf + n, buf_sz - n, "\n");
        myASSERT(0 < n && n < (int)buf_sz);
    }
}
void hexdump_8(const char *s, const uint8_t *pbytes, size_t nbytes) {
    IMSG_PRINTF("\n%s(%s, 0x%p, %zu)\n", __FUNCTION__, s, pbytes, nbytes);
    stdio_flush();
    size_t col = 0;
    for (size_t byte_ix = 0; byte_ix < nbytes; ++byte_ix) {
        IMSG_PRINTF("%02hhx ", pbytes[byte_ix]);
        if (++col > 31) {
            IMSG_PRINTF("\n");
            col = 0;
        }
        stdio_flush();
    }
}
// nwords is size in WORDS!
void hexdump_32(const char *s, const uint32_t *pwords, size_t nwords) {
    IMSG_PRINTF("\n%s(%s, 0x%p, %zu)\n", __FUNCTION__, s, pwords, nwords);
    stdio_flush();
    size_t col = 0;
    for (size_t word_ix = 0; word_ix < nwords; ++word_ix) {
        IMSG_PRINTF("%08lx ", pwords[word_ix]);
        if (++col > 7) {
            IMSG_PRINTF("\n");
            col = 0;
        }
        stdio_flush();
    }
}
// nwords is size in bytes
bool compare_buffers_8(const char *s0, const uint8_t *pbytes0, const char *s1,
                       const uint8_t *pbytes1, const size_t nbytes) {
    /* Verify the data. */
    if (0 != memcmp(pbytes0, pbytes1, nbytes)) {
        hexdump_8(s0, pbytes0, nbytes);
        hexdump_8(s1, pbytes1, nbytes);
        return false;
    }
    return true;
}
// nwords is size in WORDS!
bool compare_buffers_32(const char *s0, const uint32_t *pwords0, const char *s1,
                        const uint32_t *pwords1, const size_t nwords) {
    /* Verify the data. */
    if (0 != memcmp(pwords0, pwords1, nwords * sizeof(uint32_t))) {
        hexdump_32(s0, pwords0, nwords);
        hexdump_32(s1, pwords1, nwords);
        return false;
    }
    return true;
}
/* [] END OF FILE */
