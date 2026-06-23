/* crc.h
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
/* Derived from:
 * SD/MMC File System Library
 * Copyright (c) 2016 Neil Thiessen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SD_CRC_H
#define SD_CRC_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Calculate the CRC7 checksum for the specified data block.
 * 
 * This function calculates the CRC7 checksum for the specified data block
 * using the lookup table defined in the m_Crc7Table array.
 * 
 * @param data The data block to be checked.
 * @param length The length of the data block in bytes.
 * @return The calculated checksum.
 */
__attribute__((optimize("Ofast")))
static inline char crc7(uint8_t const *data, int const length) {
extern const char m_Crc7Table[];    
	char crc = 0;
	for (int i = 0; i < length; i++) {
		crc = m_Crc7Table[(crc << 1) ^ data[i]];
	}

	//Return the calculated checksum
	return crc;
}

/**
 * @brief Calculate the CRC16 checksum for the specified data block.
 * 
 * This function calculates the CRC16 checksum for the specified data block
 * using the lookup table defined in the m_Crc7Table array.
 * 
 * @param data The data block to be checked.
 * @param length The length of the data block in bytes.
 * @return The calculated checksum.
 */
uint16_t crc16(uint8_t const *data, int const length);

#endif

/* [] END OF FILE */
