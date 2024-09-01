#pragma once

// Fonctions in Commands.h and used by picomem.cpp

extern PMCFG_t *PM_Config;             // Pointer to the PicoMEM Configuration in the BIOS Shared Memory
extern volatile uint8_t PM_Status;     // Read from Port 0 > Command and Pico Status ( ! When a SD Command is in progress, sometimes read FFh)
extern bool PSRAM_Enabled;
extern bool SERIAL_Enabled;	// True is Serial output is enabled (Can do printf)
extern bool SERIAL_USB;

extern void main_core0();