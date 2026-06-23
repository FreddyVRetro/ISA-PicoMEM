
#pragma once

#include <stdint.h>

typedef struct {
    uint32_t sd_command;
    unsigned sd_command_retries;
    unsigned sd_lock;
    unsigned sd_spi_read;
    unsigned sd_spi_write;
    unsigned sd_spi_write_read;
    unsigned spi_lock;
    unsigned rp2040_sdio_command_R1;
    unsigned rp2040_sdio_command_R2;
    unsigned rp2040_sdio_command_R3;
    unsigned rp2040_sdio_rx_poll;
    unsigned rp2040_sdio_tx_poll;
    unsigned sd_sdio_begin;
    unsigned sd_sdio_stopTransmission;
} sd_timeouts_t;

extern sd_timeouts_t sd_timeouts;
