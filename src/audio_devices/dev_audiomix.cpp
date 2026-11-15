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
#include "../isa_devices/dev_cdrom_mke.h"
#include "../isa_devices/cdrom/cdrom.h"

#define A_DEBUG_PIN 0

#include "square/square.h"
extern cms_t *cms;

extern tandysound_t *tandysound;   // Tandy Emulation object    

extern void* tsfrenderer;

extern void dev_mpu_getbuffer(int32_t *buffer, int samples);
extern int dev_mpu_get_active_voices(void);
extern void sbdsp_updatebuffer(int16_t* buff,uint32_t samples);
extern void dev_psg_getbuffer(int32_t* buff,uint8_t samples);
void cvx_updatebuffer(int16_t* buff,uint32_t samples);
extern void gus_updatebuffer(int16_t* buff,uint32_t samples);

//extern "C" void OPL_Pico_Mix_callback(audio_buffer_t *);

dev_audiomix_t dev_audiomix;
uint8_t *buffer;

typedef struct pma_status
 {
   bool active;
   bool mix_inprogress;
 } pma_status_t;

pma_status_t PM_Audio;

volatile uint32_t audio_underruns = 0;   /* incremented in I2S DMA ISR     */
static uint64_t audio_diag_next = 0;

volatile uint16_t dev_audiomix_master_gain = 256;   /* Q8: 256 = unity */

void dev_audiomix_set_master_vol(uint8_t percent)
{
   if (percent > 100) percent = 100;
   dev_audiomix_master_gain = (uint16_t)(((uint32_t)percent * 256u) / 100u);
}

int64_t opl_TotalTime=0;
uint32_t opl_bufferCount=0;
int64_t starttime;

void oplperf_start()
{
  starttime = time_us_64();
}

void oplperf_end()
{
  opl_bufferCount++;
  int64_t delta= time_us_64()-starttime;
  opl_TotalTime += delta;
  if (opl_bufferCount==1000)
   {
     PM_INFO("[SND][OPL] 1/1k: %d/%duS CPU %%: %d\n",(int)delta, (int)opl_TotalTime,(int)(opl_TotalTime/12873));  // calculated for a 49KHz mixing
   //  PM_INFO("Voices %d\n",pm_tsf_get_active_voices(tsfrenderer));
     opl_TotalTime=0;
     opl_bufferCount=0;
   }
}

int64_t mpu_TotalTime=0;
uint32_t mpu_bufferCount=0;

void mpuperf_start()
{
  starttime = time_us_64();
}

void mpuperf_end()
{
  mpu_bufferCount++;
  int64_t delta= time_us_64()-starttime;
  mpu_TotalTime += delta;
  if (mpu_bufferCount==1000)
   {
//     PM_INFO("[SND][MPU] 1/1k: %d/%duS CPU %%: %d\n",(int)delta, (int)mpu_TotalTime,(int)(mpu_TotalTime/12873));  // calculated for a 49KHz mixing
     PM_INFO("[SND][MPU] %d voices, CPU %d\n", dev_mpu_get_active_voices(), (int)(mpu_TotalTime/12873));
   //  PM_INFO("Voices %d\n",pm_tsf_get_active_voices(tsfrenderer));
     mpu_TotalTime=0;
     mpu_bufferCount=0;
   }
}

/* Mix all the active audio devices into the PicoMEM audio output buffer.  Called
 * from the main loop, not from timing-critical paths.  Each device is expected
 * to fill the buffer with its own audio data, and the mixer will add them all
 * together and scale by the master volume.
 * UseSD tell if MicroSD access is authorized
 */
void pm_audio_domix(bool UseSD) {

if (!dev_audiomix.enabled) return;
if (dev_audiomix.mix_inprogress) {
  return;
}
DBG_ON_MIX();
dev_audiomix.mix_inprogress=true;

for (int i=0;i<4;i++)   // Try to mix 4 Buffers before giving some time for the rest
    {
       buffer = take_empty_buffer();
       if (buffer!=NULL)
       {
         memset(buffer,0,PM_SAMPLES_PER_BUFFER*PM_SAMPLES_STRIDE);  // Clean the buffer in advanced 2*16bit*PM_SAMPLES_PER_BUFFER 

#if USE_MPU
         if (dev_audiomix.dev_active & AD_MPU) {
             mpuperf_start();            
             dev_mpu_getbuffer((int32_t*) buffer, PM_SAMPLES_PER_BUFFER);
             mpuperf_end();
             }  // MPU (Various rendering)
#endif

         if (dev_audiomix.dev_active & AD_ADLIB) {
            // Fill int32_t *buffer as int32 for 2x16Bit stereo
            oplperf_start();
            OPL_Pico_stereo16((int16_t*) buffer,PM_SAMPLES_PER_BUFFER); // interleave, 16Bit
            oplperf_end();
            }  // Adlib

#if USE_SBDSP         
         if (dev_audiomix.dev_active & AD_SBDSP) {
             sbdsp_updatebuffer((int16_t*) buffer,PM_SAMPLES_PER_BUFFER);
            }  // Sound Blaster DSP
#endif

         if (dev_audiomix.dev_active & AD_CMS) {
             cms->generator(0).generate_frames((int16_t *) buffer, PM_SAMPLES_PER_BUFFER);
             cms->generator(1).generate_frames((int16_t *) buffer, PM_SAMPLES_PER_BUFFER);              
            }  // CMS

         if (dev_audiomix.dev_active & AD_TDY) {
             tandysound->generator().generate_frames((int16_t *) buffer, PM_SAMPLES_PER_BUFFER);   
            }  //Tandy (mono)

         if (dev_audiomix.dev_active & AD_MMB) {
             dev_psg_getbuffer((int32_t *) buffer,PM_SAMPLES_PER_BUFFER);
            }  //mindscape music board (Stereo)

         if (dev_audiomix.dev_active & AD_CVX) {
             cvx_updatebuffer((int16_t*) buffer,PM_SAMPLES_PER_BUFFER);
            }  // Covox (mono)

#if USE_GUS
         if (dev_audiomix.dev_active & AD_GUS) {
            gus_updatebuffer((int16_t*) buffer,PM_SAMPLES_PER_BUFFER);
           }  // Gravis Ultrasound
#endif

#if USE_CDROM && USE_CD_AUDIO
         // CD-DA (Red Book) -- adds+clamps into the buffer; cheap no-op
         // unless a disc is playing, so no dev_active flag to manage.
         if (dev_audiomix.dev_active & AD_CD) {         
            if (UseSD) cdrom_audio_callback(&cdrom, AUDIO_FIFO_SIZE - STEREO_SAMPLES_PER_SECTOR);      
            dev_cdr_mixaudio((int16_t*) buffer,PM_SAMPLES_PER_BUFFER,UseSD);
#if USE_CD_AUDIO_FIFO
            if (cdrom.cd_status != CD_STATUS_PLAYING && fifo_level(&cdrom.audio_fifo)==0) {
                 dev_cdr_disable_mix();
                 PM_INFO("[MIX] CD not playing and FIFO empty\n");
               }
#else
            if (cdrom.cd_status != CD_STATUS_PLAYING) {
              // To add : check cd_stage_level
                 dev_cdr_disable_mix();
                 PM_INFO("[MIX] CD not playing\n");
               }
#endif
         }
#endif

         // Master volume: scale the whole mix (skip when unity to save CPU).
         { uint16_t mg = dev_audiomix_master_gain;
           if (mg != 256) {
              int16_t *s = (int16_t*)buffer;
              for (int k = 0; k < PM_SAMPLES_PER_BUFFER * 2; k++)
                 s[k] = (int16_t)(((int32_t)s[k] * mg) >> 8);
           } }

         give_mixed_buffer();
       } else break;

    }  // for loop

dev_audiomix.mix_inprogress=false;
DBG_OFF_MIX();
}

void dev_audiomix_start() {

if (!dev_audiomix.enabled)   
 {
/*
printf("Timer IRQ Priority : %x %x %x %x\n",irq_get_priority(TIMER_IRQ_0),irq_get_priority(TIMER_IRQ_1)
                                         ,irq_get_priority(TIMER_IRQ_2),irq_get_priority(TIMER_IRQ_3));
*/
   PM_INFO("[SND][dev_audiomix] start\n");

   dev_audiomix.enabled=pm_audio_init(0);      // Start the PicoMEM Audio output
   dev_audiomix.dev_active=0;                  // No audio device is mixing
   dev_audiomix.mix_inprogress=false;          // No audio mixing in progress

   PIC_Init();                                 // Audio devices need the PIC
 }
}

void dev_audiomix_stop()
{
if (dev_audiomix.enabled)
 {
  PM_INFO("[SND][dev_audiomix] stop\n");
  pm_audio_stop();
  dev_audiomix.dev_active=0;         // No audio device is mixing
 }
}

bool dev_audiomix_isactive()
{
   return dev_audiomix.enabled;
}

/* Call from the main loop (not from timing-critical paths). Prints the I2S
 * DMA starve count every 5 s when audio is active and underruns occurred.  */
void dev_audiomix_diag()
{
    uint64_t now = time_us_64();
    if (audio_diag_next == 0) audio_diag_next = now + 5000000ULL;
    if (now >= audio_diag_next) {
        uint32_t u = audio_underruns;
        audio_underruns = 0;
        audio_diag_next = now + 5000000ULL;
        if (u > 0)
            PM_INFO("[AUDIODIAG] underruns: %u (last 5s)\n", (unsigned)u);
    }
}