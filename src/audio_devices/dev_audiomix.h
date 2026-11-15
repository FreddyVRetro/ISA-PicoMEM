#pragma once

//      PicoMEM audio mixing code
// Start, Update and Stop the Audio mix of all the emulated sound cards / devices

#ifdef __cplusplus
extern "C" {
#endif

#define AD_ADLIB 1
#define AD_CMS   2
#define AD_TDY   4
#define AD_MMB   8
#define AD_SBDSP 16
#define AD_CVX   32
#define AD_MPU   64
#define AD_GUS   128
#define AD_PS1   256
#define AD_CD    512

typedef struct dev_audiomix {
   volatile bool enabled;
   volatile bool mix_inprogress;
   volatile uint16_t dev_active;    // Mask of the active devices
} dev_audiomix_t;

extern dev_audiomix_t dev_audiomix;

// Data and Address Commands buffer (For CMS / Adlib)

typedef struct iow_buffer_ad_t {
    volatile uint8_t addrs[256];
    volatile uint8_t datas[256];    
    volatile uint8_t head;
    volatile uint8_t tail;
} iow_buffer_da_t;

// Data Commands buffer (For Tandy)

typedef struct iow_buffer_d_t {
    volatile uint8_t datas[256];
    volatile uint8_t head;
    volatile uint8_t tail;
} iow_buffer_d_t;


__force_inline void dev_audiomix_pause()
{

}

__force_inline void dev_audiomix_resume()
{

}

void pm_audio_domix(bool UseSD);  // Insert Audio Mix during the wait time
void dev_audiomix_start();
void dev_audiomix_stop();
bool dev_audiomix_isactive();
void dev_audiomix_diag();    /* periodic [AUDIODIAG] print — call from main loop only */

/* Master output volume applied to the final mixed buffer. Q8 gain: 256 = unity,
 * 0 = mute. Set live from the screenpad rotary (0..100%) via the setter. */
extern volatile uint16_t dev_audiomix_master_gain;
void dev_audiomix_set_master_vol(uint8_t percent);   /* 0..100 -> gain, applied live */

#ifdef __cplusplus
}
#endif