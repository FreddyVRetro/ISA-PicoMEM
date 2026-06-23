/* sd_card_spi.h
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

#include "sd_card.h"

#ifdef __cplusplus
extern "C" {
#endif

void sd_spi_ctor(sd_card_t *sd_card_p);  // Constructor for sd_card_t
uint32_t sd_go_idle_state(sd_card_t *sd_card_p);

#ifdef __cplusplus
}
#endif
/* [] END OF FILE */
