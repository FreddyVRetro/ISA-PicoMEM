/* crash.h
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
// Original from M0AGX (blog@m0agx.eu), "Preserving debugging breadcrumbs across
// reboots in Cortex-M,"
// https://m0agx.eu/2018/08/18/preserving-debugging-breadcrumbs-across-reboots-in-cortex-m/

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

//
#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The crash info section is at the beginning of the RAM,
 * that is not initialized by the linker to preserve
 * information across reboots.
 */

/**
 * These are the positions of the fault frame elements in the
 * fault frame structure.
 */
enum {
    R0_Pos = 0, /**< The position of the R0  content in a fault structure */
    R1_Pos,     /**< The position of the R1  content in a fault structure */
    R2_Pos,     /**< The position of the R2  content in a fault structure */
    R3_Pos,     /**< The position of the R3  content in a fault structure */
    R12_Pos,    /**< The position of the R12 content in a fault structure */
    LR_Pos,     /**< The position of the LR  content in a fault structure */
    PC_Pos,     /**< The position of the PC  content in a fault structure */
    PSR_Pos,    /**< The position of the PSR content in a fault structure */
    NUM_REGS,   /**< The number of registers in the fault frame */
};

typedef struct {
    uint32_t r0;  /**< R0 register content */
    uint32_t r1;  /**< R1 register content */
    uint32_t r2;  /**< R2 register content */
    uint32_t r3;  /**< R3 register content */
    uint32_t r12; /**< R12 register content */
    uint32_t lr;  /**< LR register content */
    uint32_t pc;  /**< PC register content */
    uint32_t psr; /**< PSR register content */
} __attribute__((packed)) cy_stc_fault_frame_t;

typedef enum {
    crash_magic_none = 0,
    crash_magic_bootloader_entry = 0xB000B000,
    crash_magic_hard_fault = 0xCAFEBABE,
    crash_magic_debug_mon = 0x01020304,
    crash_magic_reboot_requested = 0x00ABCDEF,
    crash_magic_stack_overflow = 0x0BADBEEF,
    crash_magic_assert = 0xDEBDEBDE
} crash_magic_t;

typedef struct {
    char file[32];
    int line;
    char func[32];
    char pred[32];
} crash_assert_t;

typedef struct {
    uint32_t magic;
    time_t timestamp;
    union {
        cy_stc_fault_frame_t cy_faultFrame;
        crash_assert_t assert;
        char calling_func[64];
    };
    uint8_t xor_checksum;  // last to avoid including in calculation
} crash_info_t;

// Trick to find struct size at compile time:
// char (*__kaboom)[sizeof(crash_info_flash_t)] = 1;
// warning: initialization of 'char (*)[132]' from 'int' makes ...

void crash_handler_init();
const crash_info_t *crash_handler_get_info();
volatile const crash_info_t *crash_handler_get_info_flash();

#define SYSTEM_RESET() system_reset_func(__FUNCTION__)
void system_reset_func(char const *const func) __attribute__((noreturn));

void capture_assert(const char *file, int line, const char *func, const char *pred)
    __attribute__((noreturn));
void capture_assert_case_not(const char *file, int line, const char *func, int v)
    __attribute__((noreturn));

int dump_crash_info(crash_info_t const *const pCrashInfo, int next, char *const buf,
                    size_t const buf_sz);

#ifdef __cplusplus
}
#endif

/* [] END OF FILE */
