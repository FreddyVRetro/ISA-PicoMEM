#ifndef TEST_TUSB_H
#define TEST_TUSB_H
#include <stdint.h>
#include <stdbool.h>
#define SCSI_CMD_READ_10 0x28
#define MSC_CSW_SIGNATURE 0x53425355u
typedef struct {uint32_t total_bytes,tag;uint8_t command[16];} msc_cbw_t;
typedef struct {uint32_t signature,tag,data_residue;uint8_t status;} msc_csw_t;
typedef struct {const msc_cbw_t *cbw;const msc_csw_t *csw;uintptr_t user_arg;bool raw_read_complete;} tuh_msc_complete_data_t;
typedef bool (*tuh_msc_complete_cb_t)(uint8_t,const tuh_msc_complete_data_t *);
bool tuh_msc_read10(uint8_t,uint8_t,void *,uint32_t,uint16_t,tuh_msc_complete_cb_t,uintptr_t);
bool tuh_msc_mounted(uint8_t);
void tuh_task(void);
void tuh_vid_pid_get(uint8_t,uint16_t *,uint16_t *);
uint32_t tuh_msc_get_block_count(uint8_t,uint8_t);
uint32_t tuh_msc_get_block_size(uint8_t,uint8_t);
#endif
