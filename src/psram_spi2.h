#pragma once

// PicoMEM Code
// FV Modifications for PicoMEM
// SPRAM used only for the Memory/ROM/EMS Access and Code when PC IRQ Are Stopped and ROM is used (BIOS)
// > Remove all the mutex code (Mutual Exclusion)
// Always the first state machine used 
// > HardCoded pio and sm number

#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "stdio.h"
#include "hardware/clocks.h"

// SPI Defines
#ifdef PM_PROTO
#define PSRAM_PIN_CS   1 // Chip Select
#define PSRAM_PIN_SCK  2 // Clock
#define PSRAM_PIN_MOSI 3 // Output
#define PSRAM_PIN_MISO 0 // Input
#else // Shift by 4 pin for the Final Board
#define PSRAM_PIN_CS   5 // Chip Select (SPI0 CS )
#define PSRAM_PIN_SCK  6 // Clock       (SPI0 SCK)
#define PIN_MOSI 7 // Output      (SPI0 TX )
#define PSRAM_PIN_MISO 4 // Input       (SPI0 RX )
#endif

// "Hardcoded"
#define PSRAM_PIO pio1

#define NOMUTEX

#include "psram_spi2.pio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pio_spi_inst {
    PIO pio;
    uint sm;
} pio_spi_inst_t;


// ** Main SPI Read/Write code **

static __force_inline void __time_critical_func(pio_spi_write_read_blocking)(
        pio_spi_inst_t* spi,
        const uint8_t* src, const size_t src_len,
        uint8_t* dst, const size_t dst_len
) {
    size_t tx_remain = src_len, rx_remain = dst_len;

    // Put bytes to write in X
    pio_sm_put_blocking(spi->pio, spi->sm, src_len * 8);
    // Put bytes to read in Y
    pio_sm_put_blocking(spi->pio, spi->sm, dst_len * 8);
//printf("tx_r %d ",tx_remain);
    io_rw_8 *txfifo = (io_rw_8 *) &spi->pio->txf[spi->sm];
    while (tx_remain) {
        if (!pio_sm_is_tx_fifo_full(spi->pio, spi->sm)) {
            *txfifo = *src++;
            --tx_remain;
        }
    }
//printf("rx_r %d ",rx_remain);
    io_rw_8 *rxfifo = (io_rw_8 *) &spi->pio->rxf[spi->sm];
    while (rx_remain) {
        if (!pio_sm_is_rx_fifo_empty(spi->pio, spi->sm)) {
            *dst++ = *rxfifo;
            --rx_remain;
        }
    }
//printf("End");
}

/**
 * Basic interface to SPI PSRAMs such as Espressif ESP-PSRAM64, apmemory APS6404L, IPUS IPS6404, Lyontek LY68L6400, etc.
 * NOTE that the read/write functions abuse type punning to avoid shifts and masks to be as fast as possible!
 * I'm open to suggestions that this is really dumb or insane. This is a fixed platform (arm-none-eabi-gcc) so I'm OK with it
 */
__force_inline pio_spi_inst_t psram_init(float clkdiv) {
    uint spi_offset = pio_add_program(pio1, &spi_fudge_program);
    uint spi_sm = pio_claim_unused_sm(pio1, true);
    pio_spi_inst_t spi;
    spi.pio = PSRAM_PIO;
    spi.sm = spi_sm;
//   printf("!! PSRAM PIO Init : SM %d \n",spi_sm);
//   printf("sm is %d\n", spi_sm);

    gpio_set_drive_strength(PSRAM_PIN_CS, GPIO_DRIVE_STRENGTH_4MA);
    gpio_set_drive_strength(PSRAM_PIN_SCK, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(PSRAM_PIN_MOSI, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_slew_rate(PSRAM_PIN_CS, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(PSRAM_PIN_SCK, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(PSRAM_PIN_MOSI, GPIO_SLEW_RATE_FAST);

//    printf("about to init fudge\n", spi_sm);
    pio_spi_fudge_cs_init(pio1, spi_sm, spi_offset, 8 /*n_bits*/, clkdiv /*clkdiv*/, PSRAM_PIN_CS, PSRAM_PIN_MOSI, PSRAM_PIN_MISO);

  //  SPI initialisation.
  //  printf("Inited SPI PIO... at sm %d pio %d\n", spi.sm, spi.pio);

    busy_wait_us(150);
    uint8_t psram_reset_en_cmd = 0x66u;
    pio_spi_write_read_blocking(&spi, &psram_reset_en_cmd, 1, nullptr, 0);
    busy_wait_us(50);
    uint8_t psram_reset_cmd = 0x99u;
    pio_spi_write_read_blocking(&spi, &psram_reset_cmd, 1, nullptr, 0);
    busy_wait_us(100);

    return spi;
};

__force_inline void psram_write8(pio_spi_inst_t* spi, uint32_t addr, uint8_t val) {
    unsigned char* addr_bytes = (unsigned char*)&addr;
    uint8_t command[5] = {
        0x02u,
        *(addr_bytes + 2),
        *(addr_bytes + 1),
        *addr_bytes,
        val
    };

    pio_spi_write_read_blocking(spi, command, sizeof(command), nullptr, 0);
};

__force_inline uint8_t psram_read8(pio_spi_inst_t* spi, uint32_t addr) {
    uint8_t val; 
    unsigned char* addr_bytes = (unsigned char*)&addr;
    uint8_t command[5] = {
        0x0bu, // fast read command
        *(addr_bytes + 2),
        *(addr_bytes + 1),
        *addr_bytes,
        0,
    };

    pio_spi_write_read_blocking(spi, command, sizeof(command), &val, 1);
    return val;
};

__force_inline void psram_write16(pio_spi_inst_t* spi, uint32_t addr, uint16_t val) {
    unsigned char* addr_bytes = (unsigned char*)&addr;
    unsigned char* val_bytes = (unsigned char*)&val;
    uint8_t command[6] = {
        0x02u,
        *(addr_bytes + 2),
        *(addr_bytes + 1),
        *addr_bytes,
        *val_bytes,
        *(val_bytes + 1)
    };

    pio_spi_write_read_blocking(spi, command, sizeof(command), nullptr, 0);
};

__force_inline uint16_t psram_read16(pio_spi_inst_t* spi, uint32_t addr) {
    uint16_t val; 
    unsigned char* addr_bytes = (unsigned char*)&addr;
    uint8_t command[5] = {
        0x0bu, // fast read command
        *(addr_bytes + 2),
        *(addr_bytes + 1),
        *addr_bytes,
        0
    };

    pio_spi_write_read_blocking(spi, command, sizeof(command), (unsigned char*)&val, 2);
    return val;
};

__force_inline void psram_write32(pio_spi_inst_t* spi, uint32_t addr, uint32_t val) {
    unsigned char* addr_bytes = (unsigned char*)&addr;
    unsigned char* val_bytes = (unsigned char*)&val;
    // Break the address into three bytes and send read command
    uint8_t command[8] = {
        0x02u, // write command
        *(addr_bytes + 2),
        *(addr_bytes + 1),
        *addr_bytes,
        *val_bytes,
        *(val_bytes + 1),
        *(val_bytes + 2),
        *(val_bytes + 3)
    };

    pio_spi_write_read_blocking(spi, command, sizeof(command), nullptr, 0);
};

__force_inline void psram_writebuffer(pio_spi_inst_t* spi, uint32_t addr, uint8_t* buff, uint16_t size) {
    unsigned char* addr_bytes = (unsigned char*)&addr;
    // Break the address into three bytes and send read command
    uint8_t command[4] = {
        0x02u, // write command
        *(addr_bytes + 2),
        *(addr_bytes + 1),
        *addr_bytes,
     };

    unsigned char* src = (unsigned char*)&command;
    size_t tx_remain = sizeof(command);

    // Put bytes to write in X (Command + Buffer Size)
    pio_sm_put_blocking(spi->pio, spi->sm, (tx_remain+size)*8 );
    // Put bytes to read in Y
    pio_sm_put_blocking(spi->pio, spi->sm, 0);

// Send the command
//printf("tx_r %d ",tx_remain);
    io_rw_8 *txfifo = (io_rw_8 *) &spi->pio->txf[spi->sm];
    while (tx_remain) {
        if (!pio_sm_is_tx_fifo_full(spi->pio, spi->sm)) {
            *txfifo = *src++;
            --tx_remain;
        }
    }
// Send the Buffer Data
    tx_remain = size;
//printf("tx_r %d ",tx_remain);    
    txfifo = (io_rw_8 *) &spi->pio->txf[spi->sm];
    while (tx_remain) {
        if (!pio_sm_is_tx_fifo_full(spi->pio, spi->sm)) {
            *txfifo = *buff++;
            --tx_remain;
//         printf(".");            
        }
    }

};

__force_inline uint32_t psram_read32(pio_spi_inst_t* spi, uint32_t addr) {
    uint32_t val;
    unsigned char* addr_bytes = (unsigned char*)&addr;
    uint8_t command[5] = {
        0x0bu, // fast read command
        *(addr_bytes + 2),
        *(addr_bytes + 1),
        *addr_bytes,
        0
    };

    pio_spi_write_read_blocking(spi, command, sizeof(command), (unsigned char*)&val, 4);
    return val;
};

__force_inline void psram_readbuffer(pio_spi_inst_t* spi, uint32_t addr, uint8_t* buff, uint16_t size) {
    unsigned char* addr_bytes = (unsigned char*)&addr;
    uint8_t command[5] = {
        0x0bu, // fast read command
        *(addr_bytes + 2),
        *(addr_bytes + 1),
        *addr_bytes,
        0
    };

    pio_spi_write_read_blocking(spi, command, sizeof(command), buff, size);
};


#ifdef __cplusplus
}
#endif
