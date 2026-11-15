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

/* dev_picomem.cpp : PicoMEM I/O Code
  Default port is "2A0-2A3"
 */ 

#include <stdio.h>
#include <string.h>     // memcpy (PMIO trace ring drain)
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "pico/multicore.h"

#include "dev_picomem_io.h"
#include "bus_irq.h"
#include "isa_dma.h"

volatile uint8_t PM_TestRead=0;
uint8_t PM_CMDState=0;  //Command State Machine State (0 : Unlocked; 1 : Intermediate : 2: locked;)

// =====================================================================
// PMIO trace ring buffer (deep diagnostic)
// ---------------------------------------------------------------------
// Single-producer (Core 1 bus handler via dev_pmio_ior/iow), single-
// consumer (Core 0 main loop calls pmio_trace_drain()). Lock-free,
// uses head/tail counters with `dmb sy` for cross-core ordering.
//
// Each entry captures EVERYTHING needed to reconstruct the host<->Pico
// protocol state at the moment of the access:
//
//   t_us  : 32-bit wall-clock microseconds (low 32 bits of time_us_64)
//           -- gives us cycle-level deltas between accesses, so we can
//           see if PMBIOS is polling fast (~50 us spacing) or stalled
//   type  : 0 = IOR, 1 = IOW
//   port  : low 3 bits of CTRL_AL8  (0=STAT 1=DATAL 2=DATAH 3=TEST 4=TR_L)
//   data  : the byte read from / written to the host bus
//   stat  : PM_Status   at the moment of the access (what the host
//           would see on a STAT read at the same instant)
//   cmd   : PM_Command  (what CMD is currently dispatched)
//   cs    : PM_CMDState (lock state machine — 0=unlocked, 1/2=locked)
//   seq   : low 16 bits of a sequence counter -- detects ring overrun
//           (consumer compares to its own running counter; mismatch =
//           dropped entries between drains)
//
// 16 bytes per entry (timestamp + 4 bytes of state + 4 bytes pad/seq);
// 256 entries = 4 KB. ~50 entries per CMD round-trip in normal traffic,
// ~5 round-trips per buffer fill — Core 0 drains every loop iteration,
// so overrun only happens during a hard bus-storm (which itself would
// be diagnostic).
//
// Disable with -DPMIO_TRACE=0 if you ever need to remove the cost.
// =====================================================================
#ifndef PMIO_TRACE
// Default disabled in production. Enable with -DPMIO_TRACE=1 (build
// flag) for chassis-IOW timing diagnostics. The ring captures every
// PM IO access (port + data + PM_Status + PM_Command at the instant)
// with microsecond timestamps. Disabled by default because the
// drain prints up to 32 formatted lines per main-loop iteration and
// PICO_STDIO_DEADLOCK_TIMEOUT_MS=100 means each blocked printf can
// stall Core 0 for 100 ms when the CDC TX FIFO is full -- which
// happens FAST under chassis IO load (mouse IRQ ACKs, DOS background).
// For short captures, enable temporarily AND drop the timeout to 0,
// OR rate-limit the drain to a few entries per call.
#define PMIO_TRACE 0
#endif

#if PMIO_TRACE
#define PMIO_TRACE_SIZE  256
#define PMIO_TRACE_MASK  (PMIO_TRACE_SIZE - 1)

typedef struct __attribute__((packed)) {
    uint32_t t_us;       /* time_us_32() at producer */
    uint8_t  type;       /* 0=R 1=W */
    uint8_t  port;       /* 0..7 */
    uint8_t  data;
    uint8_t  stat;
    uint8_t  cmd;
    uint8_t  cs;
    uint16_t seq;
} pmio_trace_t;

static_assert(sizeof(pmio_trace_t) == 12, "pmio_trace_t size must be 12B");

volatile pmio_trace_t pmio_trace_buf[PMIO_TRACE_SIZE];
volatile uint32_t     pmio_trace_head = 0;   /* written by Core 1 only */
volatile uint32_t     pmio_trace_tail = 0;   /* written by Core 0 only */
volatile uint32_t     pmio_trace_drops = 0;  /* incremented when consumer falls behind */

static inline void pmio_trace_push(uint8_t type, uint8_t port, uint8_t data) {
    uint32_t h = pmio_trace_head;
    /* Detect ring full -- if producer about to lap consumer, drop & count.
       Cheaper than blocking; the consumer will see the drop count next drain. */
    if ((h - pmio_trace_tail) >= PMIO_TRACE_SIZE) {
        pmio_trace_drops++;
        return;
    }
    /* Build the entry on the stack, then memcpy into the volatile ring.
       Avoids C++'s ban on writing fields through a (volatile-stripped) pointer. */
    pmio_trace_t e;
    e.t_us = (uint32_t)time_us_64();
    e.type = type;
    e.port = port & 0x07;
    e.data = data;
    e.stat = PM_Status;
    e.cmd  = PM_Command;
    e.cs   = PM_CMDState;
    e.seq  = (uint16_t)h;
    memcpy((void *)&pmio_trace_buf[h & PMIO_TRACE_MASK], &e, sizeof(e));
    __asm volatile ("dmb sy" ::: "memory");
    pmio_trace_head = h + 1;
}

void pmio_trace_drain(void) {
    /* Called from Core 0 main loop. Drains up to 32 entries per call so
       we don't starve the rest of the loop. Prints relative timestamps
       (delta from previous entry) for compact + readable output. */
    static uint32_t last_t_us = 0;
    static uint32_t last_drops = 0;

    if (pmio_trace_drops != last_drops) {
        printf("[PMIO !! %lu drops]\n",
               (unsigned long)(pmio_trace_drops - last_drops));
        last_drops = pmio_trace_drops;
    }

    int budget = 32;
    while (budget-- > 0 && pmio_trace_tail != pmio_trace_head) {
        pmio_trace_t e;
        /* memcpy out of the volatile array — C++ disallows direct copy of
           a volatile struct without an explicit volatile-qualified copy ctor. */
        memcpy(&e,
               (const void *)&pmio_trace_buf[pmio_trace_tail & PMIO_TRACE_MASK],
               sizeof(e));
        const char *name = (e.port == 0) ? "STAT"
                         : (e.port == 1) ? "DATAL"
                         : (e.port == 2) ? "DATAH"
                         : (e.port == 3) ? "TEST"
                         : (e.port == 4) ? "TR_L"
                         : (e.port == 5) ? "TR_H"
                         : "?";
        uint32_t dt = e.t_us - last_t_us;
        printf("[PMIO #%04X +%7luus %c %-5s = %02X | stat=%02X cmd=%02X cs=%u]\n",
               e.seq, (unsigned long)dt,
               e.type ? 'W' : 'R', name, e.data,
               e.stat, e.cmd, (unsigned)e.cs);
        last_t_us = e.t_us;
        pmio_trace_tail++;
    }
}
#else
static inline void pmio_trace_push(uint8_t, uint8_t, uint8_t) {}
void pmio_trace_drain(void) {}
#endif

volatile uint16_t PM_TR_CNT_L=0;
volatile uint16_t PM_TR_CNT_H=0;
bool PM_RE_ISREAD;
bool PM_TR_IS16B;
uint8_t * PM_TR_PTR;                 // Pointer to the table used by the IO Read / Write command


uint8_t dev_pmio_install()
{
  return 0;
}

void dev_pmio_remove(uint16_t BasePort)
{
}

// Started in the Main Command Wait Loop
void dev_pmio_update()
{
}

void test_pmio_transfer()
{
  uint8_t buff[]={0,1,2,3,5,6,7,8};
  
  dev_pmio_do_rw(buff,8,true,false);

}

// 8Bit IOW Ok
// dev_pmio_do_rw : Copy a buffer from PicoMEM to PC via I/O ports
// This is executed during a commad, to transfer data.
void dev_pmio_do_rw(uint8_t * buffer,uint16_t size,bool isread, bool do16bit)
{

// Buffer pointer and size

PM_INFO("Transfer %d : ",size);


// Transfer type
PM_RE_ISREAD=isread;
PM_TR_IS16B=do16bit;

PM_TR_PTR=buffer;
PM_CmdDataL=(uint8_t) (size&0xFF);  // Tell the PC the transfer length
PM_CmdDataH=(uint8_t) (size>>8);
if (do16bit) 
   {
    PM_TR_CNT_L=size/2;
    PM_TR_CNT_H=PM_TR_CNT_L;
    PM_Status=(isread ? STAT_DATA_READ_W:STAT_DATA_WRITE_W);
   }
 else
   {
    PM_TR_CNT_L=size;
    PM_Status=(isread ? STAT_DATA_READ:STAT_DATA_WRITE);   
   }

 do {
     if (PM_CmdReset) continue;
    } while ((PM_TR_CNT_L!=0)||(PM_TR_CNT_H!=0));

PM_INFO("Transfer End\n");

}

bool dev_pmio_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
 uint8_t port = CTRL_AL8 & 0x07;
 bool handled = false;
 switch(port)
  {
    case PORT_PM_CMD_STATUS:   // PicoMEM : Read Status
                 *Data=PM_Status;
                 handled = true; break;
    case PORT_PM_CMD_DATAL:    // PicoMEM : Read the Command L Data /Return
                 *Data=PM_CmdDataL;
                 handled = true; break;
    case PORT_PM_CMD_DATAH:    // PicoMEM : Read the Command H Data /Return
                 *Data=PM_CmdDataH;
                 handled = true; break;
    case PORT_TEST:            // Test port, Return Test value +1
                 *Data=PM_TestRead++;
                 handled = true; break;
    case PORT_TR_L:
                 if ((PM_TR_CNT_L!=0)&&(PM_RE_ISREAD)) // counter of
                   {  // Read from a table in memory, index PM_CmdDataH+PM_CmdDataL is auto increment
                    PM_TR_CNT_L--;
                    *Data=*PM_TR_PTR;
                    PM_TR_PTR++;
                    pmio_trace_push(0 /*R*/, port, *Data);
                    return true;
                   }
                 break;

   }
  if (!handled) {
     *Data=0xFF;
     pmio_trace_push(0 /*R*/, port, *Data);
     return false;
  }
  pmio_trace_push(0 /*R*/, port, *Data);
  return true;
}

void dev_pmio_iow(uint32_t CTRL_AL8,uint8_t Data)
{
  uint8_t port = CTRL_AL8 & 0x07;
  /* No PM_INFO here -- this runs on Core 1 inside the IOW CHRDY-asserted
     window. printf is backed by USB CDC; when the ring fills (e.g. an
     INT 13h dispatch from Wolf3D issues 3 IOWs back-to-back per call
     while streaming sound assets), printf blocks here, dev_pmio_iow
     doesn't return, RP_REL is never driven, CHRDY stays LOW, chassis
     CPU freezes. pmio_trace_push below is a non-blocking ring writer
     drained on Core 0 -- use that for tracing instead. */
  pmio_trace_push(1 /*W*/, port, Data);
  switch(port)
    {
      case PORT_PM_CMD_STATUS:  // PicoMEM : Write Command
			       switch (PM_CMDState)
				       { 
					      case 0:
					        if (Data==CMD_LockPort) PM_CMDState=2;
                        if (Data==CMD_PMIRQ_ACK) {
                          bus_pm_irq_ack();  // Acknowledge the IRQ
                        } else if (Data==CMD_DMACPY_ACK) {
                            isa_dmacpy_ack(); // Acknowledge the DMA Copy command
                        } else if ((PM_Status==STAT_READY)||((Data)==0))  // Ready to receive a command or Command Reset
                           {
                            if (Data==0) PM_CmdReset=true;   // Force commands reset (Then add RESET Command in command queue)
                            PM_Command=(uint8_t) Data;
                            PM_Status=STAT_CMDINPROGRESS;    // Core 1 will execute the command (I hope, at one time or another...)
                            multicore_fifo_push_blocking(0); // Hope it never block
                           }
							    break;
					    case 1:if (Data==0x94)	PM_CMDState=0;		// Unlock if PM_CMDState=1 and Data=0x94, otherwise Lock again
					  		        else PM_CMDState=2;
					           break;
					    case 2:if (Data==0x37) PM_CMDState=1;    // State 2 is Locked
                     break;
					    }
                 break;
      case PORT_PM_CMD_DATAL:    // PicoMEM : Write the Command Data L ! Command Data must be written before
                 PM_CmdDataL=Data;
                 break;
      case PORT_PM_CMD_DATAH:    // PicoMEM : Write the Command Data H ! Command Data must be written before
//                 printf("w%x",Data);
                 PM_CmdDataH=Data;               
                 break;                 
      case PORT_TEST:              // Test and 8Bit Data port
  //               gpio_put(PIN_PM_IRQ,1);  // * Send an IRQ, to Debug
                 break;
      case PORT_TR_L:
                 if ((PM_TR_CNT_L!=0)&&(!PM_RE_ISREAD)) // counter of 
                   {  // Read from a table in memory, index PM_CmdDataH+PM_CmdDataL is auto increment
                    PM_TR_CNT_L--;
                    *PM_TR_PTR=Data;
                    PM_TR_PTR++;
                    PM_INFO("CW %d, %X",PM_TR_CNT_L,Data);
                   }
                 break;  
#if DBG_PIN_AUDIO                     
        case 5: //if (Data==8) DBG_ON_INT_SB();
                //if (Data==3) DBG_ON_INT_DMA();
            //    PM_INFO("b%x,%d",Data,BV_IRQ_Cnt);       // Debug display for IRQ Begin/end
                break;
        case 6: //PM_INFO("e%x",Data);
                //if (Data==8) DBG_OFF_INT_SB();
                //if (Data==3) DBG_OFF_INT_DMA();
                break;
#endif
      default: break;
    } // PicoMEM Register Switch
}