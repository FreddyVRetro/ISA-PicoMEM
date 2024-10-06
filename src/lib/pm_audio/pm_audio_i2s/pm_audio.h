#pragma once

#include <stdlib.h>  // for "abs"

#include "../../../pm_debug.h"

#define PM_AUDIO_FREQUENCY 49716   // Adlib output frequency
//#define PM_AUDIO_FREQUENCY 44100
#define PM_SAMPLES_PER_BUFFER 64
#define PM_AUDIO_BUFFERS  8
#define PM_SAMPLES_STRIDE 4        // (Nb of byte per sample 16Bit X 2)

#define PM_AUDIO_POOLSIZE PM_AUDIO_BUFFERS*PM_SAMPLES_PER_BUFFER*PM_SAMPLES_STRIDE
#define PM_AUDIO_BUFFERSIZE PM_SAMPLES_PER_BUFFER*PM_SAMPLES_STRIDE

/** \brief Audio format definition
 */
typedef struct audio_format {
    uint32_t sample_freq;      ///< Sample frequency in Hz
    uint16_t format;           ///< Audio format \ref audio_formats
    uint16_t channel_count;    ///< Number of channels
} audio_format_t;

typedef struct pm_audio
 {
    uint8_t *bufferpool;       // Pointer to the Buffer Pool (PM_AUDIO_BUFFERS * PM_SAMPLES_PER_BUFFER * PM_SAMPLES_STRIDE )
    uint8_t *poolend;
    uint8_t *playing_buffer;
    uint8_t *mixing_buffer;
    uint32_t mixed_buffers;     // Total Nb of mixed buffer
    uint32_t played_buffers;    // Total Nb mixed buffer
    bool enabled;
    bool playing_nosound;      // Set to true if it is playing no sound
 } pm_audio_t;

extern volatile pm_audio_t pm_audio;
extern volatile bool PM_Audio_MixPause;

extern bool pm_audio_initpool();
extern bool pm_audio_init_i2s(uint32_t sample_freq);
extern void pm_audio_update_frequency(uint32_t sample_freq);
//extern static void audio_i2s_update_frequency(uint32_t sample_freq);

__force_inline void pm_audio_pause()
{
 PM_Audio_MixPause=true;
}

__force_inline void pm_audio_resume()
{
 PM_Audio_MixPause=false;
}
// Be carefull with this code, it is used by interrupts !

// Take a mixed buffer to give to the DMA (Always the buffer after the last played one)
__force_inline uint8_t * take_mixed_buffer()
{
//if (pm_audio.playing_buffer==NULL) {PM_ERROR("Err: playing_buffer NULL");}
uint32_t pm_buffers_ready=pm_audio.mixed_buffers-pm_audio.played_buffers;
if ((pm_buffers_ready)!=0)
   {
    //printf("-%d",pm_audio.mixed_buffers);
    pm_audio.played_buffers++;
    pm_audio.playing_buffer=pm_audio.playing_buffer+PM_AUDIO_BUFFERSIZE;
    if (pm_audio.playing_buffer>=pm_audio.poolend) pm_audio.playing_buffer=pm_audio.bufferpool;
    return pm_audio.playing_buffer;
   }
//PM_INFO("!Z");
return pm_audio.poolend;  // poolend contains an empty buffer
}

// Take an empty buffer to give to the DMA (Always the buffer after the last mixed one)
__force_inline uint8_t * take_empty_buffer()
{
//if (pm_audio.mixing_buffer==NULL) {PM_ERROR("Err: mixing_buffer NULL");}
// 2 counters are needed so that the DMA Interrupt don't cut the buffer increment instruction in two.
int32_t pm_buffers_ready=pm_audio.mixed_buffers-pm_audio.played_buffers;
if (abs(pm_buffers_ready)!=PM_AUDIO_BUFFERS-1)  // Remove one to remove the playing buffer
   {
    uint8_t * res=pm_audio.mixing_buffer+PM_AUDIO_BUFFERSIZE;
    if (res>=pm_audio.poolend) res=pm_audio.bufferpool;
    pm_audio.mixing_buffer=res;
   // printf(" TB:%x %x",((uint32_t)res)&0x0FFF,pm_audio.mixed_buffers);
    return res;
   }
return NULL;
}

__force_inline void give_mixed_buffer()
{
uint32_t volatile test=pm_audio.mixed_buffers;
pm_audio.mixed_buffers++;
//printf("+GB %d ",pm_audio.mixed_buffers);
//printf("+%d ",pm_audio.mixed_buffers);
}

#define AUDIO_BUFFER_FORMAT_PCM_S16 1          ///< signed 16bit PCM
#define AUDIO_BUFFER_FORMAT_PCM_S8 2           ///< signed 8bit PCM
#define AUDIO_BUFFER_FORMAT_PCM_U16 3          ///< unsigned 16bit PCM
#define AUDIO_BUFFER_FORMAT_PCM_U8 4           ///< unsigned 8bit PCM
