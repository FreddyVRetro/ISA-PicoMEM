#pragma once

#include <ctime>

extern void printdatetime(tm *t);
extern void pico_rtc_init();
extern uint32_t pico_rtc_getDOSDate();
extern void pico_rtc_setDOSDate(uint32_t ddate);
extern void pico_rtc_printdate();

/* The card has ONE clock: the RP2350 always-on timer. FatFs reads it through
   get_fattime(), so anything that sets the time must go through these. */
extern bool pico_rtc_set_tm(struct tm *t);
extern bool pico_rtc_get_tm(struct tm *out);
extern uint32_t pico_rtc_get_subsec_us();