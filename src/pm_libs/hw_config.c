
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
#include "pm_board.h"       // PicoMEM Board Definitions (GPIO)

// Modified for PicoMEM
// Hardware Configuration of SPI "objects"
// Note: multiple SD cards can be driven by one SPI if they use different slave
// selects (or "chip selects").
static spi_t spis[] = {  // One for each RP2040 SPI component used
    {   .hw_inst = SD_SPI,           // RP2040 SPI component
        .sck_gpio = SD_SPI_SCK,
        .mosi_gpio = SD_SPI_MOSI,
        .miso_gpio = SD_SPI_MISO,
        .set_drive_strength = true,
        // Reduced from 8 mA to 4 mA on MOSI/SCK while bisecting 8556 SD
        // hangs. SPI0 traces are shared with the W5500 footprint (idle but
        // capacitive) and 8 mA can ring on long traces. 4 mA matches the
        // SS pin and is plenty for ~10 MHz SPI.
        // 4 mA was too weak on the shared-W5500 traces: the host-driven MOSI
        // write data arrived corrupt -> SD data-CRC rejects the write (write-
        // only failure; MISO/reads are card-driven and were always fine).
        // Back to 8 mA now that SPI is at 12 MHz (~4 MHz wire), where the
        // ringing that pushed it down to 4 mA is far less of an issue.
        .mosi_gpio_drive_strength = GPIO_DRIVE_STRENGTH_8MA,
        .sck_gpio_drive_strength = GPIO_DRIVE_STRENGTH_8MA,
        // SD SPI baud rate request.  At PM_SYS_CLK=280 MHz the SDK
        // divider yields wire rates around request_rate / 3.  50 MHz
        // request gives ~17 MHz on the wire — at the edge of SD HS-mode
        // territory; signal integrity may matter on shared SPI0 traces.
        // Root cause was the W5500 module's capacitive load on the shared SD
        // SPI0 stub (24 cm ribbon). Clean at 8 mA + slow slew with the module
        // removed / series-terminated. 50 MHz request (~17 MHz wire at
        // request/3, 280 MHz) -- clean WITHOUT the W5500 module; with the 24 cm
        // module plugged, crosstalk corrupts and SD_CRC keeps it safe (fail+
        // retry). ~17 MHz wire is within the W5500's own 33.3 MHz SCLK guarantee.
        .baud_rate = 50 * 1000 * 1000
    },
};

/* SPI Interfaces */
static sd_spi_if_t spi_ifs[] = {
    {   .spi = &spis[0],          // Pointer to the SPI driving this card
        .ss_gpio = SD_SPI_CS,     // The SPI slave select GPIO for this SD card
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
        .use_card_detect = false, // Card detect connection pin
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

/* WonderMCA: expose the SD library's per-spi_t mutex so the W5500
 * backend (which shares spi0 with the SD card) can serialise its own
 * SPI transactions against any SD activity. The SD driver already
 * locks this mutex inside its read/write helpers via spi_lock() in
 * my_spi.h; sharing the same mutex from the W5500 side means the two
 * cannot interleave on the bus.
 *
 * Returns NULL if the requested SPI hasn't been instantiated, in
 * which case the caller should treat its access as unguarded (best-
 * effort). */
mutex_t *wm_spi_get_mutex(size_t num) {
    spi_t *p = spi_get_by_num(num);
    return p ? &p->mutex : NULL;
}

/* [] END OF FILE */
