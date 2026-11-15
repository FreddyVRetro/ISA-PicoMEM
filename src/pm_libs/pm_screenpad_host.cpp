// Host-side implementation of the screenpad menu vtable.
//
// Bridges the WonderMCA screenpad UI to the main-repo floppy mount API
// (pm_mountfddimage, FDDfile[], imageDiskList[], PM_Config->FDD[]).
// All functions run on Core 0 (called from screenpad_menu_poll inside
// the Core 0 main loop), so they can freely use FatFs.

#include "pm_screenpad_host.h"

extern "C" {
#include "screenpad_host.h"
}
#include "../pm_defines.h"
#include "../pm_gvars.h"   // PM_DP_RAM (BV_WifiInit lives here)
#include "../ne2000/dev_ne2000.h"   // wifi_infos_t PM_Wifi (SSID / rssi)
#include "ff.h"           // FIL, DIR, FILINFO -- must precede pm_disk.h
#include "pm_disk.h"

#include "../dosbox/bios_disk.h"

// Defined in src/dosbox/bios_disk.cpp -- per-slot "media changed" flag. Set on
// a runtime screenpad mount; INT 13h AH=16h reports "disk changed" (06h) while
// set (held), and the next READ of the drive clears it. (2026-07-04)
extern volatile uint8_t g_fdd_media_changed[2];

// Master output volume (src/audio_devices/dev_audiomix.cpp). Applied to the
// mix so the screenpad "Vol master" setting actually changes loudness.
extern "C" void dev_audiomix_set_master_vol(unsigned char percent);

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/watchdog.h"

#if defined(BOARD_WM10)
#include "wm_boards/wm_10.h"     // WM_VERSION_MAJOR/MINOR/BUILD
#endif

#include <string.h>
#include <stdio.h>

#if USE_USBHOST
// Same forward-declaration trick as pm_disk.cpp -- avoid pulling
// usb.h (which transitively needs CFG_TUH_DEVICE_MAX from tinyusb).
extern "C" bool usb_msc_volume_ready(void);
#endif

// Dual-volume navigation state. The cwd model mirrors the BIOS-side
// virtual-root flow in pm_disk.cpp so behaviour is consistent across
// both UIs (BIOS menu + OLED screenpad).
//   g_current_vol == 0xFF -> at virtual [SD]/[USB] selector. g_cwd is empty.
//   g_current_vol == 0    -> inside SD volume.  g_cwd starts at "0:/FLOPPY/".
//   g_current_vol == 1    -> inside USB volume. g_cwd starts at "1:/FLOPPY/".
// Boot default is virtual root so the user explicitly picks SD or USB on
// first navigation, matching the BIOS menu.
#define SP_CWD_MAX  96
static uint8_t g_current_vol = 0xFF;
static char    g_cwd[SP_CWD_MAX] = "";

// Per-volume FLOPPY root path. Mirrors fdd_dir_for() in pm_disk.cpp.
static inline const char *sph_volume_root(uint8_t vol) {
    return (vol == 1) ? "1:/FLOPPY/" : "0:/FLOPPY/";
}

// --------------------------------------------------------------------------
// Cached image list. Repopulated on every refresh().
// 64 entries is enough for a rotary-driven UI; users with more will use
// CONFIG.SYS / DOS imgmount instead.
// --------------------------------------------------------------------------
#define SP_MAX_IMAGES   64

static sp_fdd_image_t g_images[SP_MAX_IMAGES];
static uint16_t       g_image_count = 0;

// True if cwd_up would have a meaningful effect. With dual-volume
// navigation, this is true whenever we are inside ANY volume -- even
// at a volume's FLOPPY/ root -- because cwd_up there pops us back to
// the virtual [SD]/[USB] selector. Used to decide whether to inject
// a ".." row at the top of the listing.
static bool sph_cwd_above_root(void) {
    return g_current_vol != 0xFF;
}

// True if `fname` matches a known floppy-image extension (case-insensitive).
// Returns the length of the extension (incl. the dot) if matched, else 0.
static uint8_t sph_match_image_ext(const char *fname) {
    static const char * const exts[] = { ".img", ".ima", ".imz", ".vhd" };
    size_t len = strlen(fname);
    for (size_t i = 0; i < sizeof(exts)/sizeof(exts[0]); i++) {
        size_t e = strlen(exts[i]);
        if (len > e &&
            (fname[len - e] == '.') &&
            ( (fname[len - e + 1] | 0x20) == exts[i][1] ) &&
            ( (fname[len - e + 2] | 0x20) == exts[i][2] ) &&
            ( (fname[len - e + 3] | 0x20) == exts[i][3] )) {
            return (uint8_t)e;
        }
    }
    return 0;
}

// --------------------------------------------------------------------------
// Vtable callbacks
// --------------------------------------------------------------------------

static bool sph_get_mounted(uint8_t slot, char out_name[SP_FDD_NAME_LEN]) {
    if (slot > 1 || !PM_Config) return false;
    // Bit 7 of FDD_Attribute set means "PicoMEM image mounted".
    if ((PM_Config->FDD_Attribute[slot] & 0x80) == 0) return false;
    if (PM_Config->FDD[slot].name[0] == '\0') return false;
    strncpy(out_name, PM_Config->FDD[slot].name, SP_FDD_NAME_LEN - 1);
    out_name[SP_FDD_NAME_LEN - 1] = '\0';
    return true;
}

static uint16_t sph_image_count(void) { return g_image_count; }

static bool sph_list_image(uint16_t index, sp_fdd_image_t *out) {
    if (index >= g_image_count) return false;
    *out = g_images[index];
    return true;
}

static void sph_refresh(void) {
    DIR     dj;
    FILINFO fno;
    g_image_count = 0;

    // 0) Virtual root: emit [SD] always, [USB] only if the MSC mount
    //    callback in fatfs_usb.c has finished mounting "1:". No FatFs
    //    walk happens here -- the entries are pure UI affordances that
    //    cwd_enter recognises by literal name.
    if (g_current_vol == 0xFF) {
        if (g_image_count < SP_MAX_IMAGES) {
            sp_fdd_image_t *e = &g_images[g_image_count++];
            snprintf(e->name, SP_FDD_NAME_LEN, "[SD]");
            e->size_kb = 0;
            e->kind    = SP_ENT_DIR;
        }
#if USE_USBHOST
        if (usb_msc_volume_ready() && g_image_count < SP_MAX_IMAGES) {
            sp_fdd_image_t *e = &g_images[g_image_count++];
            snprintf(e->name, SP_FDD_NAME_LEN, "[USB]");
            e->size_kb = 0;
            e->kind    = SP_ENT_DIR;
        }
#endif
        return;
    }

    // 1a) Inject "[Eject]" as the first row whenever the user is inside
    //     a volume. Selecting it routes through sph_mount() which detects
    //     the literal "[Eject]" basename and forwards to sph_unmount() --
    //     gives the user a way to remove the current image without having
    //     to pick a different one. (2026-06-09)
    if (g_image_count < SP_MAX_IMAGES) {
        sp_fdd_image_t *e = &g_images[g_image_count++];
        snprintf(e->name, SP_FDD_NAME_LEN, "[Eject]");
        e->size_kb = 0;
        e->kind    = SP_ENT_FILE;       // dispatched via mount() like an image
    }

    // 1b) Inject a ".." row at the top whenever we're inside a volume.
    //     Even at the volume's FLOPPY/ root, ".." pops us back out to
    //     the [SD]/[USB] selector -- without this the user could enter
    //     [USB] on an empty stick and have no way back.
    if (sph_cwd_above_root() && g_image_count < SP_MAX_IMAGES) {
        sp_fdd_image_t *e = &g_images[g_image_count++];
        snprintf(e->name, SP_FDD_NAME_LEN, "..");
        e->size_kb = 0;
        e->kind    = SP_ENT_PARENT;
    }

    // 2) Walk g_cwd and emit DIR + FILE entries.
    FRESULT fr = f_opendir(&dj, g_cwd);
    if (fr != FR_OK) return;

    // First pass: subdirectories. We list dirs before files so they
    // group at the top of the picker (after the ".." row).
    while (g_image_count < SP_MAX_IMAGES) {
        fr = f_readdir(&dj, &fno);
        if (fr != FR_OK || fno.fname[0] == '\0') break;
        if (!(fno.fattrib & AM_DIR)) continue;
        if (fno.fname[0] == '.')     continue;   // skip hidden / "."/".."

        sp_fdd_image_t *e = &g_images[g_image_count++];
        size_t n = strlen(fno.fname);
        if (n >= SP_FDD_NAME_LEN) n = SP_FDD_NAME_LEN - 1;
        memcpy(e->name, fno.fname, n);
        e->name[n] = '\0';
        e->size_kb = 0;
        e->kind    = SP_ENT_DIR;
    }
    f_closedir(&dj);

    // Second pass: files matching a floppy-image extension.
    fr = f_opendir(&dj, g_cwd);
    if (fr != FR_OK) return;
    while (g_image_count < SP_MAX_IMAGES) {
        fr = f_readdir(&dj, &fno);
        if (fr != FR_OK || fno.fname[0] == '\0') break;
        if (fno.fattrib & AM_DIR) continue;

        uint8_t ext_len = sph_match_image_ext(fno.fname);
        if (ext_len == 0) continue;             // not a floppy image

        size_t name_len = strlen(fno.fname) - ext_len;
        if (name_len >= SP_FDD_NAME_LEN) name_len = SP_FDD_NAME_LEN - 1;

        sp_fdd_image_t *e = &g_images[g_image_count++];
        memcpy(e->name, fno.fname, name_len);
        e->name[name_len] = '\0';
        e->size_kb = (uint32_t)(fno.fsize / 1024);
        e->kind    = SP_ENT_FILE;
    }
    f_closedir(&dj);
}

// Try each known floppy-image extension under g_cwd until one of them
// resolves to an existing file. Writes the full path to `out` on success.
// Returns true if found.
static bool sph_resolve_image_path(const char *basename, char *out, size_t cap) {
    static const char * const exts[] = { ".img", ".ima", ".imz", ".vhd" };
    FILINFO fno;
    for (size_t i = 0; i < sizeof(exts)/sizeof(exts[0]); i++) {
        snprintf(out, cap, "%s%s%s", g_cwd, basename, exts[i]);
        if (f_stat(out, &fno) == FR_OK) return true;
    }
    return false;
}

static uint8_t sph_unmount(uint8_t slot);   // forward-declare for [Eject] route

static uint8_t sph_mount(uint8_t slot, const char *basename) {
    if (slot > 1 || !basename) return 0xFF;
    // Can't mount from the virtual [SD]/[USB] selector -- no real path.
    if (g_current_vol == 0xFF) return 0xFF;

    // Synthetic "[Eject]" entry emitted at the top of every in-volume
    // picker view (see sph_refresh). Routes to the unmount path so the
    // user can clear the slot without having to pick a different image.
    if (!strcmp(basename, "[Eject]")) {
        return sph_unmount(slot);
    }

    char path[160];
    if (!sph_resolve_image_path(basename, path, sizeof path)) {
        // Last-ditch fallback: assume .img under current cwd.
        snprintf(path, sizeof path, "%s%s.img", g_cwd, basename);
    }

    // Was a disk already mounted in this slot? A swap OVER a mounted disk
    // needs the not-ready+changed handshake (state 2) so MS-DOS 6.22 on PS/2
    // fully drops its cached directory -- DOS's media check does NOT react to
    // a bare change line on a live swap. A fresh mount into an empty slot only
    // needs the change line (state 1). See bios_disk.cpp read path.
    bool was_mounted = (imageDiskList[slot] != NULL);

    // Mirror the close/delete sequence performed by CMD_FDD_Mount in
    // pm_cmd.cpp before pm_mountfddimage is called.
    f_close(&FDDfile[slot]);
    if (imageDiskList[slot] != NULL) {
        delete imageDiskList[slot];
        imageDiskList[slot] = NULL;
    }

    uint8_t rc = pm_mountfddimage(path, slot);
    if (rc == 0) {
        // Update the BIOS-shared config so the IDLE screen and any other
        // UI agree on what is currently mounted. Bit 6 of FDD_Attribute
        // records the source volume (matches SelectImage in pm_disk.cpp
        // and the mount path in pm_cmd.cpp).
        strncpy(PM_Config->FDD[slot].name, basename,
                sizeof PM_Config->FDD[slot].name - 1);
        PM_Config->FDD[slot].name[sizeof PM_Config->FDD[slot].name - 1] = '\0';
        uint8_t vol_bit = (g_current_vol == 1) ? 0x40 : 0x00;
        PM_Config->FDD_Attribute[slot] = 0x80 | slot | vol_bit;
        // Flush the new mount to SD so it survives a Pico reboot. The
        // BIOS-menu mount path already does this; without it the user's
        // screenpad selection is lost on the next cold boot.
        pm_SaveConfig();
        // Assert the INT 13h media-change handshake so DOS drops its cached
        // BPB/FAT/dir and re-reads the new image. A swap OVER an already-
        // mounted disk uses state 2 (not-ready once, then changed) to force
        // MS-DOS 6.22/PS/2 to fully flush -- DOS's media check ignores a bare
        // change line on a live swap, so the software sees the "wrong disk"
        // without the not-ready step. A fresh mount into an empty slot uses
        // state 1 (plain change line).
        g_fdd_media_changed[slot] = was_mounted ? 2 : 1;
    }
    return rc;
}

static uint8_t sph_unmount(uint8_t slot) {
    if (slot > 1) return 0xFF;
    f_close(&FDDfile[slot]);
    if (imageDiskList[slot] != NULL) {
        delete imageDiskList[slot];
        imageDiskList[slot] = NULL;
    }
    PM_Config->FDD[slot].name[0] = '\0';
    PM_Config->FDD_Attribute[slot] = slot;   // back to "no PicoMEM image"
    // Persist the unmount so a subsequent cold boot doesn't re-mount the
    // stale image from the on-SD config.
    pm_SaveConfig();
    // Assert the change line so DOS notices the eject too.
    g_fdd_media_changed[slot] = 1;
    return 0;
}

// --------------------------------------------------------------------------
// CWD navigation
// --------------------------------------------------------------------------

static uint8_t sph_cwd_enter(const char *name) {
    if (!name || !name[0])      return 0xFF;

    // Virtual-root volume picks are recognised by literal label. The
    // entries were synthesised by sph_refresh as SP_ENT_DIR so the UI
    // dispatches them through this function. We MUST handle them
    // before the path-validation logic below -- "[SD]" / "[USB]" are
    // not real filesystem names.
    if (g_current_vol == 0xFF) {
        if (!strcmp(name, "[SD]")) {
            g_current_vol = 0;
            snprintf(g_cwd, sizeof g_cwd, "%s", sph_volume_root(0));
            return 0;
        }
#if USE_USBHOST
        if (!strcmp(name, "[USB]")) {
            if (!usb_msc_volume_ready()) return 0xFD;   // stick was pulled
            g_current_vol = 1;
            snprintf(g_cwd, sizeof g_cwd, "%s", sph_volume_root(1));
            return 0;
        }
#endif
        // Any other pick at the virtual root is a programming error.
        return 0xFF;
    }

    if (strchr(name, '/'))      return 0xFF;   // no path separators allowed

    // Build candidate path: g_cwd + name + '/'.
    size_t cur = strlen(g_cwd);
    size_t add = strlen(name);
    if (cur + add + 2 >= SP_CWD_MAX) return 0xFE;  // would overflow

    char candidate[SP_CWD_MAX];
    snprintf(candidate, sizeof candidate, "%s%s/", g_cwd, name);

    // Verify it really is a directory.
    FILINFO fno;
    char check[SP_CWD_MAX];
    snprintf(check, sizeof check, "%s%s", g_cwd, name);    // no trailing /
    if (f_stat(check, &fno) != FR_OK) return 0xFD;
    if (!(fno.fattrib & AM_DIR))      return 0xFC;

    strncpy(g_cwd, candidate, sizeof g_cwd - 1);
    g_cwd[sizeof g_cwd - 1] = '\0';
    return 0;
}

static uint8_t sph_cwd_up(void) {
    if (g_current_vol == 0xFF) return 0;        // already at virtual root

    // Pop out to the virtual [SD]/[USB] selector when we are sitting
    // at the volume's FLOPPY/ root. This matches the BIOS-side flow.
    const char *vol_root = sph_volume_root(g_current_vol);
    if (strcmp(g_cwd, vol_root) == 0) {
        g_current_vol = 0xFF;
        g_cwd[0]      = '\0';
        return 0;
    }

    // Strip trailing slash, then strip last path component, then re-add slash.
    size_t n = strlen(g_cwd);
    if (n == 0) return 0;
    if (g_cwd[n - 1] == '/') g_cwd[--n] = '\0';

    // Find last '/' and truncate after it.
    char *last = strrchr(g_cwd, '/');
    if (!last) {
        // Pathological -- fall back to this volume's FLOPPY root.
        snprintf(g_cwd, sizeof g_cwd, "%s", vol_root);
        return 0;
    }
    *(last + 1) = '\0';   // keep trailing slash

    // Don't allow ascending shallower than this volume's FLOPPY root.
    if (strlen(g_cwd) < strlen(vol_root)) {
        snprintf(g_cwd, sizeof g_cwd, "%s", vol_root);
    }
    return 0;
}

static void sph_cwd_get(char *out, size_t cap) {
    if (!out || cap == 0) return;
    // Compact breadcrumb sized for a 21-col OLED:
    //   virtual root -> "[Volume]"
    //   inside SD    -> "/FLOPPY/foo"      (drop "0:" prefix as before)
    //   inside USB   -> "[USB]/FLOPPY/foo" (keep tag so the user knows
    //                   they're on the stick; the SD form stays
    //                   identical to its pre-dual-volume look)
    if (g_current_vol == 0xFF) {
        snprintf(out, cap, "[Volume]");
        return;
    }
    const char *p = g_cwd;
    if (g_current_vol == 0 && strncmp(p, "0:", 2) == 0) {
        snprintf(out, cap, "%s", p + 2);
    } else if (g_current_vol == 1 && strncmp(p, "1:", 2) == 0) {
        snprintf(out, cap, "[USB]%s", p + 2);
    } else {
        snprintf(out, cap, "%s", p);
    }
}

// --------------------------------------------------------------------------
// Info getters (read-only) -- used by SP_PG_INFO.
// Where the data isn't readily accessible (e.g. heap stats, network bytes),
// we return 0 / empty -- the info page tolerates these and shows "n/a".
// --------------------------------------------------------------------------

static uint32_t sph_get_uptime_ms(void) {
    return (uint32_t)to_ms_since_boot(get_absolute_time());
}

static uint32_t sph_get_heap_free(void) {
    // pico-sdk doesn't expose a portable heap-free getter on all builds.
    // Returning 0 makes the info page show "n/a".
    return 0;
}

static uint32_t sph_get_psram_free(void) {
    return 0;     // see comment above; same story for PSRAM
}

static void sph_get_fw_version(char *out, size_t cap) {
    if (!out || cap == 0) return;
#if defined(WM_VERSION_MAJOR)
    snprintf(out, cap, "%u.%u-b%04X",
             (unsigned)WM_VERSION_MAJOR,
             (unsigned)WM_VERSION_MINOR,
             (unsigned)WM_BUILD);
#else
    snprintf(out, cap, "PicoMEM");
#endif
}

static void sph_get_build_date(char *out, size_t cap) {
    if (!out || cap == 0) return;
    snprintf(out, cap, "%s", __DATE__);
}

static void sph_get_hw_rev(char *out, size_t cap) {
    if (!out || cap == 0) return;
#if defined(BOARD_WM10)
    snprintf(out, cap, "WM 1.0");
#else
    snprintf(out, cap, "PicoMEM");
#endif
}

static uint8_t sph_get_pos_slot_id(void) {
    // POS slot ID is set by the BIOS during the per-slot scan. The bus
    // handler doesn't currently expose it; punt for now.
    return 0;
}

// Network: stubs until the W5500 backend exposes link / byte counters.
static bool      sph_get_net_link_up(void)               { return false; }
static void      sph_get_net_ip(char *out, size_t cap)   { if (out && cap) snprintf(out, cap, "n/a"); }
static uint32_t  sph_get_net_rx_bytes(void)              { return 0; }
static uint32_t  sph_get_net_tx_bytes(void)              { return 0; }

// Audio: stubs until sbdsp.cpp exports a public state accessor.
static bool      sph_get_sbdsp_active(void)              { return false; }
static uint32_t  sph_get_sbdsp_sample_rate(void)         { return 0; }

// Bus stats: counters live in the WonderMCA bus handler but aren't
// currently extern'd. Wire these up once the handler exposes them.
static uint32_t  sph_get_bus_stat(sp_stat_id_t which)    { (void)which; return 0; }

// --------------------------------------------------------------------------
// Settings get/set/save -- maps sp_settings_t to PM_Config where possible,
// and to local storage for fields PM_Config doesn't cover (volumes, OLED
// settings, etc.).
// --------------------------------------------------------------------------

// Local storage for settings PM_Config doesn't yet cover. Persisted
// alongside PM_Config when save_settings() is called. For v0.1 this is
// in-RAM only -- a later rev should add these to the PM_Config struct
// or a sidecar file on the SD card.
static struct {
    bool     initialised;
    uint8_t  sbdsp_dma;       // PicoMEM doesn't track this in PM_Config
    bool     gus_enable;
    bool     cms_enable;
    bool     mute;
    uint8_t  vol_master;
    uint8_t  vol_sb;
    uint8_t  vol_opl;
    uint8_t  vol_gus;
    uint8_t  vol_cms;
    bool     net_dhcp;
    uint32_t net_ip;
    uint32_t net_netmask;
    uint32_t net_gateway;
    uint32_t net_dns;
    char     hostname[SP_HOSTNAME_LEN];
    char     fdd_path[SP_FDD_PATH_LEN];
    bool     auto_mount;
    bool     write_protect_a;
    bool     write_protect_b;
    char     hdd_image[SP_HDD_NAME_LEN];
    uint8_t  oled_brightness;
    uint16_t oled_timeout_s;
    bool     boot_logo;
    bool     boot_sound;
    uint8_t  log_level;
} g_local_settings = {
    .initialised = true,
    .sbdsp_dma       = 1,             // SB default 8-bit DMA channel
    .gus_enable      = false,
    .cms_enable      = false,
    .mute            = false,
    .vol_master      = 100,
    .vol_sb          = 100,
    .vol_opl         = 80,
    .vol_gus         = 80,
    .vol_cms         = 80,
    .net_dhcp        = true,
    .net_ip          = 0,
    .net_netmask     = 0,
    .net_gateway     = 0,
    .net_dns         = 0,
    .hostname        = "picomem",
    .fdd_path        = "0:/FLOPPY",
    .auto_mount      = false,
    .write_protect_a = false,
    .write_protect_b = false,
    .hdd_image       = "",
    .oled_brightness = 200,
    .oled_timeout_s  = 0,
    .boot_logo       = true,
    .boot_sound      = true,
    .log_level       = 3,             // INFO
};

static void sph_get_settings(sp_settings_t *out) {
    if (!out) return;
    memset(out, 0, sizeof *out);

    if (PM_Config) {
        // From PM_Config (BIOS-shared)
        out->sbdsp_irq    = PM_Config->SBIRQ;
        out->sbdsp_base   = PM_Config->SBPort;
        out->audio_enable = (PM_Config->AudioOut  != 0);   // "Global Sound"
        out->opl2_enable  = (PM_Config->Adlib     != 0);
        out->sb_enable    = (PM_Config->SBEnable  != 0);
        out->gus_enable   = (PM_Config->GUSEnable != 0);   // excl. with SB
        out->ne2000_base  = PM_Config->ne2000Port;
        out->ne2000_irq   = PM_Config->ne2000IRQ;
        out->net_type     = PM_Config->EnableWifi;            // NetType 0/1/2
        out->net_enable   = (out->net_type != NET_NONE);
        // Connection status text (read-only), derived from BV_WifiInit.
        if (out->net_type == NET_NONE)
            strncpy(out->net_status, "Off", sizeof out->net_status - 1);
        else if (BV_WifiInit == INIT_DONE)
            strncpy(out->net_status, "Connected", sizeof out->net_status - 1);
        else if (BV_WifiInit == INIT_INPROGRESS)
            strncpy(out->net_status, "Connecting...", sizeof out->net_status - 1);
        else
            strncpy(out->net_status,
                    out->net_type == NET_ETH ? "No link" : "Disconnected",
                    sizeof out->net_status - 1);
        out->net_status[sizeof out->net_status - 1] = '\0';
        // WiFi SSID + signal strength (read-only) -- only when WiFi.
        if (out->net_type == NET_WIFI) {
            strncpy(out->net_ssid, PM_Wifi.WIFI_SSID, sizeof out->net_ssid - 1);
            out->net_ssid[sizeof out->net_ssid - 1] = '\0';
            out->net_rssi = (int16_t)PM_Wifi.rssi;
        } else {
            out->net_ssid[0] = '\0';
            out->net_rssi    = 0;
        }
        out->usb_kbd_enable = (PM_Config->EnableUSBKbd != 0);  // 2026-06-09
        // FDD path comes from PM_Config->FDDPath[0] if set, else local fallback
        if (PM_Config->FDDPath[0][0]) {
            strncpy(out->fdd_path, PM_Config->FDDPath[0], sizeof out->fdd_path - 1);
        } else {
            strncpy(out->fdd_path, g_local_settings.fdd_path, sizeof out->fdd_path - 1);
        }
        if (PM_Config->HDD[0].name[0]) {
            strncpy(out->hdd_image, PM_Config->HDD[0].name, sizeof out->hdd_image - 1);
        }
        out->hdd_enable = (PM_Config->HDD[0].name[0] != '\0');
    } else {
        // Reasonable defaults if PM_Config isn't ready yet
        out->sbdsp_irq   = 5;
        out->sbdsp_base  = 0x220;
        out->opl2_enable = true;
    }

    // From local storage
    out->sbdsp_dma       = g_local_settings.sbdsp_dma;
    out->cms_enable      = g_local_settings.cms_enable;
    out->mute            = g_local_settings.mute;
    out->vol_master      = g_local_settings.vol_master;
    out->vol_sb          = g_local_settings.vol_sb;
    out->vol_opl         = g_local_settings.vol_opl;
    out->vol_gus         = g_local_settings.vol_gus;
    out->vol_cms         = g_local_settings.vol_cms;
    out->net_dhcp        = g_local_settings.net_dhcp;
    // Prefer the live DHCP-snooped lease (PM_Wifi.dhcp_*, filled by wm_dhcp_snoop
    // in the WiFi RX path) over the static config -- DOS/mTCP owns the real IP.
    out->net_ip          = PM_Wifi.dhcp_ip   ? PM_Wifi.dhcp_ip   : g_local_settings.net_ip;
    out->net_netmask     = PM_Wifi.dhcp_mask ? PM_Wifi.dhcp_mask : g_local_settings.net_netmask;
    out->net_gateway     = PM_Wifi.dhcp_gw   ? PM_Wifi.dhcp_gw   : g_local_settings.net_gateway;
    out->net_dns         = PM_Wifi.dhcp_dns  ? PM_Wifi.dhcp_dns  : g_local_settings.net_dns;
    strncpy(out->hostname,  g_local_settings.hostname,  sizeof out->hostname  - 1);
    out->auto_mount      = g_local_settings.auto_mount;
    out->write_protect_a = g_local_settings.write_protect_a;
    out->write_protect_b = g_local_settings.write_protect_b;
    out->oled_brightness = g_local_settings.oled_brightness;
    out->oled_timeout_s  = g_local_settings.oled_timeout_s;
    out->boot_logo       = g_local_settings.boot_logo;
    out->boot_sound      = g_local_settings.boot_sound;
    out->log_level       = g_local_settings.log_level;
}

static uint8_t sph_set_settings(const sp_settings_t *in) {
    if (!in) return 0xFF;

    if (PM_Config) {
        PM_Config->SBIRQ      = in->sbdsp_irq;
        PM_Config->SBPort     = in->sbdsp_base;
        PM_Config->AudioOut   = in->audio_enable ? 1 : 0;   // "Global Sound"
        PM_Config->Adlib      = in->opl2_enable  ? 1 : 0;
        PM_Config->SBEnable   = in->sb_enable    ? 1 : 0;
        PM_Config->GUSEnable  = in->gus_enable   ? 1 : 0;   // excl. with SB
        PM_Config->ne2000Port = in->ne2000_base;
        PM_Config->ne2000IRQ  = in->ne2000_irq;
        PM_Config->EnableWifi = in->net_type;   // NetType 0=None/1=Ethernet/2=WiFi
        PM_Config->EnableUSBKbd = in->usb_kbd_enable ? 1 : 0;  // 2026-06-09
        if (in->fdd_path[0]) {
            strncpy(PM_Config->FDDPath[0], in->fdd_path,
                    sizeof PM_Config->FDDPath[0] - 1);
            PM_Config->FDDPath[0][sizeof PM_Config->FDDPath[0] - 1] = '\0';
        }
    }

    g_local_settings.sbdsp_dma       = in->sbdsp_dma;
    g_local_settings.gus_enable      = in->gus_enable;
    g_local_settings.cms_enable      = in->cms_enable;
    g_local_settings.mute            = in->mute;
    g_local_settings.vol_master      = in->vol_master;
    dev_audiomix_set_master_vol(in->mute ? 0 : in->vol_master);   // apply master volume/mute to the mix
    g_local_settings.vol_sb          = in->vol_sb;
    g_local_settings.vol_opl         = in->vol_opl;
    g_local_settings.vol_gus         = in->vol_gus;
    g_local_settings.vol_cms         = in->vol_cms;
    g_local_settings.net_dhcp        = in->net_dhcp;
    g_local_settings.net_ip          = in->net_ip;
    g_local_settings.net_netmask     = in->net_netmask;
    g_local_settings.net_gateway     = in->net_gateway;
    g_local_settings.net_dns         = in->net_dns;
    strncpy(g_local_settings.hostname, in->hostname, sizeof g_local_settings.hostname - 1);
    strncpy(g_local_settings.fdd_path, in->fdd_path, sizeof g_local_settings.fdd_path - 1);
    g_local_settings.auto_mount      = in->auto_mount;
    g_local_settings.write_protect_a = in->write_protect_a;
    g_local_settings.write_protect_b = in->write_protect_b;
    strncpy(g_local_settings.hdd_image, in->hdd_image, sizeof g_local_settings.hdd_image - 1);
    g_local_settings.oled_brightness = in->oled_brightness;
    g_local_settings.oled_timeout_s  = in->oled_timeout_s;
    g_local_settings.boot_logo       = in->boot_logo;
    g_local_settings.boot_sound      = in->boot_sound;
    g_local_settings.log_level       = in->log_level;
    return 0;
}

static uint8_t sph_save_settings(void) {
    // PicoMEM's CMD_WriteConfig (0x13) is currently unimplemented per
    // pm_lib.h. Until the BIOS-side write is wired up, this is a no-op
    // that returns success so the user gets confirmation that the
    // action was attempted (PM_Config is already updated in RAM by
    // sph_set_settings).
    return 0;
}

static uint8_t sph_reset_settings(void) {
    // Restore the local-only fields to factory defaults. PM_Config
    // itself is left to the BIOS to manage on reset.
    g_local_settings.sbdsp_dma       = 1;
    g_local_settings.vol_master      = 100;
    g_local_settings.vol_sb          = 100;
    g_local_settings.vol_opl         = 80;
    g_local_settings.vol_gus         = 80;
    g_local_settings.vol_cms         = 80;
    g_local_settings.gus_enable      = false;
    g_local_settings.cms_enable      = false;
    g_local_settings.net_dhcp        = true;
    g_local_settings.auto_mount      = false;
    g_local_settings.write_protect_a = false;
    g_local_settings.write_protect_b = false;
    g_local_settings.oled_brightness = 200;
    g_local_settings.oled_timeout_s  = 0;
    g_local_settings.boot_logo       = true;
    g_local_settings.boot_sound      = true;
    g_local_settings.log_level       = 3;
    snprintf(g_local_settings.hostname, sizeof g_local_settings.hostname, "picomem");
    snprintf(g_local_settings.fdd_path, sizeof g_local_settings.fdd_path, "0:/FLOPPY");
    return 0;
}

static uint8_t sph_reload_settings(void) {
    // Mirror of save: requires CMD_ReadConfig (0x12) which is unimplemented.
    // Returning 0 = "ok, attempted" so the UI feedback works.
    return 0;
}

static uint8_t sph_reboot_card(void) {
    // Clear the Core0 hang breadcrumb so this *deliberate* reboot isn't
    // misreported as a watchdog hang on the next boot.
    { extern volatile uint32_t g_c0_bc_magic; g_c0_bc_magic = 0; }
    // 50 ms timeout, no caller protection. Does not return on success.
    watchdog_reboot(0, 0, 50);
    return 0;     // unreachable in normal path, but keep compilers happy
}

// --------------------------------------------------------------------------
// CD-ROM picker callbacks (0:/CDROM with sub-folder browsing). Mirrors the
// FDD mount picker's cwd model (see sph_refresh / sph_cwd_* above), but CD
// is SD-only so there is no [SD]/[USB] virtual root -- g_cd_cwd is always
// rooted at 0:/CDROM/. When the build has no CD-ROM emulation the vtable
// entries stay NULL and the UI hides its row.
// --------------------------------------------------------------------------
#if USE_CDROM
#include "cdrom.h"                  // cdrom lib include dir (isa_devices/cdrom)
#include "cdrom_image_manager.h"
#include "dev_cdrom_mke.h"          // dev_cdr_mount_from_config()

#define SPH_CD_MAX      48
#define SPH_CD_NAME_LEN 24
#define SPH_CD_ROOT     "0:/CDROM/"

static char            g_cd_names[SPH_CD_MAX][SPH_CD_NAME_LEN];
static sp_entry_kind_t g_cd_kinds[SPH_CD_MAX];
static uint8_t         g_cd_count = 0;
// Full browse path, always prefixed by SPH_CD_ROOT, with a trailing '/'.
static char            g_cd_cwd[SP_CWD_MAX] = SPH_CD_ROOT;

// True if `fname` ends in .iso / .cue (case-insensitive).
static bool sph_cd_is_image(const char *fname) {
    size_t l = strlen(fname);
    if (l < 5) return false;
    if (fname[0] == '.' && fname[1] == '_') return false;   // macOS xattr file
    const char *e = fname + (l - 4);
    if (e[0] != '.') return false;
    char c1 = e[1] | 0x20, c2 = e[2] | 0x20, c3 = e[3] | 0x20;
    return (c1 == 'i' && c2 == 's' && c3 == 'o') ||
           (c1 == 'c' && c2 == 'u' && c3 == 'e');
}

static void sph_cd_refresh(void) {
    DIR     dj;
    FILINFO fno;
    g_cd_count = 0;

    // ".." whenever we're below the CDROM root.
    if (strcmp(g_cd_cwd, SPH_CD_ROOT) != 0 && g_cd_count < SPH_CD_MAX) {
        strcpy(g_cd_names[g_cd_count], "..");
        g_cd_kinds[g_cd_count] = SP_ENT_PARENT;
        g_cd_count++;
    }

    // First pass: sub-directories (grouped at the top, after "..").
    if (f_opendir(&dj, g_cd_cwd) != FR_OK) return;
    while (g_cd_count < SPH_CD_MAX) {
        if (f_readdir(&dj, &fno) != FR_OK || fno.fname[0] == 0) break;
        if (!(fno.fattrib & AM_DIR)) continue;
        if (fno.fname[0] == '.')     continue;              // skip hidden / "."/".."
        if (strlen(fno.fname) >= SPH_CD_NAME_LEN) continue;
        strcpy(g_cd_names[g_cd_count], fno.fname);
        g_cd_kinds[g_cd_count] = SP_ENT_DIR;
        g_cd_count++;
    }
    f_closedir(&dj);

    // Second pass: *.iso / *.cue files (extension kept -- cd_mount strips it).
    if (f_opendir(&dj, g_cd_cwd) != FR_OK) return;
    while (g_cd_count < SPH_CD_MAX) {
        if (f_readdir(&dj, &fno) != FR_OK || fno.fname[0] == 0) break;
        if (fno.fattrib & AM_DIR) continue;
        if (!sph_cd_is_image(fno.fname)) continue;
        if (strlen(fno.fname) >= SPH_CD_NAME_LEN) continue; // too long to show/mount
        strcpy(g_cd_names[g_cd_count], fno.fname);
        g_cd_kinds[g_cd_count] = SP_ENT_FILE;
        g_cd_count++;
    }
    f_closedir(&dj);
}

static uint8_t sph_cd_image_count(void) { return g_cd_count; }

static const char *sph_cd_image_name(uint8_t index) {
    return (index < g_cd_count) ? g_cd_names[index] : NULL;
}

static sp_entry_kind_t sph_cd_image_kind(uint8_t index) {
    return (index < g_cd_count) ? g_cd_kinds[index] : SP_ENT_FILE;
}

static uint8_t sph_cd_cwd_enter(const char *name) {
    if (!name || !name[0]) return 0xFF;
    if (strchr(name, '/'))  return 0xFF;            // no path separators
    if (strlen(g_cd_cwd) + strlen(name) + 2 >= SP_CWD_MAX) return 0xFE;

    char candidate[SP_CWD_MAX];
    snprintf(candidate, sizeof candidate, "%s%s/", g_cd_cwd, name);

    char check[SP_CWD_MAX];
    snprintf(check, sizeof check, "%s%s", g_cd_cwd, name);   // no trailing /
    FILINFO fno;
    if (f_stat(check, &fno) != FR_OK) return 0xFD;
    if (!(fno.fattrib & AM_DIR))      return 0xFC;

    strncpy(g_cd_cwd, candidate, sizeof g_cd_cwd - 1);
    g_cd_cwd[sizeof g_cd_cwd - 1] = '\0';
    return 0;
}

static uint8_t sph_cd_cwd_up(void) {
    if (strcmp(g_cd_cwd, SPH_CD_ROOT) == 0) return 0;       // already at root
    size_t n = strlen(g_cd_cwd);
    if (n && g_cd_cwd[n - 1] == '/') g_cd_cwd[--n] = '\0';
    char *last = strrchr(g_cd_cwd, '/');
    if (!last) { snprintf(g_cd_cwd, sizeof g_cd_cwd, SPH_CD_ROOT); return 0; }
    *(last + 1) = '\0';                                     // keep trailing slash
    if (strlen(g_cd_cwd) < strlen(SPH_CD_ROOT))
        snprintf(g_cd_cwd, sizeof g_cd_cwd, SPH_CD_ROOT);
    return 0;
}

static uint8_t sph_cd_mount(const char *filename) {
    if (!filename) return 1;
    size_t len = strlen(filename);
    if (len < 5) return 1;                        // "x.iso" minimum
    size_t base = len - 4;                        // strip .iso / .cue
    if (base > 13) return 2;                      // FSizeName_t.name limit

    // Size in MB for the BIOS Disk-page display. For .cue the file itself is a
    // tiny text sheet -- cd_image_size_mb() parses it and sums the .bin(s).
    char path[SP_CWD_MAX + SPH_CD_NAME_LEN];
    snprintf(path, sizeof path, "%s%s", g_cd_cwd, filename);
    uint16_t mb = (uint16_t)cd_image_size_mb(path);

    memcpy(PM_Config->CDR.name, filename, base);
    PM_Config->CDR.name[base] = 0;
    PM_Config->CDR.size = mb;
    PM_Config->CDR_Attribute = 0;
    // Sub-path relative to the CDROM root (drop the "0:/CDROM/" prefix) so the
    // mount side (dev_cdr_mount_from_config) can reach the browsed folder.
    const char *sub = g_cd_cwd + strlen(SPH_CD_ROOT);
    strncpy(PM_Config->CDRPath, sub, sizeof PM_Config->CDRPath - 1);
    PM_Config->CDRPath[sizeof PM_Config->CDRPath - 1] = 0;
    dev_cdr_mount_from_config();
    pm_SaveConfig();
    return 0;
}

static uint8_t sph_cd_eject(void) {
    PM_Config->CDR.name[0] = 0;
    PM_Config->CDR.size = 0;
    PM_Config->CDRPath[0] = 0;
    PM_Config->CDR_Attribute = 0;
    dev_cdr_mount_from_config();
    pm_SaveConfig();
    return 0;
}

static bool sph_cd_get_mounted(char *out, size_t cap) {
    if (!out || !cap) return false;
    if (PM_Config->CDR.name[0] == 0) { out[0] = 0; return false; }
    snprintf(out, cap, "%s", PM_Config->CDR.name);
    return true;
}
#endif // USE_CDROM

// --------------------------------------------------------------------------
// Vtable + registration
// --------------------------------------------------------------------------

static const screenpad_host_t g_screenpad_host = {
    .get_mounted           = sph_get_mounted,
    .image_count           = sph_image_count,
    .list_image            = sph_list_image,
    .refresh               = sph_refresh,
    .mount                 = sph_mount,
    .unmount               = sph_unmount,

    .cwd_enter             = sph_cwd_enter,
    .cwd_up                = sph_cwd_up,
    .cwd_get               = sph_cwd_get,

#if USE_CDROM
    .cd_refresh            = sph_cd_refresh,
    .cd_image_count        = sph_cd_image_count,
    .cd_image_name         = sph_cd_image_name,
    .cd_image_kind         = sph_cd_image_kind,
    .cd_cwd_enter          = sph_cd_cwd_enter,
    .cd_cwd_up             = sph_cd_cwd_up,
    .cd_mount              = sph_cd_mount,
    .cd_eject              = sph_cd_eject,
    .cd_get_mounted        = sph_cd_get_mounted,
#endif

    .get_uptime_ms         = sph_get_uptime_ms,
    .get_heap_free         = sph_get_heap_free,
    .get_psram_free        = sph_get_psram_free,
    .get_fw_version        = sph_get_fw_version,
    .get_build_date        = sph_get_build_date,
    .get_hw_rev            = sph_get_hw_rev,
    .get_pos_slot_id       = sph_get_pos_slot_id,
    .get_net_link_up       = sph_get_net_link_up,
    .get_net_ip            = sph_get_net_ip,
    .get_net_rx_bytes      = sph_get_net_rx_bytes,
    .get_net_tx_bytes      = sph_get_net_tx_bytes,
    .get_sbdsp_active      = sph_get_sbdsp_active,
    .get_sbdsp_sample_rate = sph_get_sbdsp_sample_rate,
    .get_bus_stat          = sph_get_bus_stat,

    .get_settings          = sph_get_settings,
    .set_settings          = sph_set_settings,
    .save_settings         = sph_save_settings,
    .reset_settings        = sph_reset_settings,
    .reload_settings       = sph_reload_settings,
    .reboot_card           = sph_reboot_card,
};

void pm_screenpad_host_init(void) {
    screenpad_host_register(&g_screenpad_host);
}
