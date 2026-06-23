#ifndef P_TRACE_H
#define P_TRACE_H

#include "hardware/clocks.h"


// Format :
// 32 Bits : TimeStamp (us)
// 32 Bits : Data 0
//            Bit 31 : 0 > More than 32bit event
//                   : 1 > 32 Bit event
//                     Bit 30 : 0 > Bit 29-28  0 : IOR  1: IOW 20 Next Port 8 Last Value
//                              1 > 8 or 16Bit value, the 14Bit after is the Value ID, then 16 last is the value
//

                    

// 20+8 28 bit + 
// 
#define TREV_IOR 8+0<<28
#define TREV_IOW 8+1<<28
 


typedef struct {
    uint32_t buffer[TR_BUFFER_SIZE];
    volatile uint32_t write_idx;
    volatile uint32_t read_idx;
    volatile uint32_t in_fifo;
    volatile uint32_t received;
    volatile uint32_t sent;    
} p_trace_buffer_t;

__force_inline void P_TRACE_PUSH1(uint32_t value) 
{
    if ((p_trace.received-p_trace.sent) == TR_BUFFER_SIZE) return;
    p_trace.buffer[p_trace.write_idx] = value;
    p_trace.write_idx = (p_trace.write_idx + 1) & (TR_INDEX_MASK);
    p_trace.received++;
}

__force_inline void P_TRACE_PUSH2(uint32_t value1,uint32_t value2)
{
    if ((p_trace.received-p_trace.sent) == TR_BUFFER_SIZE-1) return;
    p_trace.buffer[p_trace.write_idx] = value1;
    p_trace.buffer[p_trace.write_idx] = value2;
    p_trace.write_idx = (p_trace.write_idx + 2) & (TR_INDEX_MASK);
    p_trace.received+=2;
}

__force_inline void P_TRACE_IOR(uint32_t Addr,uint32_t Data)
{
  P_TRACE_PUSH2( ,TREV_IOR+(Addr&0x3FF)<<8+Data&0xFF)
}

__force_inline void P_TRACE_IOR(uint32_t Addr,uint32_t Data)
{
  P_TRACE_PUSH2( ,TREV_IOW+(Addr&0x3FF)<<8+Data&0xFF)
}