/* my_rtc.c
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
#include <assert.h>
#include <stdbool.h>
#include <time.h>
//
#include "pico/aon_timer.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
//#include "hardware/rtc.h"
//
#include "crc.h"
#include "ff.h"
//
#include "my_rtc.h"

time_t epochtime;

// Make an attempt to save a recent time stamp across reset:
typedef struct rtc_save {
    uint32_t signature;
    struct timespec ts;
    char checksum;  // last, not included in checksum
} rtc_save_t;
static rtc_save_t rtc_save __attribute__((section(".uninitialized_data")));

static bool get_time(struct timespec *ts) {
    if (!aon_timer_is_running()) return false;
    aon_timer_get_time(ts);
    return true;
}

/**
 * @brief Update the epochtime variable from the always-on timer
 *
 * If the always-on timer is running, copy its current value to the epochtime variable.
 * Also, copy the current always-on timer value to the rtc_save structure and
 * calculate a checksum of the structure.
 */
static void update_epochtime() {
    bool ok = get_time(&rtc_save.ts);
    if (!ok) return;
    // Set the signature to the magic number
    rtc_save.signature = 0xBABEBABE;
    // Calculate the checksum of the structure
    rtc_save.checksum = crc7((uint8_t *)&rtc_save, offsetof(rtc_save_t, checksum));
    // Copy the seconds part of the always-on timer to the epochtime variable
    epochtime = rtc_save.ts.tv_sec;
}

/**
 * @brief Get the current time in seconds since the Epoch.
 * *
 * @param[in] pxTime If not NULL, the current time is copied here.
 * @return The current time in seconds since the Epoch.
 */
time_t time(time_t *pxTime) {
    update_epochtime();
    if (pxTime) {
        *pxTime = epochtime;
    }
    return epochtime;
}

/**
 * @brief Initialize the always-on timer and save its value to the rtc_save structure
 *
 * If the always-on timer is already running, this function does nothing.
 * Otherwise, it initializes the RTC if it is available and checks if the saved
 * time is valid. If the saved time is valid, it sets the always-on timer to the saved
 * value.
 */
void time_init() {
    // If the always-on timer is already running, return immediately
    if (aon_timer_is_running()) return;

        // Initialize the RTC if it is available
#if HAS_RP2040_RTC
//    rtc_init();
#endif

    // Check if the saved time is valid
    char xor_checksum = crc7((uint8_t *)&rtc_save, offsetof(rtc_save_t, checksum));
    bool ok = rtc_save.signature == 0xBABEBABE && rtc_save.checksum == xor_checksum;

    // If the saved time is valid, set the always-on timer
    if (ok) aon_timer_set_time(&rtc_save.ts);
}

/**
 * @brief Get the current time in the FAT time format
 *
 * The FAT file system uses a specific date and time format that is different
 * from the one used by the standard C library. This function converts the
 * current time to the FAT time format.
 *
 * @return The current time in the FAT time format (DWORD)
 */
/* FatFs refuses a zero timestamp, and a firmware assert() on a date is never
   worth bricking a file write for. If the always-on timer has not been started
   yet, or holds a date FAT cannot represent, fall back to a valid constant. */
#define FATTIME_FALLBACK \
    (((DWORD)(FF_NORTC_YEAR - 1980) << 25) | ((DWORD)FF_NORTC_MON << 21) | ((DWORD)FF_NORTC_MDAY << 16))

DWORD get_fattime(void) {
    struct timespec ts;
    bool ok = get_time(&ts);
    if (!ok) return FATTIME_FALLBACK;

    struct tm t;
    localtime_r(&ts.tv_sec, &t);

    DWORD fattime = 0;
    // bit31:25
    // Year origin from the 1980 (0..127, e.g. 37 for 2017)
    // tm_year	int	years since 1900
    int yr = t.tm_year + 1900 - 1980;
    if (yr < 0 || yr > 127) return FATTIME_FALLBACK;
    fattime |= (0b01111111 & yr) << 25;
    // bit24:21
    // Month (1..12)
    // tm_mon	int	months since January 0-11
    uint8_t mo = t.tm_mon + 1;
    if (mo < 1 || mo > 12) return FATTIME_FALLBACK;
    fattime |= (0b00001111 & mo) << 21;
    // bit20:16
    // Day of the month (1..31)
    // tm_mday	int	day of the month 1-31
    uint8_t da = t.tm_mday;
    if (da < 1 || da > 31) return FATTIME_FALLBACK;
    fattime |= (0b00011111 & da) << 16;
    // bit15:11
    // Hour (0..23)
    // tm_hour	int	hours since midnight 0-23
    uint8_t hr = t.tm_hour;
    if (hr > 23) hr = 0;
    fattime |= (0b00011111 & hr) << 11;
    // bit10:5
    // Minute (0..59)
    // tm_min	int	minutes after the hour 0-59
    uint8_t mi = t.tm_min;
    if (mi > 59) mi = 0;
    fattime |= (0b00111111 & mi) << 5;
    // bit4:0
    // Second / 2 (0..29, e.g. 25 for 50)
    // tm_sec	int	seconds after the minute 0-61*
    uint8_t sd = t.tm_sec / 2;
    if (sd > 29) sd = 29;  // The extra range is to accommodate for leap seconds
    fattime |= (0b00011111 & sd);
    return fattime;
}
