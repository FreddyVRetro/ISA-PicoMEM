/* crash.c
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

#include <string.h>
#include <time.h>
//
#include "pico/stdlib.h"
//
#include "crc.h"
#include "my_debug.h"
#include "my_rtc.h"
#include "util.h"
//
#include "crash.h"

#if defined(NDEBUG) || !USE_DBG_PRINTF
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif

static volatile crash_info_t crash_info_ram __attribute__((section(".uninitialized_data")));

static crash_info_t volatile *crash_info_ram_p = &crash_info_ram;

static crash_info_t previous_crash_info;
static bool _previous_crash_info_valid = false;

__attribute__((noreturn, always_inline))
static inline void reset() {
//    if (debugger_connected()) {
        __breakpoint();
//    } else {
        NVIC_SystemReset();
//    }
    __builtin_unreachable();
}

void crash_handler_init() {
    if (crash_info_ram.magic == crash_magic_hard_fault ||
        crash_info_ram.magic == crash_magic_stack_overflow ||
        crash_info_ram.magic == crash_magic_reboot_requested ||
        crash_info_ram.magic == crash_magic_assert ||
        crash_info_ram.magic == crash_magic_debug_mon) {
        uint8_t xor_checksum = crc7((uint8_t *)crash_info_ram_p,
                offsetof(crash_info_t, xor_checksum));
        if (xor_checksum == crash_info_ram.xor_checksum) {
            // valid crash record
            memcpy(&previous_crash_info, (void *)crash_info_ram_p, sizeof previous_crash_info);
            _previous_crash_info_valid = true;
        }
    }
    memset((void *)crash_info_ram_p, 0, sizeof crash_info_ram);
}

const crash_info_t *crash_handler_get_info() {
    if (_previous_crash_info_valid) {
        return &previous_crash_info;
    }
    return NULL;
}


__attribute__((noreturn))
void system_reset_func(char const *const func) {
    memset((void *)crash_info_ram_p, 0, sizeof crash_info_ram);
    crash_info_ram.magic = crash_magic_reboot_requested;
    crash_info_ram.timestamp = epochtime;
    snprintf((char *)crash_info_ram.calling_func, sizeof crash_info_ram.calling_func, "%s",
             func);
    crash_info_ram.xor_checksum =
        crc7((uint8_t *)&crash_info_ram, offsetof(crash_info_t, xor_checksum));
    __DSB();

    reset();
    __builtin_unreachable();
}

__attribute__((noreturn))
void capture_assert(const char *file, int line, const char *func, const char *pred) {
    memset((void *)crash_info_ram_p, 0, sizeof crash_info_ram);
    crash_info_ram.magic = crash_magic_assert;
    crash_info_ram.timestamp = epochtime;

    // If the filename is too long, take the end:
    size_t full_len = strlen(file) + 1;
    size_t offset = 0;
    if (full_len > sizeof crash_info_ram.assert.file) {
        offset = full_len - sizeof crash_info_ram.assert.file;
    }
    snprintf((char *)crash_info_ram.assert.file, sizeof crash_info_ram.assert.file, "%s",
             file + offset);
    snprintf((char *)crash_info_ram.assert.func, sizeof crash_info_ram.assert.func, "%s", func);
    snprintf((char *)crash_info_ram.assert.pred, sizeof crash_info_ram.assert.pred, "%s", pred);
    crash_info_ram.assert.line = line;
    crash_info_ram.xor_checksum =
        crc7((uint8_t *)&crash_info_ram, offsetof(crash_info_t, xor_checksum));
    __DSB();   
    reset();
    __builtin_unreachable();
}

__attribute__((used)) extern void DebugMon_HandlerC(uint32_t const *faultStackAddr) {
    memset((void *)crash_info_ram_p, 0, sizeof crash_info_ram);
    crash_info_ram.magic = crash_magic_debug_mon;
    crash_info_ram.timestamp = epochtime;

    /* Stores general registers */
    crash_info_ram.cy_faultFrame.r0 = faultStackAddr[R0_Pos];
    crash_info_ram.cy_faultFrame.r1 = faultStackAddr[R1_Pos];
    crash_info_ram.cy_faultFrame.r2 = faultStackAddr[R2_Pos];
    crash_info_ram.cy_faultFrame.r3 = faultStackAddr[R3_Pos];
    crash_info_ram.cy_faultFrame.r12 = faultStackAddr[R12_Pos];
    crash_info_ram.cy_faultFrame.lr = faultStackAddr[LR_Pos];
    crash_info_ram.cy_faultFrame.pc = faultStackAddr[PC_Pos];
    crash_info_ram.cy_faultFrame.psr = faultStackAddr[PSR_Pos];
    ///* Stores the Configurable Fault Status Register state with the fault cause */
    //crash_info_ram.cy_faultFrame.cfsr.cfsrReg = SCB->CFSR;
    ///* Stores the Hard Fault Status Register */
    //crash_info_ram.cy_faultFrame.hfsr.hfsrReg = SCB->HFSR;
    ///* Stores the System Handler Control and State Register */
    //crash_info_ram.cy_faultFrame.shcsr.shcsrReg = SCB->SHCSR;
    ///* Store MemMange fault address */
    //crash_info_ram.cy_faultFrame.mmfar = SCB->MMFAR;
    ///* Store Bus fault address */
    //crash_info_ram.cy_faultFrame.bfar = SCB->BFAR;

    volatile uint8_t __attribute__((unused)) watchpoint_number = 0;
    // if (DWT->FUNCTION0 & DWT_FUNCTION_MATCHED_Msk) {
    //    watchpoint_number = 0;
    //} else if (DWT->FUNCTION1 & DWT_FUNCTION_MATCHED_Msk) {
    //    watchpoint_number = 1;
    //} else if (DWT->FUNCTION2 & DWT_FUNCTION_MATCHED_Msk) {
    //    watchpoint_number = 2;
    //}
    crash_info_ram.xor_checksum =
        crc7((uint8_t *)&crash_info_ram, offsetof(crash_info_t, xor_checksum));
    __DSB();  // make sure all data is really written into the memory before
              // doing a reset
    reset();
}

extern void DebugMon_Handler(void);
__attribute__((naked)) void DebugMon_Handler(void) {
    __asm volatile(
        " movs r0,#4 \n"
        " movs r1, lr \n"
        " tst r0, r1 \n"
        " beq _MSP2 \n"
        " mrs r0, psp \n"
        " b _HALT2 \n"
        "_MSP2: \n"
        " mrs r0, msp \n"
        "_HALT2: \n"
        " ldr r1,[r0,#20] \n"
        " b DebugMon_HandlerC \n");
}

void Hardfault_HandlerC(uint32_t const *faultStackAddr) {
    memset((void *)crash_info_ram_p, 0, sizeof crash_info_ram);
    crash_info_ram.magic = crash_magic_hard_fault;
    crash_info_ram.timestamp = epochtime;

    /* Stores general registers */
    crash_info_ram.cy_faultFrame.r0 = faultStackAddr[R0_Pos];
    crash_info_ram.cy_faultFrame.r1 = faultStackAddr[R1_Pos];
    crash_info_ram.cy_faultFrame.r2 = faultStackAddr[R2_Pos];
    crash_info_ram.cy_faultFrame.r3 = faultStackAddr[R3_Pos];
    crash_info_ram.cy_faultFrame.r12 = faultStackAddr[R12_Pos];
    crash_info_ram.cy_faultFrame.lr = faultStackAddr[LR_Pos];
    crash_info_ram.cy_faultFrame.pc = faultStackAddr[PC_Pos];
    crash_info_ram.cy_faultFrame.psr = faultStackAddr[PSR_Pos];

    crash_info_ram.xor_checksum =
        crc7((uint8_t *)&crash_info_ram, offsetof(crash_info_t, xor_checksum));
    __DSB();  // make sure all data is really written into the memory before
              // doing a reset

    reset();
}
__attribute__((naked)) void isr_hardfault(void) {
    __asm volatile(
        " movs r0,#4 \n"
        " movs r1, lr \n"
        " tst r0, r1 \n"
        " beq _MSP3 \n"
        " mrs r0, psp \n"
        " b _HALT3 \n"
        "_MSP3: \n"
        " mrs r0, msp \n"
        "_HALT3: \n"
        " ldr r1,[r0,#20] \n"
        " b Hardfault_HandlerC \n");
}

enum {
    crash_info_magic,
    crash_info_hf_lr,
    crash_info_hf_pc,
    crash_info_reset_request_line,
    crash_info_assert
};

int dump_crash_info(crash_info_t const *const crash_info_p, int next, char *const buf,
                    size_t const buf_sz) {
    int nwrit = 0;
    switch (next) {
        case crash_info_magic:
            nwrit = snprintf(buf, buf_sz, "Event: ");
            switch (crash_info_p->magic) {
                case crash_magic_none:
                    nwrit += snprintf(buf + nwrit, buf_sz - nwrit, "\tNone.");
                    next = 0;
                    break;
                case crash_magic_bootloader_entry:
                    nwrit += snprintf(buf + nwrit, buf_sz - nwrit, "\tBootloader Entry.");
                    next = 0;
                    break;
                case crash_magic_hard_fault:
                    nwrit += snprintf(buf + nwrit, buf_sz - nwrit, "\tCM4 Hard Fault.");
                    next = crash_info_hf_lr;
                    break;
                case crash_magic_debug_mon:
                    nwrit += snprintf(buf + nwrit, buf_sz - nwrit,
                                       "\tDebug Monitor Watchpoint Triggered.");
                    next = crash_info_hf_lr;
                    break;
                case crash_magic_reboot_requested:
                    nwrit += snprintf(buf + nwrit, buf_sz - nwrit, "\tReboot Requested.");
                    next = crash_info_reset_request_line;
                    break;
                case crash_magic_assert:
                    nwrit += snprintf(buf + nwrit, buf_sz - nwrit, "\tAssertion Failed.");
                    next = crash_info_assert;
                    break;
                default:
                    DBG_ASSERT_CASE_NOT(crash_info_p->magic);
                    next = 0;
            }
            {
                struct tm tmbuf;
                struct tm *ptm = localtime_r(&crash_info_p->timestamp, &tmbuf);
                char tsbuf[32];
                size_t n = strftime(tsbuf, sizeof tsbuf, "\n\tTime: %Y-%m-%d %H:%M:%S\n", ptm);
                myASSERT(n);
                nwrit = snprintf(buf + nwrit, buf_sz - nwrit, "%s", tsbuf);
            }
            break;
        case crash_info_hf_lr:
            nwrit += snprintf(buf + nwrit, buf_sz - nwrit, "\tLink Register (LR): %p\n",
                              (void *)crash_info_p->cy_faultFrame.lr);
            next = crash_info_hf_pc;
            break;
        case crash_info_hf_pc:
            nwrit += snprintf(buf + nwrit, buf_sz - nwrit, "\tProgram Counter (PC): %p\n",
                              (void *)crash_info_p->cy_faultFrame.pc);
            next = 0;
            break;
        case crash_info_reset_request_line:
            nwrit +=
                snprintf(buf + nwrit, buf_sz - nwrit, "\tReset request calling function: %s\n",
                         crash_info_p->calling_func);
            next = 0;
            break;
        case crash_info_assert:
            nwrit += snprintf(buf + nwrit, buf_sz - nwrit,
                              "\tAssertion \"%s\" failed: file \"%s\", line "
                              "%d, function: %s\n",
                              crash_info_p->assert.pred, crash_info_p->assert.file,
                              crash_info_p->assert.line, crash_info_p->assert.func);
            next = 0;
            break;
        default:
            ASSERT_CASE_NOT(crash_info_p->magic);
    }
    return next;
}

/* [] END OF FILE */
