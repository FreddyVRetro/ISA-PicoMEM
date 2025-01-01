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

#include "opl.h"

#define A_DEBUG_PIN 0

#ifdef MAME_CMS
#include "saa1099/saa1099.h"
extern saa1099_device *saa0, *saa1;
#else
#include "square/square.h"
extern cms_t *cms;
#endif  //MAME_CMS
extern tandysound_t *tandysound;   // Tandy Emulation object    

extern bool dev_adlib_active;
extern bool dev_cms_active;
extern bool dev_tdy_active;
extern volatile bool dev_adlib_playing;
extern volatile bool dev_cms_playing;
extern volatile bool dev_tdy_playing;

extern "C" void OPL_Pico_simple(int16_t *buffer, uint32_t nsamples);
extern "C" void OPL_Pico_simple_stereo(int32_t *buffer, uint32_t nsamples);

//extern "C" void OPL_Pico_Mix_callback(audio_buffer_t *);

uint32_t AudioBufferDuration;
uint32_t AudioEventDuration;
uint32_t AudioEventStart;
uint32_t BufferMixStart;

uint32_t Buff_mix_totaltime;

uint32_t MixedinLoop;

uint8_t *buffer;
#if A_DEBUG_PIN
uint8_t tp1;
uint8_t tp2;
bool Toggle1;
bool Toggle2;
#endif

volatile bool PM_Audio_MixPause=false;

bool pm_audio_active=false;

#if A_DEBUG_PIN
void Toggle_PIN(uint8_t pinnb)
{
if (pinnb==1) 
   {
   if (Toggle1)
    {   
    gpio_put(tp1,1);
    Toggle1=false;
    } 
    else
    {
    gpio_put(tp1,0);
    Toggle1=true;
    }
   } 
   else
   {
    if (Toggle2)  
     {   
    gpio_put(tp2,1);
    Toggle2=false;
    } 
    else
    {
    gpio_put(tp2,0);
    Toggle2=true;
    } 
   }
}
#endif

#if A_DEBUG_PIN
static int32_t TIMERTEST_Event(Bitu val)
{
   AudioEventStart=time_us_32();
   Toggle_PIN(1);
   busy_wait_us(100);
   Toggle_PIN(1);

return -(AudioBufferDuration-(time_us_32()-AudioEventStart));
}
#endif

//MixAudioEvent try to pre compute as much buffer in advance and give back time to the main loop regularly.

static int32_t __time_critical_func(MixAudioEvent_pm)(Bitu val)
{
if (!PM_Audio_MixPause)
  {
   AudioEventStart=time_us_32();
 //  gpio_put(20,1);

//if (pm_audio.mixed_buffers<2) printf ("!%d",pm_audio.mixed_buffers);
//Toggle_PIN(1);
   for (int i=0;i<4;i++)   // Try to mix 4 Buffers before giving some time for the rest
      {
       buffer = take_empty_buffer();
       if (buffer!=NULL)
       {
         memset(buffer,0,PM_SAMPLES_PER_BUFFER*4);  // Clean the buffer in advanced 2*16bit*PM_SAMPLES_PER_BUFFER 
         if (dev_adlib_playing) {
            // Fill int32_t *buffer as int32 for 2x16Bit stereo
            OPL_Pico_simple_stereo((int32_t*) buffer,PM_SAMPLES_PER_BUFFER);
         }  // Adlib
         if (dev_cms_playing) {            
#ifdef MAME_CMS
        saa0->sound_stream_update(buffer, PM_SAMPLES_PER_BUFFER);
        saa1->sound_stream_update(buffer, PM_SAMPLES_PER_BUFFER);
#else
        // uint32_t cms_audio_begin = time_us_32();
        cms->generator(0).generate_frames((int16_t *) buffer, PM_SAMPLES_PER_BUFFER);
        cms->generator(1).generate_frames((int16_t *) buffer, PM_SAMPLES_PER_BUFFER);
#endif // MAME_CMS            
            // Fill int16_t *buffer as 2x16Bit stereo Code modified to add every time.         
         }  // CMS
         if (dev_tdy_playing) {
         tandysound->generator().generate_frames((int16_t *) buffer, PM_SAMPLES_PER_BUFFER);   
         }  //Tandy

         give_mixed_buffer();
       }

      }
//Toggle_PIN(1);
   AudioEventDuration=time_us_32()-AudioEventStart;
   int32_t retval;
// Fire a Litle in advance
    if (AudioEventDuration<(AudioBufferDuration-200)) retval=-(AudioBufferDuration-AudioEventDuration-100);
     else retval=-(AudioBufferDuration>>2);
//gpio_put(20,0);
//  printf("ret %d",-retval);
    return retval;
  } else return AudioBufferDuration<<2;
}

void pm_audio_start() {

if (!pm_audio_active)
 {  

    AudioBufferDuration=uint32_t ((1000000*PM_SAMPLES_PER_BUFFER)/PM_AUDIO_FREQUENCY);
 //printf("AudioBufferDuration %d \n",AudioBufferDuration);
   
//tp1=20; //PIN_AD;
//tp1=0; //PIN_AD;

//gpio_init(tp1);
//gpio_set_dir(tp1, GPIO_OUT);
//gpio_put(tp1, 0);

//tp2=21; //PIN_AS;
//tp2=0; //PIN_AD;

//gpio_init(tp2);
//gpio_set_dir(tp2, GPIO_OUT);
//gpio_put(tp2, 0);

//busy_wait_us(100);

/*
printf("Timer IRQ Priority : %x %x %x %x\n",irq_get_priority(TIMER_IRQ_0),irq_get_priority(TIMER_IRQ_1)
                                         ,irq_get_priority(TIMER_IRQ_2),irq_get_priority(TIMER_IRQ_3));
*/

// Prepare 2 empty buffer in advance
    pm_audio_initpool();

    take_empty_buffer(); 
    give_mixed_buffer(); 
    take_empty_buffer();
    give_mixed_buffer();

    pm_audio_init_i2s(0);      // Start the I2S Audio output

// Start the mixing interrupt
// !!! PIC_Init(); must be started before
    PIC_Init();
    PIC_AddEvent(MixAudioEvent_pm,1000,1);  // call MixAudioEvent_pm in 1000us

   pm_audio_active=true;
 }

//   for (;;) {}

}

void pm_audio_stop()
{
if (pm_audio_active)
 {

 }
}

bool pm_audio_isactive()
{
   return pm_audio_active;   
}