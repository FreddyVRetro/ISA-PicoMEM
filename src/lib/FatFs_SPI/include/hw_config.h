/* hw_config.h
Copyright 2021 Carl John Kugler III

Licensed under the Apache License, Version 2.0 (the License); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at

   http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an AS IS BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.
*/
#pragma once

#include <stddef.h>

#include "sd_card.h"

#ifdef __cplusplus
extern "C" {
#endif

/* FatFS supports up to 10 logical drives. By default, each logical
drive is associated with the physical drive in same drive number. */

/* Return the number of physical drives (SD card sockets) in the configuration */
size_t sd_get_num();

/* Return a pointer to the SD card "object" at the given physical drive number.
(See http://elm-chan.org/fsw/ff/doc/filename.html#vol.)
Parameter `num` must be less than sd_get_num(). */
sd_card_t* sd_get_by_num(size_t num);

/* See http://elm-chan.org/fsw/ff/doc/config.html#str_volume_id */
#if FF_STR_VOLUME_ID
extern const char* VolumeStr[FF_VOLUMES]; /* User defined volume ID */
#endif

#ifdef __cplusplus
}
#endif

/* [] END OF FILE */
