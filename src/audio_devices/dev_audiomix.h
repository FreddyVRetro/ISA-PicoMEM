#pragma once

//      PicoMEM audio mixing code
// Start, Update and Stop the Audio mix of all the emulated sound cards / devices

#define AD_ADLIB 1
#define AD_CMS   2
#define AD_TDY   4
#define AD_MMB   8
#define AD_SBDSP 16

#define AUDIOMIX_NOIRQ 0

typedef struct dev_audiomix {
   volatile bool enabled=false;
   volatile bool paused;            // To pause the audio mixing during critical part or when off
   volatile uint16_t dev_active;    // Mask of the active devices
   uint32_t b_duration;             // Single buffer duration (In micro second)
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
 dev_audiomix.paused=true;
}

__force_inline void dev_audiomix_resume()
{
 dev_audiomix.paused=false;
}

extern void __time_critical_func(pm_audio_domix)();
extern void dev_audiomix_start();
extern void dev_audiomix_stop();
extern bool dev_audiomix_isactive();