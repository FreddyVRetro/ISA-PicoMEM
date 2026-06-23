#pragma once
//Definitions for the QSPI PSRAM Code

#if BOARD_PM15 || BOARD_PM20 || BOARD_WM10
#define RP2350_PSRAM_CS 0
#else
#define RP2350_PSRAM_CS 47  // Pimoroni pico plus 2
#endif

#ifndef PSRAM_HEAP    //Value 1 taken from CMakefile
#define PSRAM_HEAP 0
#endif