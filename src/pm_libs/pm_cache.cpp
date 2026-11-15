/* Copyright (C) 2026 Freddy VETELE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program.
If not, see <https://www.gnu.org/licenses/>.
*/
/* pm_cache.cpp : Direct-mapped PSRAM sector cache sitting between FatFs
   diskio (disk_read/disk_write/disk_initialize in pm_disk.cpp) and the
   real SD/USB block drivers.

   - One cache per physical drive (DEV_SD=0, DEV_USB=1), 64KB each, i.e.
     128 x 512-byte slots, direct-mapped on (sector % 128).
   - Sector data lives in PSRAM, allocated with ps_malloc()/freed with
     ps_free() -> keeps the 128KB total off the small internal SRAM.
   - The sector tag list (which LBA is currently held in each slot) is
     small (2 x 128 x 5 bytes) and stays in normal SRAM for fast lookup.

   Replacement policy: direct-mapped, no LRU. A sector's slot is fixed by
   pm_cache_slotidx() = sector % PM_CACHE_NSECT, so there is only ever one
   candidate slot per LBA; on a miss whatever sector currently occupies
   that slot is unconditionally overwritten. O(1) lookup/replacement, at
   the cost of two LBAs 128 sectors apart aliasing and evicting each other.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "ff.h"
#include "diskio.h"

#include "../pm_debug.h"
#include "pm_cache.h"

#if USE_DISK_CACHE

#include "pico_psram.h"

#define PM_CACHE_NDRIVES    2                                  // DEV_SD / DEV_USB
#define PM_CACHE_SECT_SZ    512
#define PM_CACHE_SIZE       (256u * 1024u)                      // Cache size per disk
#define PM_CACHE_NSECT      (PM_CACHE_SIZE / PM_CACHE_SECT_SZ) // 128 slots, direct-mapped
#define PM_CACHE_MAXRUN     4                                  // Max contiguous sectors cached per request

// Sector tag list (SRAM) : which LBA a cache slot currently holds
typedef struct {
    LBA_t   lba;
    uint8_t valid;
} pm_cache_slot_t;

static pm_cache_slot_t s_slots[PM_CACHE_NDRIVES][PM_CACHE_NSECT];
static uint8_t        *s_cachebuf[PM_CACHE_NDRIVES] = { NULL, NULL };

static uint32_t        s_hits[PM_CACHE_NDRIVES]   = { 0, 0 };
static uint32_t        s_misses[PM_CACHE_NDRIVES] = { 0, 0 };

static bool             s_enabled[PM_CACHE_NDRIVES] = { true, true };  // cache on by default for SD and USB

extern "C" {
extern DRESULT sd_disk_read(BYTE *buff, LBA_t sector, UINT count);
extern DRESULT sd_disk_write(const BYTE *buff, LBA_t sector, UINT count);
#if USE_USBHOST
extern DRESULT usb_disk_read(BYTE *buff, LBA_t sector, UINT count);
extern DRESULT usb_disk_write(const BYTE *buff, LBA_t sector, UINT count);
#endif
}

static inline uint32_t pm_cache_slotidx(LBA_t sector)
{
    return (uint32_t)(sector % PM_CACHE_NSECT);
}

void pm_cache_init(BYTE pdrv)
{
    if (pdrv >= PM_CACHE_NDRIVES) return;

    if (s_cachebuf[pdrv] == NULL) {
        s_cachebuf[pdrv] = (uint8_t *) ps_malloc(PM_CACHE_SIZE);
        if (s_cachebuf[pdrv] == NULL) {
            PM_WARNING("pm_cache: PSRAM alloc failed for drive %d, cache disabled\n", pdrv);
            s_enabled[pdrv] = false;  // no PSRAM buffer to cache into
        } else {
            s_enabled[pdrv] = true;
        }
    }
    memset(s_slots[pdrv], 0, sizeof(s_slots[pdrv]));  // valid=0 everywhere
    s_hits[pdrv]   = 0;
    s_misses[pdrv] = 0;
    PM_INFO("pm_cache: drive %d cache %s (%d sectors)\n",
            pdrv, s_cachebuf[pdrv] ? "armed" : "disabled", PM_CACHE_NSECT);
}

void pm_cache_deinit(BYTE pdrv)
{
    if (pdrv >= PM_CACHE_NDRIVES) return;

    if (s_cachebuf[pdrv] != NULL) {
        ps_free(s_cachebuf[pdrv]);
        s_cachebuf[pdrv] = NULL;
    }
    memset(s_slots[pdrv], 0, sizeof(s_slots[pdrv]));
}

static inline DRESULT pm_cache_raw_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
#if USE_USBHOST
    return (pdrv == 0) ? sd_disk_read(buff, sector, count) : usb_disk_read(buff, sector, count);
#else
    (void)pdrv;
    return sd_disk_read(buff, sector, count);
#endif
}

static inline DRESULT pm_cache_raw_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
#if USE_USBHOST
    return (pdrv == 0) ? sd_disk_write(buff, sector, count) : usb_disk_write(buff, sector, count);
#else
    (void)pdrv;
    return sd_disk_write(buff, sector, count);
#endif
}

DRESULT pm_cache_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv >= PM_CACHE_NDRIVES || s_cachebuf[pdrv] == NULL || !s_enabled[pdrv] || count == 0)
        { PM_INFO("pm_cache: read bypassed for drive %d (count=%u)\n", pdrv, count);
          return pm_cache_raw_read(pdrv, buff, sector, count);
        }

    pm_cache_slot_t *slots = s_slots[pdrv];
    uint8_t *cbuf = s_cachebuf[pdrv];

    // Requests longer than PM_CACHE_MAXRUN sectors are never fully cached
    // (see pm_cache_write), so only try the all-cached fast path when the
    // whole request fits within that limit.
    if (count <= PM_CACHE_MAXRUN) {
        bool allhit = true;
        for (UINT i = 0; i < count; i++) {
            uint32_t idx = pm_cache_slotidx(sector + i);
            if (!slots[idx].valid || slots[idx].lba != (sector + i)) { allhit = false; break; }
        }
        if (allhit) {
            for (UINT i = 0; i < count; i++) {
                uint32_t idx = pm_cache_slotidx(sector + i);
                memcpy(buff + (size_t)i * PM_CACHE_SECT_SZ, cbuf + (size_t)idx * PM_CACHE_SECT_SZ, PM_CACHE_SECT_SZ);
            }
            s_hits[pdrv]++;
            return RES_OK;
        }
    }

    // Cache miss: read the requested sectors into the caller buffer, then
    // copy at most PM_CACHE_MAXRUN of them into PSRAM to populate/refresh
    // the cache (a long run would just thrash the whole cache for no gain).
    s_misses[pdrv]++;
    DRESULT res = pm_cache_raw_read(pdrv, buff, sector, count);
    if (res == RES_OK) {
        UINT ncache = (count < PM_CACHE_MAXRUN) ? count : PM_CACHE_MAXRUN;
        for (UINT i = 0; i < ncache; i++) {
            uint32_t idx = pm_cache_slotidx(sector + i);
            memcpy(cbuf + (size_t)idx * PM_CACHE_SECT_SZ, buff + (size_t)i * PM_CACHE_SECT_SZ, PM_CACHE_SECT_SZ);
            slots[idx].lba   = sector + i;
            slots[idx].valid = 1;
        }
    }
    return res;
}

static void pm_cache_print_stats_one(BYTE pdrv)
{
    uint32_t hits = s_hits[pdrv], misses = s_misses[pdrv], total = hits + misses;
    float ratio = total ? (100.0f * (float)hits / (float)total) : 0.0f;
    PM_INFO("pm_cache: drive %d hits=%lu misses=%lu ratio=%.1f%%\n",
            pdrv, (unsigned long)hits, (unsigned long)misses, ratio);
}

void pm_cache_show_stats(BYTE pdrv)
{
    if (pdrv < PM_CACHE_NDRIVES) {
        pm_cache_print_stats_one(pdrv);
        return;
    }
    for (BYTE d = 0; d < PM_CACHE_NDRIVES; d++) pm_cache_print_stats_one(d);
}

void pm_cache_set_enabled(BYTE pdrv, bool enabled)
{
    if (pdrv >= PM_CACHE_NDRIVES) return;
    s_enabled[pdrv] = enabled;
}

bool pm_cache_is_enabled(BYTE pdrv)
{
    return (pdrv < PM_CACHE_NDRIVES) && s_enabled[pdrv];
}

DRESULT pm_cache_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    DRESULT res = pm_cache_raw_write(pdrv, buff, sector, count);

    if (pdrv < PM_CACHE_NDRIVES && s_cachebuf[pdrv] != NULL && s_enabled[pdrv] && res == RES_OK) {
        pm_cache_slot_t *slots = s_slots[pdrv];
        uint8_t *cbuf = s_cachebuf[pdrv];
        // Don't cache newly written sectors: only refresh a slot that
        // already holds this LBA, so writes never grow the cache.
        for (UINT i = 0; i < count; i++) {
            uint32_t idx = pm_cache_slotidx(sector + i);
            if (slots[idx].valid && slots[idx].lba == (sector + i))
                memcpy(cbuf + (size_t)idx * PM_CACHE_SECT_SZ, buff + (size_t)i * PM_CACHE_SECT_SZ, PM_CACHE_SECT_SZ);
        }
    }
    return res;
}

#else // USE_DISK_CACHE == 0 : thin passthroughs straight to the raw drivers, no PSRAM cache

extern "C" {
extern DRESULT sd_disk_read(BYTE *buff, LBA_t sector, UINT count);
extern DRESULT sd_disk_write(const BYTE *buff, LBA_t sector, UINT count);
#if USE_USBHOST
extern DRESULT usb_disk_read(BYTE *buff, LBA_t sector, UINT count);
extern DRESULT usb_disk_write(const BYTE *buff, LBA_t sector, UINT count);
#endif
}

void pm_cache_init(BYTE pdrv)   { (void)pdrv; }
void pm_cache_deinit(BYTE pdrv) { (void)pdrv; }

void pm_cache_set_enabled(BYTE pdrv, bool enabled) { (void)pdrv; (void)enabled; }
bool pm_cache_is_enabled(BYTE pdrv) { (void)pdrv; return false; }

DRESULT pm_cache_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
#if USE_USBHOST
    return (pdrv == 0) ? sd_disk_read(buff, sector, count) : usb_disk_read(buff, sector, count);
#else
    (void)pdrv;
    return sd_disk_read(buff, sector, count);
#endif
}

DRESULT pm_cache_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
#if USE_USBHOST
    return (pdrv == 0) ? sd_disk_write(buff, sector, count) : usb_disk_write(buff, sector, count);
#else
    (void)pdrv;
    return sd_disk_write(buff, sector, count);
#endif
}

void pm_cache_show_stats(BYTE pdrv)
{
    (void)pdrv;
    PM_INFO("pm_cache: disabled (USE_DISK_CACHE=0)\n");
}

#endif // USE_DISK_CACHE
