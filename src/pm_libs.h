#pragma once

// LED Error codes
#define PM_ERR_NOPSRAM 1  //PSRAM not found
#define PM_ERR_MEM     2  

//#if PM_DEBUG
//#define PM_PRINTF pm_printf
//#else
//#define PM_PRINTF(fmt, args...) /* Don't do anything in release builds*/
//#endif

extern void pm_timefrominit();
extern void pm_timefromlast();

extern void pm_led(uint8_t onoff);
extern void err_blink(uint8_t nblong, uint8_t nbshort,uint8_t nbrepeat);
//uint8_t pm_mountdiskimage(char *diskfilename, uint8_t floppynb, uint8_t hddnumber);