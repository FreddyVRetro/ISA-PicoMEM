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

void pm_audio_domix();
void dev_audiomix_start();
void dev_audiomix_stop();
bool dev_audiomix_isactive();

#ifdef __cplusplus
}
#endif