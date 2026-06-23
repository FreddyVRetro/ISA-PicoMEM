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

extern void* tsfrenderer;

extern void dev_mpu_getbuffer(int32_t *buffer, int samples);
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
     PM_INFO("OPL- 1/1k: %d/%duS CPU %%: %d\n",(int)delta, (int)opl_TotalTime,(int)(opl_TotalTime/12873));  // calculated for a 49KHz mixing
     opl_TotalTime=0;
     opl_bufferCount=0;
   }
}


void pm_audio_domix() {

if (!dev_audiomix.enabled) return;
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
             dev_mpu_getbuffer((int32_t*) buffer, PM_SAMPLES_PER_BUFFER);
             }  // MPU (Various rendering)
#endif

         if (dev_audiomix.dev_active & AD_ADLIB) {
            // Fill int32_t *buffer as int32 for 2x16Bit stereo
            oplperf_start();
            OPL_Pico_stereo_int16((int16_t*) buffer,PM_SAMPLES_PER_BUFFER); // interleave, 16Bit
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

         give_mixed_buffer();
       } else break; // Return directly if no buffer to mix

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
   PM_INFO("dev_audiomix_start\n");

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
  PM_INFO("dev_audiomix_stop\n");
  pm_audio_stop();
  dev_audiomix.dev_active=0;         // No audio device is mixing
 }
}

bool dev_audiomix_isactive()
{
   return dev_audiomix.enabled;   
}