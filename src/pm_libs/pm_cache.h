#pragma once
/* pm_cache.h : Direct-mapped PSRAM sector cache for the FatFs diskio layer
   (SD card / USB MSC). One 64KB cache per physical drive (pdrv), sector
   data lives in PSRAM (ps_malloc/ps_free), sector tags live in SRAM.
 */

// Set to 1 (e.g. via CMake target_compile_definitions) to enable the cache;
// when 0, pm_cache_* become thin passthroughs straight to the raw drivers.
#ifndef USE_DISK_CACHE
#define USE_DISK_CACHE 0
#endif

#include "ff.h"      // BYTE, LBA_t, UINT
#include "diskio.h"  // DSTATUS, DRESULT
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// (Re)allocate the PSRAM cache buffer for pdrv (first call only) and
// invalidate all cached sectors. Call after a successful disk_initialize().
void pm_cache_init(BYTE pdrv);

// Free the PSRAM cache buffer for pdrv, if any, and invalidate all entries.
void pm_cache_deinit(BYTE pdrv);

// Cache-aware replacements for the raw sd_disk_read / usb_disk_read calls.
// Serves fully cached requests straight from PSRAM; otherwise performs the
// physical read into buff and populates the cache from it.
DRESULT pm_cache_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count);

// Cache-aware replacements for the raw sd_disk_write / usb_disk_write calls.
// Always writes through to the physical media, then updates the cache.
DRESULT pm_cache_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count);

// Print hit/miss counters and hit ratio for pdrv (or all drives if pdrv is
// out of range) to the debug console.
void pm_cache_show_stats(BYTE pdrv);

// Runtime on/off switch for the cache on a given drive (SD=0, USB=1).
// Enabled by default for both drives. When disabled, pm_cache_read/write
// bypass the PSRAM cache entirely and go straight to the raw drivers,
// without touching or growing the existing cache contents.
void pm_cache_set_enabled(BYTE pdrv, bool enabled);
bool pm_cache_is_enabled(BYTE pdrv);

#ifdef __cplusplus
}
#endif
