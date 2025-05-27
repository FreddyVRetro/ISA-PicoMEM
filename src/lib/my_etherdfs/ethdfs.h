#pragma once
// *** PicoMEM Library containing the HDD and Floppy emulation functions
// 
#ifdef __cplusplus
extern "C" {
#endif

extern void ethdfs_init();
extern void ethdfs_docmd(uint8_t *buffer);
extern void ethdfs_test();

#ifdef __cplusplus
}
#endif