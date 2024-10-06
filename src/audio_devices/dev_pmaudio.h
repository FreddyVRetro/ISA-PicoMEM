#pragma once
/*
 * Command buffers for CMS and Tandy bus events
 */

// Data and Address Commands buffer (For CMS / Adlib)

typedef struct iow_buffer_ad_t {
    volatile uint8_t addrs[256];
    volatile uint8_t datas[256];    
    volatile uint8_t head;
    volatile uint8_t tail;
} iow_buffer_da_t;

// Data Commands buffer (For Tandy)

typedef struct iow_buffer_d_t {
    volatile uint8_t datas[256];
    volatile uint8_t head;
    volatile uint8_t tail;
} iow_buffer_d_t;

/*
__force_inline bool io_buffer_da_push(uint8_t addr, uint8_t data)
{

return false;
}

__force_inline bool io_buffer_da_pop(uint8_t *addr, uint8_t *data)
{
 if (iow_buffer_ad.tail != iow_buffer_ad.head) 
    {
     *addr = iow_buffer_ad.addrs[iow_buffer_ad.tail];
     *data = iow_buffer_ad.datas[iow_buffer_ad.tail];
     ++iow_buffer_da.tail;
     return true;
    }
return false;
}
*/

extern void pm_audio_start();
extern void pm_audio_stop();
extern bool pm_audio_isactive();