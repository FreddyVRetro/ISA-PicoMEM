#pragma once

#include "../dev_audiomix.h"
#include "pm_debug.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dev_cdrom_mke_t {            
    bool     active;          // True if configured
    volatile uint8_t delay;   // counter for the Nb of second since last I/O
    uint16_t baseport;
} dev_cdrom_mke_t;

extern dev_cdrom_mke_t dev_cdrom_mke;
#if USE_CD_AUDIO
extern bool cd_primed;
#endif

__force_inline void dev_cdr_enable_mix(bool init) {
#if USE_CD_AUDIO
  if (init) cd_primed = false;
#endif
  dev_audiomix.dev_active = dev_audiomix.dev_active | AD_CD;
  PM_INFO("[CDR] Enable Audio mix\n");
}

__force_inline void dev_cdr_disable_mix() {
  dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_CD;
  PM_INFO("[CDR] Disable Audio mix\n");
}

extern uint8_t dev_cdr_install(uint16_t baseport);
extern void dev_cdr_remove();
extern bool dev_cdr_installed();
extern void dev_cdr_update();
extern void dev_cdr_mount_from_config();
extern bool dev_cdr_ior(uint32_t Addr,uint8_t *Data);
extern void dev_cdr_iow(uint32_t Addr,uint8_t Data);
#if USE_CD_AUDIO
// Mix CD-DA into an interleaved stereo int16 buffer at out_rate Hz.
// No-op (and resets its resampler state) when no disc is playing.
extern void dev_cdr_mixaudio(int16_t *buffer, uint32_t frames, bool UseSD);
#endif

// --- INT 13h EDD / El Torito support (bios_disk.cpp INT13_CD) ------------
// Read the 2048-byte user data of one CD sector. Returns true on success,
// false when no disc is mounted / LBA out of range / read error.
extern bool dev_cdr_read2048(uint32_t lba, uint8_t *buf);
// Total 2048-byte sectors of the mounted image (0 if none).
extern uint32_t dev_cdr_total_sectors(void);

// Parsed El Torito default boot entry (media_type 0 = no-emulation).
typedef struct {
    uint8_t  media_type;    // 0=no-emu, 1-3=floppy 1.2/1.44/2.88M, 4=HDD
    uint16_t load_seg;      // 0 -> 0x7C0
    uint16_t sector_count;  // in 512-byte virtual sectors
    uint32_t image_lba;     // boot image start (2048-byte sectors)
    uint32_t catalog_lba;   // boot catalog LBA
} dev_cdr_eltorito_t;

// Parse the El Torito boot catalog of the mounted disc. `scratch` = any
// 2048-byte work buffer. Returns 0=ok, 1=no media, 2=not bootable /
// no valid catalog. Parses fresh each call (rare) -- no cache to stale.
extern uint8_t dev_cdr_eltorito(dev_cdr_eltorito_t *et, uint8_t *scratch);

#ifdef __cplusplus
}
#endif