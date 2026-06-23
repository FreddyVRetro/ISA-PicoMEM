/*
Title  : SoundBlaster DSP Emulation 
Date   : 2023-12-30
Author : Kevin Moonlight <me@yyzkevin.com> modified for the PicoMEM by Freddy VETELE (07/2025)
*/

/*
To test : Wolf3D, Duke Nukem 2 (ADPCM), DUNE, DUNE 2, RAPTOR, DOOM, Terminal Velocity
          GODS, Heart of China, Wacky Wheels, Day of the tentacle, XCOM, 

XT : Mod Master XT, Prince of Persia, Ghostbusters 2, Tongue of the Fatman, 4D Sports Boxing
*/
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pico_pic.h"

bool dma_inprogress=false;
uint32_t PM_DMA_SendTime;

#include "dev_audiomix.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "pm_audio.h"
#include "../../pm_libs/isa_dma.h"       // ISA/PC DMA code
#include "../../pm_libs/bus_irq.h"       // ISA/PC IRQ code
#include "dev_sbdsp.h"
#include "audio_fifo.h"

#include "sbdsp.h"

#define OUTPUT_SAMPLERATE   49716ul   

//#define DSP_VERSION_MAJOR 2
//#define DSP_VERSION_MINOR 1

#define DSP_UNUSED_STATUS_BITS_PULLED_HIGH 0x7F

#define MIXER_INTERRUPT         0x80
#define MIXER_DMA               0x81
#define MIXER_IRQ_STATUS        0x82
#define MIXER_STEREO            0xE

static uint8_t mixer_state[0x48] = { 0 };

static const int8_t DSP_VERSION_MAJOR[4] = { 0,2,3,4 };
static const int8_t DSP_VERSION_MINOR[4] = { 0,1,2,5 };

enum DSP_MODES {
	MODE_NONE,
	MODE_RESET,
	MODE_DAC,
	MODE_DMA,
	MODE_DMA_PAUSE,
	MODE_DMA_MASKED,
	MODE_DMA_REQUIRE_IRQ_ACK        /* Sound Blaster 16 style require IRQ ack to resume DMA */
};

enum DSP_OUT_MODES {
    DSP_DMA_2,         // 2-bit ADPCM
    DSP_DMA_3,         // 3-bit ADPCM
    DSP_DMA_4,         // 4-bit ADPCM    
    DSP_DMA_8U,        // 8-bit PCM Unsigned
    DSP_DMA_8S,        // 8-bit PCM Unsigned
    DSP_DMA_8S_S,      // 8-bit PCM Unsigned    
    DSP_DMA_16,        // 16-bit PCM (Always signed)
    DSP_DMA_16_S,      // 16-bit PCM Stereo(Always signed)    
};

// Sound Blaster DSP I/O port offsets
#define DSP_RESET           0x6
#define DSP_READ            0xA
#define DSP_WRITE           0xC
#define DSP_WRITE_STATUS    0xC
#define DSP_READ_STATUS     0xE

// Sound Blaster DSP commands.
#define DSP_STATUS              0x04    // 2.00+ : Read pending DSP operations status
#define DSP_DIRECT_DAC          0x10    // 1.xx+ :8-bit Output sample in direct mode
#define DSP_DIRECT_ADC          0x20    // 1.xx+ :8-bit Input sample in direct mode 

#define DSP_DMA_ADC             0x24    // 1.xx+ :8-bit DMA PCM Input, Single

#define DSP_SET_TIME_CONSTANT   0x40    // 1.xx+ :Set digitized sound transfer constant
#define DSP_SET_DAC_SAMPLE_RATE 0x41    // ! 4.xx+ :Set sample Rate (Output)
#define DSP_SET_ADC_SAMPLE_RATE 0x42    // ! 4.xx+ :Set sample Rate (Input)

#define DSP_DMA_SINGLE          0x14    // 1.xx+ :8-bit DMA PCM Output, Single
#define DSP_DMA_SINGLE2         0x15    // 1.xx+ :8-bit DMA PCM Output, Single (Alternate)
#define DSP_DMA_SINGLE_IN       0x24    // !(N/A) 1.xx+ :8-bit DMA PCM Input, Single

#define DSP_DMA_ADPCM_2         0x16    // ! 1.xx+ :8-bit DMA ADPCM 2 bit Output, Single
#define DSP_DMA_ADPCM_2_REF     0x17    // ! 1.xx+ :8-bit DMA ADPCM 2 bit Output, Single  With Reference
#define DSP_DMA_ADPCM_4         0x74    // ! 1.xx+ :8-bit DMA ADPCM 4 bit Output, Single
#define DSP_DMA_ADPCM_4_REF     0x75    // ! 1.xx+ :8-bit DMA ADPCM 4 bit Output, Single with Reference
#define DSP_DMA_ADPCM_26        0x76    // ! 1.xx+ :8-bit DMA ADPCM 3 bit Output, Single
#define DSP_DMA_ADPCM_26_REF    0x77    // ! 1.xx+ :8-bit DMA ADPCM 3 bit Output, Single with Reference

#define DSP_DMA_BLOCK_SIZE      0x48    // 2.00+ : Set block size for highspeed/dma
#define DSP_DMA_AUTO            0X1C    // 2.00+ :8-bit DMA PCM Output, Auto
#define DSP_DMA_HS_AUTO         0x90    // 2.01+ :8-bit DMA PCM Output, Auto, High Speed
#define DSP_DMA_HS_SINGLE       0x91    // 2.01+ :8-bit DMA PCM Output, single, High Speed
#define DSP_DMA_HS_AUTO_IN      0x98    // !(N/A) 2.01+ :8-bit DMA PCM Input, Auto, High Speed
#define DSP_DMA_HS_SINGLE_IN    0x99    // !(N/A) 2.01+ :8-bit DMA PCM Input, Single, High Speed
#define DSP_DMA_ADPCM           0x7F    // creative ADPCM 8bit to 3bit
#define DSP_MIDI_READ_POLL      0x30    // 1.xx+ : MIDI Input Polling
#define DSP_MIDI_WRITE_POLL     0x38    // 1.xx+ : MIDI Output Polling

/*
 075h           DMA DAC, 4-bit ADPCM Reference                      SB
 076h           DMA DAC, 2.6-bit ADPCM                              SB
 077h           DMA DAC, 2.6-bit ADPCM Reference                    SB
 07Dh           Auto-Initialize DMA DAC, 4-bit ADPCM Reference      SB2.0
 07Fh           Auto-Initialize DMA DAC, 2.6-bit ADPCM Reference    SB2.0
 080h           Silence DAC                                         SB
 098h           Auto-Initialize DMA ADC, 8-bit (High Speed)         SB2.0-Pro2
*/

#define DSP_DMA_PAUSE           0xD0    // 1.xx+ : DMA Pause
#define DSP_DMA_RESUME          0xD4    // 1.xx+ : DMA Resume
#define DSP_DMA_PAUSE16         0xD5    // !4.xx+ : DMA 16-bit Pause
#define DSP_DMA_RESUME16        0xD6    // !4.xx+ : DMA 16-bit Resume
#define DSP_DMA_EXIT_AUTO8      0xDA    // 2.00+ : Exit Autoinit mode 8-bit
#define DSP_DAC_PAUSE_DURATION  0x80    // 1.xx+ : Pause Output (Used by Tryrian for detection)
#define DSP_ENABLE_SPEAKER      0xD1    // 1.xx+ : Turn On Speaker  - Ok
#define DSP_DISABLE_SPEAKER     0xD3    // 1.xx+ : Turn Off Speaker - Ok
#define DSP_SPEAKER_STATUS      0xD8    // 2.00+ : Get Speaker Status
#define DSP_IDENT               0xE0    // 2.00+ : Returns the complement of the inbox value
#define DSP_VERSION             0xE1    // 1.xx+ : Get DSP Version
#define DSP_IDENT_E2            0xE2    // 2.00+ : Returns the complement of the inbox value, alternate
#define DSP_WRITETEST           0xE4    // 2.00+ : Write to a diagnostic register
#define DSP_READTEST            0xE8    // 2.00+ : Read the diagnostic register
#define DSP_SINE                0xF0
#define DSP_IRQ                 0xF2    // 2.00+ : IRQ requeest, trigger a DSP interrupt
#define DSP_CHECKSUM            0xF4


#define DSP_MLC_NONE          0
#define DSP_MLC_DMA_RESET     2
#define DSP_MLC_DSP_RESET     3
#define DSP_MLC_SENDIRQ       4
#define DSP_MLC_SENDIRQ_WAIT  5
#define DSP_MLC_SET_TIME_CST  6      // Set Time Constant

static irq_handler_t SBDSP_DMA_isr_pt;

sbdsp_t sbdsp;
audio_fifo_8_t sbdsp_fifo; // FIFO for the DSP samples

static uint32_t DSP_DMA_EventHandler(Bitu val);
static PIC_TimerEvent DSP_DMA_Event = {
    .handler = DSP_DMA_EventHandler,
};

/*
static uint32_t DSP_DAC_Timer_EventHandler(Bitu val);
static PIC_TimerEvent DSP_DAC_Timer_Event = {
    .handler = DSP_DAC_Timer_EventHandler,
};
*/

static __force_inline void sbdsp_dma_disable() {
    sbdsp.mode=MODE_NONE;
    sbdsp.dma_enabled=false; 
   // sbdsp.vdma_flushdata = true; // Set the flush flag
    PIC_RemoveEvent(&DSP_DMA_Event);
    sbdsp.cur_sample8 = 0x80;   // zero current sample
}

static __force_inline void sbdsp_dma_enable() {    
    if(!sbdsp.dma_enabled) {
        sbdsp.mode= MODE_DMA; // Set the mode to DMA
        sbdsp.dma_enabled=true;
        sbdsp.timer_delta=0;
#if USE_HWDMA
        isa_dma_set_isr(SBDSP_DMA_isr_pt);
#endif
   // PM_INFO("Start DMA w: %d",sbdsp.dma_rx_width);
        PIC_AddEvent(&DSP_DMA_Event, sbdsp.dma_interval, 0);
    } else { PM_INFO("SB> DMA Already Enabled"); }
}

/*
static __force_inline bool sbdsp_dac_enable() {
    if (sbdsp.mode==MODE_NONE)  // Start DAC if only in MODE_NONE
      {
        //afifo_reset8(&sbdsp_fifo); // Reset the FIFO
        //Set the sb dsp output rate to 31KHz
        sbdsp.dac_interval = 32;    //Simulate a DAC at 31250 Hz max
        sbdsp.sample_step  = (31250 * 65536ul) / PM_AUDIO_FREQUENCY; // Step for the Audio rate convertion
        sbdsp.increment    = 0;
        sbdsp.readsample   = true;
        sbdsp.prev_sample8u = 0x80;
       
        PIC_AddEvent(&DSP_DAC_Timer_Event, sbdsp.dac_interval, 0);
        sbdsp.mode=MODE_DAC;
        return true;
      }

    PM_INFO("SB> Cant start DAC"); 
    return false;
}

static __force_inline void sbdsp_dac_disable() {
    if (sbdsp.mode==MODE_DAC)
      {
        sbdsp.dac_interval=0;    //Stop the DAC timer if running
        PIC_RemoveEvent(&DSP_DAC_Timer_Event);
      }
}
*/

static uint32_t __not_in_flash_func(DSP_DMA_EventHandler)(Bitu val) {
    uint32_t current_interval;
    DBG_ON_1();
    sbdsp.dma_sample_count_rx++;
    PM_DMA_SendTime=time_us_64();
    dma_inprogress=true;

#if USE_HWDMA    
    sbdsp.dma_rx_index=0;
    isa_dma_start_write();
#else
    sbdsp.vdma_to_send++;      // Ask one more byte to transfer
#endif
 //    PM_INFO("X");

    current_interval = sbdsp.dma_interval+sbdsp.timer_delta;
    // printf("%u\n", current_interval);

    DBG_OFF_1();
    if(sbdsp.dma_sample_count_rx <= sbdsp.dma_sample_count) {
        return current_interval;
    } else {
        if(sbdsp.autoinit) {
          //  sbdsp.vdma_flushdata = true; // Set the flush flag
#if USE_HWDMA            
            bus_irq_raise(IRQ_R_AUDIO,sbdsp.irq,true); 
#else              
            sbdsp.mainloop_action=DSP_MLC_SENDIRQ;
#endif            
            sbdsp.dma_sample_count_rx=0;            
            return current_interval;
        }
        else {
          //  sbdsp.vdma_flushdata = true; // Set the flush flag
            sbdsp_dma_disable();
#if USE_HWDMA            
            bus_irq_raise(IRQ_R_AUDIO,sbdsp.irq,true);
#else             
            sbdsp.mainloop_action=DSP_MLC_SENDIRQ;
#endif
         return 0;
        }
    }
    return 0; 
}


static void __not_in_flash_func(sbdsp_dma_isr)(void) {
    const uint32_t dma_data = isa_dma_complete_write();
    afifo_add_sample8(&sbdsp_fifo, (uint8_t) dma_data);
//    sbdsp.dma_rx_index++;
//    if (sbdsp.dma_rx_index++!=sbdsp.dma_rx_width) isa_dma_start_write();
      sbdsp.dma_rx_index=0;
      if (++sbdsp.dma_rx_index!=1) isa_dma_start_write();
      else dma_inprogress=false;
}

static uint32_t __not_in_flash_func(DSP_DAC_EventHandler)(Bitu val) {
  if (sbdsp.mode==MODE_DAC)
     {
       afifo_add_sample8(&sbdsp_fifo, sbdsp.cur_sample8);
     }
  return sbdsp.dac_interval;
}

// Send an IRQ after some time, for "Tinny" DMA transfer simulation
static uint32_t DSP_DMA_IRQ_EventHandler(Bitu val) {
  DBG_ON_IRQ_SB();
  bus_irq_raise(IRQ_R_AUDIO,sbdsp.irq,true);
  //sbdsp.mainloop_action=DSP_MLC_SENDIRQ;
  //PM_INFO("SB IRQ ");
  return 0;
}
static PIC_TimerEvent DSP_DMA_IRQ_Event = {
    .handler = DSP_DMA_IRQ_EventHandler,
};

// Used by the DAC PAUSE command pending status is reflected in the DSP status port read
static uint32_t DSP_DAC_Resume_eventHandler(Bitu val) {
  bus_irq_raise(IRQ_R_AUDIO,sbdsp.irq,true);
  //sbdsp.mainloop_action=DSP_MLC_SENDIRQ;
  sbdsp.dac_resume_pending = false;
  return 0;
}
static PIC_TimerEvent DSP_DAC_Resume_event = {
    .handler = DSP_DAC_Resume_eventHandler,
};


void sbdsp_init() 
{

// Need to be reinitialized when the SB emulation is restarted
    sbdsp.dav_dsp=0;        // No DSP Data available
    sbdsp.dsp_busy=0;       // DSP Not busy
    sbdsp.mainloop_action=0;
	sbdsp.mode=MODE_NONE;

    sbdsp.outbox = 0xAA;

    // Initialize buffer for the E2 command
    sbdsp.ident_e2[0] = 0xAA;
    sbdsp.ident_e2[1] = 0x96;    


    afifo_init8(&sbdsp_fifo); // initialize the audio FIFO

#ifdef BOARD_PICOMEM
    PM_INFO("sbdsp_init\n");
    SBDSP_DMA_isr_pt = sbdsp_dma_isr;
    //    PIC_Init();  // To remove when Audio mix is added !!!!

#else // PicoGUS
    puts("Initing ISA DMA ISR...");
    SBDSP_DMA_isr_pt = sbdsp_dma_isr;
    dma_config = DMA_init(pio0, DMA_PIO_SM, SBDSP_DMA_isr_pt);         
#endif
}

void sbdsp_start_pcm_dma(bool autoinit, uint16_t xfer_size) {
    PM_INFO("DMA %d,%d",autoinit,xfer_size);
     sbdsp.dav_dsp=0;
     sbdsp.autoinit=autoinit;
     sbdsp.dma_sample_count = xfer_size;
     sbdsp.dma_sample_count_rx=0;
     sbdsp.dma_rx_width=sbdsp.dma_stereo ? 2 : 1;
     sbdsp.dma_xfer_count = (xfer_size + 1) / sbdsp.dma_rx_width;
     sbdsp.dma_xfer_count_left = sbdsp.dma_xfer_count;     
     sbdsp_dma_enable();
    }

static __force_inline void sbdsp_output(uint8_t value) {
    sbdsp.outbox=value;
    sbdsp.dav_pc=1;
}

void sbdsp_process(void) {
    if(sbdsp.reset_state) return;
    sbdsp.dsp_busy=1;

    if(sbdsp.dav_dsp) {
        if(!sbdsp.current_command) {            
            sbdsp.current_command = sbdsp.inbox;
            sbdsp.current_command_index=0;
            sbdsp.dav_dsp=0;
        }
    }

    switch(sbdsp.current_command) {  

        case DSP_DMA_PAUSE:               // 1.xx+ : DMA Pause  > To add : Insert silences
            sbdsp.current_command=0;
            sbdsp_dma_disable();
#if USE_HWDMA
            isa_dma_stop_write();
#endif            
            sbdsp.mode= MODE_DMA_PAUSE; // Set the mode to DMA_PAUSE
            PM_INFO("DMA PAUSE\n");
            break;

        case DSP_DMA_RESUME:              // 1.xx+ : DMA Resume
            if (sbdsp.mode==MODE_DMA_PAUSE) {
              sbdsp_dma_enable();
             } else PM_INFO("!R");
            sbdsp.current_command=0;
            PM_INFO("DMA RESUME\n");
            break;

        case DSP_DMA_EXIT_AUTO8:       // 2.00+ : Exit Autoinit mode 8-bit
            sbdsp.current_command=0;
            sbdsp.autoinit=false;
            //PM_INFO("EXIT AUTO\n");
            break;

        case DSP_DMA_HS_AUTO:    // 2.01+ :8-bit DMA PCM Output, Auto, High Speed
        case DSP_DMA_AUTO:       // 2.00+ :8-bit DMA PCM Output, Auto
            PM_INFO("DMA_AUTO %d\n",sbdsp.dma_block_size);
            sbdsp.dav_dsp=0;
            sbdsp.current_command=0;              
            sbdsp_start_pcm_dma(true, sbdsp.dma_block_size);         
            break;
 
        case DSP_SET_TIME_CONSTANT: // 1.xx+ :Set digitized sound transfer constant
            if(sbdsp.dav_dsp) {
                if(sbdsp.current_command_index==1) {
//                PM_INFO("DSP_SET_TIME_CONSTANT %d\n",sbdsp.time_constant);
//                    sbdsp.mainloop_action=DSP_MLC_SET_TIME_CST;
                    sbdsp.time_constant = sbdsp.inbox;
                    sbdsp.dma_interval = 256 - sbdsp.time_constant;  // Interval is the Nb of us between each sample read

        // Initialize the timing values for the DMA Buffer read
                    sbdsp.sample_rate  = 1000000ul / sbdsp.dma_interval;
                    // Maybe change this in the buffer read code, triggered by a counter
        /// Buffer resampling variables init            
                    sbdsp.sample_step  = (sbdsp.sample_rate * 65536ul) / PM_AUDIO_FREQUENCY; // Step for the Audio rate convertion
                    sbdsp.increment    = 0;
                    sbdsp.readsample   = true;
                    sbdsp.prev_sample8u = 0x80;
                    sbdsp.prev_sample8u_left = 0x80;
                    sbdsp.prev_sample16u = 0x8000;
                    sbdsp.prev_sample16u_left = 0x8000;
                    
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;
                }
                sbdsp.current_command_index++;
            }
            break;

        case DSP_DMA_BLOCK_SIZE:   // 2.00+ : Set block size for highspeed/dma            
            if(sbdsp.dav_dsp) {
                if(sbdsp.current_command_index==1) {      //Block size LSB
                    sbdsp.dma_block_size=sbdsp.inbox;
                    sbdsp.dav_dsp=0;
                }
                else if(sbdsp.current_command_index==2) { // Block size MSB
                    sbdsp.dma_block_size += (sbdsp.inbox << 8);
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;
                    PM_INFO("SB BSize:%u ",sbdsp.dma_block_size);
                }
                sbdsp.current_command_index++;
            }
            break;
        
        case DSP_DMA_HS_SINGLE:    // 2.01+ :8-bit DMA PCM Output, single, High Speed
            PM_INFO("DMA_HS_SINGLE\n");
            sbdsp.dav_dsp=0;
            sbdsp.current_command=0;
            sbdsp_start_pcm_dma(false, sbdsp.dma_block_size);
            break;

        case DSP_DMA_SINGLE:       // 1.xx+ :8-bit DMA PCM Output, Single
        case DSP_DMA_SINGLE2:
            if(sbdsp.dav_dsp) {            
                if(sbdsp.current_command_index==1) {
                    sbdsp.dma_sample_count = sbdsp.inbox;
                    sbdsp.dav_dsp=0;                    
                }
                else if(sbdsp.current_command_index==2) {
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;
                    sbdsp.dma_sample_count += (sbdsp.inbox << 8);
                    PM_INFO("DMA_SINGLE %u\n",sbdsp.dma_sample_count);
#if USE_HWDMA                              
                   sbdsp_start_pcm_dma(false, sbdsp.dma_sample_count);
#else     
                   if (sbdsp.dma_sample_count<10)
                     {
                      sbdsp_dma_disable();                      
                      PIC_AddEvent(&DSP_DMA_IRQ_Event,sbdsp.dma_interval*(sbdsp.dma_sample_count+1),0);  // Send an IRQ fast with no DMA transfer
                     } else  sbdsp_start_pcm_dma(false, sbdsp.dma_sample_count);         
#endif                                       
                }
                sbdsp.current_command_index++;
            }
            break;

        case DSP_DMA_ADC:          // 1.xx+ :8-bit DMA PCM Input, Single (Used by Windows 3.11 driver install)
            if(sbdsp.dav_dsp) {            
                if(sbdsp.current_command_index==1) {
                    sbdsp.dma_sample_count = sbdsp.inbox;
                    sbdsp.dav_dsp=0;                    
                }
                else if(sbdsp.current_command_index==2) {
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;
                    sbdsp.dma_sample_count += (sbdsp.inbox << 8);
                    PM_INFO("DMA_ADC_SINGLE %u\n",sbdsp.dma_sample_count);
                    PIC_AddEvent(&DSP_DMA_IRQ_Event,sbdsp.dma_interval*(sbdsp.dma_sample_count+1),0);  // Send an IRQ with no DMA transfer (fake)
                }
                sbdsp.current_command_index++;
            }
            break;

        case DSP_DMA_ADPCM_2_REF:   // First byte used as reference
            sbdsp.adpcm_ref=true;
        case DSP_DMA_ADPCM_2:
            if(sbdsp.dav_dsp) {            
                if(sbdsp.current_command_index==1) {
                    sbdsp.dma_sample_count = sbdsp.inbox;
                    sbdsp.dav_dsp=0;                    
                }
                else if(sbdsp.current_command_index==2) {
                    PM_INFO("DMA_ADPCM_2 %u\n",sbdsp.dma_sample_count);
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0; 
                    sbdsp.dma_sample_count += (sbdsp.inbox << 8);
                }
                sbdsp.current_command_index++;
            }                        
            break;


        case DSP_DMA_ADPCM_4_REF: // ! 1.xx+ :8-bit DMA ADPCM 2 bit Output, Single  With Reference
            sbdsp.adpcm_ref=true;
        case DSP_DMA_ADPCM_4:     // ! 1.xx+ :8-bit DMA ADPCM 2 bit Output, Single
            if(sbdsp.dav_dsp) {            
                if(sbdsp.current_command_index==1) {
                    sbdsp.dma_sample_count = sbdsp.inbox;
                    sbdsp.dav_dsp=0;                    
                }
                else if(sbdsp.current_command_index==2) {
                    PM_INFO("DMA_ADPCM_4 %u\n",sbdsp.dma_sample_count);
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;                     
                    sbdsp.dma_sample_count += (sbdsp.inbox << 8);

                }
                sbdsp.current_command_index++;
            }                        
            break;

        case DSP_DMA_ADPCM_26_REF:   // First byte used as reference  LENGTH = (SAMPLES-1 + 1)/2 + 1
            sbdsp.adpcm_ref=true;
        case DSP_DMA_ADPCM_26: // LENGTH = (SAMPLES-1 + 2)/3
            if(sbdsp.dav_dsp) {            
                if(sbdsp.current_command_index==1) {
                    sbdsp.dma_sample_count = sbdsp.inbox;
                    sbdsp.dav_dsp=0;                    
                }
                else if(sbdsp.current_command_index==2) {                 
                    PM_INFO("DMA_ADPCM_26 %u\n",sbdsp.dma_sample_count);
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;                      
                    sbdsp.dma_sample_count += (sbdsp.inbox << 8);

                }
                sbdsp.current_command_index++;
            }                        
            break;

        case DSP_IRQ:
     //       PM_INFO("DSP_IRQ\n");
            sbdsp.current_command=0;
            DBG_ON_IRQ_SB();
            bus_irq_raise(IRQ_R_AUDIO,sbdsp.irq,true);   // Force an SB Interrupt
            break;

        case DSP_VERSION:
            if(sbdsp.current_command_index==0) {
                sbdsp.current_command_index=1;
                sbdsp_output(DSP_VERSION_MAJOR[sbdsp.type]);
            }
            else {
                if(!sbdsp.dav_pc) {
                    sbdsp.current_command=0;                    
                    sbdsp_output(DSP_VERSION_MINOR[sbdsp.type]);
                }
                
            }
            break;

        case DSP_IDENT:
            if(sbdsp.dav_dsp) {            
                if(sbdsp.current_command_index==1) {
                  //  PM_INFO("DSP_IDENT\n");                    
                    sbdsp.dav_dsp=0;                    
                    sbdsp.current_command=0;        
                    sbdsp_output(~sbdsp.inbox);                                        
                }                
                sbdsp.current_command_index++;
            }                                       
            break;
        case DSP_IDENT_E2: // yet another "protection"... used by CT-VOICE.DRV
            if (sbdsp.dav_dsp) {
                if (sbdsp.current_command_index == 1) {
                    sbdsp.dav_dsp = 0;
                    sbdsp.current_command = 0;
                    sbdsp.ident_e2[0] += sbdsp.inbox ^ sbdsp.ident_e2[1];
                    sbdsp.ident_e2[1] = (sbdsp.ident_e2[1] >> 2) | (sbdsp.ident_e2[1] << 6);
                    // ideally it should output this byte via DMA, but we don't support DMA IORs yet
                    // so stuff to outbox for now
                    sbdsp_output(sbdsp.ident_e2[0]);
                }
                sbdsp.current_command_index++;
            }
        case DSP_ENABLE_SPEAKER:
            PM_INFO("ENABLE SPEAKER\n");          
            sbdsp.speaker_on = true;
            sbdsp.current_command=0;
            break;
        case DSP_DISABLE_SPEAKER:
            PM_INFO("DISABLE SPEAKER\n");           
            sbdsp.speaker_on = false;
            sbdsp.current_command=0;
            break;
        case DSP_SPEAKER_STATUS:
            if(sbdsp.current_command_index==0) {
                sbdsp.current_command=0;
                sbdsp_output(sbdsp.speaker_on ? 0xff : 0x00);
            }
            break;

        case DSP_DIRECT_DAC:
            if(sbdsp.dav_dsp) {
                if(sbdsp.current_command_index==1) {
                    sbdsp.cur_sample8=sbdsp.inbox;
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;
                }
                sbdsp.current_command_index++;
            }
            break;
        case DSP_DIRECT_ADC:
              sbdsp.outbox = 0x79;  // Return a fake "Silence" value
              sbdsp.dav_pc=1;
              sbdsp.current_command=0;
            break;
        //case DSP_MIDI_READ_POLL:
        //case DSP_MIDI_WRITE_POLL:    
        case DSP_WRITETEST:
            if(sbdsp.dav_dsp) {            
                if(sbdsp.current_command_index==1) {                    
                    //PM_INFO("DSP_WRITETEST\n\r");
                    sbdsp.test_register = sbdsp.inbox;
                    sbdsp.dav_dsp=0;                    
                    sbdsp.current_command=0;                                                
                }                
                sbdsp.current_command_index++;
            }                                       
            break;
        case DSP_READTEST:
            if(sbdsp.current_command_index==0) {
                sbdsp.current_command=0;
                sbdsp_output(sbdsp.test_register);
            }
            break;
        
        case DSP_DAC_PAUSE_DURATION:
            if(sbdsp.dav_dsp) {                             
                if(sbdsp.current_command_index==1) {                    
                    sbdsp.dac_pause_duration=sbdsp.inbox;
                    sbdsp.dav_dsp=0;                    
                }
                else if(sbdsp.current_command_index==2) {
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;                    
                    sbdsp.dac_pause_duration += (sbdsp.inbox << 8);
                    sbdsp.dac_resume_pending = true;
                    PM_INFO("SB Pause:%u\n\r",sbdsp.dac_pause_duration);                    
                    // When the specified duration elapses, the DSP generates an interrupt.
                    PIC_AddEvent(&DSP_DAC_Resume_event, sbdsp.dma_interval * sbdsp.dac_pause_duration, 0);         
                }
                sbdsp.current_command_index++;
            }
            break;
        case DSP_STATUS:   // DSP Status SB 2.0 version
              sbdsp.dav_pc=1;
              sbdsp.current_command=0;        
              switch(sbdsp.type) {
                  case SBT_2:
                      sbdsp.outbox = 0x88;  // Return SB2.00 status
                      break;
                  case SBT_PRO2:
                      sbdsp.outbox = 0x7B;  // Return SBPro2 status
                      break;
                  default:
                      sbdsp.outbox = 0Xff;  //Everything enabled
                      break;                      
              }
            break;
        //case DSP_SINE:
        //case DSP_CHECKSUM:
        case 0:
            //not in a command
            break;            
        default:
            PM_INFO("Unknown Command: %x\n",sbdsp.current_command);
            sbdsp.current_command=0;
            break;

    }                
    sbdsp.dsp_busy=0;
}

static uint32_t DSP_Reset_EventHandler(Bitu val) {
    sbdsp.reset_state=0;                
    sbdsp.outbox = 0xAA;
    sbdsp.dav_pc=1;
    sbdsp.current_command=0;
    sbdsp.current_command_index=0;

    sbdsp.dma_block_size=0x7FF; //default per 2.01
    sbdsp.dma_xfer_count = 0;
    sbdsp.dma_xfer_count_left = 0;    
    sbdsp.dma_sample_count=0;
    sbdsp.dma_sample_count_rx=0;              
    sbdsp.dma_stereo = false;
    sbdsp.dma_signed = false;
    sbdsp.speaker_on = false;
//    sbdsp.dma_done = false;
    sbdsp.dac_resume_pending = false;
    sbdsp.mode=MODE_NONE;
    return 0;
}
static PIC_TimerEvent DSP_Reset_Event = {
    .handler = DSP_Reset_EventHandler,
};

static __force_inline void sbdsp_reset(uint8_t value) {
    //TODO: COLDBOOT ? WARMBOOT ?    
    value &= 1; // Some games may write unknown data for bits other than the LSB.
    switch(value) {
        case 1:
            PM_INFO("SB>RST ");
            PIC_RemoveEvent(&DSP_Reset_Event); 
            sbdsp.autoinit=false;
            sbdsp_dma_disable();
#if USE_HWDMA
            isa_dma_stop_write();
            afifo_reset8(&sbdsp_fifo); // Reset the Audio FIFO
#else            
            sbdsp.mainloop_action=DSP_MLC_DMA_RESET;
#endif            
	        sbdsp.mode=MODE_RESET; // Set the mode to RESET
            sbdsp.reset_state=1;
//            PM_INFO("Ok\n");
            break;
        case 0:
            if(sbdsp.reset_state==1) {                
                sbdsp.dav_pc=0;
                // launch the RESET a little later
                PIC_RemoveEvent(&DSP_Reset_Event);  
                PIC_AddEvent(&DSP_Reset_Event, 100, 0); // Wait for 100uS
                sbdsp.reset_state = 2;
            }
            break;
        default:
            break;
    }
}

static __force_inline uint8_t sbmixer_read(void) {
  uint8_t ret=0;

  switch (sbdsp.mixer_command) {
        case 0x00:
            return 0x00;
        case MIXER_INTERRUPT:
			 switch (sbdsp.irq) {
					case 2:  return 0x1;
					case 5:  return 0x2;
					case 7:  return 0x4;
					case 10: return 0x8;
				   }
             break;
        case MIXER_DMA:
			 switch (sbdsp.dma8) {
					case 0:ret|=0x1;break;
					case 1:ret|=0x2;break;
					case 3:ret|=0x8;break;
				    } 
             break;
        case MIXER_IRQ_STATUS:
            // Bits 0-1: 8/16-bit IRQ pending. Bit 5: SB16 type identifier.
            return (sbdsp.irq_8_pending ? 0x01 : 0) | (sbdsp.irq_16_pending ? 0x02 : 0) | 0x20;
        default:
            if (sbdsp.mixer_command<0x48) return mixer_state[sbdsp.mixer_command];
               else return 0x0A;
    }
  return ret;
}

static __force_inline void sbmixer_write(uint8_t value) {
    switch (sbdsp.mixer_command) {
        case 0x00: // Mixer reset
            memset(mixer_state, 0, sizeof(mixer_state));
            break;
        case MIXER_INTERRUPT:
            // sbdsp.interrupt = value;
            break;
        case MIXER_DMA:
            // sbdsp.dma = value;
            break;
        case MIXER_STEREO:
            sbdsp.dma_stereo = value & 2;
            // no break
        default:
            if (sbdsp.mixer_command<0x48) mixer_state[sbdsp.mixer_command] = value;
            break;
    }
}


uint8_t sbdsp_read(uint8_t address) 
{
 switch(address) {        
      case DSP_READ:          // 0x0A : Return DSP Data, if the outbox is not empty
            sbdsp.dav_pc=0;
            return sbdsp.outbox;
      case DSP_READ_STATUS:   // 0x0E : Return bit7=1 if there is data to read from the DSP (Also acknowledge IRQ)    
            bus_irq_lower(IRQ_R_AUDIO,sbdsp.irq); // Acknowledge the IRQ
            DBG_OFF_IRQ_SB();
//            PM_INFO("iAck");
        return sbdsp.dav_pc << 7 | DSP_UNUSED_STATUS_BITS_PULLED_HIGH;
      case DSP_WRITE_STATUS:  // 0x0C : Return bit7=0 if the DSP is ready to receive Command/Data
        return (sbdsp.dav_dsp | sbdsp.dsp_busy | sbdsp.dac_resume_pending) << 7 | DSP_UNUSED_STATUS_BITS_PULLED_HIGH;                            
      default:
//            PM_INFO("SB READ: %x\n\r",address);
            return 0xFF;            
  }
}


// Return true if a value is really written
bool sbdsp_write(uint8_t address, uint8_t value) 
{
    switch(address) {
        case DSP_WRITE:         // 0x0C : Write command/Data to the DSP
            if(sbdsp.dav_dsp) PM_INFO("WARN - DAV_DSP OVERWRITE\n");
            sbdsp.inbox = value;
            sbdsp.dav_dsp = 1;
            return true;
            break;
        case DSP_RESET:         // 0x06 : Reset Port (Write 0 then 1)
            sbdsp_reset(value);
            return true;            
            break;
        default:
          //  PM_INFO("SB WRITE: %x => %x \n\r",value,address);                      
            break;
    }
return false;
}

// dma buffer level must be !=0
// Use another variable to have the value stable during the transfer
void sbdsp_transfer(uint16_t samples)
{
 uint32_t to_transfer;

 /*
if ((sbdsp_fifo.samples_received-sbdsp_fifo.samples_sent) < AUDIO_FIFO_START_THRESHOLD-256) {
    sbdsp.timer_delta= -5; // Speed up the transfer
   } else if ((sbdsp_fifo.samples_received-sbdsp_fifo.samples_sent) < AUDIO_FIFO_START_THRESHOLD-256) {
    sbdsp.timer_delta= 5; // Slow down the transfer
   } else sbdsp.timer_delta= 0; // Normal transfer speed
*/

  uint32_t afifolvl=AUDIO_FIFO_SIZE - (sbdsp_fifo.samples_received-sbdsp_fifo.samples_sent);
  to_transfer = MIN(samples,afifolvl);  // Minimum of samples to send and audio fifo level
  if (to_transfer==0) return;
  if (to_transfer==1)
       {
        afifo_add_sample8(&sbdsp_fifo,isa_dma_buffer_get());
        sbdsp.vdma_sent++;
       } else 
       {
        to_transfer = MIN(isa_dma.buffer_level,samples); // Get the number of bytes to transfer to the Audio fifo

        afifo_add_samples8(&sbdsp_fifo, isa_dma_buffer_get_ptr(to_transfer), to_transfer);  
        sbdsp.vdma_sent+=to_transfer;  // !!! If the else is here, Raptor crack
       }
}

// Worked/Code to call regularly from the main loop
// Transfer data from the DMA to the Sound Blaster Audio FIFO
// "input" is sbdsp.vdma_to_send the value change during the code execution.
void sbdsp_dma_worker()
{
#if USE_HWDMA

    if (dma_inprogress)
        if ((time_us_64()-PM_DMA_SendTime) > 100) { // 100us
         PM_INFO("SB DMA Timeout\n");
        // isa_dma_complete_write();
        // dma_inprogress=false;
        }

#else

#if DBG_PIN_AUDIO   
  if (SVAR_IRQ->PM_IRR) DBG_ON_INT_SB();
     else DBG_OFF_INT_SB();
#endif

  if (sbdsp.mainloop_action==DSP_MLC_DMA_RESET) {  // Perform the reset here, to not change the variable in the middle of the worker execution
      PM_INFO(">MLCRST ");
      sbdsp.vdma_to_send=0;   // Reset the Virtual DMA sample count
      sbdsp.vdma_sent=0;
      isa_dma_buffer_free();  // Free the DMA buffer
      sbdsp.mainloop_action=DSP_MLC_NONE;
      return;
     }
/*
  if (sbdsp.mainloop_action==DSP_MLC_RESET) {
     afifo_reset8(&sbdsp_fifo); // Reset the FIFO
   }
*/

  if (sbdsp.vdma_to_send!=sbdsp.vdma_sent) {  // Execute only if there is data to transfer
  DBG_ON_SB_UPD();
  dev_sbdsp_enable_mix(); // Enable mixing

  if (isa_dma.buffer_level==0) isa_dma_transfer(sbdsp.dma8); //(SB DMA for test=1)
  if (isa_dma.buffer_level>0)  sbdsp_transfer(sbdsp.vdma_to_send-sbdsp.vdma_sent);

  DBG_OFF_SB_UPD();
 }

if (sbdsp.mainloop_action==DSP_MLC_SENDIRQ) {  // Perform the reset here, to not change the variable in the middle of the worker execution
 //     PM_INFO(">I ");
      DBG_ON_IRQ_SB();
      bus_irq_raise(IRQ_R_AUDIO,sbdsp.irq,true);
      sbdsp.mainloop_action=DSP_MLC_NONE;
      return;
     }

#endif
}

// return an audio buffer by reading from the audio fifo
void sbdsp_updatebuffer(int16_t* buff,uint32_t samples)
{
  int8_t   res8u;
  uint16_t prev_increment;
  int16_t  fres;
  res8u=sbdsp.prev_sample8u;
  /*
 if (sbdsp.mainloop_action==DSP_MLC_SET_TIME_CST)
    {

    }
*/

//  PM_INFO("SB_GetBuffer\n");
 if (sbdsp.dma_stereo)
  {
  }
    else
  {
    for (uint32_t sn=0;sn<samples*2;sn+=2)  // Stereo, so 2x more data than "samples"
      {
       // 8Bit Not signed
        if (sbdsp.readsample)
             if (!afifo_take_sample8(&sbdsp_fifo, &res8u)) 
              {
                 sbdsp.prev_sample8u=0x80;  // If the FIFO is empty, return silence
                return;
              }
        fres=(int16_t) ((res8u<<8)+0x8000)/4;  // Convert to 16Bit signed
        prev_increment=sbdsp.increment;
        sbdsp.increment+=sbdsp.sample_step;    // Adjust the DMA buffer to the Output frequency 
        sbdsp.readsample = (sbdsp.increment<=prev_increment) ? true : false;

        buff[sn]   += fres;
        buff[sn+1] += fres;
      }
    sbdsp.prev_sample8u=res8u;  // Save the last Sample for the next time
   }    

}