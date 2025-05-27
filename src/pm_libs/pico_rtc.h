#pragma once

#include <ctime>

extern void printdatetime(tm *t);
extern void pico_rtc_init();
extern uint32_t pico_rtc_getDOSDate();
extern void pico_rtc_setDOSDate(uint32_t ddate);
extern void pico_rtc_printdate();