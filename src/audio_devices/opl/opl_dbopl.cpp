// DOSBox OPL core adaptation for PicoGUS
// mostly just copied from opl_ymfm.cpp

#include "opl.h"
#include "hardware/sync.h"
#include "hardware/timer.h"

#include "dbopl/dbopl.h"

#if OPL_CMD_BUFFER
#include "include/cmd_buffers.h"
extern cms_buffer_t opl_cmd_buffer;
#endif

#ifdef BOARD_PICOMEM
#include "pm_audio.h"
#endif

// ---------------------------------------------------------------------------
// Chip selection
// ---------------------------------------------------------------------------
#ifdef USE_YMF3812
static constexpr unsigned int OPL_STATUS_BASE = 0x06; // OPL2 detection
#else
static constexpr unsigned int OPL_STATUS_BASE = 0x00; // OPL3 detection
#endif

// ---------------------------------------------------------------------------
// Timer state — Core 0 only
// ---------------------------------------------------------------------------
typedef struct
{
    unsigned int rate;
    unsigned int enabled;
    unsigned int value;
    uint64_t expire_time;
} opl_timer_t;

static opl_timer_t timer1 = { 12500, 0, 0, 0 };
static opl_timer_t timer2 = { 3125,  0, 0, 0 };

static void OPLTimer_CalculateEndTime(opl_timer_t *timer)
{
    if (timer->enabled)
    {
        int tics = 0x100 - timer->value;
        timer->expire_time = time_us_64()
                           + ((uint64_t)tics * OPL_SECOND) / timer->rate;
    }
}

static DBOPL::Chip dbopl3(true);

// Temporary buffer as dbopl3 generates 32-bit samples, but we need to return 16-bit samples to the audio mixer.
static int32_t opl_buf32s[2*PM_SAMPLES_PER_BUFFER];   // interleaved stereo

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int OPL_Pico_Init(unsigned int port_base)
{
    dbopl3.Setup(49716);
#if !BOARD_PICOMEM
    s_prebuf_head = PREBUF_SIZE;
#endif    
    return 1;
}

#ifdef BOARD_PICOMEM
void OPL_Pico_Shutdown(void)
{

}
#endif

unsigned int OPL_Pico_PortRead(opl_port_t port)
{
    unsigned int result = OPL_STATUS_BASE;
    __dmb();
    uint64_t pico_time = time_us_64();

    if (timer1.enabled && pico_time > timer1.expire_time)
    {
        result |= 0x80 | 0x40;
    }
    if (timer2.enabled && pico_time > timer2.expire_time)
    {
        result |= 0x80 | 0x20;
    }

    return result;
}

void OPL_Pico_WriteRegister(unsigned int reg_num, unsigned int value)
{
    switch (reg_num)
    {
        case OPL_REG_TIMER1:
            timer1.value = value;
            OPLTimer_CalculateEndTime(&timer1);
            break;

        case OPL_REG_TIMER2:
            timer2.value = value;
            OPLTimer_CalculateEndTime(&timer2);
            break;

        case OPL_REG_TIMER_CTRL:
            if (value & 0x80)
            {
                timer1.enabled = 0;
                timer2.enabled = 0;
            }
            else
            {
                if ((value & 0x40) == 0)
                {
                    timer1.enabled = (value & 0x01) != 0;
                    OPLTimer_CalculateEndTime(&timer1);
                }
                if ((value & 0x20) == 0)
                {
                    timer2.enabled = (value & 0x02) != 0;
                    OPLTimer_CalculateEndTime(&timer2);
                }
            }
            break;

        default:
            dbopl3.WriteReg(reg_num, value);
            break;
    }
}

// Mono interface — kept for compatibility with non-ymfm OPL backends.
// Returns the average of L+R so mono output is at the same level as stereo.
void OPL_Pico_simple16(int16_t *buffer, uint32_t nsamples)
{
    int32_t *b =opl_buf32s;
    dbopl3.GenerateBlock3(nsamples,b);

    for (uint32_t i = 0; i < nsamples; i++)
    {
        buffer[i] = b[2*i]+b[2*i+1]>> 1 + 17;
    }
}

void OPL_Pico_stereo_int16(int16_t  *buffer, uint32_t nsamples)
{
    int32_t *b =opl_buf32s;
    dbopl3.GenerateBlock3(nsamples,b);

    for (uint32_t i = 0; i < nsamples*2; i++)
    {
      buffer[i] = (int16_t) (b[i]);
    }
}