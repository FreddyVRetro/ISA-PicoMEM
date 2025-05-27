/* PicoMEM Sound Cards mixer used to mix all the different sources.  */

#include <stdio.h>
#include <math.h>
#include <string.h>  // For memset

#if PICO_ON_DEVICE
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "pico_pic.h"
#endif

#include "pico/stdlib.h"
#include "pm_audio.h"
#include "dev_audiomix.h"

#include "opl.h"

#define A_DEBUG_PIN 0

#include "square/square.h"
extern cms_t *cms;

extern tandysound_t *tandysound;   // Tandy Emulation object    

extern volatile bool dev_adlib_active;
extern volatile bool dev_cms_active;
extern volatile bool dev_tdy_active;
extern volatile bool dev_sbdsp_playing;
extern volatile bool dev_mmb_active;

extern "C" void OPL_Pico_simple(int16_t *buffer, uint32_t nsamples);
extern "C" void OPL_Pico_simple_stereo(int32_t *buffer, uint32_t nsamples);
extern void sbdsp_getbuffer(int16_t* buff,uint32_t samples);
extern void dev_psg_getbuffer(int32_t* buff,uint8_t samples);

//extern "C" void OPL_Pico_Mix_callback(audio_buffer_t *);

dev_audiomix_t dev_audiomix;

uint8_t *buffer;

typedef struct pma_status
 {
   bool active;
   bool pause;
   bool mix_inprogress;
 } pma_status_t;

pma_status_t PM_Audio;

void __time_critical_func(pm_audio_domix)() {
DBG_ON_MIX();
for (int i=0;i<4;i++)   // Try to mix 4 Buffers before giving some time for the rest
    {
       buffer = take_empty_buffer();
       if (buffer!=NULL)
       {
         memset(buffer,0,PM_SAMPLES_PER_BUFFER*PM_SAMPLES_STRIDE);  // Clean the buffer in advanced 2*16bit*PM_SAMPLES_PER_BUFFER 

         if (dev_audiomix.dev_active & AD_ADLIB) {
            // Fill int32_t *buffer as int32 for 2x16Bit stereo
            OPL_Pico_simple_stereo((int32_t*) buffer,PM_SAMPLES_PER_BUFFER);
            }  // Adlib

#if USE_SBDSP         
       //  PM_INFO(".");
         if (dev_sbdsp_playing) {
       //      PM_INFO("x");
             sbdsp_getbuffer((int16_t*) buffer,PM_SAMPLES_PER_BUFFER);
            }  // Sound Blaster DSP
#endif

         if (dev_audiomix.dev_active & AD_CMS) {            
        cms->generator(0).generate_frames((int16_t *) buffer, PM_SAMPLES_PER_BUFFER);
        cms->generator(1).generate_frames((int16_t *) buffer, PM_SAMPLES_PER_BUFFER);              
            }  // CMS

         if (dev_audiomix.dev_active & AD_TDY) {
         tandysound->generator().generate_frames((int16_t *) buffer, PM_SAMPLES_PER_BUFFER);   
            }  //Tandy

         if (dev_audiomix.dev_active & AD_MMB) {
            dev_psg_getbuffer((int32_t *) buffer,PM_SAMPLES_PER_BUFFER);
            }  //mindscape music board

         give_mixed_buffer();
       } else break; // Return directly if no buffer to mix
    }
DBG_OFF_MIX();
}

//MixAudioEvent try to pre compute as much buffer in advance and give back time to the main loop regularly.
static int32_t __time_critical_func(MixAudioEvent_pm)(Bitu val)
{
if (!dev_audiomix.paused)
  {
   uint32_t EventStart;
   EventStart=time_us_32();

// if (pm_audio.mixed_buffers<2) printf ("!%d",pm_audio.mixed_buffers);
// Toggle_PIN(1);
   pm_audio_domix();
// Toggle_PIN(1);

   int32_t retval;
   uint32_t EventDuration;

   EventDuration=time_us_32()-EventStart;

// Fire a Litle in advance
    if (EventDuration<(dev_audiomix.b_duration-200)) retval=-(dev_audiomix.b_duration-EventDuration-100);
     else retval=-(dev_audiomix.b_duration>>2);
//  printf("ret %d",-retval);
    return retval;
  } else return dev_audiomix.b_duration<<2;
}

void dev_audiomix_start() {

if (!dev_audiomix.enabled)   
 {
  dev_audiomix.b_duration=uint32_t ((1000000*PM_SAMPLES_PER_BUFFER)/PM_AUDIO_FREQUENCY);
  PM_INFO("dev_audiomix.b_duration %d \n",dev_audiomix.b_duration);

/*
printf("Timer IRQ Priority : %x %x %x %x\n",irq_get_priority(TIMER_IRQ_0),irq_get_priority(TIMER_IRQ_1)
                                         ,irq_get_priority(TIMER_IRQ_2),irq_get_priority(TIMER_IRQ_3));
*/

   dev_audiomix.enabled=pm_audio_init(0);      // Start the PicoMEM Audio output
   dev_audiomix.paused=false;                  // Audio mixing is allowed
   dev_audiomix.dev_active=0;                  // No audio device is mixing

// Start the mixing interrupt
// !!! PIC_Init(); must be started before
   PIC_Init();
#if AUDIOMIX_NOIRQ
#else
   PIC_AddEvent(MixAudioEvent_pm,1000,1);  // call MixAudioEvent_pm in 1000us
#endif
   
 }
//   for (;;) {}

}

void dev_audiomix_stop()
{
if (dev_audiomix.enabled)
 {
  pm_audio_stop();
  dev_audiomix.paused=true;          // Audio mixing is not allowed
  dev_audiomix.dev_active=0;         // No audio device is mixing
 }
}

bool dev_audiomix_isactive()
{
   return dev_audiomix.enabled;   
}