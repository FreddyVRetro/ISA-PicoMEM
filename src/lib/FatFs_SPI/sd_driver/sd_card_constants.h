/* sd_card_constants.h
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

#ifdef __cplusplus
extern "C" {
#endif

/*!< Block size supported for SD card is 512 bytes */
// Only HC block size is supported.
static const size_t sd_block_size = 512;

typedef enum {
    SD_BLOCK_DEVICE_ERROR_NONE = 0,
    SD_BLOCK_DEVICE_ERROR_WOULD_BLOCK = 1 << 0,     /*!< operation would block */
    SD_BLOCK_DEVICE_ERROR_UNSUPPORTED = 1 << 1,     /*!< unsupported operation */
    SD_BLOCK_DEVICE_ERROR_PARAMETER = 1 << 2,       /*!< invalid parameter */
    SD_BLOCK_DEVICE_ERROR_NO_INIT = 1 << 3,         /*!< uninitialized */
    SD_BLOCK_DEVICE_ERROR_NO_DEVICE = 1 << 4,       /*!< device is missing or not connected */
    SD_BLOCK_DEVICE_ERROR_WRITE_PROTECTED = 1 << 5, /*!< write protected */
    SD_BLOCK_DEVICE_ERROR_UNUSABLE = 1 << 6,        /*!< unusable card */
    SD_BLOCK_DEVICE_ERROR_NO_RESPONSE = 1 << 7,     /*!< No response from device */
    SD_BLOCK_DEVICE_ERROR_CRC = 1 << 8,             /*!< CRC error */
    SD_BLOCK_DEVICE_ERROR_ERASE = 1 << 9,           /*!< Erase error: reset/sequence */
    SD_BLOCK_DEVICE_ERROR_WRITE = 1 << 10           /*!< Write error: !SPI_DATA_ACCEPTED */
} block_dev_err_t;

/** Represents the different SD/MMC card types  */
typedef enum {
    SDCARD_NONE = 0, /**< No card is present */
    SDCARD_V1 = 1,   /**< v1.x Standard Capacity */
    SDCARD_V2 = 2,   /**< v2.x Standard capacity SD card */
    SDCARD_V2HC = 3, /**< v2.x High capacity SD card */
    CARD_UNKNOWN = 4 /**< Unknown or unsupported card */
} card_type_t;

/* On the wire, convert to hex and add 0x40 for transmitter bit.
e.g.: CMD17_READ_SINGLE_BLOCK: 17 = 0x11; 0x11 | 0x40 = 0x51 */
// Supported SD Card Commands
typedef enum {                          /* Number on wire in parens */
    CMD_NOT_SUPPORTED = -1,             /* Command not supported error */
    CMD0_GO_IDLE_STATE = 0,             /* Resets the SD Memory Card */
    CMD1_SEND_OP_COND = 1,              /* Sends host capacity support */
    CMD2_ALL_SEND_CID = 2,              /* Asks any card to send the CID. */
    CMD3_SEND_RELATIVE_ADDR = 3,        /* Ask the card to publish a new RCA. */
    CMD6_SWITCH_FUNC = 6,               /* Check and Switches card function */
    CMD7_SELECT_CARD = 7,               /* SELECT/DESELECT_CARD - toggles between the stand-by and transfer states. */
    CMD8_SEND_IF_COND = 8,              /* Supply voltage info */
    CMD9_SEND_CSD = 9,                  /* Provides Card Specific data */
    CMD10_SEND_CID = 10,                /* Provides Card Identification */
    CMD12_STOP_TRANSMISSION = 12,       /* Forces the card to stop transmission */
    CMD13_SEND_STATUS = 13,             /* (0x4D) Card responds with status */
    CMD16_SET_BLOCKLEN = 16,            /* Length for SC card is set */
    CMD17_READ_SINGLE_BLOCK = 17,       /* (0x51) Read single block of data */
    CMD18_READ_MULTIPLE_BLOCK = 18,     /* (0x52) Continuously Card transfers data blocks to host
         until interrupted by a STOP_TRANSMISSION command */
    CMD24_WRITE_BLOCK = 24,             /* (0x58) Write single block of data */
    CMD25_WRITE_MULTIPLE_BLOCK = 25,    /* (0x59) Continuously writes blocks of data
        until    'Stop Tran' token is sent */
    CMD27_PROGRAM_CSD = 27,             /* Programming bits of CSD */
    CMD32_ERASE_WR_BLK_START_ADDR = 32, /* Sets the address of the first write
     block to be erased. */
    CMD33_ERASE_WR_BLK_END_ADDR = 33,   /* Sets the address of the last write
       block of the continuous range to be erased.*/
    CMD38_ERASE = 38,                   /* Erases all previously selected write blocks */
    CMD55_APP_CMD = 55,                 /* Extend to Applications specific commands */
    CMD56_GEN_CMD = 56,                 /* General Purpose Command */
    CMD58_READ_OCR = 58,                /* Read OCR register of card */
    CMD59_CRC_ON_OFF = 59,              /* Turns the CRC option on or off*/
    // App Commands
    ACMD6_SET_BUS_WIDTH = 6,
    ACMD13_SD_STATUS = 13,
    ACMD22_SEND_NUM_WR_BLOCKS = 22,
    ACMD23_SET_WR_BLK_ERASE_COUNT = 23,
    ACMD41_SD_SEND_OP_COND = 41,
    ACMD42_SET_CLR_CARD_DETECT = 42,
    ACMD51_SEND_SCR = 51,
} cmdSupported;
//------------------------------------------------------------------------------

///* Disk Status Bits (DSTATUS) */
// See diskio.h.
// enum {
//    STA_NOINIT = 0x01, /* Drive not initialized */
//    STA_NODISK = 0x02, /* No medium in the drive */
//    STA_PROTECT = 0x04 /* Write protected */
//};

#ifdef __cplusplus
}
#endif
