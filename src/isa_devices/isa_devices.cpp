// Table to the IO devices fonctions

#include "pico/stdlib.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"

#include "dev_post.h"        // 1
#include "dev_picomem_io.h"
#include "dev_dma.h"
#include "dev_memory.h"      // 5 EMS
#ifdef RASPBERRYPI_PICO_W
#include "dev_ne2000.h"      // 6   > ! Need to change the code 
#endif
#include "dev_joystick.h"
#include "dev_adlib.h"
#include "dev_tandy.h"
#include "dev_cms.h"
#include "dev_tandy.h"
#include "dev_mindscape.h"
#include "dev_sbdsp.h"


// Devices 0 to 3 generate no IOCHRDY (Write only)
/*
#define DEV_NULL   0   // No devide use this port
#define DEV_POST   1   // POST Code port
#define DEV_IRQ    2   // ! NoWS IRQ Controller IOW > Out 20h counter and Port 21h register
#define DEV_DMA    3   // DMA Controller IOW
#define DEV_PM     4   // WS PicoMEM Port
#define DEV_LTEMS  5   // WS Lotech EMS Emulation (2Mb)
#define DEV_NE2000 6
#define DEV_JOY    7
#define DEV_ADLIB  8
#define DEV_CMS    9
#define DEV_TANDY  10
#define DEV_SBDSP  11
#define DEV_RTC    12
#define DEV_MMB    13
*/

bool dev_null_ior(uint32_t Addr,uint8_t *Data)
{
// *Data=0xFF;
 return false;
}

void dev_null_iow(uint32_t Addr,uint8_t Data)
{}

bool (*dev_ior[12])(uint32_t Addr, uint8_t *Data) =
     {dev_null_ior, dev_null_ior, dev_null_ior  , dev_dma_ior,
      dev_pmio_ior, dev_ems_ior , dev_ne2000_ior, dev_joystick_ior,
      dev_adlib_ior, dev_cms_ior, dev_tdy_ior, dev_sbdsp_ior, dev_rtc_ior, dev_mmb_ior};

void (*dev_iow[12])(uint32_t Addr, uint8_t Data) = 
     {dev_null_iow, dev_post_iow, dev_null_iow  , dev_dma_iow,
      dev_pmio_iow, dev_ems_iow , 
#ifdef RASPBERRYPI_PICO_W
      dev_ne2000_iow,
#else
      dev_null_iow,
#endif      
      dev_joystick_iow,
      dev_adlib_iow, dev_cms_iow, dev_tdy_iow,dev_sbdsp_iow, dev_rtc_iow, dev_mmb_iow};