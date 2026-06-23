#pragma once

uint8_t PM_PICO_StartPSRAM();
bool PM_StartSD();

// Exported functions // (For CPP and C)
extern "C" bool PM_PICO_EnablePSRAM();
extern "C" bool PM_EnableSD();

bool PM_StartUSB();
bool PM_StopUSB();
