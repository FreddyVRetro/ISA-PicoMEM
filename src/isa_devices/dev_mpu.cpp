/* Copyright (C) 2026 Freddy VETELE

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

/* dev_mpu.cpp : PicoMEM simple MPU (Midi Processor Unit) implementation
To test : Doom, Doom2, Hexen, Duke nukem 3D, Raptor, Descent, Gods, Dune 2, Sim City 2000, Ultima 7, Ultima 8, Wing Commander 1/2/3, 
          Alone in the Dark, Lemmings, Warcraft 1/2, 
          X-COM UFO Defense, X-COM Terror from the Deep, 
          Civilization 1/2, Star Control 2, Daggerfall, 
          Eye of the Beholder 1/2/3, Dark Sun Shattered Lands, 
          Betrayal at Krondor, Anvil of Dawn, 
          Might and Magic 3/4/5, 
          Heroes of Might and Magic 1/2, 
          Lands of Lore 1/2, 
          The Elder Scrolls: Arena/Daggerfall, 
          Fallout 1/2, 
          System Shock 1/2, 
          Planescape Torment, Baldur's Gate 1/2,
          Icewind Dale 1/2,
          Arcanum,
          Messiah,
          Freespace 1/2,
          Descent Freespace 3

https://www.midimusicadventures.com/queststudios/mt32-resource/utilities/
https://drive.google.com/drive/folders/1TbLOvr6GwqrxteWH8RD7Z7Q7t8q-vsIe

 */  

#include <stdlib.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "dev_picomem_io.h"
#include "dev_audiomix.h"
#include "pm_audio.h"
#if PSRAM_HEAP
#include "pico_psram.h"    // Add QSPI PSRAM Code
#endif

#if USE_USBHOST
#include "hid_dev.h"
//uint32_t tuh_midi_stream_flush( uint8_t dev_addr);
//#include "usb_midi_host.h"
#include "tusb.h"
#endif

#include <string.h>

#define TSF_MALLOC pmtsf_malloc
#define TSF_FREE pmtsf_free
#if PSRAM_HEAP
#define TSF_REALLOC pmtsf_realloc
#else
#define TSF_REALLOC realloc
#endif

static void* pmtsf_malloc(size_t size)
{
#if PSRAM_HEAP
  PM_INFO("tsfmalloc(psram):%d ",size);
  return ps_malloc(size); 
#else
  PM_INFO("tsfmalloc:%d ",size);
  return malloc(size);
#endif
}

#if PSRAM_HEAP
static void* pmtsf_realloc(void *ptr, size_t size)
{
  PM_INFO("pmtsf_realloc !!");
	void* new_data = NULL;
	if(size)
	{
		if(!ptr)
		{
			return ps_malloc(size);
		}

		new_data = ps_malloc(size);
		if(new_data)
		{
			memcpy(new_data, ptr, size); // TODO: unsafe copy...
			ps_free(ptr); // we always move the data. free.
		}
	}

	return new_data;
}
#endif

static void  pmtsf_free(void* ptr)
{
  PM_INFO("tsffree:%p ",ptr);
  #if PSRAM_HEAP
  if (ptr!=NULL) ps_free(ptr); 
#else
  free(ptr);
#endif
}

#define USE_2X
#define TSF_SAMPLES_SHORT
#define TSF_SHORT_DIRECTSAMPLES

#define PROGMEM
#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
//#include "midi/tsfvsb/TSF.H"
//#include "midi/tsfesp/TSF.H"
#include "midi/tsfesp/tsfnew.h"
#include "midi/tsfesp/pm_tsf.h"

//#include "midi/CT2MGM.h"
//#include "midi/1mgm.h"

//#include "midi/5MBGMGS.h"
//#include "midi/CT8MGM.h"
#if SF2_GMGSx
#include "midi/GMGSx.h"
#else
#include "midi/4MBGMGSMT.h"
#endif
//#include "midi/TimGM6mb.h"


#define OUTPUT_SAMPLERATE   49716ul 

tsf* tsfrenderer = NULL;

/* 0x330: data port
 * 0x331: read: status port
 *       write: command port
 * status port:
 * bit 6: 0=ready to write cmd or MIDI data; 1=interface busy
 * bit 7: 0=data ready to read; 1=no data at data port
 * command port:
 *  0xff: reset - triggers ACK (FE) to be read from data port
 *  0x3f: set to UART mode - triggers ACK (FE) to be read from data port
 */

static bool bReset = false;
static uint8_t bUART = 0;

static unsigned char midi_status_byte = 0x80;
static unsigned char midi_mpu_status  = 0x80;
static unsigned char midi_msg_len;
static unsigned char midi_msg_cnt;
static unsigned char midi_msg_tx_cache[3];      // Store the midi message being sent to the mpu (message are 3 bytes long max)
static unsigned char midi_msg_tx_len;
static unsigned char midi_msg_tx_cnt;
static bool midi_in_message    = false;
static bool midi_in_message_tx = false;
static bool midi_in_sysex      = false;

static const int midi_lengths[8] = { 2, 2, 2, 2, 1, 1, 2, 0 };

bool dev_mpu_active = false;
volatile uint8_t dev_mpu_delay=0;      // counter for the Nb of second since last I/O

/*
uint16_t dev_mpu_sample_rate;
uint16_t dev_mpu_sample_step;
uint16_t dev_mpu_increment;
bool     dev_mpu_readsample;
int16_t  dev_mpu_prev_sample16_l;
int16_t  dev_mpu_prev_sample16_r;
*/

int16_t TmpBuffer32 [PM_SAMPLES_PER_BUFFER*2*2];       // 64 samples, 2 channels, 32 bits


// Tiny 256 bytes FIFO
typedef struct mpu_fifo_t {
    uint8_t data[256];
    volatile uint8_t head;
    volatile uint8_t tail;
} mpu_fifo_t;

mpu_fifo_t *mpu_fifo=NULL;
#define MIDI_FIFO_SIZE 255

uint8_t mpu_fifo_init()
{
 if (!mpu_fifo)
  { 
    mpu_fifo=(mpu_fifo_t *) calloc(1,sizeof(mpu_fifo_t));
    if (mpu_fifo == NULL) return CMDERR_MEM_ERR;  // Not enaugh memory
  }
 return 0;
}

void mpu_fifo_end()
{
 if (mpu_fifo)
  { 
    free(mpu_fifo);
    mpu_fifo=NULL;
  }
}

void mpu_fifo_clear()
{
  if (mpu_fifo)
   {
     mpu_fifo->tail=0;
     mpu_fifo->head=0;
   }
}

uint8_t mpu_fifo_level()
{
 return mpu_fifo->head-mpu_fifo->tail;
}

void mpu_fifo_tx(uint8_t Data)
{
 //if (mpu_fifo_level()==MIDI_FIFO_SIZE) 
 mpu_fifo->data[mpu_fifo->head]=Data;
 mpu_fifo->head++;
}

// Must be sure the fifo is not empty before
bool mpu_fifo_rx(uint8_t *Data)
{
 uint8_t res;
 if (mpu_fifo->head==mpu_fifo->tail) return false;  // Empty !
 *Data=mpu_fifo->data[mpu_fifo->tail];
 mpu_fifo->tail++;
 return true;
}

uint8_t pm_tsf_renderer_init()
{
PM_INFO("pm_tsf_renderer_init\n");
  if (!tsfrenderer) {  // Not load again if already loaded
//  PM_INFO("Load 4MBGMGSMT sound font\n");
//if ( tsfrenderer = tsf_load_memory(&CTMGM_SF2,sizeof(CTMGM_SF2)) ) {
//if ( tsfrenderer = tsf_load_memory(&_1MGM_SF2,sizeof(_1MGM_SF2)) ) {
//if ( tsfrenderer = tsf_load_memory(&TimGM6mb_SF2,sizeof(TimGM6mb_SF2)) ) {
//if ( tsfrenderer = tsf_load_memory(&_5MBGMGS_SF2,sizeof(_5MBGMGS_SF2)) ) {
#if SF2_GMGSx
  PM_INFO("Load GMGSx sound font\n");
  if ( tsfrenderer = tsf_load_memory(&GMGSx_SF2,sizeof(GMGSx_SF2)) ) {    
#else
  PM_INFO("Load 4MBGMGSMT sound font\n");
  if ( tsfrenderer = tsf_load_memory(&_4MBGMGSMT_SF2,sizeof(_4MBGMGSMT_SF2)) ) {
#endif  
  PM_INFO("TSF: soundfont [presets=%d]\n",tsf_get_presetcount(tsfrenderer) );
  for (int i = 0; i < tsf_get_presetcount(tsfrenderer); i++)
	     {
		    PM_INFO("Preset #%d '%s'\n",i, tsf_get_presetname(tsfrenderer, i));
	     }
  tsf_set_output(tsfrenderer, TSF_STEREO_INTERLEAVED, OUTPUT_SAMPLERATE, -3.0f);
  tsf_set_max_voices(tsfrenderer, 32);
  int channel = 0;
  for (channel = 0; channel < 16; channel++)
        tsf_channel_midi_control(tsfrenderer, channel, 121, 0); // 121 = reset controller
    }
     else
    {
      PM_INFO("TSF: soundfont load FAILED\n");
      return CMDERR_MEM_ERR;  // Not enaugh memory
    };
  } // if tsfrenderer
  else PM_INFO("already initialized\n");
 return 0;
}

uint8_t dev_mpu_install(uint16_t port)
{
 uint8_t retcode;

 if (dev_mpu_active) {
    PM_INFO("MPU already active\n");
    return 0; // Already active
  }

// 1 - Load the soundfont in memory

 retcode=pm_tsf_renderer_init();
 if (retcode) {
    PM_INFO("TSF renderer init Failed\n");
    return retcode;
   }

// 2 - Install the device

 PM_INFO("Install MPU Device on Port %x\n",port);
 bReset = false;
 midi_in_sysex = false;
 retcode=mpu_fifo_init();  // Return 0 if the fifo can be allocated
 if (retcode) {
    PM_INFO("MPU FIFO Init Failed\n");
    return retcode;
   }

 SetPortType(port,DEV_MPU,1);
 dev_mpu_active=true;
 return 0;
}

void dev_mpu_remove()
{
 PM_INFO("Remove MPU Device\n");        
 if (dev_mpu_active)  {
   DelPortType(DEV_MPU);
   mpu_fifo_end();  // Unallocate the mpu fifo 
   if (tsfrenderer) {
      tsf_close( tsfrenderer );
      tsfrenderer=NULL;
     }
  }
}

void dev_mpu_process_messages()
{
  uint8_t Data;
  uint8_t cmd_ch;

// if (mpu_fifo_level()) PM_INFO("-PROCESS: h:%d t:%d LVL:%d\n",mpu_fifo->head, mpu_fifo->tail,mpu_fifo_level());

 while (mpu_fifo_rx(&Data)) {
  //  PM_INFO("D%X ",Data);
    if ((Data & 0xF0) >= 0x80) {  // The value is a message Start
        midi_msg_tx_len=midi_lengths[(Data>>4)-8];  // Read the Nb of parameters for this command
        midi_in_message_tx = true;
        midi_msg_tx_cnt = 0;
       } 

    if (midi_in_message_tx) {
        midi_msg_tx_cache[midi_msg_tx_cnt]=Data;  // Store the Data in the message buffer
        if (midi_msg_tx_len==midi_msg_tx_cnt) {   // Execute the message/Command when all the parameters are read
           cmd_ch=midi_msg_tx_cache[0];
/*
           if (pm_midi_usb.mounted)
              {
                tuh_midi_stream_write(pm_midi_usb.dev_idx, 0, &midi_msg_tx_cache[0], 1 + midi_msg_tx_len);
              } else
*/               
              {
              switch (cmd_ch & 0xF0) {
               case 0xD0:
               //  PM_INFO("Channel Pressure %d\n", midi_msg_tx_cache[1]);
                 tsf_channel_set_pressure(tsfrenderer, cmd_ch & 0x0F, midi_msg_tx_cache[1] / 127.f);
                 break;                
               case 0xA0:
              //   PM_INFO("Polyphonic Key Pressure (NS) %x %x\n", midi_msg_tx_cache[1],midi_msg_tx_cache[2]);
                break;
               case 0x80:
               //  PM_INFO("Note Off %x\n", midi_msg_tx_cache[1]);
                 tsf_channel_note_off(tsfrenderer, cmd_ch & 0x0F, midi_msg_tx_cache[1]);
                break;
               case 0x90:
              //   PM_INFO("Note On %x %x\n", midi_msg_tx_cache[1],midi_msg_tx_cache[2]);                 
                 tsf_channel_note_on(tsfrenderer, cmd_ch & 0x0F, midi_msg_tx_cache[1], midi_msg_tx_cache[2] / 127.0f);
                break;
               case 0xE0:
               //  PM_INFO("Pitch Wheel %x %x\n", midi_msg_tx_cache[1],midi_msg_tx_cache[2]);
                 tsf_channel_set_pitchwheel(tsfrenderer, cmd_ch & 0x0F, (midi_msg_tx_cache[1] & 0x7F) | ((midi_msg_tx_cache[2] & 0x7F) << 7));
                break;
               case 0xC0:
              //   PM_INFO("Program Change ch %d %x\n", cmd_ch & 0x0F, midi_msg_tx_cache[1]);
                 tsf_channel_set_presetnumber(tsfrenderer, cmd_ch & 0x0F, midi_msg_tx_cache[1], (cmd_ch & 0x0F) == 0x9);
                break;
               case 0xB0:       
              //   PM_INFO("Control Change ch %d %x %x\n", cmd_ch & 0x0F, midi_msg_tx_cache[1],midi_msg_tx_cache[2]);
                 tsf_channel_midi_control(tsfrenderer, cmd_ch & 0x0F, midi_msg_tx_cache[1], midi_msg_tx_cache[2]);
                break;
             }
            }

            midi_in_message_tx = false;
            }                         
           midi_msg_tx_cnt++;
          }
  }
/*
 if (pm_midi_usb.mounted) 
    { 
      PM_INFO("USB Flush");
     // tuh_midih_get_num_tx_cables(pm_midi_usb.dev_idx);
      tuh_midi_write_flush(pm_midi_usb.dev_idx);
    }
*/
}

void dev_mpu_update()
{
  if (mpu_fifo) dev_mpu_process_messages();
}

extern bool dev_mpu_ior(uint32_t Addr,uint8_t *Data)
{
//  PM_INFO("MPU R(%X) ", Addr );
  if ( Addr == 0x330 ) {
        midi_mpu_status |= 0x80;
        if ( bReset ) {
                PM_INFO("CLR Rst ");                
                bReset = false;
                *Data=0xfe;
                return true;
            } else {
              *Data=0xfe; // Always return Active Sensing.
              PM_INFO("%X ", *Data );
              return true;              
            } 
      } else {
         *Data=midi_mpu_status | (mpu_fifo_level()>=(MIDI_FIFO_SIZE-4) ? 0x40 : 0);
   //      PM_INFO("s%X ", *Data );
         return true;
        }
  return false; // nothing to return        
}

void dev_mpu_iow(uint32_t Addr,uint8_t Data)
{
//   PM_INFO("MPU W(%X)=%X ", Addr, Data );
   if ( Addr == 0x331 ) {        // 0x331 : Command port
        if ( Data == 0x3f ) {   // 0x3F  : After RESET, Set UART mode
        	midi_mpu_status &= ~0x80;
               }
        if ( Data == 0xff ) {   // 0xFF  : Reset command
              //  dev_mpu_process_messages();
                PM_INFO("SET Rst ");
                bReset = true;   // Remains in Reset mode until status is read;
                midi_in_sysex = false;
                midi_in_message = false;
                mpu_fifo_clear();
                midi_mpu_status &= ~0x80;
               }
      } else if ( Addr == 0x330 ) { // 0x330 : Data Port
        if (!bReset) {
            dev_audiomix.dev_active = dev_audiomix.dev_active | AD_MPU;   // enable the mixing
            dev_mpu_delay=0;
                 // PM_INFO("Data %d ",Data);
                  if (mpu_fifo_level()==MIDI_FIFO_SIZE) {  
                        PM_ERROR("MPU:Fifo Full\n");
                        return;  // Should never happens as the host code should check the status first
                     }

                  if (midi_in_sysex) {  // PicoMEM : Ignore sysex
                        PM_INFO("SYSEX\n");
                          // midi_buffer[midi_ptr++] = value;
                          if (Data == 0xF7) {
                          //      midi_available_ptr = midi_ptr;
                          //      midi_message_cntr = 0;
                          //      midi_check_status_byte = false; 
                                  midi_in_sysex = false;
                                 }
                          return;
                         }

                  if (Data == 0xF0) {
               //         PM_INFO("SYSEX (F0)\n");
                        midi_in_sysex = true;
                        return;
                       }                         

                  if ((Data & 0xF0) >= 0x80) {  // The value is a message Start
                       midi_msg_len=midi_lengths[(Data>>4)-8];
                       midi_in_message = true;
                       midi_msg_cnt = 0;
                 //      PM_INFO("MSG %X, %d ",Data,midi_msg_len);
                      } 

                  if (midi_in_message) {
                  //      PM_INFO("TX:%X ",Data);
                        mpu_fifo_tx(Data);  // Send the Data to the fifo
                        //PM_INFO("L%dC%d ",midi_msg_len,midi_msg_cnt);
                        if (midi_msg_len==midi_msg_cnt) 
                           {
                   //         PM_INFO("\n",Data);
                            midi_in_message = false; //Stop sending until next valid message/command
                           }
                        midi_msg_cnt++;
                       }
                 }
  }
  return;
}

int64_t mpu_TotalTime=0;
uint32_t mpu_bufferCount=0;

void dev_mpu_getbuffer(int32_t *buffer, int samples)
{
  int64_t starttime;
  starttime = time_us_64();
#ifdef USE_2X
  if (tsfrenderer) 
   {
    // This rendering code need a buffer 2x bigger than the requested one
    if (pm_tsf_render_short_2x(tsfrenderer, TmpBuffer32))
        memcpy(buffer, TmpBuffer32, PM_SAMPLES_PER_BUFFER * 4);
   }
#else
  if (tsfrenderer) tsf_render_short(tsfrenderer, (short int *) buffer, samples, 1);
#endif
  mpu_bufferCount++;
  int64_t delta= time_us_64()-starttime;
  mpu_TotalTime += delta;
  if (mpu_bufferCount==1000)
   {
     //PM_INFO("MPU Mix delta %d time: %d ratio: %d %",(int)delta, (int)mpu_TotalTime,(int)(mpu_TotalTime/12873) /*(int)(mpu_TotalTime*100*(OUTPUT_SAMPLERATE/PM_SAMPLES_PER_BUFFER*1000)/1000000)*/);
     //PM_INFO("Voices %d\n",pm_tsf_get_active_voices(tsfrenderer));
     PM_INFO("MPU Mix: %d voices, %d CPU", pm_tsf_get_active_voices(tsfrenderer), (int)(mpu_TotalTime/12873));
     mpu_TotalTime=0;
     mpu_bufferCount=0;
   }

}

/*
void mpu_updatebuffer(int16_t* buff,uint32_t samples)
{
  int16_t  res16_l;
  int16_t  res16_r;
  uint16_t prev_increment;
  int16_t  fres;
  res16_l=dev_mpu.prev_sample16_l;
  res16_r=dev_mpu.prev_sample16_r;

//  PM_INFO("mpu_updatebuffer\n");
  for (uint32_t sn=0;sn<samples*2;sn+=2)  // Stereo, so 2x more data than "samples"
      {
       // 8Bit Not signed
        if (dev_mpu_readsample)
             if (!afifo_take_sample8(&sbdsp_fifo, &res8u)) 
              {
                 dev_mpu.prev_sample16_l=0;  // If the FIFO is empty, return silence
                dev_mpu.prev_sample16_r=0;
                return;
              }
        fres=(int16_t) ((res8u<<8)+0x8000)/4;  // Convert to 16Bit signed
        prev_increment=dev_mpu_increment;
        dev_mpu_increment+=dev_mpu_sample_step;    // Adjust the DMA buffer to the Output frequency 
        dev_mpu_readsample = (dev_mpu_increment<=prev_increment) ? true : false;

        buff[sn]   += fres;
        buff[sn+1] += fres;
      }

      dev_mpu.prev_sample16_l=res16_l;  // Save the last Sample for the next time
      dev_mpu.prev_sample16_r=res16_r;
}
*/

void dev_mpu_senddata(uint8_t Data)
{
  uint8_t Status;
  do { dev_mpu_ior(0x331,&Status); } while (Status!=0x80); // Wait ready
  dev_mpu_iow(0x330,Data);
}

uint8_t dev_mpu_readdata()
{
  uint8_t Status;
  uint8_t Data;

  do { dev_mpu_ior(0x331,&Status); } while (Status!=0x80); // Wait ready
  dev_mpu_ior(0x330,&Data);
  return Data;
}

void dev_mpu_sendcommand(uint8_t Data)
{
  uint8_t Status;
  do { dev_mpu_ior(0x331,&Status); } while (Status!=0x80); // Wait ready
  dev_mpu_iow(0x331,Data);
}


//This is a minimal SoundFont with a single loopin saw-wave sample/instrument/preset (484 bytes)
const static unsigned char MinimalSoundFont[] =
{
	#define TEN0 0,0,0,0,0,0,0,0,0,0
	'R','I','F','F',220,1,0,0,'s','f','b','k',
	'L','I','S','T',88,1,0,0,'p','d','t','a',
	'p','h','d','r',76,TEN0,TEN0,TEN0,TEN0,0,0,0,0,TEN0,0,0,0,0,0,0,0,255,0,255,0,1,TEN0,0,0,0,
	'p','b','a','g',8,0,0,0,0,0,0,0,1,0,0,0,'p','m','o','d',10,TEN0,0,0,0,'p','g','e','n',8,0,0,0,41,0,0,0,0,0,0,0,
	'i','n','s','t',44,TEN0,TEN0,0,0,0,0,0,0,0,0,TEN0,0,0,0,0,0,0,0,1,0,
	'i','b','a','g',8,0,0,0,0,0,0,0,2,0,0,0,'i','m','o','d',10,TEN0,0,0,0,
	'i','g','e','n',12,0,0,0,54,0,1,0,53,0,0,0,0,0,0,0,
	's','h','d','r',92,TEN0,TEN0,0,0,0,0,0,0,0,50,0,0,0,0,0,0,0,49,0,0,0,34,86,0,0,60,0,0,0,1,TEN0,TEN0,TEN0,TEN0,0,0,0,0,0,0,0,
	'L','I','S','T',112,0,0,0,'s','d','t','a','s','m','p','l',100,0,0,0,86,0,119,3,31,7,147,10,43,14,169,17,58,21,189,24,73,28,204,31,73,35,249,38,46,42,71,46,250,48,150,53,242,55,126,60,151,63,108,66,126,72,207,
		70,86,83,100,72,74,100,163,39,241,163,59,175,59,179,9,179,134,187,6,186,2,194,5,194,15,200,6,202,96,206,159,209,35,213,213,216,45,220,221,223,76,227,221,230,91,234,242,237,105,241,8,245,118,248,32,252
};

/*
uint8_t dev_tsf_test()
{

  const char outfilen[]="0:/TSF_TEST.BIN";
  FRESULT fr; // Return value
  FIL outfile;
  uint bw;

  PM_INFO("Start dev_tsf_test\n");

	fr = f_open(&outfile, outfilen, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
	if (FR_OK != fr) 
     { 
       PM_INFO("f_open failed %d\n",fr);
       return 1; // Return error 1
     }

	// Load the SoundFont from the memory block
	tsfrenderer = tsf_load_memory(MinimalSoundFont, sizeof(MinimalSoundFont));
	if (!tsfrenderer)
	{
		PM_INFO("Could not load SoundFont\n");
		return 1;
	}
 
	// Set the rendering output mode to 44.1khz and -10 decibel gain
	tsf_set_output(tsfrenderer, TSF_STEREO_INTERLEAVED, (int)deviceConfig.sampleRate, -10);

	// Start two notes before starting the audio playback
	tsf_note_on(tsfrenderer, 0, 48, 1.0f); //C2
	tsf_note_on(tsfrenderer, 0, 52, 1.0f); //E2  

  tsf_render_short_fast(tsfrenderer, (short int *) buffer, samples, 1);
  
  tsf_reset(tsfrenderer);
  tsf_close(tsfrenderer);
  f_close(&outfile);  
    
}

*/


void dev_mpu_test()
{
  const uint8_t TestData[10] = { 0xA0, 1, 2, 0xB0, 3, 4, 0xC0, 5, 0xD0, 6};
  uint8_t Status;
  dev_mpu_install(0x330);

  /*
  dev_mpu_sendcommand(0xFF);
  dev_mpu_ior(0x330,&Status);
  dev_mpu_sendcommand(0x3F);
  dev_mpu_readdata();
*/

 for (int j=0; j<60; j++)
  for (int i=0; i<10; i++)
   {
    if (i==5) dev_mpu_update();
    dev_mpu_senddata(TestData[i]);
   }

  dev_mpu_remove();

}