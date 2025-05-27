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

/* pm_mindscape.cpp : PicoMEM mindscape music board emulation

*/ 

#if USE_AUDIO
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "pm_audio.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType

#include "dev_audiomix.h"
#include "../audio_devices/emu2149/emu2149.h"

#define MINDSCAPE_CLK 4772726/2

static PSG* psg0 = NULL;
static PSG* psg1 = NULL;
static uint audioSlice = 0;
static uint8_t psg0Reg = 0;
static uint8_t psg1Reg = 0;

uint8_t dev_psg_freq_low;
uint8_t dev_psg_freq_med;
bool dev_mmb_active=false;    // True if configured
uint8_t dev_mmb_port;
volatile uint8_t dev_mmb_delay=0;      // counter for the Nb of second since last I/O
uint64_t dev_mmb_lastaccess;  // Last access time (For Audo mute)


static PSG* createPSG(int psgClock, int sampleRate)
{
  PSG* psg = PSG_new(psgClock, sampleRate);
  PSG_setVolumeMode(psg, EMU2149_VOL_AY_3_8910);  // AY-3-8910 mode
  PSG_reset(psg);
  return psg;
}

uint8_t dev_mmb_install(uint16_t baseport)
{
 if (!dev_mmb_active)   // Don't re enable if active
  {
   PM_INFO("Install Mindscape Music Board (%x)\n",baseport);

   psg0 = createPSG(MINDSCAPE_CLK, PM_AUDIO_FREQUENCY);
   psg1 = createPSG(MINDSCAPE_CLK, PM_AUDIO_FREQUENCY);

   SetPortType(baseport,DEV_MMB,1);   
   
   dev_mmb_port=baseport;
   dev_mmb_active=true;
   dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_MMB;
  }
  return 0;
}

void dev_mmb_remove()
{
 if (dev_mmb_active)       // Don't stop if not active
  {
   dev_mmb_active=false;
   dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_MMB;
   PM_INFO("Remove Mindscape Music Board (0x300\n");
  
//   OPL_Pico_Shutdown();
   SetPortType(dev_mmb_port,DEV_NULL,1);
  }
}

// Return true if Adlib is installed
bool dev_mmb_installed()
{
  return dev_mmb_active;
}

// Started in the Main Command Wait Loop
void dev_mmb_update()
{
}

bool dev_mmb_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
  switch (CTRL_AL8&0x07) 
   {
    case 1: *Data=PSG_readReg(psg0, psg0Reg); 
     return true;
     break; 
    case 3: *Data=PSG_readReg(psg1, psg1Reg);
     return true;
     break;          
   }
  *Data=0xFF;
  return false;
}

void dev_mmb_iow(uint32_t CTRL_AL8,uint8_t Data)
{
;  PM_INFO("W%x %x ",CTRL_AL8,Data);
  switch (CTRL_AL8&0x07) 
  { 
   case 0: psg0Reg = Data;
     return;
     break;
   case 1: if (psg0Reg>15) 
             {  // PicoMEM > New register to update the PSG Frequency
              if (psg0Reg==16) dev_psg_freq_low=Data;
              if (psg0Reg==17) dev_psg_freq_med=Data;
              if (psg0Reg==18)
                 {
                  uint32_t NewFreq= (uint32_t)Data<<16+(uint32_t)dev_psg_freq_med<<8+dev_psg_freq_low;
                  PM_INFO("PSG New Freq: %d",NewFreq);
                  PSG_setClock(psg0, NewFreq);
                  PSG_setClock(psg1, NewFreq);
                 }
              return;
             }
           PSG_writeReg(psg0, psg0Reg, Data); 
           dev_audiomix.dev_active = dev_audiomix.dev_active | AD_MMB;
       //    PM_INFO("WR0:%x,%x ",psg0Reg, Data);
     return;
     break; 
   case 2: psg1Reg = Data;
     return;
     break;
   case 3: PSG_writeReg(psg1, psg1Reg, Data);
           dev_audiomix.dev_active = dev_audiomix.dev_active | AD_MMB;
       //    PM_INFO("WR1:%x,%x ",psg0Reg, Data);
     return;
     break;          
  }
    
}

void dev_psg_getbuffer(int32_t* buff,uint8_t samples)
{
 int32_t fres;

 for (int8_t sn=0;sn<samples;sn++)
  {
   // generate audio data
   fres = ((int32_t) PSG_calc(psg0) + (int32_t) PSG_calc(psg1))/2 ;
  // fres+= PSG_calc(psg1);
//   PM_INFO("%d,%d %d",psg1->ch_out[0],psg1->ch_out[1],psg1->ch_out[2]);
//   PM_INFO("1:%d,%d,%d",psg0->ch_out[0],psg0->ch_out[1],psg0->ch_out[2]);

 //  fres=((uint32_t) psg0->ch_out[0] + (uint32_t) psg0->ch_out[1] + (uint32_t) psg0->ch_out[2] + 
 //            (uint32_t) psg1->ch_out[0] + (uint32_t) psg1->ch_out[1] + (uint32_t) psg1->ch_out[2]);
 //  fres=fres/4;
   buff[sn] += (fres<<16 | (uint16_t) fres);
  }
}

#endif
