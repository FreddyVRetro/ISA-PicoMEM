#if USE_CDROM
#include "cdrom_image_manager.h"

#include "ff.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <stdio.h>

#include "cdrom_error_msg.h"
#include "f_util.h"
#include "pm_debug.h"

// Maximum filename length
#define MAX_FILENAME_LEN 127

// All CD images live under the CDROM folder of the SD card (volume 0).
#define CDMAN_DIR "0:/CDROM"

// Current browse/mount directory (full path, no trailing slash). Defaults
// to the CDROM root; cdman_set_subdir() points it at a sub-folder so the
// list/load helpers below operate inside the folder the user descended
// into (BIOS menu + screenpad both browse sub-folders now).
static char cdman_dir[96] = CDMAN_DIR;

// Set the active directory from a sub-path relative to 0:/CDROM. A NULL or
// empty sub-path resets to the CDROM root. Any trailing '/' is stripped so
// FatFs f_opendir accepts it. `subpath` mirrors PM_Config->CDRPath (a
// "sub/dir/" fragment with a trailing '/').
void cdman_set_subdir(const char *subpath) {
    if (!subpath || !subpath[0]) { strcpy(cdman_dir, CDMAN_DIR); return; }
    snprintf(cdman_dir, sizeof cdman_dir, "%s/%s", CDMAN_DIR, subpath);
    size_t n = strlen(cdman_dir);
    while (n > 0 && (cdman_dir[n-1] == '/' || cdman_dir[n-1] == '\\')) cdman_dir[--n] = 0;
    if (cdman_dir[0] == 0) strcpy(cdman_dir, CDMAN_DIR);
}

// Node structure for the linked list of CD image files
typedef struct CDImageNode {
    char *filename;
    struct CDImageNode *next;
} CDImageNode;

static CDImageNode* create_cd_node(const char *filename) {
    CDImageNode *new_node = malloc(sizeof(CDImageNode));
    if (!new_node) {
        return NULL; // Memory allocation failed
    }
    
    // Allocate memory for the filename and copy it
    new_node->filename = malloc(strlen(filename) + 1);
    if (!new_node->filename) {
        free(new_node);
        return NULL; // Memory allocation failed
    }
    
    strcpy(new_node->filename, filename);
    new_node->next = NULL;
    return new_node;
}

// Add a filename to the sorted list (case insensitive)
static bool add_cd_image_sorted(CDImageNode **head, const char *filename) {
    CDImageNode *new_node = create_cd_node(filename);
    if (!new_node) {
        return false; // Memory allocation failed
    }
    
    // Case 1: Empty list or new filename should be first
    if (!*head || strncasecmp(filename, (*head)->filename, MAX_FILENAME_LEN) < 0) {
        new_node->next = *head;
        *head = new_node;
        return true;
    }
    
    // Case 2: Find the correct position to insert
    CDImageNode *current = *head;
    while (current->next && strncasecmp(filename, current->next->filename, MAX_FILENAME_LEN) > 0) {
        current = current->next;
    }
    
    // Insert the new node
    new_node->next = current->next;
    current->next = new_node;
    
    return true;
}

static bool isCDImage(const char *filename) {
    int len = strlen(filename);
    if (len < 4) return false;
    // Filter out macOS extended attribute files
    if (strncmp(filename, "._", 2) == 0) {
        return false;
    }
    return (strncasecmp(filename + (len - 4), ".iso", 4) == 0 ||
            strncasecmp(filename + (len - 4), ".cue", 4) == 0);
}

// convert linked list to array and free the list
static char** cd_list_to_array(CDImageNode *head, int *count) {
    if (!head || !count) {
        return NULL;
    }
    
    // Count nodes
    *count = 0;
    CDImageNode *current = head;
    while (current) {
        (*count)++;
        current = current->next;
    }
    
    // Allocate array of string pointers
    char **fileList = (char **)malloc(*count * sizeof(char *));
    if (!fileList) {
        return NULL;
    }
    
    // Transfer filename ownership from nodes to array and free nodes
    current = head;
    int i = 0;
    while (current) {
        CDImageNode *temp = current;
        
        // Transfer ownership of filename string
        fileList[i] = current->filename;
        i++;
        
        current = current->next;
        free(temp); // Only free the node, not the filename
    }
    
    return fileList;
}

static void free_cd_list(CDImageNode *head) {
    CDImageNode *current = head;
    while (current) {
        CDImageNode *temp = current;
        current = current->next;
        free(temp->filename);
        free(temp);
    }
}

/**
 * Lists files in a directory with .iso and .cue extensions, sorted alphabetically
 * 
 * @param fileCount: Pointer to int that will receive the number of files found
 * @return: Array of strings containing filenames, or NULL on error
 *          Caller needs to free memory with cdman_list_images_free
 */
char** cdman_list_images(int *fileCount) {
    if (!fileCount) {
        return NULL;
    }
    
    *fileCount = 0;
    
    // Open the directory
    DIR dp;
    FRESULT res = f_opendir(&dp, cdman_dir);
    if (res != FR_OK) {
        cdrom_errorstr_set("No CDROM folder on the SD card");
        return NULL;
    }
    
    CDImageNode *head = NULL;
    FILINFO fno;
    
    // Read directory entries and build sorted linked list
    while (1) {
        res = f_readdir(&dp, &fno);
        if (res != FR_OK || fno.fname[0] == 0) {
            break; // End of directory or error
        }
        
        // Skip directories
        if (fno.fattrib & AM_DIR) {
            continue;
        }
        
        // Check if it has valid extension and add to sorted list
        if (isCDImage(fno.fname)) {
            if (!add_cd_image_sorted(&head, fno.fname)) {
                // Memory allocation failed, clean up and return error
                f_closedir(&dp);
                free_cd_list(head);
                cdrom_errorstr_set("Memory allocation failed");
                return NULL;
            }
        }
    }
    
    f_closedir(&dp);
    
    if (!head) {
        cdrom_errorstr_set("No image files in the CDROM folder");
        return NULL;
    }
    
    // Convert linked list to array and clean up list
    char **fileList = cd_list_to_array(head, fileCount);
    if (!fileList) {
        free_cd_list(head);
        cdrom_errorstr_set("Memory allocation failed");
        return NULL;
    }
    
    return fileList;
}

/**
 * Helper function to free the array returned by cdman_list_images
 * 
 * @param fileList: Array of strings returned by cdman_list_images
 * @param fileCount: Number of files in the array
 */
void cdman_list_images_free(char **fileList, int fileCount) {
    if (fileList) {
        for (int i = 0; i < fileCount; i++) {
            if (fileList[i]) {
                free(fileList[i]);
            }
        }
        free(fileList);
    }
}

static uint8_t current_index, last_loaded_index;

uint8_t cdman_current_image_index(void) {
    return current_index;
}


void cdman_load_image_index(cdrom_t *dev, int imageIndex) {
    /* printf("Loading index %u (was %u)\n", imageIndex, current_index); */
    if (imageIndex == 0) {
        cdman_unload_image(dev);
    } else {
        int imageCount;
        char** images = cdman_list_images(&imageCount);
        if (!images) {
            dev->image_command = CD_COMMAND_NONE;
            dev->image_status = CD_STATUS_ERROR;
            return;
        }
        if (imageIndex > imageCount) {
            // Wrap around index for autoadvance
            imageIndex = 1;
        }
        // Full path: the CD loader (cdrom_image_open) opens image_path as-is
        snprintf(dev->image_path, sizeof(dev->image_path), "%s/%s", cdman_dir, images[imageIndex - 1]);
        cdman_list_images_free(images, imageCount);
        dev->image_command = CD_COMMAND_IMAGE_LOAD;
    }
    current_index = last_loaded_index = imageIndex;
}

// Load an image by its plain filename (with extension) as listed in the
// CDROM folder. Returns 0 on success, 1 if the name is not in the list.
uint8_t cdman_load_image_named(cdrom_t *dev, const char *filename) {
    int imageCount;
    char **images = cdman_list_images(&imageCount);
    if (!images) return 1;
    uint8_t rc = 1;
    for (int i = 0; i < imageCount; ++i) {
        if (strncasecmp(filename, images[i], MAX_FILENAME_LEN) == 0) {
            cdman_load_image_index(dev, i + 1);
            rc = 0;
            break;
        }
    }
    cdman_list_images_free(images, imageCount);
    return rc;
}

void cdman_set_image_index(cdrom_t *dev) {
    int imageCount;
    char** images = cdman_list_images(&imageCount);
    if (!images) {
        dev->image_command = CD_COMMAND_NONE;
        dev->image_status = CD_STATUS_ERROR;
        return;
    }
    // image_path now carries the CDMAN_DIR prefix -- compare basenames
    const char *base = strrchr(dev->image_path, '/');
    base = base ? base + 1 : dev->image_path;
    current_index = 0;
    for (int i = 0; i < imageCount; ++i) {
        if (strncasecmp(base, images[i], MAX_FILENAME_LEN) == 0) {
            current_index = last_loaded_index = i + 1;
            // Copy back the canonical name to image_path with proper case
            snprintf(dev->image_path, sizeof(dev->image_path), "%s/%s", cdman_dir, images[i]);
            break;
        }
    }
    cdman_list_images_free(images, imageCount);
}


void cdman_unload_image(cdrom_t *dev) {
    dev->image_path[0] = 0;
    dev->image_command = CD_COMMAND_IMAGE_LOAD;
    current_index = 0;
}


void cdman_clear_image(void) {
    current_index = 0;
}


static uint32_t drive_serial;
static bool autoadvance;

void cdman_set_autoadvance(bool setting) {
    PM_INFO("setting autoadvance to %u\n", setting);
    autoadvance = setting;
}


void cdman_reload_image(cdrom_t *dev) {
    cdman_load_image_index(dev, autoadvance ? (last_loaded_index + 1) : last_loaded_index);
}


void cdman_set_serial(cdrom_t *dev, uint32_t serial) {
    if (drive_serial == serial) {
        // If we are re-inserting the same drive, maybe advance the disc image
        PM_INFO("Inserting the same drive...\n");
        cdman_reload_image(dev);
    } else {
        drive_serial = serial;
        PM_INFO("New drive with serial %u inserted", serial);
        cdman_load_image_index(dev, 1);
    }
}

// Display size (MB) for a CD image at `fullpath` (e.g. "0:/CDROM/game.cue").
// For .iso, stat the file directly. For .cue, the file itself is a tiny text
// sheet (would show 0 MB), so parse its FILE "name" line(s) and sum the sizes
// of the referenced data file(s), resolved relative to the cue's own directory.
//
// STACK OVERFLOW HISTORY (root cause of the "0MB"/crash-after-N-refreshes
// bug): this function used to declare a 2048-byte whole-file read buffer
// (plus FIL, which embeds a 512-byte sector window since FF_FS_TINY==0 for
// RP2350) as locals, putting the compiled stack frame at ~3.1KB while Core
// 0's *entire* stack is only 2048 bytes (__StackBottom=0x20081800 ..
// __StackTop=0x20082000, see the .map file / PICO_STACK_SIZE) -- a single
// call already overran the whole stack. Parsing was rewritten to read the
// .cue line-at-a-time via f_gets() (FF_USE_STRFUNC==1, see ffconf.h)
// instead of slurping the whole file, so the working buffers are now small
// enough (CD_CUE_LINE_MAX=160 + two short path buffers + FIL) to live back
// on the stack as ordinary locals -- confirmed via `arm-none-eabi-objdump
// -d` that the resulting frame is a few hundred bytes, comfortably inside
// budget alongside the rest of the call chain (ListImages ~444 bytes).
#define CD_CUE_LINE_MAX 160

uint32_t cd_image_size_mb(const char *fullpath) {
    if (!fullpath) return 0;
    size_t len = strlen(fullpath);
    bool is_cue = (len >= 4) &&
                  (fullpath[len-4] == '.') &&
                  (fullpath[len-3] == 'c' || fullpath[len-3] == 'C') &&
                  (fullpath[len-2] == 'u' || fullpath[len-2] == 'U') &&
                  (fullpath[len-1] == 'e' || fullpath[len-1] == 'E');
    if (!is_cue) {
        FILINFO fno;
        FRESULT fr = f_stat(fullpath, &fno);
        if (fr != FR_OK) {
            PM_INFO("[CDR] cd_image_size_mb: f_stat(%s) failed: %s (%d)\n",
                   fullpath, FRESULT_str(fr), fr);
            return 0;
        }
        return (uint32_t)(fno.fsize >> 20);
    }
    // Derive the cue's directory (keep the trailing '/').
    char dir[96];
    size_t dlen = len;
    while (dlen > 0 && fullpath[dlen-1] != '/' && fullpath[dlen-1] != '\\') dlen--;
    if (dlen >= sizeof dir) {
        PM_INFO("[CDR] cd_image_size_mb: directory of %s too long\n", fullpath);
        return 0;
    }
    memcpy(dir, fullpath, dlen); dir[dlen] = 0;

    FIL f;
    FRESULT fr = f_open(&f, fullpath, FA_READ);
    if (fr != FR_OK) {
        PM_INFO("[CDR] cd_image_size_mb: f_open(%s) failed: %s (%d)\n",
               fullpath, FRESULT_str(fr), fr);
        return 0;
    }

    char cue_line[CD_CUE_LINE_MAX];
    uint64_t total = 0;
    bool any_line = false;
    while (f_gets(cue_line, sizeof cue_line, &f)) {
        any_line = true;
        char *buf = cue_line;
        UINT br = (UINT)strlen(buf);
        for (UINT i = 0; i + 4 <= br; i++) {
            // case-insensitive match of the FILE keyword
            if ((buf[i]|0x20) != 'f' || (buf[i+1]|0x20) != 'i' ||
                (buf[i+2]|0x20) != 'l' || (buf[i+3]|0x20) != 'e') continue;
            // find the quoted filename on this line
            UINT j = i + 4;
            while (j < br && buf[j] != '"' && buf[j] != '\n') j++;
            if (j >= br || buf[j] != '"') continue;
            UINT s = ++j;
            while (j < br && buf[j] != '"') j++;
            if (j >= br) break;
            UINT blen = j - s;
            if (blen == 0 || blen > 80) { i = j; continue; }
            char binpath[96 + 82];
            int k = snprintf(binpath, sizeof binpath, "%s", dir);
            if (k < 0 || (size_t)k + blen >= sizeof binpath) { i = j; continue; }
            memcpy(binpath + k, buf + s, blen);
            binpath[k + blen] = 0;
            FILINFO fno;
            fr = f_stat(binpath, &fno);
            if (fr == FR_OK) {
                total += fno.fsize;
            } else {
                PM_INFO("[CDR] cd_image_size_mb: referenced file %s not found: %s (%d)\n",
                       binpath, FRESULT_str(fr), fr);
            }
            i = j;   // continue scanning after this FILE entry
        }
    }
    FRESULT gfr = f_error(&f);
    f_close(&f);
    if (!any_line) {
        PM_INFO("[CDR] cd_image_size_mb: %s read no lines (f_error=%s (%d))\n",
               fullpath, FRESULT_str(gfr), gfr);
        return 0;
    }
    return (uint32_t)(total >> 20);
}

#endif // USE_CDROM
