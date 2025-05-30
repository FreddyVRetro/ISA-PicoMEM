// Originally from https://github.com/sparkfun/sparkfun-pico
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

// Hacked by Earle Philhower to work with the Arduino-Pico core

#include <../../psram_board.h>


//#define PIMORONI_PICO_PLUS2_PSRAM_CS_PIN 47
//#define RP2350_PSRAM_CS PIMORONI_PICO_PLUS2_PSRAM_CS_PIN
#if BOARD_PM15
#define RP2350_PSRAM_CS 0
#else
#define RP2350_PSRAM_CS 47  // Pimoroni pico plus 2
#endif
#define PSRAM_HEAP 0


#ifdef RP2350_PSRAM_CS

#include <stdio.h>

#include <hardware/address_mapped.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/regs/addressmap.h>
#include <hardware/spi.h>
#include <hardware/structs/qmi.h>
#include <hardware/structs/xip_ctrl.h>
#include  <hardware/sync.h>
#include <pico/runtime_init.h>

#if PSRAM_HEAP
// Include TLSF in this compilation unit
#include "tlsf/tlsf.c"

static tlsf_t _mem_heap = nullptr;
static pool_t _mem_psram_pool = nullptr;
// PSRAM heap minus PSRAM global/static variables from the linker
/*
extern "C" {
    extern uint8_t __psram_start__;
    extern uint8_t __psram_heap_start__;
}
*/

//#define __psram_start__ 0x11000000  // XIP Cache
//#define __psram_start__ 0x15000000  // XIP Cache Translate
#define __psram_start__ 0x1D000000    // XIP No Cache No translate
uint32_t __psram_heap_start__;

// PSRAM Start : 0x11000000
// Heap Start : + 4096

static bool _bInitalized = false;
size_t ps_heap_size = 0;
#endif  //PSRAM_HEAP
size_t ps_size = 0;


#define RP2350_PSRAM_MAX_SCK_HZ (100'000'000)
// RP2350_PSRAM_MAX_SCK_HZ=100 :
// System Clk 280000000, cooldown 2, rxdelay 4, Max Select: 35, Min Deselect: 15, clock divider: 3

#define PICO_RUNTIME_INIT_PSRAM "11001" // Towards the end, after alarms

#ifndef RP2350_PSRAM_MAX_SELECT_FS64
#define RP2350_PSRAM_MAX_SELECT_FS64 (125'000'000)
#endif

#ifndef RP2350_PSRAM_MIN_DESELECT_FS
#define RP2350_PSRAM_MIN_DESELECT_FS (50'000'000)
#endif

#ifndef RP2350_PSRAM_MAX_SCK_HZ
#define RP2350_PSRAM_MAX_SCK_HZ (109'000'000)
#endif

#ifndef RP2350_PSRAM_ID
#define RP2350_PSRAM_ID (0x5D)
#endif

// DETAILS/
//
// SparkFun RP2350 boards use the following PSRAM IC:
//
//      apmemory APS6404L-3SQR-ZR
//      https://www.mouser.com/ProductDetail/AP-Memory/APS6404L-3SQR-ZR?qs=IS%252B4QmGtzzpDOdsCIglviw%3D%3D
//
// The origin of this logic is from the Circuit Python code that was downloaded from:
//     https://github.com/raspberrypi/pico-sdk-rp2350/issues/12#issuecomment-2055274428
//
//  PicoMEM : apmemory APS6404L-3SQR-SN
// Details on the PSRAM IC that are used during setup/configuration of PSRAM on SparkFun RP2350 boards.

// For PSRAM timing calculations - to use int math, we work in femto seconds (fs) (1e-15),
// NOTE: This idea is from micro python work on psram..

#define SFE_SEC_TO_FS 1000000000000000ll

// max select pulse width = 8us => 8e6 ns => 8000 ns => 8000 * 1e6 fs => 8000e6 fs
// Additionally, the MAX select is in units of 64 clock cycles - will use a constant that
// takes this into account - so 8000e6 fs / 64 = 125e6 fs

const uint32_t SFE_PSRAM_MAX_SELECT_FS64 = RP2350_PSRAM_MAX_SELECT_FS64;

// min deselect pulse width = 50ns => 50 * 1e6 fs => 50e7 fs
const uint32_t SFE_PSRAM_MIN_DESELECT_FS = RP2350_PSRAM_MIN_DESELECT_FS;

// from psram datasheet - max Freq with VDDat 3.3v - SparkFun RP2350 boards run at 3.3v.
// If VDD = 3.0 Max Freq is 133 Mhz
const uint32_t SFE_PSRAM_MAX_SCK_HZ = RP2350_PSRAM_MAX_SCK_HZ;

// PSRAM SPI command codes
const uint8_t PSRAM_CMD_QUAD_END = 0xF5;
const uint8_t PSRAM_CMD_QUAD_ENABLE = 0x35;
const uint8_t PSRAM_CMD_READ_ID = 0x9F;
const uint8_t PSRAM_CMD_RSTEN = 0x66;
const uint8_t PSRAM_CMD_RST = 0x99;
const uint8_t PSRAM_CMD_QUAD_READ = 0xEB;
const uint8_t PSRAM_CMD_QUAD_WRITE = 0x38;
const uint8_t PSRAM_CMD_NOOP = 0xFF;

const uint8_t PSRAM_ID = RP2350_PSRAM_ID;

//-----------------------------------------------------------------------------
/// @brief Communicate directly with the PSRAM IC - validate it is present and return the size
///
/// @return size_t The size of the PSRAM
///
/// @note This function expects the CS pin set
static size_t __no_inline_not_in_flash_func(get_psram_size)(void) {
    size_t psram_size = 0;
    uint32_t intr_stash = save_and_disable_interrupts();

    // Try and read the PSRAM ID via direct_csr.
    qmi_hw->direct_csr = 30 << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;

    // Need to poll for the cooldown on the last XIP transfer to expire
    // (via direct-mode BUSY flag) before it is safe to perform the first
    // direct-mode operation
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
    }

    // Exit out of QMI in case we've inited already
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;

    // Transmit the command to exit QPI quad mode - read ID as standard SPI
    qmi_hw->direct_tx =
        QMI_DIRECT_TX_OE_BITS | QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB | PSRAM_CMD_QUAD_END;

    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
    }

    (void)qmi_hw->direct_rx;
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS);

    // Read the id
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    uint8_t kgd = 0;
    uint8_t eid = 0;
    for (size_t i = 0; i < 7; i++) {
        qmi_hw->direct_tx = (i == 0 ? PSRAM_CMD_READ_ID : PSRAM_CMD_NOOP);

        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_TXEMPTY_BITS) == 0) {
        }
        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
        }
        if (i == 5) {
            kgd = qmi_hw->direct_rx;
        } else if (i == 6) {
            eid = qmi_hw->direct_rx;
        } else {
            (void)qmi_hw->direct_rx;    // just read and discard
        }
    }

    // Disable direct csr.
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

    // is this the PSRAM we're looking for obi-wan?
    if (kgd == PSRAM_ID) {
        // PSRAM size
        psram_size = 1024 * 1024; // 1 MiB
        uint8_t size_id = eid >> 5;
        if (eid == 0x26 || size_id == 2) {
            psram_size *= 8;
        } else if (size_id == 0) {
            psram_size *= 2;
        } else if (size_id == 1) {
            psram_size *= 4;
        }
    }
    restore_interrupts(intr_stash);
    return psram_size;
}
//-----------------------------------------------------------------------------
/// @brief Update the PSRAM timing configuration based on system clock
///
/// @note This function expects interrupts to be enabled on entry

static void __no_inline_not_in_flash_func(set_psram_timing)(void) {
    // Get secs / cycle for the system clock - get before disabling interrupts.
    uint32_t sysHz = (uint32_t)clock_get_hz(clk_sys);

    // Calculate the clock divider - goal to get clock used for PSRAM <= what
    // the PSRAM IC can handle - which is defined in SFE_PSRAM_MAX_SCK_HZ
    volatile uint8_t clockDivider = (sysHz + SFE_PSRAM_MAX_SCK_HZ - 1) / SFE_PSRAM_MAX_SCK_HZ;

    uint32_t intr_stash = save_and_disable_interrupts();

    // Get the clock femto seconds per cycle.
    uint32_t fsPerCycle = SFE_SEC_TO_FS / sysHz;

    // Field       : QMI_M1_TIMING_MAX_SELECT
    // Description : Enforce a maximum assertion duration for this window's chip
    //               select, in units of 64 system clock cycles.    
    // the maxSelect value is defined in units of 64 clock cycles
    // So maxFS / (64 * fsPerCycle) = maxSelect = SFE_PSRAM_MAX_SELECT_FS64/fsPerCycle
    volatile uint8_t maxSelect = SFE_PSRAM_MAX_SELECT_FS64 / fsPerCycle;

    // Field       : QMI_M1_TIMING_MIN_DESELECT
    // Description : After this window's chip select is deasserted, it remains
    //               deasserted for half an SCK cycle (rounded up to an integer
    //               number of system clock cycles), plus MIN_DESELECT additional
    //               system clock cycles, before the QMI reasserts either chip
    //               select pin.    
    //  minDeselect time - in system clock cycle
    // Must be higher than 50ns (min deselect time for PSRAM) so add a fsPerCycle - 1 to round up
    // So minFS/fsPerCycle = minDeselect = SFE_PSRAM_MIN_DESELECT_FS/fsPerCycle

    volatile uint8_t minDeselect = (SFE_PSRAM_MIN_DESELECT_FS + fsPerCycle - 1) / fsPerCycle;

    // -----------------------------------------------------------------------------
    // Field       : QMI_M1_TIMING_SELECT_HOLD
    // Description : Add up to three additional system clock cycles of active hold
    //               between the last falling edge of SCK and the deassertion of
    //               this window's chip select.
   
// Adjust / Test
// https://forums.raspberrypi.com/viewtopic.php?t=375975
// For Flash (Same with PSRAM ? )
// RXDELAY (you have to be careful, usually for QSPI>100MHz it works with RXDELAY = CLKDIV)
// CLKDIV  (can be 1,2,3,4,... in contrast with 2040 which accept only odd values 2,4,6 ...)

    //maxSelect = 35;
    //minDeselect = 15;

    uint8_t selectHold = 0;     
    uint8_t cooldown = 1;
    uint8_t rxdelay = 4;
    clockDivider = 4;

  // if (sysHz/clockDivider>100000000) rxdelay = clockDivider;

   printf("System Clk %d, divider: %d (%d), cooldown %d, rxdelay %d, Max Select: %d, Min Deselect: %d, Sel Hold %d\n ",
           sysHz/1000000,clockDivider,sysHz/(1000000*clockDivider), cooldown,rxdelay,maxSelect,minDeselect,selectHold);
 
    qmi_hw->m[1].timing = QMI_M1_TIMING_PAGEBREAK_VALUE_1024 << QMI_M1_TIMING_PAGEBREAK_LSB | // Break between pages.
                          selectHold << QMI_M1_TIMING_SELECT_HOLD_LSB   | // Delay releasing CS for 3 extra system cycles.
                          cooldown   << QMI_M1_TIMING_COOLDOWN_LSB      |
                          rxdelay    << QMI_M1_TIMING_RXDELAY_LSB       |
                          maxSelect  << QMI_M1_TIMING_MAX_SELECT_LSB    |
                          minDeselect << QMI_M1_TIMING_MIN_DESELECT_LSB |
                          clockDivider << QMI_M1_TIMING_CLKDIV_LSB;

    restore_interrupts(intr_stash);
}


//-----------------------------------------------------------------------------
/// @brief The setup_psram function - note that this is not in flash
///
/// Fail if ps_size=0
void __no_inline_not_in_flash_func(setup_psram)(bool useheap,uint32_t heapstart) 
{
    // Set the PSRAM CS pin in the SDK
    gpio_set_function(RP2350_PSRAM_CS, GPIO_FUNC_XIP_CS1);
    //gpio_set_slew_rate(RP2350_PSRAM_CS, GPIO_SLEW_RATE_FAST);
    //gpio_set_drive_strength(RP2350_PSRAM_CS, GPIO_DRIVE_STRENGTH_8MA);

    // start with zero size
    size_t psram_size = get_psram_size();

    // No PSRAM - no dice
    if (psram_size == 0) {
        return;
    }

     uint32_t intr_stash = save_and_disable_interrupts();
    // Enable quad mode.
    qmi_hw->direct_csr = 30 << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;

    // Need to poll for the cooldown on the last XIP transfer to expire
    // (via direct-mode BUSY flag) before it is safe to perform the first
    // direct-mode operation
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
    }

    // RESETEN, RESET and quad enable
    for (uint8_t i = 0; i < 3; i++) {
        qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        if (i == 0) {
            qmi_hw->direct_tx = PSRAM_CMD_RSTEN;
        } else if (i == 1) {
            qmi_hw->direct_tx = PSRAM_CMD_RST;
        } else {
            qmi_hw->direct_tx = PSRAM_CMD_QUAD_ENABLE;
        }

        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
        }
        qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS);

        for (size_t j = 0; j < 20; j++) {
            asm("nop");     // Maybe need to be changed if bigger overclocking
        }

        (void)qmi_hw->direct_rx;
    }

    // Disable direct csr.
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

    // check our interrupts and setup the timing
    restore_interrupts(intr_stash);
    set_psram_timing();

    // and now stash interrupts again
    intr_stash = save_and_disable_interrupts();

    qmi_hw->m[1].rfmt = (QMI_M1_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_PREFIX_WIDTH_LSB |
                         QMI_M1_RFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_RFMT_ADDR_WIDTH_LSB |
                         QMI_M1_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_SUFFIX_WIDTH_LSB |
                         QMI_M1_RFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_RFMT_DUMMY_WIDTH_LSB |
                         QMI_M1_RFMT_DUMMY_LEN_VALUE_24 << QMI_M1_RFMT_DUMMY_LEN_LSB |
                         QMI_M1_RFMT_DATA_WIDTH_VALUE_Q << QMI_M1_RFMT_DATA_WIDTH_LSB |
                         QMI_M1_RFMT_PREFIX_LEN_VALUE_8 << QMI_M1_RFMT_PREFIX_LEN_LSB |
                         QMI_M1_RFMT_SUFFIX_LEN_VALUE_NONE << QMI_M1_RFMT_SUFFIX_LEN_LSB);

    qmi_hw->m[1].rcmd = PSRAM_CMD_QUAD_READ << QMI_M1_RCMD_PREFIX_LSB | 0 << QMI_M1_RCMD_SUFFIX_LSB;

    qmi_hw->m[1].wfmt = (QMI_M1_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_PREFIX_WIDTH_LSB |
                         QMI_M1_WFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_WFMT_ADDR_WIDTH_LSB |
                         QMI_M1_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_SUFFIX_WIDTH_LSB |
                         QMI_M1_WFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_WFMT_DUMMY_WIDTH_LSB |
                         QMI_M1_WFMT_DUMMY_LEN_VALUE_NONE << QMI_M1_WFMT_DUMMY_LEN_LSB |
                         QMI_M1_WFMT_DATA_WIDTH_VALUE_Q << QMI_M1_WFMT_DATA_WIDTH_LSB |
                         QMI_M1_WFMT_PREFIX_LEN_VALUE_8 << QMI_M1_WFMT_PREFIX_LEN_LSB |
                         QMI_M1_WFMT_SUFFIX_LEN_VALUE_NONE << QMI_M1_WFMT_SUFFIX_LEN_LSB);

    qmi_hw->m[1].wcmd = PSRAM_CMD_QUAD_WRITE << QMI_M1_WCMD_PREFIX_LSB | 0 << QMI_M1_WCMD_SUFFIX_LSB;

    // Mark that we can write to PSRAM.
    xip_ctrl_hw->ctrl |= XIP_CTRL_WRITABLE_M1_BITS;

    restore_interrupts(intr_stash);

    ps_size = psram_size;
#if PSRAM_HEAP
    if (useheap)        // if useheap is false, heap is not initializes as ps_heap_size=0
    {
     __psram_heap_start__=__psram_start__+heapstart;    
     uint32_t used_psram_size = __psram_heap_start__ - __psram_start__;
     ps_heap_size = ps_size - used_psram_size;
    }
#endif    
}

// update timing -- used if the system clock/timing was changed.
void psram_reinit_timing() {
    set_psram_timing();
}

#if PSRAM_HEAP
static bool ps_heap_init() {
    if (_bInitalized) {
        return true;
    }

    if (!ps_heap_size) {
        return false;
    }
    _mem_heap = NULL;
    _mem_psram_pool = NULL;
    //_mem_heap = tlsf_create_with_pool((void *)__psram_heap_start__, ps_heap_size, 16 * 1024 * 1024);
    _mem_heap = tlsf_create_with_pool((void *)__psram_heap_start__, ps_heap_size);
    if (!_mem_heap) {
        return false;
    }
    _mem_psram_pool = tlsf_get_pool(_mem_heap);
    if (!_mem_psram_pool) {
        return false;
    }
    _bInitalized = true;
    return true;
}

void *ps_malloc(size_t size) {
    if (!ps_heap_init() || !_mem_heap) {
        return NULL;
    }
    return tlsf_malloc(_mem_heap, size);
}

void ps_free(void *ptr) {
    if (!ps_heap_init() || !_mem_heap) {
        return;
    }
    tlsf_free(_mem_heap, ptr);
}

void *ps_realloc(void *ptr, size_t size) {
    if (!ps_heap_init() || !_mem_heap) {
        return NULL;
    }
    return tlsf_realloc(_mem_heap, ptr, size);
}

void *ps_calloc(size_t num, size_t size) {
    if (!ps_heap_init() || !_mem_heap) {
        return NULL;
    }
    void *ptr = tlsf_malloc(_mem_heap, num * size);
    if (ptr) {
        bzero(ptr, num * size);
    }
    return ptr;
}

static bool max_free_walker(void *ptr, size_t size, int used, void *user) {
    size_t *max_size = (size_t *)user;
    if (!used && *max_size < size) {
        *max_size = size;
    }
    return true;
}

size_t ps_largest_free_block() {
    if (!ps_heap_init() || !_mem_heap) {
        return 0;
    }
    size_t max_free = 0;
    if (_mem_psram_pool) {
        tlsf_walk_pool(_mem_psram_pool, (tlsf_walker) max_free_walker, &max_free);
    }
    return max_free;
}

static bool memory_size_walker(void *ptr, size_t size, int used, void *user) {
    *((size_t *)user) += size;
    return true;
}

size_t ps_total_space() {
    if (!ps_heap_init() || !_mem_heap) {
        return 0;
    }
    size_t total_size = 0;
    if (_mem_psram_pool) {
        tlsf_walk_pool(_mem_psram_pool, (tlsf_walker) memory_size_walker, &total_size);
    }
    return total_size;
}

static bool memory_used_walker(void *ptr, size_t size, int used, void *user) {
    if (used) {
        *((size_t *)user) += size;
    }
    return true;
}

size_t ps_total_used() {
    if (!ps_heap_init() || !_mem_heap) {
        return 0;
    }
    size_t total_size = 0;
    if (_mem_psram_pool) {
        tlsf_walk_pool(_mem_psram_pool, (tlsf_walker) memory_used_walker, &total_size);
    }
    return total_size;
}
#endif

#endif // RP2350_PSRAM_CS
