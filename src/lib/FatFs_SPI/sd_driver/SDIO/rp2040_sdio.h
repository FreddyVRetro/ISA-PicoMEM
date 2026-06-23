
// SD card access using SDIO for RP2040 platform.
// This module contains the low-level SDIO bus implementation using
// the PIO peripheral. The high-level commands are in sd_card_sdio.cpp.

#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "sd_card.h"

//FIXME: why?
typedef struct sd_card_t sd_card_t;

typedef
enum sdio_status_t {
    SDIO_OK = 0,
    SDIO_BUSY = 1,
    SDIO_ERR_RESPONSE_TIMEOUT = 2, // Timed out waiting for response from card
    SDIO_ERR_RESPONSE_CRC = 3,     // Response CRC is wrong
    SDIO_ERR_RESPONSE_CODE = 4,    // Response command code does not match what was sent
    SDIO_ERR_DATA_TIMEOUT = 5,     // Timed out waiting for data block
    SDIO_ERR_DATA_CRC = 6,         // CRC for data packet is wrong
    SDIO_ERR_WRITE_CRC = 7,        // Card reports bad CRC for write
    SDIO_ERR_WRITE_FAIL = 8,       // Card reports write failure
} sdio_status_t;

#define SDIO_BLOCK_SIZE 512
#define SDIO_WORDS_PER_BLOCK (SDIO_BLOCK_SIZE / 4) // 128

// Maximum number of 512 byte blocks to transfer in one request
#define SDIO_MAX_BLOCKS 256

typedef enum sdio_transfer_state_t { SDIO_IDLE, SDIO_RX, SDIO_TX, SDIO_TX_WAIT_IDLE} sdio_transfer_state_t;

typedef struct sd_sdio_if_state_t {
    bool resources_claimed;

    uint32_t ocr; // Operating condition register from card
    uint32_t rca; // Relative card address
    int error_line;
    sdio_status_t error;
    uint32_t dma_buf[128];
    
    int SDIO_DMA_CH;
    int SDIO_DMA_CHB;
    int SDIO_CMD_SM;
    int SDIO_DATA_SM;

    uint32_t pio_cmd_clk_offset;
    uint32_t pio_data_rx_offset;
    pio_sm_config pio_cfg_data_rx;
    uint32_t pio_data_tx_offset;
    pio_sm_config pio_cfg_data_tx;

    sdio_transfer_state_t transfer_state;
    uint32_t transfer_start_time;
    uint32_t *data_buf;
    uint32_t blocks_done; // Number of blocks transferred so far
    uint32_t total_blocks; // Total number of blocks to transfer
    uint32_t blocks_checksumed; // Number of blocks that have had CRC calculated
    uint32_t checksum_errors; // Number of checksum errors detected

    // Variables for block writes
    uint64_t next_wr_block_checksum;
    uint32_t end_token_buf[3]; // CRC and end token for write block
    sdio_status_t wr_status;
    uint32_t card_response;

    // Variables for extended block writes
    bool ongoing_wr_mlt_blk;
    uint32_t wr_mlt_blk_cnt_sector;
    
    // Variables for block reads
    // This is used to perform DMA into data buffers and checksum buffers separately.
    struct {
        void * write_addr;
        uint32_t transfer_count;
    } dma_blocks[SDIO_MAX_BLOCKS * 2];
    struct {
        uint32_t top;
        uint32_t bottom;
    } received_checksums[SDIO_MAX_BLOCKS];
} sd_sdio_if_state_t;

// Execute a command that has 48-bit reply (response types R1, R6, R7)
// If response is NULL, does not wait for reply.
sdio_status_t rp2040_sdio_command_R1(sd_card_t *sd_card_p, uint8_t command, uint32_t arg, uint32_t *response);

// Execute a command that has 136-bit reply (response type R2)
// Response buffer should have space for 16 bytes (the 128 bit payload)
sdio_status_t rp2040_sdio_command_R2(const sd_card_t *sd_card_p, uint8_t command, uint32_t arg, uint8_t *response);

// Execute a command that has 48-bit reply but without CRC (response R3)
sdio_status_t rp2040_sdio_command_R3(sd_card_t *sd_card_p, uint8_t command, uint32_t arg, uint32_t *response);

// Start transferring data from SD card to memory buffer
sdio_status_t rp2040_sdio_rx_start(sd_card_t *sd_card_p, uint8_t *buffer, uint32_t num_blocks, size_t block_size);

// Check if reception is complete
// Returns SDIO_BUSY while transferring, SDIO_OK when done and error on failure.
sdio_status_t rp2040_sdio_rx_poll(sd_card_t *sd_card_p, size_t block_size_words);

// Start transferring data from memory to SD card
sdio_status_t rp2040_sdio_tx_start(sd_card_t *sd_card_p, const uint8_t *buffer, uint32_t num_blocks);

// Check if transmission is complete
sdio_status_t rp2040_sdio_tx_poll(sd_card_t *sd_card_p, uint32_t *bytes_complete /* = nullptr */);

// (Re)initialize the SDIO interface
bool rp2040_sdio_init(sd_card_t *sd_card_p, float clk_div);

void __not_in_flash_func(sdio_irq_handler)(sd_card_t *sd_card_p);

#ifdef __cplusplus
}
#endif
