/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
/* 
Define the GPIO not used in PIO
*/
#pragma once

#define SD_SPI_CS    3 // SD SPI Chip select
#define SD_SPI_SCK   6 // SD SPI Clock  (And PSRAM)
#define SD_SPI_MOSI  7 // SD SPI Output (And PSRAM)
#define SD_SPI_MISO  4 // SD SPI Input  (And PSRAM)

#define PIN_GP26  26  // 
#define PIN_GP27  27  //
#define PIN_GP28  28  //

#define PIN_QWIIC_SDA 26
#define PIN_QWIIC_SCL 27

#define PIN_DEBUG 27  // Used for debug