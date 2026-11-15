/* wm_sbdsp.cpp -- WonderMCA (WM10) Sound Blaster DSP.
 *
 * 2026-08-30: replaced the Merge-CDA copy with the PROVEN WM10 implementation
 * from branch WMCA (src/audio_devices/sbdsp/sbdsp.cpp). Upstream's copy was
 * forked from an older revision and had lost several WM10 fixes -- the
 * '|| BOARD_WM10' DMA-REQ guard, the cmd-0x48 length-1 (+1) correction, the
 * reset-clears-pending-EOT rule, the 'D' response trace -- and had a printf
 * re-enabled inside the Core-1 IOW path (blocks -> CHRDY held -> bus wedge).
 * Keeping FreddyV's file split (PicoMEM sbdsp.cpp vs WM10 wm_sbdsp.cpp) but
 * with the implementation that is validated on hardware.
 *
 * The whole file is WM10-only; sbdsp.cpp carries the PicoMEM arm.
 */
#if BOARD_WM10
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
#if BOARD_WM10
// WonderMCA build: route DMA primitives to the MCA-bus implementation.
// wm_dma.h exports the same function signatures as isa_dma.h
// (start_write / complete_write / set_isr / install), so the rest of
// this file -- including the per-sample DMA event handler and the
// completion ISR -- compiles unchanged. The semantics are: each call
// to wm_dma_start_write() asserts PIN_R_DMA_REQ, the CPLD asserts
// /PREEMPT, CACP grants, the CDMA runs ONE bus cycle (MEMR + IOW),
// the WonderMCA bus handler captures the byte via wm_dma_consume_byte,
// drops REQ, and fires the registered completion ISR. See the wm_dma.h
// header docs for the full flow.
#include "wm_dma.h"
#include "mca_irq.h"    // wm_sb_eot_holding / wm_pm_irq_deferred / mca_irq_raise (PoP double-entry fix)
#define isa_dma_install        wm_dma_install
#define isa_dma_set_isr        wm_dma_set_isr
#define isa_dma_start_write    wm_dma_start_write
#define isa_dma_stop_write     wm_dma_stop_write
#define isa_dma_complete_write wm_dma_complete_write
#else
#include "../../pm_libs/isa_dma.h"       // ISA/PC DMA code
#endif
#include "../../pm_libs/bus_irq.h"       // ISA/PC IRQ code
#include "dev_sbdsp.h"
//#include "audio_fifo.h"   // wm_sbdsp.h already pulls audio_fifo8.h (8-bit); including both collides on audio_sample_t

#include "sbdsp.h"

// ---------------------------------------------------------------------------
// SB DSP command/event trace  (DOOM SFX diagnosis)
// ---------------------------------------------------------------------------
// DOOM ignores the BLASTER var and auto-detects the SB; if detection aborts,
// no DMA ever starts (no [DMA][SBDSP]).  This ring records what the guest
// actually does at the DSP so we can see WHERE DOOM stops vs a working game.
//
// PRODUCER runs on Core 1 (MCA IOW/IOR bus context via dev_sbdsp_iow/ior ->
// sbdsp_process / sbdsp_write / sbdsp_read / sbdsp_start_pcm_dma). It does
// ONLY array writes -- never printf (printf in the bus window stalls CHRDY
// and hangs the cycle). CONSUMER drains + prints on Core 0 via
// sbdsp_trace_drain() from the main loop. Single-producer / single-consumer,
// so the head/tail indices need no locking.
//
// Tags:  R = DSP reset asserted (write 1 -> 2x6)      C = DSP command byte
//        r = DSP reset released (write 0 -> 2x6)      T = time constant (val)
//        S = DMA start, single-cycle (val=count lo)   B = block size lo (val)
//        A = DMA start, auto-init   (val=count lo)    K = IRQ ack (read 2xE)
// (SB_CMD_TRACE lives in sbdsp.h so the pm_cmd.cpp dump shares the flag.)
// ---------------------------------------------------------------------------

// FIX (2026-07-18) -- auto-init /TC handling (DOOM, Duke3D, DOOM-era SFX).
// The DMA event handler used to treat a chassis /TC as a hard STOP in BOTH
// single and auto-init mode. That killed the auto-init stream at the FIRST
// /TC (right after DMX's ~16-byte 0x80 silence pad), so the real samples
// after the pad never transferred -> music but no SFX.
// With this flag set, /TC in AUTO-INIT mode is a BLOCK BOUNDARY: fire the
// block IRQ, reset the per-block counter, and KEEP pacing so the chassis
// (which programs the CDMA in true auto-init, i.e. auto-reload) keeps
// delivering the next block. Single-cycle mode (Wolf3D) is unchanged --
// still stops on /TC.
// CONFIRMED on hardware 2026-07-18: DOOM + other auto-init titles now play
// SFX with NO POST 113 -> the chassis CDMA really is auto-reload, so holding
// pace across /TC is safe. Set to 0 to restore the old always-stop behavior.
#define SB_AUTOINIT_TC_CONTINUE 1

#if SB_CMD_TRACE
#define SB_TRACE_N 256                       // power of two
typedef struct { uint8_t tag; uint8_t val; } sb_trace_ev_t;
static volatile sb_trace_ev_t sb_trace_buf[SB_TRACE_N];
static volatile uint16_t sb_trace_head = 0;  // written by Core 1 (producer)
static volatile uint16_t sb_trace_tail = 0;  // written by Core 0 (consumer)

static __force_inline void sb_trace(uint8_t tag, uint8_t val) {
    uint16_t h = sb_trace_head;
    sb_trace_buf[h & (SB_TRACE_N - 1)].tag = tag;
    sb_trace_buf[h & (SB_TRACE_N - 1)].val = val;
    sb_trace_head = (uint16_t)(h + 1);        // overwrite-on-wrap is fine for a diag
}

// SB FIFO content / output probe (all Core 0, updated in sbdsp_updatebuffer).
// Answers: is the guest sending REAL PCM (in range wide, not all 0x80) and is
// the mix stage producing audible amplitude (out_peak > 0)?  Reset on read by
// the [DMA][SBDSP] dump in pm_cmd.cpp.
volatile uint8_t  sb_dbg_in_min  = 0xFF;   // min raw FIFO byte consumed
volatile uint8_t  sb_dbg_in_max  = 0x00;   // max raw FIFO byte consumed
volatile int16_t  sb_dbg_out_peak= 0;      // peak |fres| produced into the mix
volatile uint32_t sb_dbg_drained = 0;      // input bytes consumed this window

// Drain on Core 0 ONLY. Batches events onto one line to limit UART time.
void sbdsp_trace_drain(void) {
    char buf[160];
    int  len = 0;
    while (sb_trace_tail != sb_trace_head && len < 140) {
        uint16_t i = sb_trace_tail & (SB_TRACE_N - 1);
        uint8_t tag = sb_trace_buf[i].tag;
        uint8_t val = sb_trace_buf[i].val;
        sb_trace_tail = (uint16_t)(sb_trace_tail + 1);
        len += snprintf(buf + len, sizeof(buf) - len, "%c%02X ", tag, val);
    }
    if (len) PM_INFO("[SBTRACE] %s\n", buf);
}
#else
static __force_inline void sb_trace(uint8_t tag, uint8_t val) { (void)tag; (void)val; }
void sbdsp_trace_drain(void) { }
#endif

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
#define DSP_DMA_ADC             0x24    // 1.xx+ :8-bit DMA PCM Input, Single (Win3.11 driver install)

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
static void sbdsp_default_rate(void);   // defined above sbdsp_init; used by the DMA pacing alarm guard

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
    // Drop PIN_R_DMA_REQ on EVERY disable. The /TC path had consume_byte
    // clear REQ; the count-based EOT (8557, no /TC) has no consume for the
    // final un-granted request, so without this REQ stays HIGH -> DMA_SESSION
    // never async-resets (DMA_SESSION.AR = !RP_DMA_REQ) -> RP_DMA_GRANT FF
    // never releases -> /PREEMPT held -> bus wedged, DOS can't re-arm.
    isa_dma_stop_write();
    PIC_RemoveEvent(&DSP_DMA_Event);
    sbdsp.cur_sample8 = 0x80;   // zero current sample
    //PM_INFO("Stop DMA");
}

static __force_inline void sbdsp_dma_enable() {
#if BOARD_WM10    
    wm_dma.dma_enable_count++;   // INSTRUMENTATION: count re-arm calls (cmd 0x14)
#endif    
    if(!sbdsp.dma_enabled) {
        sbdsp.mode= MODE_DMA; // Set the mode to DMA
        sbdsp.dma_enabled=true;
        sbdsp.timer_delta=0;
#if BOARD_WM10
        // Fresh transfer: clear the terminal end-of-transfer latch so
        // wm_dma_start_write() will assert REQ again. /TC from the
        // previous block (if any) sticks tc_latched=true to block stale
        // sample-rate timer ticks from hammering /PREEMPT after CDMA
        // is done; we re-enable here at the start of every new block.
        wm_dma.tc_latched = false;
        wm_dma.req_pending = false;
        // Reset per-transfer diagnostic counters so the [wm_dma] dump
        // reflects THIS block, not cumulative state from earlier runs.
        wm_dma.bytes_received   = 0;
        wm_dma.iow_entries      = 0;
        wm_dma.hook_invocations = 0;
        wm_dma.grant_captures   = 0;
        gpio_put(PIN_R_DMA_REQ, 0);
#endif
#if USE_HWDMA || BOARD_WM10
        // Register the SBDSP completion ISR. On ISA this is dispatched
        // from the PIO state machine; on WM10 it's called from the
        // wm_dma bus handler after each captured byte. Same callback.
        isa_dma_set_isr(SBDSP_DMA_isr_pt);
#endif
        // No PM_INFO -- sbdsp_dma_enable() is called from Core 1 IOW
        // context (cmd 0x14 / 0x1C / 0xD4 handlers). Fires every TADA
        // replay. See DSP_DMA_PAUSE comment for the deadlock mechanism.
        PIC_AddEvent(&DSP_DMA_Event, sbdsp.dma_interval, 0);
    }
}

/*
static __force_inline bool sbdsp_dac_enable() {
    if (sbdsp.mode==MODE_NONE)  // Start DAC if only in MODE_NONE
      {
        //afifo_reset8(&sbdsp_fifo); // Reset the FIFO
        //Set the sb dsp output rate to 31KHz
        sbdsp.dac_interval = 32;    //Simulate a DAC at 31250 Hz max
        sbdsp.sample_step  = (uint32_t)(((uint64_t)31250 << 32) / PM_AUDIO_FREQUENCY);
        sbdsp.increment    = 0;
        sbdsp.readsample   = true;
        sbdsp.prev_sample8u    = 0x80;
        sbdsp.prev_sample8u_s0 = 0x80;
       
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

// 2026-09-12 (8570, PoP end-of-level: ack+2 per EOT, endless 0x14 re-arms):
// single-cycle EOT shape for REAL sounds. 0 = hold until the 0x22E read
// (mcsb.v shape, unchanged behaviour). N = present the level for N us, then
// lower it from a timer event -- edge emulation for a level-triggered 8259
// whose guest ISR EOIs BEFORE reading 0x22E (the held level re-latches at
// EOI -> second ISR entry). The 0x22E ack still lowers it early; an un-acked
// pulse is re-presented every WM_EOT_IRQ_RETRY_US by pm_cmd.cpp's retry
// block via wm10_eot_repulse() until the 100 ms window closes.
#define SB_EOT_SINGLE_PULSE_US 0   // 6 us tested 2026-09-12 on the 8570: adt pair 14/42 unchanged -> the two 0x22E reads are ONE ISR pass, hold is not the PoP loop
#if BOARD_WM10
// PicoMEM / Snark-Barker EOT mechanism on WM10 (2026-09-02 step-back):
// raise the SB IRQ right here in the sample-rate alarm at block completion
// and HOLD the level; the guest's 0x22E read (Core 1, sbdsp_read) lowers it.
// This is exactly what the USE_HWDMA (PicoMEM) path does -- no Core-0
// main-loop deferral, no 30us pulse, no retry train. The alarm is a hw-timer
// IRQ so it's prompt even during main-loop stalls. force=false: the line is
// already low (cleared by the previous 0x22E ack, or the per-transfer
// clean-edge lower) so a bare gpio_put is a clean rising edge -- and avoids a
// busy_wait under the alarm-pool spinlock. eot_retry_pending drives the
// 100ms safety release (main-loop retry block) so a handler-less guest can
// never wedge the held line into POST.
static inline void wm10_eot_raise(void) {
    uint32_t now = time_us_32();
    wm_dma.eot_lat_last_us    = 0;      // raised at completion in the alarm -- no queue delay
    wm_dma.eot_raise_us       = now;
    wm_dma.eot_retry_since_us = now;
    wm_dma.eot_retry_next_us  = now + WM_EOT_IRQ_RETRY_US;
    wm_dma.eot_retry_pending  = true;
    wm_sb_eot_holding         = true;   // BV/USB IRQ deferred during the block ISR window
    mca_irq_raise(sbdsp.irq, false);    // set the level (held); 0x22E read lowers it
    wm_dma.eot_irq_fired++;
    sb_trace('E', (uint8_t)(time_us_32() >> 12));   // EOT raised (alarm path), 4 ms units
}

#if SB_EOT_SINGLE_PULSE_US
// End of a pulse-shaped single-cycle EOT (timer event, SB_EOT_SINGLE_PULSE_US
// after the raise). If the guest already read 0x22E the line is down and
// pending is clear -- nothing to do.
static uint32_t wm10_eot_pulse_end(Bitu v) {
    (void)v;
    if (wm_dma.eot_retry_pending) {
        mca_irq_lower(sbdsp.irq);
        wm_sb_eot_holding = false;       // pm_cmd's retry block may now re-present it
    }
    return 0;
}
static PIC_TimerEvent wm10_eot_pulse_end_evt = { .handler = wm10_eot_pulse_end };
#endif

// Re-present an un-acked pulse-shaped EOT (called from the Core-0 retry block
// every WM_EOT_IRQ_RETRY_US while pending && !holding). No-op in hold mode.
void wm10_eot_repulse(void) {
#if SB_EOT_SINGLE_PULSE_US
    if (!wm_dma.eot_retry_pending || wm_sb_eot_holding) return;
    wm_sb_eot_holding = true;
    mca_irq_raise(sbdsp.irq, false);
    wm_dma.eot_retry_pulses++;
    PIC_AddEvent(&wm10_eot_pulse_end_evt, SB_EOT_SINGLE_PULSE_US, 0);
#endif
}

#ifndef SB_EOT_IRQ_HOLD_AI_US
#define SB_EOT_IRQ_HOLD_AI_US 30   // auto-init block-IRQ pulse width (us): must outlive INTA latency yet end before DMX's early EOI
#endif

// EOI-snoop -- the missing half of the mcsb.v/PicoMEM model on level-triggered
// MCA (2026-09-02). Edge-triggered ISA turns a held IRQ level into exactly ONE
// edge no matter when the guest EOIs; MCA's level-triggered 8259 re-latches the
// held level at the guest's EOI, so a guest that EOIs BEFORE reading 0x22E
// (DMX/DOOM in every phase incl. detection; PoP when USB preempts its ISR)
// storms/double-enters -> crash/freeze. We reproduce the edge behavior by
// snooping the guest's write to the master 8259 command port (0x20) and
// dropping our held SB IRQ RIGHT at its EOI, before the 8259 re-samples.
// Called PASSIVELY from the bus handler on an IOW to 0x20 (observe only --
// the handler must never drive the bus for 0x20, esp. on reads/OCW3 readback).
// Guests that ack 0x22E first already lowered the line -> this is a no-op.
extern "C" void __not_in_flash_func(sb_pic_eoi_observe)(uint8_t data) {
    if (!wm_sb_eot_holding) return;                 // no SB EOT held -> nothing to do
    // OCW2 is D4=D3=0; an EOI command has the EOI bit D5 set. Covers
    // non-specific EOI (0x20), specific EOI (0x60|n), rotate-on-nonspec (0xA0).
    if ((data & 0x18) == 0 && (data & 0x20)) {
        wm_sb_eot_holding        = false;
        // eot_retry_pending is deliberately LEFT SET: a non-specific EOI carries
        // no source, so this may be an unrelated ISR (timer, kbd, PM/USB IRQ7)
        // ending inside our hold window and swallowing an EOT the guest never
        // serviced (PoP: irq=1, ack flat, irqpin=0, playback stalls). The retry
        // block re-presents it. A guest that really did service us reads 0x22E
        // well within WM_EOT_IRQ_RETRY_US and clears pending there, so the
        // storm-prevention this snoop exists for is unchanged.
        mca_irq_lower(sbdsp.irq);                   // release BEFORE the 8259 re-samples the level
        // (deferred PM pulse replays from the Core-0 flush block -- never a
        //  bare held raise from Core-1 context)
    }
}
// AUTO-INIT EOT: a bounded PULSE, not a hold (2026-09-02, DOOM fix).
// DMX's streaming ISR EOIs the 8259 BEFORE reading 0x22E; on the level-
// triggered MCA 8259 a HELD level re-latches at that EOI -> unbounded ISR
// re-entry -> DOS4GW "transfer stack overflow on interrupt 0Dh" (hardware-
// observed 2026-07-23; re-confirmed by the 2026-09-02 audit as the DOOM
// sound-init crash once auto-init was put on hold). So for auto-init we drive
// a short pulse whose trailing edge lands BEFORE DMX's early EOI: assert, a
// bounded busy_wait, then release -- the line is LOW by the time DMX EOIs, so
// nothing re-latches. Single-cycle (PoP/Wolf3D/Cubic, whose ISRs ack 0x22E
// before EOI or poll) keeps the proven hold via wm10_eot_raise(). Runs in the
// sample-rate alarm; the busy_wait is short and blocks in the guest's ISR
// entry latency, not a hot bus cycle. No retry train: the next auto-init
// block (~12ms) re-pulses on its own; a lost pulse just drops one block.
static inline void wm10_eot_pulse(void) {
    wm_dma.eot_raise_us      = time_us_32();
    wm_dma.eot_irq_fired++;
    mca_irq_raise(sbdsp.irq, false);        // assert
    busy_wait_us(SB_EOT_IRQ_HOLD_AI_US);    // bounded: must end before DMX's early EOI
    mca_irq_lower(sbdsp.irq);               // release -- line LOW before the guest EOIs
    // No hold, no eot_retry_pending: nothing to re-latch, nothing to safety-release.
}

// Single-cycle EOT: choose PULSE for tiny transfers, HOLD for real sounds.
// DMX's detection probes are a few bytes (S03 -> 4); DMX runs a temporary
// IRQ5 handler, then REMOVES it -- a HELD probe EOT re-latches AFTER removal
// -> IRQ5 into a stale vector -> DOS4GW "unrelocated crash address" in
// I_StartupSound (DOOM startup crash). A bounded pulse leaves the line low
// before removal: one clean edge, nothing to re-latch. Real single-cycle
// SOUNDS (PoP 156B, Wolf3D, Cubic 64K) ack 0x22E before EOI, so they keep the
// proven hold. 32-byte split cleanly separates probes (<=4) from sounds (100s+).
static inline void wm10_eot_single(void) {
    if (sbdsp.dma_sample_count <= 32) wm10_eot_pulse();   // tiny detection probe
    else {
        wm10_eot_raise();                                  // real sound: hold-until-ack...
#if SB_EOT_SINGLE_PULSE_US
        PIC_AddEvent(&wm10_eot_pulse_end_evt, SB_EOT_SINGLE_PULSE_US, 0);   // ...or a timed pulse
#endif
    }
}
#endif

static uint32_t __not_in_flash_func(DSP_DMA_EventHandler)(Bitu val) {
    uint32_t current_interval;
    //DBG_ON_1();
#if BOARD_WM10
    // Bus-side terminal latch: chassis CDMA asserted /TC, so no more
    // data is coming. Synchronize SBDSP-side EOT to the bus event:
    // fire the SB IRQ to DOS immediately and stop the timer. We skip
    // the rx++/start_write below entirely -- wm_dma_start_write would
    // already no-op on tc_latched, and not bumping rx keeps the
    // counters honest. Without this hook the SBDSP timer keeps
    // ticking blindly against its programmed sample count, racing
    // wm_dma and leaving REQ asserted with no way to drop it.
    if (wm_dma.tc_latched) {
        // No PM_INFO here -- this runs in Core 0 PIC callback context
        // while the alarm pool spinlock is held. printf blocks on CDC
        // ring full -> alarm pool spinlock held indefinitely -> Core 1
        // PIC_RemoveEvent on cmd 0xD0 spins forever -> dev_sbdsp_iow
        // never returns -> CHRDY stuck LOW. Counters available via the
        // wm_dma watchdog dump from Core 0 main loop instead.
#if SB_AUTOINIT_TC_CONTINUE
        if (sbdsp.autoinit) {
            // Auto-init: the CHASSIS /TC (its DMA buffer wrapped) is NOT the
            // SB block boundary -- the chassis is programmed auto-reload and
            // keeps feeding. Just clear the latch and keep pacing. The SB
            // block-completion IRQ is fired further down when bytes_received
            // reaches dma_sample_count (the 0x48 block size).
            //
            // Do NOT fire the IRQ here and do NOT reset bytes_received: the
            // chassis count is much smaller than the SB block (e.g. /TC every
            // ~16 bytes vs a 127-byte block), so firing per /TC delivered the
            // IRQ ~8x too often and reset the block counter so the real
            // boundary never hit -> DOOM/DMX "transfer stack overflow"
            // (error 2002) on the SB IRQ (INT 0Dh = IRQ5). Just re-arm.
            wm_dma.tc_latched = false;
            // (fall through -- keep counting toward the SB block, keep pacing)
        } else
#endif
        {
            sbdsp_dma_disable();
            wm_dma.eot_queued_us = time_us_32();   // EOT-IRQ latency probe: queue timestamp
            wm10_eot_single();                      // tiny probe -> pulse (DMX detect), real sound -> hold
            return 0;
        }
    }
#endif
    sbdsp.dma_sample_count_rx++;
    PM_DMA_SendTime=time_us_64();
    dma_inprogress=true;

// WM10 builds with USE_HWDMA=0 (no ISA PIO DMA engine) but still needs this
// block: the MCA path asserts PIN_R_DMA_REQ from here. The merge dropped the
// `|| BOARD_WM10` half of this guard, so WM10 fell through to the virtual-DMA
// fallback below -- REQ never asserted, zero bytes moved, no EOT IRQ, silence.
#if USE_HWDMA || BOARD_WM10
#if BOARD_WM10 //WonderMCA
    // No periodic PM_INFO here -- this is Core 0 PIC callback context
    // with the alarm pool spinlock held. See the /TC EOT block above
    // for the deadlock mechanism. Per-1024-samples trace was useful
    // during bring-up; for production health use the wm_dma watchdog
    // dump from main-loop context instead.
    // Real DMA path: assert REQ (ISA: PIO state machine drives DREQ;
    // WM10: PIN_R_DMA_REQ -> CPLD /PREEMPT -> CACP grants -> CDMA runs
    // one MEMR+IOW cycle -> wm_dma_consume_byte captures the byte and
    // fires sbdsp_dma_isr via the completion_isr callback).
    // Request the next byte only while the BUFFER isn't full yet -- authority
    // is DELIVERED bytes (wm_dma.bytes_received, reset to 0 per block), NOT
    // timer ticks. On a chassis whose CDMA grant lags the 22 kHz sample timer
    // (8557) the tick count (dma_sample_count_rx) outruns delivery, so a
    // tick-based EOT abandons the buffer mid-transfer (REQ dropped, bytes
    // stuck). Back-pressure (req_pending) keeps one outstanding REQ, so this
    // paces requests to the actual delivery rate.
    if (wm_dma.bytes_received < sbdsp.dma_sample_count) {
        sbdsp.dma_rx_index=0;
        isa_dma_start_write();
    }
#else  //PicoMEM
    sbdsp.dma_rx_index=0;
    isa_dma_start_write();
#endif    
#else
    sbdsp.vdma_to_send++;      // Virtual-DMA fallback: ask one more byte
#endif
 //    PM_INFO("X");

    current_interval = sbdsp.dma_interval+sbdsp.timer_delta;
    if (current_interval == 0 || current_interval > 1000) {   // 0 = alarm would stop (one byte then silence); guard the default
        sbdsp_default_rate();
        current_interval = sbdsp.dma_interval;
    }
    //DBG_OFF_1();
#if BOARD_WM10
    if(wm_dma.bytes_received < sbdsp.dma_sample_count) {
#else
    if(sbdsp.dma_sample_count_rx <= sbdsp.dma_sample_count) {
#endif                
        return current_interval;   // buffer not full yet -- keep pacing requests to delivery
    } else {
#if BOARD_WM10
        // Single-mode WM10: do NOT trust the SBDSP-side rx count alone
        // to decide EOT -- if a cross-core race made consume_byte miss
        // a cycle, chassis CDMA still has more bytes to deliver and
        // shutting the timer here leaves /PREEMPT permanently
        // un-asserted with chassis count > 0 (-> POST 113 NMI).
        //
        // Truth source is the bus: /TC asserts when chassis CDMA hits
        // count=0. Until tc_latched flips true, keep ticking so the
        // sample-rate timer re-asserts REQ and the chassis catches up.
        // The tc_latched short-circuit at the top of this handler will
        // fire EOT on the FIRST tick after /TC actually arrives.
        // Count-based EOT for single mode: dma_sample_count is the
        // authority. The old code waited HERE for /TC indefinitely -- fine
        // on 8570/8550, but the 8557 never asserts /TC, so the timer ticked
        // forever re-asserting REQ (stuck REQ, no grant -> the 8557 stall).
        // We already stopped asserting REQ above once rx>count, so /PREEMPT
        // is released (no stuck-REQ / POST-113 exposure); just fall through
        // to disable + IRQ below. /TC (top of handler) stays the fast path
        // where the chassis does assert it.
        if (!sbdsp.autoinit) wm_dma.tc_latched = true;   // single-mode terminal latch; autoinit keeps looping
#endif
        if(sbdsp.autoinit) {
          //  sbdsp.vdma_flushdata = true; // Set the flush flag
#if USE_HWDMA
            bus_irq_raise(IRQ_R_AUDIO,sbdsp.irq,true);
#elif BOARD_WM10
            wm10_eot_pulse();                       // PULSE, not hold: DMX EOIs before ack -> a held level re-latches (DOOM crash)
#else
            wm_dma.eot_queued_us = time_us_32();   // EOT-IRQ latency probe: queue timestamp
            sbdsp.mainloop_action=DSP_MLC_SENDIRQ;
#endif
            sbdsp.dma_sample_count_rx=0;
#if BOARD_WM10
            wm_dma.bytes_received=0;   // reset per-block DELIVERED count for the next autoinit block
#endif
            return current_interval;
        }
        else {
          //  sbdsp.vdma_flushdata = true; // Set the flush flag
            sbdsp_dma_disable();
#if USE_HWDMA
            bus_irq_raise(IRQ_R_AUDIO,sbdsp.irq,true);
#elif BOARD_WM10
            wm10_eot_single();                      // tiny probe -> pulse (DMX detect), real sound -> hold
#else
            wm_dma.eot_queued_us = time_us_32();   // EOT-IRQ latency probe: queue timestamp
            sbdsp.mainloop_action=DSP_MLC_SENDIRQ;
#endif
        }
        // Note: rev attempt 2026-05-25 added `|| BOARD_WM10` to the
        // single-mode guard above so the EOT IRQ would fire directly
        // (mirroring the autoinit branch). Vincent observed it
        // changed bench behaviour for the worse and asked to revert.
        // The WM10 build falls back to mainloop_action=DSP_MLC_SENDIRQ
        // for single-mode EOT, same as the legacy ISA-non-HWDMA path.
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
#if BOARD_WM10 && SB_EOT_CORE1_RAISE
    // Zero-latency single-cycle EOT (2026-09-02): raise the SB IRQ the instant
    // the chassis asserts /TC, right here on Core 1 -- no Core-0 main-loop
    // deferral (which USB tuh_task / SD reads / UART logging stalled to
    // 283us..2520us -> DOOM detect timeout, MIDI stall, PoP double-entry).
    // Trigger is /TC ONLY (wm_dma.last_tc, set per byte by consume_byte): it
    // is the per-block terminal signal and is reset every byte, so it can't
    // misfire. (An earlier bytes_received>=count clause was WRONG -- that
    // counter is cumulative across single-cycle blocks, so from block 2 on it
    // fired the EOT on byte 1 -> premature IRQ -> DOOM crash / CP-MIDI stall.)
    // The no-/TC 8557 falls back to the alarm/main-loop path, which counts
    // correctly via dma_sample_count_rx. One-in-flight via eot_retry_pending.
    // consume_byte already set tc_latched on this /TC byte, so we don't here.
    if (!sbdsp.autoinit && !wm_dma.eot_retry_pending && wm_dma.last_tc) {
        uint32_t now = time_us_32();
        wm_dma.eot_lat_last_us    = 0;      // raised at completion: no queue delay
        wm_dma.eot_raise_us       = now;
        wm_dma.eot_retry_since_us = now;
        wm_dma.eot_retry_next_us  = now + WM_EOT_IRQ_RETRY_US;
        wm_dma.eot_retry_pending  = true;
        wm_dma.eot_raised_core1   = true;   // tell the fallback drain we handled this one
        wm_sb_eot_holding         = true;   // hold until 0x22E ack (Core 1 lowers on the read)
        mca_irq_raise(sbdsp.irq, false);    // bare gpio_put -- Core-1 hot-path safe
        wm_dma.eot_irq_fired++;
    }
#endif
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


// DSP default sample rate (no 0x40 yet, or after a DSP reset). A real DSP has
// a default time constant; ours was ZERO -> the DMA pacing alarm armed with a
// 0 us delay, ran once and stopped -> exactly ONE byte per 0x14 transfer
// (Flashback SETUP: "bytes=1/129, req=0", 2026-09-12). TC 0xA5 = 91 us = 10989 Hz.
#define SB_DEFAULT_TIME_CONSTANT 0xA5
static void sbdsp_default_rate(void) {
    sbdsp.time_constant = SB_DEFAULT_TIME_CONSTANT;
    sbdsp.dma_interval  = 256 - SB_DEFAULT_TIME_CONSTANT;
    sbdsp.sample_rate   = 1000000ul / sbdsp.dma_interval;
    sbdsp.sample_step   = (uint32_t)(((uint64_t)sbdsp.sample_rate << 32) / PM_AUDIO_FREQUENCY);
    sbdsp.increment     = 0;
    sbdsp.readsample    = true;
}

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
    sbdsp_default_rate();


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

// Consolidated PCM-DMA arm (ported from upstream PicoMEM). Sets the sample
// counts/width then calls sbdsp_dma_enable(), which on WM10 routes through
// the wm_dma MCA path and on ISA through the virtual/HW DMA path.
void sbdsp_start_pcm_dma(bool autoinit, uint16_t xfer_size) {
     // No PM_INFO here -- on WM10 this is reached from DSP_DMA_SINGLE in the
     // Core 1 MCA IOW context, where printf can deadlock (see wm_dma notes).
     sb_trace(autoinit ? 'A' : 'S', (uint8_t)(xfer_size & 0xFF));  // DOOM SFX diag: DMA start
     wm_dma.eot_retry_pending=0;   // new transfer supersedes any un-acked EOT pulse train (DMX nags 0x14 without acking)
     wm_sb_eot_holding=false;      // lifecycle fix 2026-09-03: keep holding paired with pending (a stale hold wedged the PM-IRQ deferral)
     // (2026-09-02: the "clean-edge lower" that was here cancelled a held EOT
     // when a fast streamer issued the next 0x14 -- removed; pure PicoMEM
     // model lowers ONLY on the 0x22E read.)
     sbdsp.dav_dsp=0;
     sbdsp.autoinit=autoinit;
     // SB block-size register (DSP cmd 0x48) is length-1: DMX's "B7F" = 0x7F
     // means a 128-byte block, not 127. Count to xfer_size+1 so the block IRQ
     // lands at the real boundary (matches dma_xfer_count below, which already
     // uses xfer_size+1); the old raw xfer_size fired the EOT IRQ one byte early
     // every block, perpetually 1 byte offset from the chassis CDMA count.
     // SB length register is length-1, so +1 = real byte count -- but do the
     // +1 in 32-bit: Cubic Player programs 0x14 with len 0xFFFF (one 64K
     // single-cycle transfer, position tracked by polling the DMA count) and
     // 0xFFFF+1 in uint16 overflowed to 0 -> the count-based EOT fired
     // INSTANTLY with zero bytes moved -> ~7.8k re-arms/s runaway + silence
     // (en=irq=thousands, bytes=0/0). Clamp to 0xFFFF: the EOT lands one byte
     // early on a 64K block, which a count-polling streamer never notices.
     { uint32_t _n = (uint32_t)xfer_size + 1u;
       if (_n > 0xFFFFu) _n = 0xFFFFu;
       sbdsp.dma_sample_count = (uint16_t)_n;
       sbdsp.dma_sample_count_rx=0;
       sbdsp.dma_rx_width=sbdsp.dma_stereo ? 2 : 1;
       sbdsp.dma_xfer_count = (uint16_t)(_n / sbdsp.dma_rx_width);
       sbdsp.dma_xfer_count_left = sbdsp.dma_xfer_count; }
     sbdsp_dma_enable();
}

static __force_inline void sbdsp_output(uint8_t value) {
    sbdsp.outbox=value;
    sbdsp.dav_pc=1;
}

// ---------------------------------------------------------------------------
// Direct DAC (DSP cmd 0x10 -- Another World / Out of This World and other early
// digitised titles): the GUEST paces the samples, one 0x10 + data byte per
// sample from its own timer or delay loop; no DMA, no time constant. This was
// dead code until 2026-09-12 (8570 report "no audio in Another World"): the
// byte was stored in cur_sample8 and the DAC timer / sbdsp_dac_enable that
// should have played it were never wired. DOSBox shape instead: push every
// sample into the PCM FIFO and let the mixer's resampler consume it. The rate
// is measured from the guest's pace, then servoed on the FIFO level so a
// slightly wrong estimate cannot drift into underrun (clicks) or overflow.
// ---------------------------------------------------------------------------
#define SB_DAC_IDLE_US      100000u   // >100 ms without a sample = the sound ended
#define SB_DAC_FIFO_TARGET  160u      // ~16 ms at 10 kHz: latency vs. the guest's delivery gaps
#define SB_DAC_FIFO_START   32u       // FIFO start threshold for the DAC stream (~3 ms)
static volatile uint32_t sb_dac_last_us   = 0;
static volatile uint32_t sb_dac_period_us = 0;   // smoothed guest inter-sample period (us)
static uint32_t          sb_dac_rate      = 0;   // Hz currently fed to the resampler

static int32_t           sb_dac_integ     = 0;   // servo integrator (Hz)
static volatile uint32_t sb_dac_in_count  = 0;   // samples pushed by the guest (Core 1)
static uint32_t          sb_dac_in_last   = 0;   // ...as seen at the last servo window (Core 0)
static uint32_t          sb_dac_blocks    = 0;   // mixer blocks since the last servo window
static uint32_t          sb_dac_dry       = 0;   // consecutive output samples with an empty FIFO
#define SB_DAC_SERVO_BLOCKS 32                    // 32 x 64 / 49716 = 41 ms window
// Diag (printed by pm_cmd every 4 s while in MODE_DAC, then reset)
static volatile uint32_t sb_dac_st_meas = 0, sb_dac_st_lmin = 0xFFFF, sb_dac_st_lmax = 0, sb_dac_st_dry = 0, sb_dac_st_in = 0, sb_dac_st_win = 0;

static __force_inline void sb_dac_set_rate(uint32_t hz) {
    if (hz < 1000)  hz = 1000;
    if (hz > 45000) hz = 45000;
    sb_dac_rate       = hz;
    sbdsp.sample_rate = (uint16_t)hz;
    sbdsp.sample_step = (uint32_t)(((uint64_t)hz << 32) / PM_AUDIO_FREQUENCY);
}

// Core 1, from the 0x10 handler when the data byte is consumed.
static void sbdsp_dac_sample(uint8_t v) {
    if (sbdsp.mode != MODE_NONE && sbdsp.mode != MODE_DAC) return;   // a DMA transfer owns the FIFO
    uint32_t now = time_us_32();
    uint32_t dt  = now - sb_dac_last_us;
    sb_dac_last_us = now;
    if (sbdsp.mode != MODE_DAC || dt > SB_DAC_IDLE_US) {
        afifo_reset8(&sbdsp_fifo);                 // (re)start the stream
        afifo_set_start8(&sbdsp_fifo, SB_DAC_FIFO_START);
        sbdsp.prev_sample8u = sbdsp.prev_sample8u_s0 = 0x80;
        sbdsp.increment  = 0;
        sbdsp.readsample = true;
        sb_dac_period_us = 0;
        sb_dac_integ     = 0;
        sb_dac_in_count  = 0; sb_dac_in_last = 0; sb_dac_blocks = 0; sb_dac_dry = 0;
        sb_dac_set_rate(8000);                     // provisional until measured
        sbdsp.mode = MODE_DAC;
    } else if (dt >= 20 && dt <= 1000) {           // 1-50 kHz: a real sample period, not a gap
        sb_dac_period_us = sb_dac_period_us ? (sb_dac_period_us * 15 + dt) / 16 : dt;
    }
    afifo_add_sample8(&sbdsp_fifo, (int8_t)v);
    sb_dac_in_count++;
}

bool sbdsp_in_dac_mode(void) { return sbdsp.mode == MODE_DAC; }   // for the Core-0 [SB][POLL] monitor

// Core-0 main loop: a 0xE2 DMA byte the host never came to fetch (channel not
// programmed / wrong direction) must not leave REQ asserted into the next
// arbitration -- drop it after 100 ms.
void sbdsp_e2_tick(void) {
    if (wm_dma.e2_pending && (uint32_t)(time_us_32() - wm_dma.e2_req_us) > 100000u) {
        wm_dma.e2_pending  = false;
        wm_dma.req_pending = false;
        gpio_put(PIN_R_DMA_REQ, 0);
#if SB_CMD_TRACE
        PM_INFO("[SB][E2] DMA byte %02X never fetched (100 ms) -- channel not programmed card->host?\n", wm_dma.e2_value);
#endif
    }
}

// One line per call (pm_cmd, every 4 s in MODE_DAC): what the DAC servo saw.
void sbdsp_dac_stats_print(void) {
    if (sb_dac_st_dry == 0) {                    // healthy stream: stay silent (printf = audio glitch)
        sb_dac_st_lmin = 0xFFFF; sb_dac_st_lmax = 0; sb_dac_st_in = 0; sb_dac_st_win = 0;
        return;
    }
    PM_INFO("[SB][DAC] rate=%lu meas=%lu lvl=%lu..%lu dry=%lu in=%lu win=%lu per=%lu\n",
            (unsigned long)sb_dac_rate, (unsigned long)sb_dac_st_meas,
            (unsigned long)(sb_dac_st_lmin == 0xFFFF ? 0 : sb_dac_st_lmin), (unsigned long)sb_dac_st_lmax,
            (unsigned long)sb_dac_st_dry, (unsigned long)sb_dac_st_in, (unsigned long)sb_dac_st_win,
            (unsigned long)sb_dac_period_us);
    sb_dac_st_lmin = 0xFFFF; sb_dac_st_lmax = 0; sb_dac_st_dry = 0; sb_dac_st_in = 0; sb_dac_st_win = 0;
}

// Core 0, called every mixer block in MODE_DAC but ACTS only once per
// SB_DAC_SERVO_BLOCKS (41 ms). The rate is the count of samples the guest
// actually delivered in the window (bursts and gaps included -- unbiased),
// smoothed, plus a small level correction toward the FIFO target. Nothing here
// may move at audio rate: a per-block correction turned the FIFO's natural
// +-10-sample wobble into +-1 % FM at hundreds of Hz (audible roughness,
// first attempt 2026-09-12). Until the first window completes, the provisional
// rate comes from the Core-1 inter-sample period estimate.
static __force_inline void sbdsp_dac_servo(void) {
    if (++sb_dac_blocks < SB_DAC_SERVO_BLOCKS) {
        // Early lock (~5 ms) from the period estimate -- FIRST window only. (It
        // fired every window at first and pinned the rate to the gap-blind
        // period estimate, 8 % high: lvl 13..39, dry 2600/4 s, 2026-09-12.)
        if (sb_dac_blocks == 4 && sb_dac_period_us && sb_dac_in_last == 0)
            sb_dac_set_rate(1000000u / sb_dac_period_us);
        return;
    }
    sb_dac_blocks = 0;
    uint32_t got = sb_dac_in_count - sb_dac_in_last;          // samples delivered this window
    sb_dac_in_last += got;
    if ((uint32_t)(time_us_32() - sb_dac_last_us) > SB_DAC_IDLE_US) {
        sbdsp.mode = MODE_NONE;                               // stream over; next 0x10 restarts it
        return;
    }
    { uint32_t l = afifo_count8(&sbdsp_fifo);   // (afifo_level8 = FREE space -- the first servo read it as the level: positive feedback)
      if (l < sb_dac_st_lmin) sb_dac_st_lmin = l;
      if (l > sb_dac_st_lmax) sb_dac_st_lmax = l;
      sb_dac_st_in += got; sb_dac_st_win++; }
    if (got < 8) return;                                      // gap window: keep the current rate
    // delivered rate over the window, in Hz
    uint32_t meas = (uint32_t)(((uint64_t)got * PM_AUDIO_FREQUENCY) / (SB_DAC_SERVO_BLOCKS * PM_SAMPLES_PER_BUFFER));
    uint32_t base = sb_dac_integ ? (uint32_t)sb_dac_integ : meas;   // integ doubles as the smoothed rate
    base = (base * 3 + meas) / 4;                              // ~160 ms smoothing
    sb_dac_integ = (int32_t)base;
    sb_dac_st_meas = meas;
    int32_t err = (int32_t)afifo_count8(&sbdsp_fifo) - (int32_t)SB_DAC_FIFO_TARGET;   // + = draining too slowly
    int32_t hz  = (int32_t)base + (err * (int32_t)base / (int32_t)SB_DAC_FIFO_TARGET) / 16;  // full error -> +-6 %, once per 41 ms
    sb_dac_set_rate((uint32_t)hz);
}

void sbdsp_process(void) {
    if(sbdsp.reset_state) return;
    sbdsp.dsp_busy=1;

    if(sbdsp.dav_dsp) {
        if(!sbdsp.current_command) {
            sbdsp.current_command = sbdsp.inbox;
            sbdsp.current_command_index=0;
            sbdsp.dav_dsp=0;
            sb_trace('C', sbdsp.current_command);   // DOOM SFX diag: DSP command byte
        }
    }

    switch(sbdsp.current_command) {  

        case DSP_DMA_PAUSE:               // 1.xx+ : DMA Pause  > To add : Insert silences
            sbdsp.current_command=0;
            sbdsp_dma_disable();
#if USE_HWDMA || BOARD_WM10
            isa_dma_stop_write();   // drop PIN_R_DMA_REQ + clear req_pending
#endif
            sbdsp.mode= MODE_DMA_PAUSE; // Set the mode to DMA_PAUSE
            // No PM_INFO -- this runs on Core 1 inside dev_sbdsp_iow
            // while CHRDY is asserted. SBDIAG's stop-key path issues
            // cmd 0xD0 here, and a blocking printf turns the IOW into
            // a permanent CHRDY hold (plus risks the alarm-pool-vs-
            // PIC_RemoveEvent deadlock described in DSP_DMA_EventHandler).
            break;

        case DSP_DMA_RESUME:              // 1.xx+ : DMA Resume
            if (sbdsp.mode==MODE_DMA_PAUSE)
             {
           //   if (sbdsp.mode!=MODE_DMA_PAUSE) PM_INFO("!R");
              sbdsp_dma_enable();
             } //else PM_INFO("!R");
             sbdsp.current_command=0;
           // PM_INFO("DMA RESUME\n");
            break;

        case DSP_DMA_EXIT_AUTO8:       // 2.00+ : Exit Autoinit mode 8-bit
            sbdsp.current_command=0;
            sbdsp.autoinit=false;
            //PM_INFO("EXIT AUTO\n");
            break;

        case DSP_DMA_HS_AUTO:    // 2.01+ :8-bit DMA PCM Output, Auto, High Speed
        case DSP_DMA_AUTO:       // 2.00+ :8-bit DMA PCM Output, Auto
            //PM_INFO("DMA_AUTO %d\n",sbdsp.dma_block_size);
            sbdsp.current_command=0;
#if BOARD_WM10
            // Arm the passive CDMA trace for the auto-init stream too -- until
            // 2026-09-02 only cmd 0x14 armed it, so DOOM's 0x1C start never
            // opened a capture window. 96 IOWs ~ 3/4 of a 128-byte block:
            // enough to see reload behavior right at stream start.
            cdma_trace.head    = 0;
            cdma_trace.tail    = 0;
            cdma_trace.dropped = 0;
            cdma_trace.budget  = 96;
            wm_dma.tc_events      = 0;   // fresh cadence window per stream
            wm_dma.blk_gap_max_us = 0;
            wm_dma.blk_last_us    = 0;
#endif
            sbdsp_start_pcm_dma(true, sbdsp.dma_block_size);
            break;
 
        case DSP_SET_TIME_CONSTANT: // 1.xx+ :Set digitized sound transfer constant
            if(sbdsp.dav_dsp) {
                if(sbdsp.current_command_index==1) {
//                PM_INFO("DSP_SET_TIME_CONSTANT %d\n",sbdsp.time_constant);
//                    sbdsp.mainloop_action=DSP_MLC_SET_TIME_CST;
                    sbdsp.time_constant = sbdsp.inbox;
                    sb_trace('T', sbdsp.time_constant);   // DOOM SFX diag: sample-rate time constant
                    sbdsp.dma_interval = 256 - sbdsp.time_constant;  // Interval is the Nb of us between each sample read

        // Initialize the timing values for the DMA Buffer read
                    sbdsp.sample_rate  = 1000000ul / sbdsp.dma_interval;
                    // Maybe change this in the buffer read code, triggered by a counter
        /// Buffer resampling variables init            
                    sbdsp.sample_step  = (uint32_t)(((uint64_t)sbdsp.sample_rate << 32) / PM_AUDIO_FREQUENCY);
                    sbdsp.increment    = 0;
                    sbdsp.readsample   = true;
                    sbdsp.prev_sample8u    = 0x80;
                    sbdsp.prev_sample8u_s0 = 0x80;
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
                    sb_trace('B', (uint8_t)(sbdsp.dma_block_size & 0xFF));  // DOOM SFX diag: block size lo
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
                    sb_trace('L', sbdsp.inbox);           // 0x14 length lo (PoP 8570 crawl diag)
                    sbdsp.dav_dsp=0;                    
                }
                else if(sbdsp.current_command_index==2) {
                    sbdsp.dma_sample_count += (sbdsp.inbox << 8);
                    sb_trace('l', sbdsp.inbox);           // 0x14 length hi
                    sb_trace('m', (uint8_t)(time_us_32() >> 12));   // coarse time, 4 ms units
                    // No PM_INFO -- Core 1 IOW context. Fires every TADA
                    // replay (~5/sec at SBDIAG loop); contributes to CDC
                    // ring backfill that triggers the alarm-pool deadlock
                    // on the cmd 0xD0 stop path.
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;
                    // (dma_sample_count_rx / autoinit / dma_rx_width now set by sbdsp_start_pcm_dma)
#if BOARD_WM10
                    // Arm passive CDMA trace ring: bus handler will push
                    // the next N IOWs to ports 0x18/0x1A into a ring;
                    // Core 0 drains via PM_INFO. Lets us see exactly how
                    // DOS programs CDMA before each single-mode block.
                    cdma_trace.head    = 0;
                    cdma_trace.tail    = 0;
                    cdma_trace.dropped = 0;
                    cdma_trace.budget  = 64;   // first 64 IOWs after cmd 0x14
#endif
#if USE_HWDMA || BOARD_WM10
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
#if BOARD_WM10
                    // Real DSP: the byte goes out through the 8-bit DMA channel
                    // (the host programmed it card->memory, count 1) and the host
                    // compares memory. Was sent to the outbox (never read; dav_pc
                    // stuck at 1) -> Flashback SETUP "Bad Sound Board
                    // configuration" on the 8570, 2026-09-12. One DMA cycle:
                    // REQ up, handle_ior answers the granted IOR with e2_value.
                    wm_dma.e2_value    = sbdsp.ident_e2[0];
                    wm_dma.e2_req_us   = time_us_32();
                    wm_dma.e2_pending  = true;
                    wm_dma.tc_latched  = false;
                    wm_dma.req_pending = true;
                    gpio_put(PIN_R_DMA_REQ, 1);
#else
                    // ideally output via DMA, but no DMA IORs yet -> outbox
                    sbdsp_output(sbdsp.ident_e2[0]);
#endif
                }
                sbdsp.current_command_index++;
            }
            break;   // upstream omitted this break (fell into ENABLE_SPEAKER) -- added
        case DSP_ENABLE_SPEAKER:
            // No PM_INFO -- Core 1 IOW context.
            sbdsp.speaker_on = true;
            sbdsp.current_command=0;
            break;
        case DSP_DISABLE_SPEAKER:
            // No PM_INFO -- Core 1 IOW context. SBDIAG's stop-key path
            // issues cmd 0xD3 right after cmd 0xD0; the back-to-back
            // printfs were what saturated the CDC ring and triggered
            // the dev_sbdsp_iow / alarm-pool deadlock.
            sbdsp.speaker_on = false;
            sbdsp.current_command=0;
            break;
        case DSP_SPEAKER_STATUS:
            if(sbdsp.current_command_index==0) {
                sbdsp.current_command=0;
                sbdsp_output(sbdsp.speaker_on ? 0xff : 0x00);
            }
            break;

        case DSP_DIRECT_DAC:       // 1.xx+ : one 8-bit sample, guest-paced (see sbdsp_dac_sample)
            if(sbdsp.dav_dsp) {
                if(sbdsp.current_command_index==1) {
                    sbdsp.cur_sample8=sbdsp.inbox;
                    sbdsp.dav_dsp=0;
                    sbdsp.current_command=0;
                    sbdsp_dac_sample(sbdsp.inbox);
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
                    sb_trace('P', (uint8_t)(sbdsp.dac_pause_duration & 0xFF));   // 0x80 silence lo
                    sb_trace('p', (uint8_t)(sbdsp.dac_pause_duration >> 8));     // 0x80 silence hi
                    sb_trace('m', (uint8_t)(time_us_32() >> 12));
                    sbdsp.dac_resume_pending = true;
#if SB_CMD_TRACE
                    // Gated (2026-09-04): this sits in the DSP command path, so
                    // a game that polls 0x80 (Tyrian uses it for detection)
                    // floods the console AND stalls the audio mix on the UART
                    // drain. Diagnostic only -- keep it behind the trace flag.
                    PM_INFO("SB Pause:%u\n\r",sbdsp.dac_pause_duration);
#endif
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
    sb_trace(value ? 'R' : 'r', value);        // DOOM SFX diag: DSP reset edge
    switch(value) {
        case 1:
            #if SB_CMD_TRACE
            PM_INFO("SB>RST ");
            #endif   // blocking printf per DSP reset = audible tick in direct-DAC titles (2026-09-12)
#if BOARD_WM10
            // SB driver always pulses reset before re-initializing the
            // card. Refresh the CDMA trace budget here so we capture the
            // driver's post-reset DMA programming (mode/count/page/mask).
            cdma_trace.head    = 0;
            cdma_trace.tail    = 0;
            cdma_trace.dropped = 0;
            cdma_trace.budget  = 200;
            // Clear any stale end-of-transfer latch from a previous
            // block so the next cmd 0x14 starts clean.
            wm_dma.tc_latched  = false;
            wm_dma.req_pending = false;
            gpio_put(PIN_R_DMA_REQ, 0);
            wm_dma.e2_pending  = false;           // cancel an unanswered 0xE2 DMA byte
            sbdsp.ident_e2[0]  = 0xAA;            // 0xE2 seeds restart at DSP reset (real DSP / DOSBox)
            sbdsp.ident_e2[1]  = 0x96;
            sbdsp_default_rate();                 // DSP reset restores the default time constant
            // DSP reset clears pending IRQ state on real HW: stop the EOT
            // pulse train and make sure the line is released so a
            // mid-detection reset can't carry an IRQ into the next round.
            wm_dma.eot_retry_pending = false;
            wm_sb_eot_holding        = false;   // lifecycle fix 2026-09-03: pending cleared -> hold window over
            bus_irq_lower(IRQ_R_AUDIO, sbdsp.irq);
#endif
            PIC_RemoveEvent(&DSP_Reset_Event);
            sbdsp.autoinit=0;
            sbdsp_dma_disable();
#if USE_HWDMA || BOARD_WM10
            isa_dma_stop_write();      // drop PIN_R_DMA_REQ + clear req_pending
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
            sb_trace('D', sbdsp.outbox);   // guest read a DSP response byte (2xA)
            return sbdsp.outbox;
      case DSP_READ_STATUS:   // 0x0E : Return bit7=1 if there is data to read from the DSP (Also acknowledge IRQ)
#if BOARD_WM10
            wm_dma.irq_ack_count++;   // INSTRUMENTATION: DOS ISR read 0x22E to ack the SB IRQ
            // Decimated 64:1 -- during the DOOM stuck-at-load 0x22E poll storm
            // (~2000 reads/s) an un-decimated K would flood the 256-slot ring
            // and bury the command tape we actually need.
            if ((wm_dma.irq_ack_count & 63) == 0)
                sb_trace('K', (uint8_t)wm_dma.irq_ack_count);   // guest 2xE read (64th)
#endif
            bus_irq_lower(IRQ_R_AUDIO,sbdsp.irq); // Acknowledge the IRQ
#if BOARD_WM10
            // Re-armed 2026-09-03 (double-entry hunt REOPENED -- PoP still
            // freezes with sound playing). Stamp EVERY 0x22E read, not just the
            // one that clears a live EOT: the diagnostic value is the SPACING
            // of the pair. Two reads a few us apart = ONE ISR reading status
            // twice (benign). Two reads ~20-100us apart = TWO ISR ENTRIES, i.e.
            // the held level re-latched at an early guest EOI -> nesting.
            wm_dma.ack_dt[wm_dma.ack_dt_idx & 3] =
                (uint32_t)(time_us_32() - wm_dma.eot_raise_us);
            wm_dma.ack_dt_idx++;
            if (wm_dma.eot_retry_pending) {
                // This read actually cleared a live EOT. Record how long the
                // line was up: ms-scale = a real ISR ack; us-scale = a polling
                // guest stealing the level before the 8259 could deliver it.
                uint32_t _h = time_us_32() - wm_dma.eot_raise_us;
                wm_dma.eot_hold_last_us = _h;
                if (_h < wm_dma.eot_hold_min_us) wm_dma.eot_hold_min_us = _h;
                wm_dma.eot_ack_pending++;
            }
            wm_dma.eot_retry_pending = false;     // guest acked -- stop the EOT pulse train
            // SB level released: end the hold window. A PM pulse deferred
            // during it is replayed by the Core-0 main loop with the full
            // pulse discipline (raise/200us/lower) -- NEVER a bare held raise
            // from this Core-1 bus-cycle context (a held IRQ7 re-fires at
            // every guest STI -> nested PM_Int -> stack overflow).
            wm_sb_eot_holding = false;
#endif
            DBG_OFF_IRQ_SB();
//            PM_INFO("iAck");
        return sbdsp.dav_pc << 7 | DSP_UNUSED_STATUS_BITS_PULLED_HIGH;
      case DSP_WRITE_STATUS:  // 0x0C : Return bit7=0 if the DSP is ready to receive Command/Data
      {
#if SB_DSP_WRITE_BUSY_TOGGLE
        // DOSBox shape (sblaster.cpp write_busy): report BUSY on 8 of every 16
        // status reads. Some drivers wait for bit 7 to go HIGH ("command
        // accepted") before waiting for it to go LOW; a DSP that is never busy
        // hangs them. Both loop styles exit within 8 reads.
        // CONFIRMED 2026-09-12 (8570, Prince of Persia end of level 1): with a
        // never-busy DSP the game polled 0x22C ~190k/s forever and its ISR
        // replayed the same 4436-byte block on every EOT (ack+2, adt 14/42 =
        // one ISR pass, NOT a double entry); with this toggle the level ends.
        static uint8_t wb = 0; wb++;
        bool tgl = (wb & 8) != 0;
#else
        bool tgl = false;
#endif
        return ((sbdsp.dav_dsp | sbdsp.dsp_busy | sbdsp.dac_resume_pending | tgl) ? 0x80 : 0x00) | DSP_UNUSED_STATUS_BITS_PULLED_HIGH;
      }
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
#if BOARD_WM10
 // WM10 doesn't use the isa_dma virtual buffer pump -- sbdsp_dma_isr
 // pushes captured bytes directly into sbdsp_fifo from the wm_dma
 // completion callback. Nothing to do here.
 (void)samples;
 return;
#else
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
#endif // !BOARD_WM10
}

// Worked/Code to call regularly from the main loop
// Transfer data from the DMA to the Sound Blaster Audio FIFO
// "input" is sbdsp.vdma_to_send the value change during the code execution.
void sbdsp_dma_worker()
{
#if BOARD_WM10
    // WM10 takes neither the HW-DMA nor the virtual-DMA path. wm_dma's
    // completion ISR (sbdsp_dma_isr) handles byte capture directly into
    // sbdsp_fifo, and the timeout/watchdog is in pm_cmd.cpp's main loop
    // (the wm_dma watchdog drops REQ after 100us idle). Nothing to do
    // here. The #elif below ensures the rest of the function body is
    // not even parsed on the WM10 build, so the isa_dma_* references
    // don't trigger "not declared" errors.
    return;
#elif USE_HWDMA

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
  if (sbdsp.mode == MODE_DAC) sbdsp_dac_servo();   // direct-DAC (0x10) rate tracking
  int8_t   res8u;
  uint32_t prev_increment;
  int16_t  fres;
  int8_t   s0;                          /* LI: previous consumed sample (s0 in s0→s1 pair) */
  res8u = sbdsp.prev_sample8u;
  s0    = (int8_t)sbdsp.prev_sample8u_s0;
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
        if (sbdsp.readsample) {
            s0 = res8u;   /* old s1 becomes s0 before consuming new s1 */
            if (!afifo_take_sample8(&sbdsp_fifo, &res8u))
              {
                if (sbdsp.mode == MODE_DAC) {
                    // Direct DAC ran dry (guest jitter / sound ended): a real DAC
                    // HOLDS its last value. Snapping to 0x80 here was an audible
                    // click on every gap (Another World, 2026-09-12). Hold; only
                    // after a full block dry (sound really stopped) creep toward
                    // mid-scale one LSB per output sample so the silence ends
                    // without a step.
                    sb_dac_st_dry++;
                    if (++sb_dac_dry > PM_SAMPLES_PER_BUFFER) {
                        uint8_t u = (uint8_t)res8u;
                        if (u > 0x80) u--; else if (u < 0x80) u++;
                        res8u = (int8_t)u;
                    }
                } else {
                 sbdsp.prev_sample8u    = 0x80;
                 sbdsp.prev_sample8u_s0 = (uint8_t)s0;
                return;
                }
              }
            else sb_dac_dry = 0;
#if SB_CMD_TRACE
            { uint8_t u = (uint8_t)res8u;                 // DOOM SFX diag: raw byte range
              if (u < sb_dbg_in_min) sb_dbg_in_min = u;
              if (u > sb_dbg_in_max) sb_dbg_in_max = u;
              sb_dbg_drained++; }
#endif
        }
        /* Linear interpolation between s0 (previous FIFO sample) and res8u (current).
         * frac = top 16 bits of the 32-bit accumulator = position within the s0→s1
         * interval (0 = just consumed s1, 65535 = just before next sample boundary).
         * Eliminates ZOH aliasing (images at n*f_s ± f) and the uint16 overflow bug
         * for programmed rates ≥ 49716 Hz. */
        { uint32_t frac = sbdsp.increment >> 16;
          int16_t v0 = (int16_t)((s0    << 8) + 0x8000) / 4;
          int16_t v1 = (int16_t)((res8u << 8) + 0x8000) / 4;
          fres = (int16_t)((int32_t)v0 + (((int32_t)(v1 - v0) * (int32_t)frac) >> 16)); }
#if SB_CMD_TRACE
        { int16_t a = fres < 0 ? (int16_t)-fres : fres;   // DOOM SFX diag: output amplitude
          if (a > sb_dbg_out_peak) sb_dbg_out_peak = a; }
#endif
        prev_increment=sbdsp.increment;
        sbdsp.increment+=sbdsp.sample_step;
        sbdsp.readsample = (sbdsp.increment<=prev_increment) ? true : false;

        /* ssat #16: clip the mixed sum cleanly instead of wrapping. */
        { int32_t _s;
          asm("ssat %0, #16, %1" : "=r"(_s) : "r"((int32_t)buff[sn]   + fres)); buff[sn]   = (int16_t)_s;
          asm("ssat %0, #16, %1" : "=r"(_s) : "r"((int32_t)buff[sn+1] + fres)); buff[sn+1] = (int16_t)_s; }
      }
    sbdsp.prev_sample8u    = (uint8_t)res8u;
    sbdsp.prev_sample8u_s0 = (uint8_t)s0;
   }    

}
#endif // BOARD_WM10
