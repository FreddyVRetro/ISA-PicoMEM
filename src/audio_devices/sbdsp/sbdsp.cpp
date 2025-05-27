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
uint8_t nodata_cnt;

volatile bool in_sb_irq;
volatile bool in_isa_dma;

#ifdef BOARD_PICOMEM

#include "dev_audiomix.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "pm_audio.h"
#include "../../pm_libs/isa_dma.h"       // ISA/PC DMA code
#include "../../pm_libs/isa_irq.h"       // ISA/PC IRQ code
#include "dev_sbdsp.h"

#else // PicoMEM : Remove isa_dma
#include "isa_dma.h"
#endif

#define DMA_TRANSFER_CNT 128

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

#define DSP_DMA_BLOCK_SIZE      0x48    // 2.00+ : Set block size for highspeed/dma
#define DSP_DMA_HS_AUTO         0x90    // 2.01+ :8-bit DMA PCM Output, Auto, High Speed
#define DSP_DMA_HS_SINGLE       0x91    // 2.01+ :8-bit DMA PCM Output, single, High Speed
#define DSP_DMA_ADPCM           0x7F    // creative ADPCM 8bit to 3bit
#define DSP_DMA_AUTO            0X1C    // length based on 48h
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


#define DSP_MLC_DMA_START    1
#define DSP_MLC_DMA_STOP     2
#define DSP_MLC_DSP_RESET    3
#define DSP_MLC_SENDIRQ      4
#define DSP_MLC_SENDIRQ_WAIT 5

//#define DSP_DMA_FIFO_SIZE       1024

#ifdef BOARD_PICOMEM
#define E_DMA_FIRST_DELAY 20
#endif

#include "sbdsp.h"
sbdsp_t sbdsp;

static uint32_t DSP_DMA_IRQ_Event(Bitu val);

static __force_inline void sbdsp_dma_disable() {
    sbdsp.dma_enabled=false;    
//    PIC_RemoveEvents((PIC_EventHandler) DSP_DMA_IRQ_Event);
    isa_dma_stop();
    dev_sbdsp_playing=false;
    dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_SBDSP;
}

static __force_inline void sbdsp_dma_reset() {
    sbdsp.dma_enabled=false;
    sbdsp.autoinit=0;
//    PIC_RemoveEvents((PIC_EventHandler) DSP_DMA_IRQ_Event);
    isa_dma_reset();
    sbdsp.increment    = 0;
    sbdsp.readsample   = true;
    sbdsp.prev_sample8 = 0;
    dev_sbdsp_playing  = false;
    dev_audiomix.dev_active = dev_audiomix.dev_active & ~AD_SBDSP;
}

static __force_inline void sbdsp_dma_enable() {    
    if(!sbdsp.dma_enabled) {
        sbdsp.dma_sample_count++; // Buffer size-1 is sent to the Sound Blaster
        sbdsp.dma_sample_count_rx=sbdsp.dma_sample_count;
        sbdsp.dma_interval = 256 - sbdsp.time_constant;                  // Interval is the Nb of us between each sample read
        sbdsp.dma_time_to_irq=sbdsp.dma_interval * sbdsp.dma_sample_count;

        // Initialize the timing values for the DMA Buffer read
        sbdsp.sample_rate  = 1000000ul / (256 - sbdsp.time_constant);
        sbdsp.sample_step  = (sbdsp.sample_rate * 65535ul) / PM_AUDIO_FREQUENCY; // Step for the Audio rate convertion
        sbdsp.increment    = 0;
        sbdsp.readsample   = true;
        sbdsp.prev_sample8 = 0;

        PM_INFO("Start DMA: %duS %dKHz\n",sbdsp.dma_time_to_irq,sbdsp.sample_rate);
        //  isa_irq_lower();  // Test !
        PIC_AddEvent((PIC_EventHandler) DSP_DMA_IRQ_Event,sbdsp.dma_time_to_irq,2);
        isa_dma_start_fifo(1,sbdsp.dma_sample_count,DMA_TRANSFER_CNT);   // 512 bytes per transfer 
        sbdsp.dma_enabled=true;
        dev_sbdsp_playing=true;
        dev_audiomix.dev_active = dev_audiomix.dev_active | AD_SBDSP;
    }
    else {      
        PM_INFO("INFO: DMA already enabled");        
    }
}

static uint32_t DSP_DMA_IRQ_Event(Bitu val) {
in_sb_irq=true;
DBG_ON_IRQ_SB();
//PM_INFO("!SB ");
  if (sbdsp.dma_enabled) 
     { 
/*        if (in_isa_dma==true) 
           {
            DBG_ON_1();
            PM_INFO("D ");
           }
        if (!isa_irq_Wait_End_Timeout(10000))
        { 
          sbdsp.mainloop_action=DSP_MLC_SENDIRQ;
          PM_INFO("TIMEOUT, IRQ still in progress\n");
        } else isa_irq_raise(IRQ_IRQ5,0,true);
*/
  /*     if ((in_isa_dma==true) ||(isa_dma.transfer_remain))
          {
           if(sbdsp.autoinit) isa_dma_continue_fifo(1,sbdsp.dma_sample_count);   //Continue the transfer when the previous is finished
            else sbdsp.dma_enabled=false;
          }
*/            
       sbdsp.mainloop_action=DSP_MLC_SENDIRQ_WAIT;

      if(sbdsp.autoinit) {     
          if (isa_dma.transfer_remain) DBG_ON_2();
//          PM_INFO("C %d %d\n",isa_dma.transfer_remain,get_mixed_buffer_nb());  // MODM XT Bug quand enleve !!
//          isa_dma_continue_fifo(1,sbdsp.dma_sample_count);   //Continue the transfer when the previous is finished
          DBG_OFF_IRQ_SB();
          DBG_OFF_2();
          in_sb_irq=false;
          return sbdsp.dma_time_to_irq;  //current_interval
         }
        else {
//          PM_INFO("End %d %d\n",sbdsp.dma_sample_count_rx,get_mixed_buffer_nb());
//          sbdsp.dma_enabled=false;
      //    sbdsp_dma_disable();
         }
   } else PM_INFO("Off");

  DBG_OFF_IRQ_SB();
  in_sb_irq=false;
  return 0;  // No more DMA Event
}

#ifndef BOARD_PICOMEM
static void sbdsp_dma_isr(void) {
    const uint32_t dma_data = DMA_Complete_Write(&dma_config);    
    sbdsp.cur_sample = (int16_t)(dma_data & 0xFF) - 0x80 << 5;
}

int16_t sbdsp_sample() {
    return sbdsp.speaker_on ? sbdsp.cur_sample : 0;
} 
#endif

void sbdsp_init(uint8_t sbirq) 
{
    PM_INFO("sbdsp_init, IRQ %d\n",sbirq);

// Need to be reinitialized when the SB emulation is restarted
    sbdsp.dav_dsp=0;        // No DSP Data available
    sbdsp.dsp_busy=0;       // DSP Not busy
    sbdsp.mainloop_action=0;

    bool in_sb_irq=false;
    bool in_isa_dma=false;

    switch (sbirq)
     {
      case 3:sbdsp.irq=IRQ_IRQ3;
             break;
      case 5:sbdsp.irq=IRQ_IRQ5;
             break;
      case 7:sbdsp.irq=IRQ_IRQ7;
             break;
     }
#ifdef BOARD_PICOMEM
//    PM_INFO("sbdsp_init > PIC_init to remove !!!\n");
//    PIC_Init();  // To remove when Audio mix is added !!!!

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
            sbdsp_dma_disable();
            PM_INFO("(0xD0) DMA PAUSE\n\r");            
            break;

        case DSP_DMA_RESUME:
            sbdsp.current_command=0;        
//            sbdsp_dma_enable();
            PM_INFO("DMA RESUME\n\r");                                            
            break;

        case DSP_DMA_AUTO:     
            PM_INFO("DMA_AUTO\n\r");                   
            sbdsp.autoinit=1;           
            sbdsp.dma_sample_count = sbdsp.dma_block_size;
            sbdsp_dma_enable();                    
            sbdsp.current_command=0;                 
            break;

        case DSP_DMA_HS_AUTO:    // 2.01+ :8-bit DMA PCM Output, Auto, High Speed
            PM_INFO("DMA_HS_AUTO\n");            
            sbdsp.dav_dsp=0;
            sbdsp.current_command=0;  
            sbdsp.autoinit=1;
            sbdsp.dma_sample_count = sbdsp.dma_block_size;
            sbdsp_dma_enable();                   
            break;

        case DSP_SET_TIME_CONSTANT:
            if(sbdsp.dav_dsp) {
                if(sbdsp.current_command_index==1) {                              
                    sbdsp.time_constant = sbdsp.inbox;
            //        PM_INFO("DSP_SET_TIME_CONSTANT %d\n",sbdsp.time_constant);                   
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
                    //PM_INFO("Set BlockSize:%u\n\r",sbdsp.dma_block_size); 
                    PM_INFO("SB BSize:%u ",sbdsp.dma_block_size);
                }
                sbdsp.current_command_index++;
            }
            break;          
        
        case DSP_DMA_HS_SINGLE:    // 2.01+ :8-bit DMA PCM Output, single, High Speed
            PM_INFO("DMA_HS_SINGLE\n");            
            sbdsp.dav_dsp=0;
            sbdsp.current_command=0;  
            sbdsp.autoinit=0;
            sbdsp.dma_sample_count = sbdsp.dma_block_size;
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
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;  
                    sbdsp.autoinit=0;                                  
                    PM_INFO("DMA_SINGLE %u\n",sbdsp.dma_sample_count);                                        
//#ifndef BOARD_PICOMEM   // PicoMEM : To Code
                    sbdsp_dma_enable();
//#endif                    
                }
                sbdsp.current_command_index++;
            }                        
            break;

        case DSP_IRQ:
            PM_INFO("DSP_IRQ\n");
            sbdsp.current_command=0;         
            isa_irq_raise(IRQ_IRQ5,0,true);

  //          busy_wait_us(50);            // If present, Prince of persio no more detect/use the SB
  //          isa_irq_lower();
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
                    PM_INFO("DSP_IDENT\n");
                    sbdsp.dav_dsp=0;                    
                    sbdsp.current_command=0;        
                    sbdsp_output(~sbdsp.inbox);                                        
                }                
                sbdsp.current_command_index++;
            }                                       
            break;

        case DSP_ENABLE_SPEAKER:
            PM_INFO("ENABLE SPEAKER\n");
            PM_INFO("ENABLE SPEAKER\n");            
            sbdsp.speaker_on = true;
            sbdsp.current_command=0;
            break;

        case DSP_DISABLE_SPEAKER:
            PM_INFO("DISABLE SPEAKER\n");
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
#ifdef BOARD_PICOMEM
#else
                    sbdsp.cur_sample=(int16_t)(sbdsp.inbox) - 0x80 << 5;
#endif
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
                    PM_INFO("(0xE4) DSP_WRITETEST\n\r");
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
                    PM_INFO("(0x80) Pause Duration:%u\n\r",sbdsp.dma_pause_duration);                                        
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
            PM_INFO("Unknown Command: %x\n",sbdsp.current_command);
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
            PM_INFO("SB RST\n");
            sbdsp_dma_reset();    
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
 switch(address) {        
      case DSP_READ:          // 0x0A : Return DSP Data, if the outbox is not empty
            sbdsp.dav_pc=0;
            return sbdsp.outbox;
      case DSP_READ_STATUS:   // 0x0E : Return bit7=1 if there is data to read from the DSP (Also acknowledge IRQ)
#ifdef BOARD_PICOMEM        
            isa_irq_lower();
#else
            PIC_DeActivateIRQ();
#endif
//            PM_INFO("iAck");
            return (sbdsp.dav_pc << 7);
      case DSP_WRITE_STATUS:  // 0x0C : Return bit7=0 if the DSP is ready to receive Command/Data
            return (sbdsp.dav_dsp | sbdsp.dsp_busy) << 7;                                
      default:
//            PM_INFO("SB READ: %x\n\r",address);
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
            if(sbdsp.dav_dsp) PM_INFO("WARN - DAV_DSP OVERWRITE\n");
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
          //  PM_INFO("SB WRITE: %x => %x \n\r",value,address);                      
            break;
    }
#ifdef BOARD_PICOMEM
return false;
#endif
}

// commands to be executed in the main loop
void sbdsp_ml_command()
{
  DBG_ON_DMA();
  in_isa_dma=true;
  isa_dma_transfer_fifo();
  in_isa_dma=false;
  DBG_OFF_DMA(); 
  DBG_OFF_1();

  switch(sbdsp.mainloop_action)
  {
    case DSP_MLC_DMA_START:
         PM_INFO("DSP_MLC_DMA_START\n");
         sbdsp.mainloop_action=0;
         break;
    case DSP_MLC_DMA_STOP:
         PM_INFO("DSP_MLC_DMA_STOP\n");    
         sbdsp.mainloop_action=0;
         break;
    case DSP_MLC_DSP_RESET:
         PM_INFO("DSP_MLC_DSP_RESET\n");    
         sbdsp.mainloop_action=0;
         break;
    case DSP_MLC_SENDIRQ: // Sed the SB IRQ directly (triggerend by the SB Timer Interrupt)
         isa_irq_raise(IRQ_IRQ5,0,true);
         sbdsp.mainloop_action=0;
//         PM_INFO("DSP_MLC_SENDIRQ\n");
         break;
    case DSP_MLC_SENDIRQ_WAIT: // Send the SB IRQ When DMA transfer is finished (triggerend by the SB Timer Interrupt)
         if ((isa_dma.transfer_remain==0)&&sbdsp.dma_enabled)
            {
             isa_irq_raise(IRQ_IRQ5,0,true);
             if(sbdsp.autoinit) isa_dma_continue_fifo(1,sbdsp.dma_sample_count);   //Continue the transfer when the previous is finished
               else sbdsp.dma_enabled=false;
               PM_INFO(">SBI\n");
             sbdsp.mainloop_action=0;  
            }
//         PM_INFO("DSP_MLC_SENDIRQ\n");
         break;
  }

}

// return an audio buffer by reading from the DMA buffer
void sbdsp_getbuffer(int16_t* buff,uint32_t samples)
{
  uint8_t res8u;
  uint16_t prev_increment;
  int16_t fres=0;
  res8u=sbdsp.prev_sample8;

//  PM_INFO("SB_GetBuffer\n");
  if (!isa_dma_fifo_empty())    // Start the Audio only when the DMA FIFO is not empty
    {
     for (int8_t sn=0;sn<samples*2;sn+=2)  // Stereo, so 2x more data than "samples"
      {
       // 8Bit Not signed
       if (!isa_dma_fifo_empty()) 
         {
          if (sbdsp.readsample) 
             { 
               res8u=isa_dma_fifo_getb();
               if (sbdsp.dma_sample_count_rx--==0);
                  if (sbdsp.autoinit) sbdsp.dma_sample_count_rx=sbdsp.dma_sample_count;               
             }
          fres=(int16_t) ((res8u<<8)+0x8000)/2;  // Convert to 16Bit signed
          prev_increment=sbdsp.increment; 
          sbdsp.increment+=sbdsp.sample_step;    // Adjust the DMA buffer to the Output frequency 
          sbdsp.readsample = (sbdsp.increment<=prev_increment) ? true : false;  
         } //else PM_INFO("y");
//       PM_INFO("%X,%d ",res8u,fres);
       buff[sn]   += fres;
       buff[sn+1] += fres;    
    }
    } //else PM_INFO("x");
 sbdsp.prev_sample8=res8u;  // Save the last Sample for the next time
// PM_INFO("\n");
}