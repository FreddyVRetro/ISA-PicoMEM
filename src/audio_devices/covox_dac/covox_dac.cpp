#include <stdio.h>

#define CVX_SIZE 128
#define CVX_FREQUENCY 49716
#define CVX_PERIOD           // Period in number of us

// value  : sample value
// repeat : the number of time it is repeated
// repeat represend a delay of repeat/(output frequency) seconds

typedef struct samples_8b_mono {
    uint8_t value;
    uint8_t repeat;
} samples_8b_mono;

volatile uint8_t dac_head;
volatile uint8_t dac_tail;
volatile uint16_t dac_length;
uint32_t dac_previous_time; //
uint32_t dec_dfifo_duration;

samples_8b_mono cvx_dac_buffer[CVX_SIZE]

// dfifo : FIFO in Delta value (Value, count)
// fifo used for audio with variable input and fixed output rate

void dac_resetbuffer()
{
 // Reset the buffer
 cvx_head=1;
 cvx_tail=0;
 vcx_length=0;
}

void dac_dfifo_push(int8_t value,uint16_t count );
{

 if (value==samples_8b_mono[cvx_head-1].value)
    {  // Add more time to the previous sample
      current_increment+=
    } else
      {  // Add a new sample

      }

    remain_increment=current_increment;
 do {
     increment=(current_increment<256) ? (uint8_t)current_increment : 255)
     remain_increment-=increment;
                    
     cvx_buffer_duration+=increment;
    } while (remain_increment!=0)

}

void dac_dfifo_push_delay(int8_t value, uint32_t time)
{
 uint32_t stored_time;          // time "added" in the buffer
 uint32_t current_increment;
 uint32_t remain_increment;
 uint8_t increment;

// add_sample change only the head
 if (cvx_head!=cvx_tail)
    {

     if (!dac_length) cvx_previous_time=time;
     if (time<dac_previous_time) 
        {
         PM_INFO("time>cvx_previous_time");
         current_increment=1;
        }
        else current_increment=(uint32_t)(((uint64_t)(time-dac_previous_time)*CVX_FREQUENCY)/1000000);

     if (current_increment==0) current_increment=1

     stored_time=(uint32_t)(((uint64_t)(current_increment)*1000000/CVX_FREQUENCY));
     dac_previous_time+=stored_time;  // move the previous time value (Used for delta)

    } else PM_ERROR("dac_dfifo Overflow\n");

}