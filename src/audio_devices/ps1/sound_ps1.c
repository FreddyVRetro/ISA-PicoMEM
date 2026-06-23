// IBM PS/1 Audio card emulation 
// Emulate the FIFO based DAC, the SN76489 is initialized in dev_ps1.cpp
// Code originally from PCEM, Adapted for the PicoMEM by Freddy VETELE
// https://github.com/sarah-walker-pcem/pcem/blob/dev/src/sound/sound_ps1.c


#include <stdio.h>

#include "pm_audio.h"
#include "pico_pic.h"
#include "audio_fifo.h"
#include "covox_dac.h"
#include "../dev_audiomix.h"

#include "../pm_defines.h" // Error code

#include <string.h>  // For memset

typedef struct ps1_audio_t {

        uint8_t status, ctrl;
        uint8_t timer_latch;

        bool timer_active;
        uint16_t timer_interval;  // Delay between timer ticks in microseconds
        uint16_t sample_step;
        uint16_t increment;
        bool     readsample;
        uint8_t  prev_sample8;

        audio_fifo_8_t *afifo;    // Output/Audio FIFO
        uint8_t fifo[2048];       // PS1 Audio FIFO
        uint16_t fifo_read_idx, fifo_write_idx;
        uint16_t fifo_threshold;

        uint8_t dac_val;

        uint16_t pos;
} ps1_audio_t;

ps1_audio_t *ps1=NULL;

static void ps1_update_irq_status() {
        if (((ps1->status & ps1->ctrl) & 0x12) && (ps1->ctrl & 0x01))
            bus_irq_raise(IRQ_R_AUDIO,7,true); // IRQ On, Always IRQ 7
        else
            bus_irq_lower(IRQ_R_AUDIO,7); // IRQ On, Always IRQ 7
}

void ps1_update_timer()
{

   if (ps1->dac_interval == 0)
   {
     PM_INFO("> Stop PS1 timer\n");
     dac_timer_active=false;
     return;
   }
   PM_INFO("> Start/Update PS1 timer\n");
   dac_timer_active=true;
   //Set the covox output rate to 43478 Hz
   ps1->dac_interval = 23;    //Simulate a DAC at 43478 Hz max
   ps1->sample_step  = ((43478 * 65536ul) / PM_AUDIO_FREQUENCY)-5; // Step for the Audio rate convertion
   ps1->increment    = 0;
   ps1->readsample   = true;
   ps1->prev_sample8 = 0x80;
   ps1->ctrlcount    = 0;

   if (!ps1->timer_active)
      {
        PIC_AddEvent(&CVX_DAC_Timer_Event, cvx.dac_interval, 0);
        break;
      }
}

static uint8_t ps1_audio_read(uint16_t port) {
        uint8_t temp;

        switch (port & 7) {
        case 0: /*ADC data*/
                ps1->status &= ~0x10;
                ps1_update_irq_status(ps1);
                return 0;
        case 2: /*Status*/
                temp = ps1->status;
                temp |= (ps1->ctrl & 0x01);
                if ((ps1->fifo_write_idx - ps1->fifo_read_idx) >= 2048)
                        temp |= 0x08; /*FIFO full*/
                if (ps1->fifo_read_idx == ps1->fifo_write_idx)
                        temp |= 0x04; /*FIFO empty*/
                                      //                pclog("Return status %02x\n", temp);
                return temp;
        case 3: /*FIFO timer*/
                /*PS/1 technical reference says this should return the current value,
                  but the PS/1 BIOS and Stunt Island expect it not to change*/
                return ps1->timer_latch;
        case 4:
        case 5:
        case 6:
        case 7:
                return 0;
        }
        return 0xff;
}

static void ps1_audio_write(uint16_t port, uint8_t val) {

        switch (port & 7) {
        case 0: /*DAC output*/
                if ((ps1->fifo_write_idx - ps1->fifo_read_idx) < 2048) {
                        ps1->fifo[ps1->fifo_write_idx & 2047] = val;
                        ps1->fifo_write_idx++;
                }
                break;
        case 2: /*Control*/
                ps1->ctrl = val;
                if (!(val & 0x02))
                        ps1->status &= ~0x02;
                ps1_update_irq_status(ps1);
                break;
        case 3: /*Timer reload value*/
                ps1->timer_latch = val;
                if (val-1>0xF0) ps1->timer_interval=0xFF-val;
                   else ps1->timer_interval=0;
                ps1_update_timer(); // Start or Stop the timer
                PM_INFO("PS1 Timer interval %d us\n", ps1->timer_interval);
                break;
        case 4: /*Almost empty*/
                ps1->fifo_threshold = val * 4;
                PM_INFO("PS1 FIFO threshold: %d\n", ps1->fifo_threshold);
                break;
        }
}

void ps1_audio_update(ps1_audio_t *ps1) {
        for (; ps1->pos < sound_pos_global; ps1->pos++)
                ps1->buffer[ps1->pos] = (int8_t)(ps1->dac_val ^ 0x80) * 0x20;
}

// Timer event to add the samples to the audio FIFO
static uint32_t PS1_Timer_EventHandler(Bitu val) {

  if (ps1->fifo_read_idx != ps1->fifo_write_idx) {
        ps1->dac_val = ps1->fifo[ps1->fifo_read_idx & 2047];
        ps1->fifo_read_idx++;
      }
  if ((ps1->fifo_write_idx - ps1->fifo_read_idx) == ps1->fifo_threshold) {
        PM_INFO("PS1 FIFO almost empty\n");
        ps1->status |= 0x02; /*FIFO almost empty > Send IRQ*/
      }
        ps1->status |= 0x10; /*ADC data ready*/
        ps1_update_irq_status(ps1);
        afifo_add_sample8(ps1->fifo, ps1->sample);
  }
 return ps1->dac_interval;
}

uint8_t ps1_audio_init() {
  if (ps1 != NULL) return 0; // Already initialized

  ps1 = (ps1_audio_t *)malloc(sizeof(ps1_audio_t));
  if (ps1 == NULL) return CMDERR_MEM_ERR;  
  memset(ps1, 0, sizeof(ps1_audio_t));

  cvx.fifo=(audio_fifo_8_t*) malloc(sizeof(audio_fifo_8_t));
  if (cvx.fifo == NULL) 
        {
         return_code=CMDERR_MEM_ERR;
         free(ps1);
         return CMDERR_MEM_ERR;  // Not enaugh memory
        }
  afifo_init8(cvx.fifo);
  return 0; // Success
}

void ps1_audio_close() {
  if (ps1 != NULL) {
      free(ps1);
      ps1 = NULL;
    }
}