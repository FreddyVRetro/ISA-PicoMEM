#pragma once

#include "cdrom.h"

#ifdef __cplusplus
extern "C" {
#endif

char** cdman_list_images(int *fileCount);
void cdman_list_images_free(char **fileList, int fileCount);

// Point the list/load helpers at a sub-folder of 0:/CDROM. `subpath` is
// relative to the CDROM root ("sub/dir/" with a trailing '/'); NULL/empty
// resets to the root. Threaded from PM_Config->CDRPath by the mount path.
void cdman_set_subdir(const char *subpath);
uint8_t cdman_load_image_named(cdrom_t *dev, const char *filename);

uint8_t cdman_current_image_index(void);
void cdman_load_image_index(cdrom_t *dev, int imageIndex);
void cdman_set_image_index(cdrom_t *dev);
void cdman_unload_image(cdrom_t *dev);
void cdman_clear_image(void);
void cdman_reload_image(cdrom_t *dev);
void cdman_set_serial(cdrom_t *dev, uint32_t serial);
void cdman_set_autoadvance(bool setting);

// Display size (MB) for a CD image path ("0:/CDROM/x.iso" or ".../x.cue").
// .cue -> sums the referenced .bin file(s); .iso -> stats the file. See .c.
uint32_t cd_image_size_mb(const char *fullpath);

#ifdef __cplusplus
} // extern "C"
#endif
