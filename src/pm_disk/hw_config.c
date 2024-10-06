
/* hw_config.c
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

/*

This file should be tailored to match the hardware design.

See 
  https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/tree/main#customizing-for-the-hardware-configuration

There should be one element of the spi[] array for each RP2040 hardware SPI used.

There should be one element of the spi_ifs[] array for each SPI interface object.
* Each element of spi_ifs[] must point to an spi_t instance with member "spi".

There should be one element of the sdio_ifs[] array for each SDIO interface object.

There should be one element of the sd_cards[] array for each SD card slot.
* Each element of sd_cards[] must point to its interface with spi_if_p or sdio_if_p.
* The name (pcName) should correspond to the FatFs "logical drive" identifier.
  (See http://elm-chan.org/fsw/ff/doc/filename.html#vol)
  In general, this should correspond to the (zero origin) array index.

*/

/* Hardware configuration for Pico SD Card Development Board
See https://oshwlab.com/carlk3/rp2040-sd-card-dev

See https://docs.google.com/spreadsheets/d/1BrzLWTyifongf_VQCc2IpJqXWtsrjmG7KnIbSBy-CPU/edit?usp=sharing,
tab "Monster", for pin assignments assumed in this configuration file.
*/

#include <assert.h>
//
#include "hw_config.h"
#include "..\pm_gpiodef.h"     // Various PicoMEM GPIO Definitions

// Modified for PicoMEM
// Hardware Configuration of SPI "objects"
// Note: multiple SD cards can be driven by one SPI if they use different slave
// selects (or "chip selects").
static spi_t spis[] = {  // One for each RP2040 SPI component used
    {   .hw_inst = spi0,           // RP2040 SPI component
        .sck_gpio = PM_SPI_SCK,    // 6 (PicoMEM)
        .mosi_gpio = PM_SPI_MOSI,  // 7 (PicoMEM)
        .miso_gpio = PM_SPI_MISO,  // 4 (PicoMEM)
        .set_drive_strength = true,
        .mosi_gpio_drive_strength = GPIO_DRIVE_STRENGTH_8MA,        // Same value as the PSRAM
        .sck_gpio_drive_strength = GPIO_DRIVE_STRENGTH_8MA,         // Same value as the PSRAM
        .baud_rate = 30 * 1000 * 1000   // Actual frequency: 10416666.
    },
};

/* SPI Interfaces */
static sd_spi_if_t spi_ifs[] = {
    {   .spi = &spis[0],  // Pointer to the SPI driving this card
        .ss_gpio = 3,     // The SPI slave select GPIO for this SD card
        .set_drive_strength = true,
        .ss_gpio_drive_strength = GPIO_DRIVE_STRENGTH_4MA
    },
};

/* Hardware Configuration of the SD Card "objects"
    These correspond to SD card sockets
*/
static sd_card_t sd_card = {  // One for each SD card
       // sd_cards[0]: Socket sd0
        .type = SD_IF_SPI,
        .spi_if_p = &spi_ifs[0],  // Pointer to the SPI interface driving this card
        // SD Card detect:
        .use_card_detect = false,
        .card_detect_gpio = 9,  
        .card_detected_true = 0, // What the GPIO read returns when a card is
                                 // present.
        .card_detect_use_pull = true,
        .card_detect_pull_hi = true                                 
    
};

/* ********************************************************************** */
size_t sd_get_num() { return 1; }

sd_card_t *sd_get_by_num(size_t num) {
 return &sd_card;
}

size_t spi_get_num() { return count_of(spis); }

spi_t *spi_get_by_num(size_t num) {
    assert(num <= spi_get_num());
    if (num <= spi_get_num()) {
        return &spis[num];
    } else {
        return NULL;
    }
}

/* [] END OF FILE */
