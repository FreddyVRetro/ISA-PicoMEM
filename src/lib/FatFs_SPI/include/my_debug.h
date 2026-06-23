/* my_debug.h
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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* USE_PRINTF
If this is defined and not zero,
these message output functions will use the Pico SDK's stdout.
*/

/* USE_DBG_PRINTF
If this is not defined or is zero or NDEBUG is defined,
DBG_PRINTF statements will be effectively stripped from the code.
*/

/* Single string output callbacks: send message output somewhere.
To use these, do not define the USE_PRINTF compile definition,
and override these "weak" functions by strongly implementing them in user code.
The weak implementations do nothing.
 */
void put_out_error_message(const char *s);
void put_out_info_message(const char *s);
void put_out_debug_message(const char *s);

// https://gcc.gnu.org/onlinedocs/gcc-3.2.3/cpp/Variadic-Macros.html

int error_message_printf(const char *func, int line, const char *fmt, ...)
    __attribute__((format(__printf__, 3, 4)));
#ifndef EMSG_PRINTF
#define EMSG_PRINTF(fmt, ...) error_message_printf(__func__, __LINE__, fmt, ##__VA_ARGS__)
#endif

int error_message_printf_plain(const char *fmt, ...) __attribute__((format(__printf__, 1, 2)));

int debug_message_printf(const char *func, int line, const char *fmt, ...)
    __attribute__((format(__printf__, 3, 4)));
#ifndef DBG_PRINTF
#  if defined(USE_DBG_PRINTF) && USE_DBG_PRINTF // && !defined(NDEBUG)
#    define DBG_PRINTF(fmt, ...) debug_message_printf(__func__, __LINE__, fmt, ##__VA_ARGS__)
#  else
#    define DBG_PRINTF(fmt, ...) (void)0
#  endif
#endif

int info_message_printf(const char *fmt, ...) __attribute__((format(__printf__, 1, 2)));
#ifndef IMSG_PRINTF
#define IMSG_PRINTF(fmt, ...) info_message_printf(fmt, ##__VA_ARGS__)
#endif

void lock_printf();
void unlock_printf();

void my_assert_func(const char *file, int line, const char *func, const char *pred)
    __attribute__((noreturn));
#ifdef NDEBUG           /* required by ANSI standard */
#  define myASSERT(__e) ((void)0)
#else
#  define myASSERT(__e) \
    { ((__e) ? (void)0 : my_assert_func(__func__, __LINE__, __func__, #__e)); }
#endif    

void assert_always_func(const char *file, int line, const char *func, const char *pred)
    __attribute__((noreturn));
#define ASSERT_ALWAYS(__e) \
    ((__e) ? (void)0 : my_assert_func(__FILE__, __LINE__, __func__, #__e))

void assert_case_is(const char *file, int line, const char *func, int v, int expected)
    __attribute__((noreturn));
#define ASSERT_CASE_IS(__v, __e) \
    ((__v == __e) ? (void)0 : assert_case_is(__FILE__, __LINE__, __func__, __v, __e))

void assert_case_not_func(const char *file, int line, const char *func, int v)
    __attribute__((noreturn));
#define ASSERT_CASE_NOT(__v) (assert_case_not_func(__FILE__, __LINE__, __func__, __v))

#ifdef NDEBUG /* required by ANSI standard */
#define DBG_ASSERT_CASE_NOT(__e) ((void)0)
#else
#define DBG_ASSERT_CASE_NOT(__v) (assert_case_not_func(__FILE__, __LINE__, __func__, __v))
#endif
static inline void dump_bytes(size_t num, uint8_t bytes[]) {
    (void)num;
    (void)bytes;
    DBG_PRINTF("     ");
    for (size_t j = 0; j < 16; ++j) {
        DBG_PRINTF("%02hhx", j);
        if (j < 15)
            DBG_PRINTF(" ");
        else {
            DBG_PRINTF("\n");
        }
    }
    for (size_t i = 0; i < num; i += 16) {
        DBG_PRINTF("%04x ", i);
        for (size_t j = 0; j < 16 && i + j < num; ++j) {
            DBG_PRINTF("%02hhx", bytes[i + j]);
            if (j < 15)
                DBG_PRINTF(" ");
            else {
                DBG_PRINTF("\n");
            }
        }
    }
    DBG_PRINTF("\n");
}

void dump8buf(char *buf, size_t buf_sz, uint8_t *pbytes, size_t nbytes);
void hexdump_8(const char *s, const uint8_t *pbytes, size_t nbytes);
bool compare_buffers_8(const char *s0, const uint8_t *pbytes0, const char *s1,
                       const uint8_t *pbytes1, const size_t nbytes);

// sz is size in BYTES!
void hexdump_32(const char *s, const uint32_t *pwords, size_t nwords);
// sz is size in BYTES!
bool compare_buffers_32(const char *s0, const uint32_t *pwords0, const char *s1,
                        const uint32_t *pwords1, const size_t nwords);

#ifdef __cplusplus
}
#endif

/* [] END OF FILE */
