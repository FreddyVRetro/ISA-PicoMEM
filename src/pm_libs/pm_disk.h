#pragma once
// *** PicoMEM Library containing the HDD and Floppy emulation functions
// 

#ifdef __cplusplus
extern "C" {
#endif

uint8_t pm_mountfddimage(char *diskfilename, uint8_t fddnumber);
void    pm_disk_mount_fdd(uint8_t fdd_nb);
void    pm_disk_update_fdd_nb();

uint8_t pm_mounthddimage(char *diskfilename, uint8_t hddnumber);

void    pm_disk_InitSearchPath();
void    pm_disk_UpdateSearchPath(uint8_t id,FSizeName_t *DiskListPrt);
uint8_t SelectImage(uint8_t device,uint8_t imageid, FSizeName_t *DiskListPrt);
uint8_t ListImages(uint8_t dtype,uint8_t *ImageNb, FSizeName_t *DiskListPrt);

uint8_t NewHDDImage();
uint8_t pm_LoadConfig();  // Load the config file, return false if failed
uint8_t pm_SaveConfig();  // Save the config file, return an error if failed
void ls(const char *dir);

extern uint32_t PC_DB_Start; // Start of the Shared disk buffer in the Host (PC) RAM
extern char image_path[64];  // Search path for Image disk load

// Disk Files objects
extern FIL FDDfile[2];
extern FIL HDDfile[4];

#ifdef __cplusplus
}
#endif