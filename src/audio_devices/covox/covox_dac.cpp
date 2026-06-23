// Covox based devices emulation (covox / disney sound source)
// https://nerdlypleasures.blogspot.com/2014/09/the-mysterious-covox-pc-sound-devices.html
// https://en.wikipedia.org/wiki/Covox_Speech_Thing
// https://github.com/mcgurk/Covox
// By Freddy VETELE

// BasePort   : Data Port
// BasePort+1 : Status Port  (Stereo in One detection, DSS FIFO full)
// BasePort+2 : Control Port (To select Left/Right for Stereo in One)

#include <stdio.h>

#include "pm_audio.h"
#include "pico_pic.h"
#include "audio_fifo.h"
#include "covox_dac.h"
#include "../dev_audiomix.h"

#include "../pm_defines.h" // Error code

#include <string.h>  // For memset

cvx_t cvx;

bool dac_timer_active; // DAC timer is active or not ?

void cvx_stop_dac();
static uint32_t CVX_DAC_Timer_EventHandler(Bitu val);
static PIC_TimerEvent CVX_DAC_Timer_Event = {
    .handler = CVX_DAC_Timer_EventHandler,
};

// init to ensure everything is cleared/initialized correctly at the startup
void cvx_init()
{
 cvx.type=CVX_TYPE_NONE;
 dac_timer_active=false;
 cvx_stop_dac();
}

uint8_t cvx_enable(uint8_t type, uint16_t baseport, uint16_t baseport2)
{
 PM_INFO("cvx_enable ");

 if (cvx.type==CVX_TYPE_NONE) {
  switch(type) {
    case CVX_TYPE_DAC_STEREO:
    case CVX_TYPE_DAC_DUAL:
    case CVX_TYPE_DAC:
        PM_INFO("DAC p:%x\n",baseport);    
      break;

    case CVX_TYPE_DISNEY:
       // Initialize the Disney Sound Source FIFO
        PM_INFO("Disney p:%x\n",baseport);
        cvx.dss_fifo_head = 0;
        cvx.dss_fifo_tail = 0;
      break; // Valid types
    default:
        return false; // Invalid type
    }
  // Initialize the Covox/Disney Audio fifo
  if (cvx.type==CVX_TYPE_DAC_STEREO)
    {
     cvx.fifo_stereo= (audio_fifo_8s_t*) malloc(sizeof(audio_fifo_8s_t));
     if (cvx.fifo_stereo == NULL) return CMDERR_MEM_ERR;  // Not enaugh memory
     afifo_init8s(cvx.fifo_stereo);
     cvx.sampleleft = 0x80;
    } 
    else
    {
     cvx.fifo=(audio_fifo_8_t*) malloc(sizeof(audio_fifo_8_t));
     if (cvx.fifo == NULL) return CMDERR_MEM_ERR;  // Not enaugh memory
     afifo_init8(cvx.fifo);
    }
  cvx.sample = 0x80;
  cvx.isleft = true; // Default to left channel for stereo DAC   
  cvx.type   = type; // Set the type of the Covox DAC
  } 
return 0;
}

void cvx_disable()
{
 if (cvx.type!=CVX_TYPE_NONE)
   {
    cvx_stop_dac();
    if (cvx.fifo != NULL) {
        free(cvx.fifo);
        cvx.fifo = NULL;
       }
    if (cvx.fifo_stereo != NULL) {
        free(cvx.fifo_stereo);
        cvx.fifo_stereo = NULL;
       }       
   }
 cvx.type = CVX_TYPE_NONE; // Reset the type
 cvx.dac_interval = 0; // Stop the DAC timer if running
}

void cvx_start_dac()
{
if (!dac_timer_active) {

  switch(cvx.type) {
    case CVX_TYPE_DAC: // Covox DAC
    case CVX_TYPE_DAC_STEREO: // Covox DAC Stereo in one  
    case CVX_TYPE_DAC_DUAL:   // Covox DAC Dual (LPT1+LPT2)
        PM_INFO("> Start Covox DAC\n");
        dac_timer_active=true;
        //Set the covox output rate to 43478 Hz
        cvx.dac_interval = 23;    //Simulate a DAC at 43478 Hz max
        cvx.sample_step  = ((43478 * 65536ul) / PM_AUDIO_FREQUENCY)-5; // Step for the Audio rate convertion
        cvx.increment    = 0;
        cvx.readsample   = true;
        cvx.prev_sample8 = 0x80;
        cvx.ctrlcount    = 0;
        PIC_AddEvent(&CVX_DAC_Timer_Event, cvx.dac_interval, 0);
        break;
    case CVX_TYPE_DISNEY: // Disney Sound Source
        dac_timer_active=true;
        //Set the disney Sound Source output rate to 6993 Hz
        cvx.dac_interval = 143;    //Simulate a DAC at 6993 Hz max
        cvx.sample_step  = (6993 * 65536ul) / PM_AUDIO_FREQUENCY; // Step for the Audio rate convertion
        cvx.increment    = 0;
        cvx.readsample   = true;
        cvx.prev_sample8 = 0x80;
        PIC_AddEvent(&CVX_DAC_Timer_Event, cvx.dac_interval, 0);
        break;
    }
  }

}

// DAC to be stopped after 5s without new data
void cvx_stop_dac()
{
 if (dac_timer_active) {
      PM_INFO("> Stop Covox DAC\n");
      dac_timer_active=false;     // DAC is stopped
      cvx.dac_interval = 0; // Stop the DAC timer if running
      PIC_RemoveEvent(&CVX_DAC_Timer_Event); // Remove the DAC timer event
   }
}

// Timer event to add the samples to the audio FIFO
static uint32_t CVX_DAC_Timer_EventHandler(Bitu val) {
 if (dev_audiomix.dev_active & AD_CVX)
  {
   switch(cvx.type) {
    case CVX_TYPE_DAC_STEREO: // Covox DAC Stereo in one 
    case CVX_TYPE_DAC_DUAL: // Covox DAC Dual (LPT1+LPT2)
         afifo_add_sample8s(cvx.fifo_stereo, cvx.sample, cvx.sampleleft);   // Add stereo sample
    case CVX_TYPE_DAC: // Covox DAC
 DBG_ON_1();
         if (!afifo_add_sample8(cvx.fifo, cvx.sample)) //PM_INFO("O");     
 DBG_OFF_1();
         break;
    case CVX_TYPE_DISNEY: // Disney Sound Source
         if (cvx.dss_fifo_head != cvx.dss_fifo_tail)
               {
                cvx.sample=cvx.dss_fifo[cvx.dss_fifo_tail];
                cvx.dss_fifo_tail=(cvx.dss_fifo_tail + 1) & 0x0F; // Wrap around
               }
         afifo_add_sample8(cvx.fifo, cvx.sample);
    break;
    } // switch(cvx.type)
  }
 return cvx.dac_interval;
}

// Status port :
// DSS : Bit 6 : 1 if FIFO is full (Can't receive data)
bool cvx_inp(uint16_t address, uint8_t *data)
{
  uint8_t retval;

  if (cvx.type==CVX_TYPE_NONE) return false;

  switch(address & 0x3) {
    case 0: 
           *data=cvx.sample;           
           return true;
	  case 1:	// Status Port
        switch(cvx.type) {
          case CVX_TYPE_DISNEY:     // Disney Sound Source
		                    retval = 0x00; //0x07; //0x40; // Stereo-on-1 and (or) New-Stereo DACs present
                        if (((cvx.dss_fifo_head + 1) & 0x0F )== cvx.dss_fifo_tail) retval |= 0x40u; // FIFO full
                        PM_INFO("DSS Status: %X \n",retval);
                        *data=retval;
                        return true;
          case CVX_TYPE_DAC_DUAL:   // Covox DAC Dual (LPT1+LPT2)
		                    retval = 0x07; //0x40; // Stereo-on-1 and (or) New-Stereo DACs present
                        *data=retval;
                        return true;          
         }
        break;
     case 2: // Return the last written control value
           *data=cvx.control;
           return true;        
    }

 return false;
}

void cvx_outp(uint16_t address, uint8_t data)
{
 // PM_INFO("Type %d, data:%x",cvx.type,data);
 if ((address & 0x3)==2) { // Control port
    cvx.control=data;
   }

 switch(cvx.type) {
    case CVX_TYPE_NONE:
        return;    // No Covox DAC active
    case CVX_TYPE_DAC: // Covox Single DAC
        if ((address&0x03)==0) {
            cvx.sample=data;
           }
        break;
    case CVX_TYPE_DAC_STEREO: // Covox DAC Stereo in one
          switch(address & 0x03) {
             case 0:  // Data Port
                  if (cvx.ctrlcount<10)
                      { 
                      //cvx.ctrlcount++;
                      if (cvx.channel) cvx.sampleleft=data;
                       else cvx.sample=data;
                      } 
                     else 
                      { // Switch to Mono
                           cvx.sample=data;
                           cvx.sampleleft=data;
                      }
                     
                break;
             case 2:  // Control Port
                    cvx.ctrlcount=0;
                    cvx.channel = data & 0x01;                
                break;
            }
        break;
    case CVX_TYPE_DAC_DUAL:   // Covox DAC Dual (LPT1+LPT2)
        if ((address&0x03)==0) {  // Data Port
            cvx.sample=data;
           }
        break;
    case CVX_TYPE_DISNEY:     // Disney Sound Source
        if ((address&0x03)==0) {
            uint8_t next_head = (cvx.dss_fifo_head + 1) & 0x0F;
            if (next_head == cvx.dss_fifo_tail) return ;          // Ignore if the FIFO is full
            cvx.dss_fifo[cvx.dss_fifo_head] = data;
            cvx.dss_fifo_head = (cvx.dss_fifo_head + 1) & 0x0F;   // Wrap around
           }
       break;
    }
}


// Update the audio buffer (16bit Stereo) by reading from the DMA buffer
void cvx_updatebuffer(int16_t* buff,uint32_t samples)
{
  int8_t   res8ur, res8ul;
  uint16_t prev_increment;
  int16_t  fresr, fresl;

  if (cvx.type==CVX_TYPE_DAC_DUAL)
   {  // Get Stereo data
    res8ur=cvx.prev_sample8;
    res8ul=cvx.prev_sample8left;
    for (uint32_t sn=0;sn<(samples*2);sn+=2)  // Stereo, so 2x more data than "samples"
      {
       // 8Bit Not signed
        if (cvx.readsample) 
             if (!afifo_take_sample8s(cvx.fifo_stereo, &res8ur, &res8ul)) {
                 cvx.prev_sample8=0x80;  // If the FIFO is empty, return silence
                 cvx.prev_sample8left=0x80;  // If the FIFO is empty, return silence
                return;
              }
        fresr=(int16_t) ((res8ur<<8)+0x8000)/4;  // Convert to 16Bit signed
        fresl=(int16_t) ((res8ul<<8)+0x8000)/4;  // Convert to 16Bit signed        
        prev_increment=cvx.increment;
        cvx.increment+=cvx.sample_step;    // Adjust the DMA buffer to the Output frequency 
        cvx.readsample = (cvx.increment<=prev_increment) ? true : false;
        if ((sn & 0x02)==0) // Right channel
            buff[sn]   += fresr;
         else              // Left channel
            buff[sn+1] += fresl;
      }
    cvx.prev_sample8=res8ur;      // Save the last Sample for the next time
    cvx.prev_sample8left=res8ul;  // Save the last Sample for the next time

   }
   else 
   {  // Get Mono data
    res8ur=cvx.prev_sample8;
    for (uint32_t sn=0;sn<(samples*2);sn+=2)  // Stereo, so 2x more data than "samples"
      {
       // 8Bit Not signed
        if (cvx.readsample) 
             if (!afifo_take_sample8(cvx.fifo, &res8ur)) {
                 cvx.prev_sample8=0x80;  // If the FIFO is empty, return silence
                return;
              }
        fresr=(int16_t) ((res8ur<<8)+0x8000)/4;  // Convert to 16Bit signed
        prev_increment=cvx.increment;
        cvx.increment+=cvx.sample_step;    // Adjust the DMA buffer to the Output frequency 
        cvx.readsample = (cvx.increment<=prev_increment) ? true : false;
        buff[sn]   += fresr;
        buff[sn+1] += fresr;
      }
    cvx.prev_sample8=res8ur;  // Save the last Sample for the next time   
   }
}


void cvx_test()
{
  int16_t buff[2048*2];

  cvx.dac_interval = 23;    //Simulate a DAC at 43478 Hz max
  cvx.sample_step  = (43478 * 65536ul) / PM_AUDIO_FREQUENCY; // Step for the Audio rate convertion
  cvx.increment    = 0;
  cvx.readsample   = true;
  cvx.prev_sample8 = 0x80;
  cvx.prev_sample8left = 0x80;  

/*
  PM_INFO("TakeSample:\n");

  int8_t res8u;
  for (int i = 0; i < 2048; i++) {
      if (afifo_take_sample8(cvx.fifo, &res8u))
      PM_INFO("%d,",res8u);
  }
*/
  PM_INFO("UpdateBuffer\n");

  memset(buff,0,2048*2);
  cvx_updatebuffer(buff,2048);

  PM_INFO("step:%d\n",cvx.sample_step);

  PM_INFO("Buffer OUT:\n");
  
  for (int i = 0; i < 2048; i+=2) PM_INFO("%d,",buff[i]);

}