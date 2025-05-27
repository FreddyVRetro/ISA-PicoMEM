/* Copyright (C) 2024 Freddy VETELE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. 
If not, see <https://www.gnu.org/licenses/>.
*/

// Simplified audio library based on the PicoSDK Audio, with I2S

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico.h"   // uint16_t def and so on
#include "../../../pm_debug.h"

#include "audio_i2s.h"
#include "pm_audio.h"


volatile pm_audio_t pm_audio;
bool pm_i2s_active=false;
bool pm_pool_initialized=false;

bool pm_audio_init_i2s(uint32_t sample_freq);
bool pm_audio_stop_i2s();

void pm_audio_freepool()
{ 
  free(pm_audio.bufferpool);
  pm_pool_initialized=false;
// Now we have a deadpool :)     
}

// Initialise the Audio Buffers pool
bool pm_audio_initpool()
{
    PM_INFO("Audio Init : Allocated buffer : %x\n",pm_audio.bufferpool);

    //Allocate all the buffers plus one (Empty buffer)
    pm_audio.bufferpool=(uint8_t *)malloc(PM_AUDIO_POOLSIZE+PM_AUDIO_BUFFERSIZE);
    if (pm_audio.bufferpool==NULL)
    { 
     PM_ERROR("Can't allocate Audio buffer\n");
     pm_pool_initialized=false;
     return false;
    }

    pm_audio.playing_buffer=pm_audio.bufferpool;
    pm_audio.mixing_buffer=pm_audio.bufferpool;  
	  pm_audio.poolend=pm_audio.bufferpool+PM_AUDIO_POOLSIZE;
    pm_audio.mixed_buffers=0;
    
    // Clean all the buffer, including the "Empty" buffer (pm_audio.poolend)
    memset(pm_audio.bufferpool,0,PM_AUDIO_POOLSIZE+PM_AUDIO_BUFFERSIZE);
    pm_pool_initialized=true;
    return true;
}

bool pm_audio_init(uint32_t sample_freq){

 pm_audio_initpool();

 //Provide 2 empty buffers
 take_empty_buffer();
 give_mixed_buffer(); 
 take_empty_buffer();
 give_mixed_buffer();

 pm_audio.enabled=pm_audio_init_i2s(sample_freq);
return pm_audio.enabled;
}

bool pm_audio_stop(){
if (pm_audio.enabled) return pm_audio_stop_i2s();
return true;
}

bool pm_audio_init_i2s(uint32_t sample_freq) {

    struct audio_i2s_config config = {
            .data_pin = PICO_AUDIO_I2S_DATA_PIN,
            .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
//            .dma_channel = 6,                /// No more hardcoded
            .pio_sm = PICO_AUDIO_I2S_SM,     /// Hardcoded !!!
    };
 
 if (!pm_i2s_active)
   {
    PM_INFO("audio_i2s_setup\n");

    if (!audio_i2s_setup(&config)) {
        PM_ERROR("! PicoAudio: Unable to open audio device.\n");
        return false;
     }

    audio_i2s_set_enabled(true);
    pm_i2s_active=true;
   }
  return true;
}

bool pm_audio_stop_i2s() {
 if (pm_i2s_active)
   {
    audio_i2s_set_enabled(false);
    pm_audio_freepool();
    pm_i2s_active=false;
   }
  return true;
}