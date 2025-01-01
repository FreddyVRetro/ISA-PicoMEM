#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pico_pic.h"

/*
Title  : SoundBlaster DSP Emulation 
Date   : 2023-12-30
Author : Kevin Moonlight <me@yyzkevin.com>

*/

#define BOARD_PICOMEM

bool dma_inprogress;

#ifdef BOARD_PICOMEM
extern "C" {
  extern void PM_IRQ_Lower(void);
  extern uint8_t PM_IRQ_Raise(uint8_t Source,uint8_t Arg);  
}

#include "../pm_defines.h"
#include "../pm_gvars.h"
#include "pm_audio.h"
#include "../../pm_libs/pm_e_dma.h"       // Emulated DMA code
/*
#define PM_AUDIO_FREQUENCY 49716   // Adlib output frequency
#define PM_SAMPLES_PER_BUFFER 64
#define PM_AUDIO_BUFFERS  8
#define PM_SAMPLES_STRIDE 4        // (Nb of byte per sample 16Bit X 2)
*/
#else // PicoMEM : Remove isa_dma
#include "isa_dma.h"
#endif

static irq_handler_t SBDSP_DMA_isr_pt;
//static dma_inst_t dma_config;
#define DMA_PIO_SM 2

#define DSP_VERSION_MAJOR 2
#define DSP_VERSION_MINOR 1

// Sound Blaster DSP I/O port offsets
#define DSP_RESET           0x6
#define DSP_READ            0xA
#define DSP_WRITE           0xC
#define DSP_WRITE_STATUS    0xC
#define DSP_READ_STATUS     0xE

#define OUTPUT_SAMPLERATE   49716ul     

// Sound Blaster DSP commands.
#define DSP_DIRECT_DAC          0x10    // 1.xx+ :8-bit Output sample in direct mode
#define DSP_DIRECT_ADC          0x20    // 1.xx+ :8-bit Input sample in direct mode 

#define DSP_SET_TIME_CONSTANT   0x40    // 1.xx+ :Set digitized sound transfer constant

#define DSP_DMA_SINGLE          0x14    // 1.xx+ :8-bit DMA PCM Output, Single
//#define DSP_DMA_SINGLE_IN     0x24    // 1.xx+ :8-bit DMA PCM Input, Single
//#define DSP_DMA_ADPCM_3       0x76    // 1.xx+ :8-bit DMA ADPCM 8bit to 3 Output, Single

#define DSP_DMA_HS_AUTO         0x90    // 2.01+ :8-bit DMA PCM Output, Auto, High Speed
#define DSP_DMA_HS_SINGLE       0x91    // 2.01+ :8-bit DMA PCM Output, single, High Speed
#define DSP_DMA_ADPCM           0x7F    // creative ADPCM 8bit to 3bit
#define DSP_DMA_AUTO            0X1C    // length based on 48h
#define DSP_DMA_BLOCK_SIZE      0x48    // 2.00+ : Set block size for highspeed/dma
#define DSP_MIDI_READ_POLL      0x30
#define DSP_MIDI_WRITE_POLL     0x38


#define DSP_DMA_PAUSE           0xD0    // 1.xx+ : DMA Pause
#define DSP_DMA_RESUME          0xD4    // 1.xx+ : DMA Resume
#define DSP_DMA_PAUSE_DURATION  0x80    // 1.xx+ : Pause Output (Used by Tryrian)
#define DSP_ENABLE_SPEAKER      0xD1    // 1.xx+ : Turn On Speaker
#define DSP_DISABLE_SPEAKER     0xD3    // 1.xx+ : Turn Off Speaker
#define DSP_SPEAKER_STATUS      0xD8    // 2.00+ : Get Speaker Status
#define DSP_IDENT               0xE0
#define DSP_VERSION             0xE1    // 1.xx+ : Get DSP Version
#define DSP_WRITETEST           0xE4
#define DSP_READTEST            0xE8
#define DSP_SINE                0xF0
#define DSP_IRQ                 0xF2
#define DSP_CHECKSUM            0xF4

#define DSP_DMA_FIFO_SIZE       1024

#ifdef BOARD_PICOMEM
#define E_DMA_FIRST_DELAY 20
#endif

#include "sbdsp.h"
static sbdsp_t sbdsp;

static uint32_t DSP_DMA_Event(Bitu val);
static uint32_t DSP_E_DMA_Event(Bitu val);


uint16_t sbdsp_fifo_level() {
    if(sbdsp.dma_buffer_tail < sbdsp.dma_buffer_head) return DSP_DMA_FIFO_SIZE - (sbdsp.dma_buffer_head - sbdsp.dma_buffer_tail);
    return sbdsp.dma_buffer_tail - sbdsp.dma_buffer_head;
}
void sbdsp_fifo_rx(uint8_t byte) {    
    if(sbdsp_fifo_level()+1 == DSP_DMA_FIFO_SIZE) printf("OVERRRUN");
    sbdsp.dma_buffer[sbdsp.dma_buffer_tail]=byte;        
    sbdsp.dma_buffer_tail++;
    if(sbdsp.dma_buffer_tail == DSP_DMA_FIFO_SIZE) sbdsp.dma_buffer_tail=0;
}
void sbdsp_fifo_clear() {    
    sbdsp.dma_buffer_head=sbdsp.dma_buffer_tail;
}
bool sbdsp_fifo_half() {
    if(sbdsp_fifo_level() >= (DSP_DMA_FIFO_SIZE/2)) return true;
    return false;
}

uint16_t sbdsp_fifo_tx(char *buffer,uint16_t len) {
    uint16_t level = sbdsp_fifo_level();
    if(!level) return 0;
    if(!len) return 0;
    if(len > level) len=level;
    if(sbdsp.dma_buffer_head < sbdsp.dma_buffer_tail || ((sbdsp.dma_buffer_head+len) < DSP_DMA_FIFO_SIZE)) {          
            memcpy(buffer,sbdsp.dma_buffer+sbdsp.dma_buffer_head,len);
            sbdsp.dma_buffer_head += len;
            return len;
    }
    else {
        memcpy(buffer,sbdsp.dma_buffer+sbdsp.dma_buffer_head,DSP_DMA_FIFO_SIZE-sbdsp.dma_buffer_head);
        memcpy(buffer+256-sbdsp.dma_buffer_head,sbdsp.dma_buffer,len-(DSP_DMA_FIFO_SIZE-sbdsp.dma_buffer_head));        
        sbdsp.dma_buffer_head += (len-DSP_DMA_FIFO_SIZE);
        return len;
    }
    return 0;
}


// Get the Sound Blaster audio from its fifo buffer start at a certain level
// sbdsp is aware of the sample per buffer value
// return false if no audio to send

bool sbdsp_getbuffer(int16_t * buffer) 
{
int8_t sbbuffer[128];  // Intermediate buffer where the SB data is copied
int16_t current_fifo_level;
int32_t buffer_index;  // 16.16 index in the buffer

// check if we have to get Data
// Need to start if > minimum_fifo_level or if same level since 10 call (for Short buffer output)
current_fifo_level=sbdsp_fifo_level();
if (current_fifo_level==sbdsp.previous_level)
   {
    sbdsp.samelevel_count++;
    if (current_fifo_level==0) return false;
   } else
   {
    sbdsp.samelevel_count=0;
   }

// compute the NB of bytes to get from the Buffer
// First 2 Bytres must be the previous buffer read Data (for Continuity)
//sbdsp.previous_level;
//sbdsp.samelevel_count;

return true;
}

static __force_inline void sbdsp_dma_disable() {
    sbdsp.dma_enabled=false;    
#ifndef BOARD_PICOMEM   // PicoMEM : To Code
    PIC_RemoveEvents(DSP_DMA_Event);  
#endif
    sbdsp.cur_sample = 0;  // zero current sample
}

#ifndef BOARD_PICOMEM   // PicoMEM : To Code
static __force_inline void sbdsp_dma_enable() {    
    if(!sbdsp.dma_enabled) {
        // sbdsp_fifo_clear();
        sbdsp.dma_enabled=true;            
        if(sbdsp.dma_pause_duration) {            
            PIC_AddEvent(DSP_DMA_Event,sbdsp.dma_interval * sbdsp.dma_pause_duration,1);
            sbdsp.dma_pause_duration=0;
        }
        else {
            PIC_AddEvent(DSP_DMA_Event,sbdsp.dma_interval,1);
        }
    }
    else {
        //printf("INFO: DMA Already Enabled");        
    }
}
#else
static __force_inline void sbdsp_dma_enable() {    
    if(!sbdsp.dma_enabled) {
        // sbdsp_fifo_clear();
        sbdsp.dma_enabled=true;            
        if(sbdsp.dma_pause_duration) {            
            PIC_AddEvent((PIC_EventHandler) DSP_DMA_Event,sbdsp.dma_interval * sbdsp.dma_pause_duration,1);
            sbdsp.dma_pause_duration=0;
        }
        else {  // For the test, go to IRQ directly
            sbdsp.dma_sample_count++; // Buffer size-1 is sent to the Sound Blaster
            printf("Start DMA: %d\n",sbdsp.dma_interval * sbdsp.dma_sample_count);
            sbdsp.e_dma_time_to_irq=sbdsp.dma_interval * sbdsp.dma_sample_count;
            PIC_AddEvent((PIC_EventHandler) DSP_DMA_Event,sbdsp.e_dma_time_to_irq,1);
        }
    }
    else {
        //printf("INFO: DMA Already Enabled");        
    }
}

static __force_inline void sbdsp_e_dma_enable() {
    if(!sbdsp.dma_enabled) {
        // sbdsp_fifo_clear();
/*
        sbdsp.dma_enabled=true;            
        if(sbdsp.dma_pause_duration) {            
            PIC_AddEvent((PIC_EventHandler) DSP_DMA_Event,sbdsp.dma_interval * sbdsp.dma_pause_duration,1);
            sbdsp.dma_pause_duration=0;
        }
        else 
        { */ 
            // Minimum value to start the Sound Blaster Audio is equivalent to PM_AUDIO_BUFFERS-2 buffers length
            sbdsp.minimum_fifo_level=(sbdsp.sample_step*PM_SAMPLES_PER_BUFFER*(PM_AUDIO_BUFFERS-2))>>16;
            sbdsp.dma_sample_count++;       // Buffer size-1 is sent to the Sound Blaster
            sbdsp.e_dma_samples_read=0;     // Next DMA event will start the copies (No sample read for the moment)
            sbdsp.e_dma_firsttransfer=true; // first transfer (so that the E_DMA Interrupt save the initial address/size)
            printf("Start DMA : i %d s %d",sbdsp.dma_interval, sbdsp.dma_sample_count);
            sbdsp.e_dma_time_to_irq=sbdsp.dma_interval * sbdsp.dma_sample_count -E_DMA_FIRST_DELAY;
            PIC_AddEvent((PIC_EventHandler) DSP_E_DMA_Event,E_DMA_FIRST_DELAY,1);
        }
//    }
    else {
        printf("INFO: DMA Already Enabled");        
    }
}

#endif

#ifndef BOARD_PICOMEM   // PicoMEM : To Code
// DMA transfer Event
static uint32_t DSP_DMA_Event(Bitu val) {
    DMA_Start_Write(&dma_config);    
    uint32_t current_interval;
    sbdsp.dma_sample_count_rx++;    

    if(sbdsp.dma_pause_duration) {
        current_interval = sbdsp.dma_interval * sbdsp.dma_pause_duration;        
        sbdsp.dma_pause_duration=0;
    }
    else {
        current_interval = sbdsp.dma_interval;
    }

    if(sbdsp.dma_sample_count_rx <= sbdsp.dma_sample_count) {        
        return current_interval;   // Continue DMA Events
    }
    else {
        PIC_ActivateIRQ();
        if(sbdsp.autoinit) {            
            sbdsp.dma_sample_count_rx=0;            
            return current_interval;
        }
        else {            
            sbdsp_dma_disable();            
        }
    } 
    return 0;  // No more DMA Events
}
#else  
// PicoMEM Test, generate the IRQ at the end of the DMA copy time
static uint32_t DSP_DMA_Event(Bitu val) {
    uint32_t current_interval;
    if(sbdsp.dma_pause_duration) {
        current_interval = sbdsp.dma_interval * sbdsp.dma_pause_duration;        
        sbdsp.dma_pause_duration=0;
    }
    else {
        current_interval = sbdsp.dma_interval * sbdsp.dma_sample_count;
    }

//  printf("SB IRQ ");
  PM_IRQ_Raise(IRQ_IRQ5,0);

  if(sbdsp.autoinit) {            
        sbdsp.dma_sample_count_rx=0;            
        return current_interval;
    }
    else {            
        sbdsp_dma_disable();            
    }

  return 0;  // No more DMA Event
//  PM_IRQ_Lower();
}

// Emulated DMA event
static uint32_t DSP_E_DMA_Event(Bitu val) {
    uint32_t current_interval;
    uint8_t  bytes_to_copy;

    PM_IRQ_Lower();
    printf("Ev:%d",sbdsp.dma_sample_count_rx);

    if (sbdsp.e_dma_samples_read)
       { // The previous DMA event started a DMA transfer
         // verify if it is finished and push data to the DMA fifo
       }

    if(sbdsp.dma_pause_duration) {  // Pause : Place a longer interval between DMA (Not correct, to change)
        current_interval = sbdsp.dma_interval * sbdsp.dma_pause_duration;        
        sbdsp.dma_pause_duration=0;
    }
    else { // Compute the number of bytes to copy and the time before next copy
        if ((sbdsp.dma_sample_count-sbdsp.dma_sample_count_rx)<256) {
           // This is the last transfer (Never transfer less than 128)
           bytes_to_copy = sbdsp.dma_sample_count-sbdsp.dma_sample_count_rx;
           current_interval = sbdsp.e_dma_time_to_irq;
        } else {  // by default, copy block of 128Bytes
           bytes_to_copy = 128;
           current_interval = 128 * sbdsp.dma_interval;
        }
//        printf("tc%d",bytes_to_copy);
    }

  sbdsp.e_dma_time_to_irq-=current_interval; // Remaining time to the Sound Blaster IRQ
  sbdsp.e_dma_samples_read=bytes_to_copy;

  if (bytes_to_copy==0)
     { // dma_block_size bytes transfer completed > Generate the SB Interrupt
      printf(" I>");
      PM_IRQ_Raise(IRQ_IRQ5,0);

      if(sbdsp.autoinit) {
        sbdsp.dma_sample_count_rx=0;
        sbdsp.e_dma_time_to_irq=sbdsp.dma_interval * sbdsp.dma_block_size-E_DMA_FIRST_DELAY;
         return E_DMA_FIRST_DELAY;  // Start rapidly the next DMA
        }
        else {
            sbdsp_dma_disable();
            return 0;  // No AutoInit > No more DMA Event
        }

     } else
     { // Read the needed bytes from the emulated DMA
        printf(" D>");
  //      e_dma_start(1, 1, sbdsp.autoinit, bytes_to_copy); // Channel, first transfer,
        sbdsp.e_dma_firsttransfer=false;
        sbdsp.dma_sample_count_rx+=bytes_to_copy;
        return current_interval; // interval from when the current interrupt was called
     }

  return 0;  
//  PM_IRQ_Lower();
}

#endif

#ifndef BOARD_PICOMEM   // PicoMEM : To Code
static void sbdsp_dma_isr(void) {
    const uint32_t dma_data = DMA_Complete_Write(&dma_config);    
    sbdsp.cur_sample = (int16_t)(dma_data & 0xFF) - 0x80 << 5;
}
#endif

int16_t sbdsp_sample() {
    return sbdsp.speaker_on ? sbdsp.cur_sample : 0;
} 

void sbdsp_init() {

// Need to be reinitialized when the SB emulation is restarted
    sbdsp.dav_dsp=0;        // No DSP Data available
    sbdsp.dsp_busy=0;       // DSP Not busy
#ifdef BOARD_PICOMEM
    printf("sbdsp_init\n");

    PIC_Init();  // To remove when Audio mix is added !!!!

#else
    puts("Initing ISA DMA PIO...");    
    SBDSP_DMA_isr_pt = sbdsp_dma_isr;
    dma_config = DMA_init(pio0, DMA_PIO_SM, SBDSP_DMA_isr_pt);         
#endif    
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
        case DSP_DMA_PAUSE:
            sbdsp.current_command=0;
#ifndef BOARD_PICOMEM   // PicoMEM : To Code            
            sbdsp_dma_disable();
#endif
            printf("(0xD0) DMA PAUSE\n\r");            
            break;
        case DSP_DMA_RESUME:
            sbdsp.current_command=0;
#ifndef BOARD_PICOMEM   // PicoMEM : To Code            
            sbdsp_dma_enable();                        
#endif            
            printf("DMA RESUME\n\r");                                            
            break;
        case DSP_DMA_AUTO:     
             printf("DMA_AUTO\n\r");                   
            sbdsp.autoinit=1;           
            sbdsp.dma_sample_count = sbdsp.dma_block_size;
            sbdsp.dma_sample_count_rx=0;
//#ifndef BOARD_PICOMEM   // PicoMEM : To Code
            sbdsp_dma_enable();            
//#endif            
            sbdsp.current_command=0;                 
            break;        
        case DSP_DMA_HS_AUTO:    // 2.01+ :8-bit DMA PCM Output, Auto, High Speed
            printf("DMA_HS_AUTO\n\r");            
            sbdsp.dav_dsp=0;
            sbdsp.current_command=0;  
            sbdsp.autoinit=1;
            sbdsp.dma_sample_count = sbdsp.dma_block_size;
            sbdsp.dma_sample_count_rx=0;
//#ifndef BOARD_PICOMEM   // PicoMEM : To Code
            sbdsp_e_dma_enable();            
//#endif            
            break;            

        case DSP_SET_TIME_CONSTANT:
            if(sbdsp.dav_dsp) {
                if(sbdsp.current_command_index==1) {                              
                    sbdsp.time_constant = sbdsp.inbox;
                    printf("DSP_SET_TIME_CONSTANT %d\n\r",sbdsp.time_constant);
                    /*
                    sbdsp.sample_rate = 1000000ul / (256 - sbdsp.time_constant);           
                    sbdsp.dma_interval = 1000000ul / sbdsp.sample_rate; // redundant.                    
                    */
                    sbdsp.sample_rate = 1000000ul / (256 - sbdsp.time_constant);                   
                    sbdsp.dma_interval = 256 - sbdsp.time_constant;         // Interval is the Nb of us between each sample read

#ifdef BOARD_PICOMEM
                    sbdsp.sample_step = (sbdsp.sample_rate * 65535ul) / PM_AUDIO_FREQUENCY; // Step for the Audio rate convertion
#endif

                    // sbdsp.sample_rate = 1000000ul / sbdsp.dma_interval;           
                    // sbdsp.sample_step = sbdsp.sample_rate * 65535ul / OUTPUT_SAMPLERATE;                    
                    // sbdsp.sample_factor = (OUTPUT_SAMPLERATE / sbdsp.sample_rate)+5; //Estimate                    
                    // sbdsp.dma_transfer_size = 4;                    
                    // sbdsp.i2s_buffer_size = ((OUTPUT_SAMPLERATE * 65535ul) / sbdsp.sample_rate * sbdsp.dma_buffer_size) >> 16;
                    
                    
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;                    
                }    
                sbdsp.current_command_index++;
            }
            break;
        case DSP_DMA_BLOCK_SIZE:            
            if(sbdsp.dav_dsp) {                             
                if(sbdsp.current_command_index==1) {      //Block size LSB                  
                    sbdsp.dma_block_size=sbdsp.inbox;
                    sbdsp.dav_dsp=0;                    
                }
                else if(sbdsp.current_command_index==2) { // Block size MSB
                    sbdsp.dma_block_size += (sbdsp.inbox << 8);
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;          
                    printf("Set BlockSize:%u\n\r",sbdsp.dma_block_size);                                        
                }
                sbdsp.current_command_index++;
            }
            break;          
        
        case DSP_DMA_HS_SINGLE:    // 2.01+ :8-bit DMA PCM Output, single, High Speed
            printf("DMA_HS_SINGLE\n\r");            
            sbdsp.dav_dsp=0;
            sbdsp.current_command=0;  
            sbdsp.autoinit=0;
            sbdsp.dma_sample_count = sbdsp.dma_block_size;
            sbdsp.dma_sample_count_rx=0;
//#ifndef BOARD_PICOMEM   // PicoMEM : To Code
            sbdsp_dma_enable();            
//#endif            
            break;            
        case DSP_DMA_SINGLE:              
            if(sbdsp.dav_dsp) {            
                if(sbdsp.current_command_index==1) {
                    sbdsp.dma_sample_count = sbdsp.inbox;
                    sbdsp.dav_dsp=0;                    
                }
                else if(sbdsp.current_command_index==2) {
                    sbdsp.dma_sample_count += (sbdsp.inbox << 8);
                    sbdsp.dma_sample_count_rx=0;                    
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;  
                    sbdsp.autoinit=0;                                  
                    printf("DMA_SINGLE %u\n",sbdsp.dma_sample_count);                                        
//#ifndef BOARD_PICOMEM   // PicoMEM : To Code
                    sbdsp_dma_enable();
//#endif                    
                }
                sbdsp.current_command_index++;
            }                        
            break;            
        case DSP_IRQ:
            sbdsp.current_command=0;
#ifdef BOARD_PICOMEM            
            PM_IRQ_Raise(IRQ_IRQ5,0);
#else            
            PIC_ActivateIRQ();
#endif
            break;            
        case DSP_VERSION:
            if(sbdsp.current_command_index==0) {
                sbdsp.current_command_index=1;
                sbdsp_output(DSP_VERSION_MAJOR);
            }
            else {
                if(!sbdsp.dav_pc) {
                    sbdsp.current_command=0;                    
                    sbdsp_output(DSP_VERSION_MINOR);
                }
                
            }
            break;
        case DSP_IDENT:
            if(sbdsp.dav_dsp) {            
                if(sbdsp.current_command_index==1) {                    
                    printf("DSP_IDENT\n\r");
                    sbdsp.dav_dsp=0;                    
                    sbdsp.current_command=0;        
                    sbdsp_output(~sbdsp.inbox);                                        
                }                
                sbdsp.current_command_index++;
            }                                       
            break;
        case DSP_ENABLE_SPEAKER:
            printf("ENABLE SPEAKER\n");
            sbdsp.speaker_on = true;
            sbdsp.current_command=0;
            break;
        case DSP_DISABLE_SPEAKER:
            printf("DISABLE SPEAKER\n");
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
                    sbdsp.cur_sample=(int16_t)(sbdsp.inbox) - 0x80 << 5;
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;
                }
                sbdsp.current_command_index++;
            }
            break;
        //case DSP_DIRECT_ADC:
        //case DSP_MIDI_READ_POLL:
        //case DSP_MIDI_WRITE_POLL:
        
        //case DSP_HALT_DMA:       
        case DSP_WRITETEST:
            if(sbdsp.dav_dsp) {            
                if(sbdsp.current_command_index==1) {                    
                    printf("(0xE4) DSP_WRITETEST\n\r");
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
        
        case DSP_DMA_PAUSE_DURATION:
            if(sbdsp.dav_dsp) {                             
                if(sbdsp.current_command_index==1) {                    
                    sbdsp.dma_pause_duration_low=sbdsp.inbox;
                    sbdsp.dav_dsp=0;                    
                }
                else if(sbdsp.current_command_index==2) {                    
                    sbdsp.dma_pause_duration = sbdsp.dma_pause_duration_low + (sbdsp.inbox << 8);
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;          
                    printf("(0x80) Pause Duration:%u\n\r",sbdsp.dma_pause_duration);                                        
                }
                sbdsp.current_command_index++;
            }
            break;       
        //case DSP_SINE:
        //case DSP_CHECKSUM:          
        case 0:
            //not in a command
            break;            
        default:
            printf("Unknown Command: %x\n",sbdsp.current_command);
            sbdsp.current_command=0;
            break;

    }                
    sbdsp.dsp_busy=0;
}
static __force_inline void sbdsp_reset(uint8_t value) {
    //TODO: COLDBOOT ? WARMBOOT ?    
    value &= 1; // Some games may write unknown data for bits other than the LSB.
    switch(value) {
        case 1:                        
            sbdsp.autoinit=0;
#ifndef BOARD_PICOMEM   // PicoMEM : To Code            
            sbdsp_dma_disable();
#endif            
            sbdsp.reset_state=1;
            break;
        case 0:
            if(sbdsp.reset_state==0) return; 
            if(sbdsp.reset_state==1) {                
                sbdsp.reset_state=0;                
                sbdsp.outbox = 0xAA;
                sbdsp.dav_pc=1;
                sbdsp.current_command=0;
                sbdsp.current_command_index=0;

                sbdsp.dma_block_size=0x7FF; //default per 2.01
                sbdsp.dma_sample_count=0;
                sbdsp.dma_sample_count_rx=0;              
                sbdsp.speaker_on = false;
            }
            break;
        default:
            break;
    }
}

uint8_t sbdsp_read(uint8_t address) 
{
//    uint8_t x;
    switch(address) {        
        case DSP_READ:          // 0x0A : Return DSP Data, if the outbox is not empty
            sbdsp.dav_pc=0;
            return sbdsp.outbox;
        case DSP_READ_STATUS:   // 0x0E : Return bit7=1 if there is data to read from the DSP (Also acknowledge IRQ)
#ifdef BOARD_PICOMEM        
            PM_IRQ_Lower();
#else
            PIC_DeActivateIRQ();
#endif
//            printf("iAck");
            return (sbdsp.dav_pc << 7);
        case DSP_WRITE_STATUS:  // 0x0C : Return bit7=0 if the DSP is ready to receive Command/Data
            return (sbdsp.dav_dsp | sbdsp.dsp_busy) << 7;                                
        default:
            printf("SB READ: %x\n\r",address);
            return 0xFF;            
    }
}

#ifdef BOARD_PICOMEM
// Return true if a value is really written
bool sbdsp_write(uint8_t address, uint8_t value) 
#else
void sbdsp_write(uint8_t address, uint8_t value)
#endif
{
    switch(address) {         
        case DSP_WRITE:         // 0x0C : Write command/Data to the DSP
            if(sbdsp.dav_dsp) printf("WARN - DAV_DSP OVERWRITE\n");
            sbdsp.inbox = value;
            sbdsp.dav_dsp = 1;
#ifdef BOARD_PICOMEM
            return true;
#endif
            break;            
        case DSP_RESET:         // 0x06 : Reset Port (Write 0 then 1)
            sbdsp_reset(value);
            break;        
        default:
            printf("SB WRITE: %x => %x \n\r",value,address);                      
            break;
    }
#ifdef BOARD_PICOMEM
return false;
#endif
}