/* sd_spi.c
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

/* Standard includes. */
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
//
#include "hardware/gpio.h"
//
#include "my_debug.h"
#include "delays.h"
#include "my_spi.h"
//
#if !defined(USE_DBG_PRINTF) || defined(NDEBUG)
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif
//
#include "sd_spi.h"

// #define TRACE_PRINTF(fmt, args...)
// #define TRACE_PRINTF printf

void sd_spi_go_high_frequency(sd_card_t *sd_card_p) {
    uint actual = spi_set_baudrate(sd_card_p->spi_if_p->spi->hw_inst, sd_card_p->spi_if_p->spi->baud_rate);
    DBG_PRINTF("%s: Actual frequency: %lu\n", __FUNCTION__, (long)actual);
}
void sd_spi_go_low_frequency(sd_card_t *sd_card_p) {
    uint actual = spi_set_baudrate(sd_card_p->spi_if_p->spi->hw_inst, 400 * 1000); // Actual frequency: 398089
    DBG_PRINTF("%s: Actual frequency: %lu\n", __FUNCTION__, (long)actual);
}

/* 
After power up, the host starts the clock and sends the initializing sequence on the CMD line. 
This sequence is a contiguous stream of logical ‘1’s. The sequence length is the maximum of 1msec, 
74 clocks or the supply-ramp-uptime; the additional 10 clocks 
(over the 64 clocks after what the card should be ready for communication) is
provided to eliminate power-up synchronization problems. 
*/
void sd_spi_send_initializing_sequence(sd_card_t *sd_card_p) {
    if ((uint)-1 == sd_card_p->spi_if_p->ss_gpio) return; 
    bool old_ss = gpio_get(sd_card_p->spi_if_p->ss_gpio);
    // Set DI and CS high and apply 74 or more clock pulses to SCLK:
    gpio_put(sd_card_p->spi_if_p->ss_gpio, 1);
    uint8_t ones[10];
    memset(ones, 0xFF, sizeof ones);
    uint32_t start = millis();
    do {
        spi_transfer(sd_card_p->spi_if_p->spi, ones, NULL, sizeof ones);
    } while (millis() - start < 1);
    gpio_put(sd_card_p->spi_if_p->ss_gpio, old_ss);
}

/* [] END OF FILE */
