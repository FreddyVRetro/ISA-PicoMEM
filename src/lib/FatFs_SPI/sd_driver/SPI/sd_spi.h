/* sd_spi.h
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

#include <stdint.h>
//
#include "pico/stdlib.h"
//
#include "delays.h"
#include "my_debug.h"
#include "my_spi.h"
#include "sd_card.h"
#include "sd_timeouts.h"

#ifdef NDEBUG
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void sd_spi_go_low_frequency(sd_card_t *this);
void sd_spi_go_high_frequency(sd_card_t *this);

/* 
After power up, the host starts the clock and sends the initializing sequence on the CMD line. 
This sequence is a contiguous stream of logical ‘1’s. The sequence length is the maximum of
1msec, 74 clocks or the supply-ramp-uptime; the additional 10 clocks (over the 64 clocks after
what the card should be ready for communication) is provided to eliminate power-up
synchronization problems.
*/
void sd_spi_send_initializing_sequence(sd_card_t *sd_card_p);

//FIXME: sd_spi_read, sd_spi_write, and sd_spi_write_read should return an error code on timeout.

static inline uint8_t sd_spi_read(sd_card_t *sd_card_p) {
    uint8_t received = SPI_FILL_CHAR;
    uint32_t start = millis();
    while (!spi_is_writable(sd_card_p->spi_if_p->spi->hw_inst) &&
           millis() - start < sd_timeouts.sd_spi_read)
        tight_loop_contents();
    myASSERT(spi_is_writable(sd_card_p->spi_if_p->spi->hw_inst));
    int num = spi_read_blocking(sd_card_p->spi_if_p->spi->hw_inst, SPI_FILL_CHAR, &received, 1);
    myASSERT(1 == num);
    return received;
}

static inline void sd_spi_write(sd_card_t *sd_card_p, const uint8_t value) {
    uint32_t start = millis();
    while (!spi_is_writable(sd_card_p->spi_if_p->spi->hw_inst) &&
           millis() - start < sd_timeouts.sd_spi_write)
        tight_loop_contents();
    myASSERT(spi_is_writable(sd_card_p->spi_if_p->spi->hw_inst));
    int num = spi_write_blocking(sd_card_p->spi_if_p->spi->hw_inst, &value, 1);
    myASSERT(1 == num);
}
static inline uint8_t sd_spi_write_read(sd_card_t *sd_card_p, const uint8_t value) {
    uint8_t received = SPI_FILL_CHAR;
    uint32_t start = millis();
    while (!spi_is_writable(sd_card_p->spi_if_p->spi->hw_inst) &&
           millis() - start < sd_timeouts.sd_spi_write_read)
        tight_loop_contents();
    myASSERT(spi_is_writable(sd_card_p->spi_if_p->spi->hw_inst));
    int num = spi_write_read_blocking(sd_card_p->spi_if_p->spi->hw_inst, &value, &received, 1);    
    myASSERT(1 == num);
    return received;
}

// Would do nothing if sd_card_p->spi_if_p->ss_gpio were set to GPIO_FUNC_SPI.
static inline void sd_spi_select(sd_card_t *sd_card_p) {
    if ((uint)-1 == sd_card_p->spi_if_p->ss_gpio) return;
    gpio_put(sd_card_p->spi_if_p->ss_gpio, 0);
    // See http://elm-chan.org/docs/mmc/mmc_e.html#spibus
    sd_spi_write(sd_card_p, SPI_FILL_CHAR);
    LED_ON();
}

static inline void sd_spi_deselect(sd_card_t *sd_card_p) {
    if ((uint)-1 == sd_card_p->spi_if_p->ss_gpio) return;
    gpio_put(sd_card_p->spi_if_p->ss_gpio, 1);
    LED_OFF();
    /*
    MMC/SDC enables/disables the DO output in synchronising to the SCLK. This
    means there is a posibility of bus conflict with MMC/SDC and another SPI
    slave that shares an SPI bus. Therefore to make MMC/SDC release the MISO
    line, the master device needs to send a byte after the CS signal is
    deasserted.
    */
    sd_spi_write(sd_card_p, SPI_FILL_CHAR);
}

static inline void sd_spi_lock(sd_card_t *sd_card_p) { spi_lock(sd_card_p->spi_if_p->spi); }
static inline void sd_spi_unlock(sd_card_t *sd_card_p) { spi_unlock(sd_card_p->spi_if_p->spi); }

static inline void sd_spi_acquire(sd_card_t *sd_card_p) {
    sd_spi_lock(sd_card_p);
    sd_spi_select(sd_card_p);
}
static inline void sd_spi_release(sd_card_t *sd_card_p) {
    sd_spi_deselect(sd_card_p);
    sd_spi_unlock(sd_card_p);
}

static inline void sd_spi_transfer_start(sd_card_t *sd_card_p, const uint8_t *tx, uint8_t *rx,
                                         size_t length) {
    return spi_transfer_start(sd_card_p->spi_if_p->spi, tx, rx, length);
}
static inline bool sd_spi_transfer_wait_complete(sd_card_t *sd_card_p, uint32_t timeout_ms) {
    return spi_transfer_wait_complete(sd_card_p->spi_if_p->spi, timeout_ms);
}
/* Transfer tx to SPI while receiving SPI to rx. 
tx or rx can be NULL if not important. */
static inline bool sd_spi_transfer(sd_card_t *sd_card_p, const uint8_t *tx, uint8_t *rx,
                                   size_t length) {
	return spi_transfer(sd_card_p->spi_if_p->spi, tx, rx, length);
}

#ifdef __cplusplus
}
#endif

/* [] END OF FILE */
