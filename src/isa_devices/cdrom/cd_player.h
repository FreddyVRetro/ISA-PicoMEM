/* cd_player.h - High-level Red Book audio player API on top of cdrom_t.
 * Wraps cdrom_audio_playmsf / cdrom_audio_pause_resume / cdrom_stop with
 * track enumeration, duration lookup, and +/-10s seeking within a track.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CDPLAYER_STATUS_NO_AUDIO,  // no disc, empty drive, or data-only disc
    CDPLAYER_STATUS_STOPPED,
    CDPLAYER_STATUS_PLAYING,
    CDPLAYER_STATUS_PAUSED,
    CDPLAYER_STATUS_COMPLETED,
} cdplayer_status_t;

typedef struct {
    uint8_t number;      // track number as printed on the disc (1-based)
    uint8_t min;
    uint8_t sec;
    uint8_t frame;        // duration MM:SS:FF (75 frames/sec)
} cdplayer_track_t;

// Number of audio (Red Book) tracks on the loaded disc, 0 if none/no disc.
uint8_t cdplayer_get_track_count(void);

// Highest track number on the disc (data + audio), 0 if no disc. Track
// numbers run 1..this value; use cdplayer_get_track_info() to test each
// one, since data tracks can be interleaved with audio tracks.
uint8_t cdplayer_get_last_track_number(void);

// Fill info->{number,min,sec,frame} with the duration of an audio track.
// Returns false if the track does not exist or is not an audio track.
bool cdplayer_get_track_info(uint8_t track, cdplayer_track_t *info);

// Start playback of the given audio track. Returns false if the track
// does not exist, is not an audio track, or the drive rejected the seek.
bool cdplayer_play(uint8_t track);

void cdplayer_stop(void);
void cdplayer_pause(void);
void cdplayer_resume(void);

// Jump +/-10s within the currently playing/paused track, clamped to the
// track boundaries. No-op if nothing is playing/paused.
void cdplayer_fast_forward(void);
void cdplayer_rewind(void);

cdplayer_status_t cdplayer_get_status(void);

#ifdef __cplusplus
}
#endif
