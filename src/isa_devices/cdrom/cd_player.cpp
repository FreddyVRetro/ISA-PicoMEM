/* cd_player.cpp - High-level Red Book audio player on top of cdrom_t.
 * See cd_player.h for the API contract.
 */
#include "cd_player.h"
#include "cdrom.h"
#include "../../pm_debug.h"
#include "../../audio_devices/dev_audiomix.h"

#include "hardware/sync.h"  // __dmb

// Red Book track attribute for an audio track (matches AUDIO_TRACK in
// cdrom_image_backend.h; kept local so this file has no backend dependency).
#define CDPLAYER_TRACK_ATTR_AUDIO 0x10

#define CDPLAYER_FRAMES_PER_SECOND 75
#define CDPLAYER_SEEK_SECONDS      10

// LBA bounds of the track currently loaded by cdplayer_play(), used to
// clamp fast-forward/rewind. Only valid while g_track_active is true.
static uint32_t g_track_start_lba = 0;
static uint32_t g_track_end_lba   = 0;
static bool     g_track_active    = false;

static inline uint32_t cdplayer_frames_of(uint8_t m, uint8_t s, uint8_t f)
{
    return (((uint32_t)m * 60) + s) * 75 + f;
}

static inline void cdplayer_frames_to_msf(uint32_t frames, uint8_t *m, uint8_t *s, uint8_t *f)
{
    *m = (uint8_t)(frames / (60 * 75));
    frames %= (60 * 75);
    *s = (uint8_t)(frames / 75);
    *f = (uint8_t)(frames % 75);
}

// True once a disc is loaded and its track list can be queried, i.e. not
// empty and not a data-only (no audio track) image.
static bool cdplayer_disc_ready(void)
{
    bool ready = cdrom.disk_loaded && cdrom.ops
        && cdrom.cd_status != CD_STATUS_EMPTY
        && cdrom.cd_status != CD_STATUS_DATA_ONLY;
    PM_INFO("[CDPLAYER] disc_ready=%d disk_loaded=%d ops=%p cd_status=%d\n",
            ready, cdrom.disk_loaded, (void *)cdrom.ops, cdrom.cd_status);
    return ready;
}

// Fetch a track's start MSF and following-track (or lead-out) start MSF,
// used both for duration and for the play range end.
static bool cdplayer_get_track_bounds(uint8_t track, track_info_t *ti, track_info_t *ti_next)
{
    int first = 0, last = 0;
    cdrom.ops->get_tracks(&cdrom, &first, &last);
    if ((int)track < first || (int)track > last)
        return false;

    cdrom.ops->get_track_info(&cdrom, (uint32_t)track, 0, ti);

    uint32_t next_track = (track < last) ? (uint32_t)(track + 1) : (uint32_t)(last + 1);
    cdrom.ops->get_track_info(&cdrom, next_track, 0, ti_next);
    return true;
}

static void cdplayer_reset_staging(void)
{
    cdrom.audio_staging_sectors = 0;
    cdrom.audio_staging_index = 0;
    cdrom.audio_sector_consumed_pairs = 0;
#if USE_CD_AUDIO_FIFO
    fifo_reset(&cdrom.audio_fifo);
#endif
}

uint8_t cdplayer_get_track_count(void)
{
    if (!cdplayer_disc_ready())
        return 0;

    int first = 0, last = 0;
    cdrom.ops->get_tracks(&cdrom, &first, &last);
    PM_INFO("[CDPLAYER] get_track_count: first=%d last=%d\n", first, last);

    uint8_t count = 0;
    track_info_t ti;
    for (int t = first; t <= last; t++) {
        cdrom.ops->get_track_info(&cdrom, (uint32_t)t, 0, &ti);
        PM_INFO("[CDPLAYER]   track %d attr=%02X\n", t, ti.attr);
        if (ti.attr == CDPLAYER_TRACK_ATTR_AUDIO)
            count++;
    }
    PM_INFO("[CDPLAYER] get_track_count -> %d\n", count);
    return count;
}

uint8_t cdplayer_get_last_track_number(void)
{
    if (!cdplayer_disc_ready())
        return 0;

    int first = 0, last = 0;
    cdrom.ops->get_tracks(&cdrom, &first, &last);
    return (last > 0 && last <= 255) ? (uint8_t)last : 0;
}

bool cdplayer_get_track_info(uint8_t track, cdplayer_track_t *info)
{
    if (!info || !cdplayer_disc_ready())
        return false;

    track_info_t ti, ti_next;
    if (!cdplayer_get_track_bounds(track, &ti, &ti_next)) {
        PM_INFO("[CDPLAYER] get_track_info(%d): out of range\n", track);
        return false;
    }
    if (ti.attr != CDPLAYER_TRACK_ATTR_AUDIO) {
        PM_INFO("[CDPLAYER] get_track_info(%d): not audio, attr=%02X\n", track, ti.attr);
        return false;
    }

    uint32_t start_frames = cdplayer_frames_of(ti.m, ti.s, ti.f);
    uint32_t end_frames   = cdplayer_frames_of(ti_next.m, ti_next.s, ti_next.f);
    uint32_t duration     = (end_frames > start_frames) ? (end_frames - start_frames) : 0;

    info->number = (uint8_t)ti.number;
    cdplayer_frames_to_msf(duration, &info->min, &info->sec, &info->frame);
    return true;
}

bool cdplayer_play(uint8_t track)
{
    if (!cdplayer_disc_ready())
        return false;

    track_info_t ti, ti_next;
    if (!cdplayer_get_track_bounds(track, &ti, &ti_next))
        return false;
    if (ti.attr != CDPLAYER_TRACK_ATTR_AUDIO)
        return false;

    if (!cdrom_audio_playmsf(&cdrom, ti.m, ti.s, ti.f, ti_next.m, ti_next.s, ti_next.f)) {
        g_track_active = false;
        return false;
    }

    // pm_audio_domix() is a no-op until the mixer is started; the OLED menu
    // can request playback before any DOS driver has touched a sound port.
    dev_audiomix_start();

    // Same LBA domain as dev->seek_pos/dev->cd_end (MSFtoLBA - 150 lead-in).
    g_track_start_lba = cdplayer_frames_of(ti.m, ti.s, ti.f) - 150;
    g_track_end_lba   = cdplayer_frames_of(ti_next.m, ti_next.s, ti_next.f) - 150;
    g_track_active    = true;
    return true;
}

void cdplayer_stop(void)
{
    cdrom_stop(&cdrom);
    g_track_active = false;
}

void cdplayer_pause(void)
{
    if (cdrom.cd_status == CD_STATUS_PLAYING)
        cdrom_audio_pause_resume(&cdrom, 0);
}

void cdplayer_resume(void)
{
    if (cdrom.cd_status == CD_STATUS_PAUSED)
        cdrom_audio_pause_resume(&cdrom, 1);
}

void cdplayer_fast_forward(void)
{
    if (!g_track_active)
        return;
    if (cdrom.cd_status != CD_STATUS_PLAYING && cdrom.cd_status != CD_STATUS_PAUSED)
        return;

    uint32_t delta  = CDPLAYER_SEEK_SECONDS * CDPLAYER_FRAMES_PER_SECOND;
    uint32_t newpos = cdrom.seek_pos + delta;
    if (newpos > g_track_end_lba)
        newpos = g_track_end_lba;

    cdrom.seek_pos = newpos;
    cdplayer_reset_staging();
    __dmb();
}

void cdplayer_rewind(void)
{
    if (!g_track_active)
        return;
    if (cdrom.cd_status != CD_STATUS_PLAYING && cdrom.cd_status != CD_STATUS_PAUSED)
        return;

    uint32_t delta  = CDPLAYER_SEEK_SECONDS * CDPLAYER_FRAMES_PER_SECOND;
    uint32_t newpos = (cdrom.seek_pos > g_track_start_lba + delta) ?
                       cdrom.seek_pos - delta : g_track_start_lba;

    cdrom.seek_pos = newpos;
    cdplayer_reset_staging();
    __dmb();
}

cdplayer_status_t cdplayer_get_status(void)
{
    if (!cdrom.disk_loaded || !cdrom.ops)
        return CDPLAYER_STATUS_NO_AUDIO;

    switch (cdrom.cd_status) {
        case CD_STATUS_PLAYING:
            return CDPLAYER_STATUS_PLAYING;
        case CD_STATUS_PAUSED:
            return CDPLAYER_STATUS_PAUSED;
        case CD_STATUS_STOPPED:
            return CDPLAYER_STATUS_STOPPED;
        case CD_STATUS_PLAYING_COMPLETED:
            return CDPLAYER_STATUS_COMPLETED;
        case CD_STATUS_EMPTY:
        case CD_STATUS_DATA_ONLY:
        default:
            return CDPLAYER_STATUS_NO_AUDIO;
    }
}
