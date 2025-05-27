#pragma once
// *** PicoMEM Library containing the HDD and Floppy emulation functions
// 

extern uint8_t pm_mountfddimage(char *diskfilename, uint8_t fddnumber);
extern uint8_t pm_mounthddimage(char *diskfilename, uint8_t hddnumber);
extern uint8_t ListImages(uint8_t dtype,uint8_t *ImageNb);
extern uint8_t NewHDDImage();
extern uint8_t PM_LoadConfig();     // Load the config file, return false if failed
extern uint8_t PM_SaveConfig();  // Save the config file, return an error if failed
extern void ls(const char *dir);

extern uint32_t PC_DB_Start;  // Start of the Shared disk buffer in the Host (PC) RAM

// Disk Files objects
extern FIL FDDfile[2];
extern FIL HDDfile[4];