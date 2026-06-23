/*
 * file_stream.h
 *
 * Wraps a FreeRTOS+FAT FF_FILE in a standard I/O stream FILE
 * to take advantage of the buffering provided by the standard I/O library
 * for a tremendous speedup of random-sized writes (e.g., fprintf output).
 *
 *  Created on: Jun 20, 2024
 *      Author: carlk
 */

#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

FILE *open_file_stream( const char *pcFile, const char *pcMode );

#ifdef __cplusplus
}
#endif
