/* sd_card.c
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
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
//
#include "pico/mutex.h"
//
#include "SDIO/SdioCard.h"
#include "SPI/sd_card_spi.h"
#include "hw_config.h"  // Hardware Configuration of the SPI and SD Card "objects"
#include "my_debug.h"
#include "sd_card_constants.h"
#include "sd_regs.h"
#include "sd_timeouts.h"
#include "util.h"
//
#include "diskio.h" /* Declarations of disk functions */  // Needed for STA_NOINIT, ...
//
#include "sd_card.h"

#define TRACE_PRINTF(fmt, args...)
// #define TRACE_PRINTF printf

#ifdef NDEBUG 
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif

static bool driver_initialized;

// An SD card can only do one thing at a time.
void sd_lock(sd_card_t *sd_card_p) {
    myASSERT(mutex_is_initialized(&sd_card_p->state.mutex));
    mutex_enter_blocking(&sd_card_p->state.mutex);
}
void sd_unlock(sd_card_t *sd_card_p) {
    myASSERT(mutex_is_initialized(&sd_card_p->state.mutex));
    mutex_exit(&sd_card_p->state.mutex);
}
bool sd_is_locked(sd_card_t *sd_card_p) {
    myASSERT(mutex_is_initialized(&sd_card_p->state.mutex));
    uint32_t owner_out;
    return !mutex_try_enter(&sd_card_p->state.mutex, &owner_out);
}

sd_card_t *sd_get_by_drive_prefix(const char *const drive_prefix) {
    // Numeric drive number is always valid
    if (2 == strlen(drive_prefix) && isdigit((unsigned char)drive_prefix[0]) &&
        ':' == drive_prefix[1])
        return sd_get_by_num(atoi(drive_prefix));
#if FF_STR_VOLUME_ID
    for (size_t i = 0; i < sd_get_num(); ++i) {
        // Ignore '/', trailing ':'
        if (strstr(drive_prefix, VolumeStr[i])) return sd_get_by_num(i);
    }
    EMSG_PRINTF("%s: unknown drive prefix %s\n", __func__, drive_prefix);
#endif
    return NULL;
}

/* Return non-zero if the SD-card is present. */
bool sd_card_detect(sd_card_t *sd_card_p) {
    TRACE_PRINTF("> %s\r\n", __FUNCTION__);
    if (!sd_card_p->use_card_detect) {
        sd_card_p->state.m_Status &= ~STA_NODISK;
        return true;
    }
    /*!< Check GPIO to detect SD */
    if (gpio_get(sd_card_p->card_detect_gpio) == sd_card_p->card_detected_true) {
        // The socket is now occupied
        sd_card_p->state.m_Status &= ~STA_NODISK;
        TRACE_PRINTF("SD card detected!\r\n");
        return true;
    } else {
        // The socket is now empty
        sd_card_p->state.m_Status |= (STA_NODISK | STA_NOINIT);
        sd_card_p->state.card_type = SDCARD_NONE;
        EMSG_PRINTF("No SD card detected!\r\n");
        return false;
    }
}

void sd_set_drive_prefix(sd_card_t *sd_card_p, size_t phy_drv_num) {
#if FF_STR_VOLUME_ID == 0
    int rc = snprintf(sd_card_p->state.drive_prefix, sizeof sd_card_p->state.drive_prefix,
                      "%d:", phy_drv_num);
#elif FF_STR_VOLUME_ID == 1 /* Arbitrary string is enabled */
    // Add ':'
    int rc = snprintf(sd_card_p->state.drive_prefix, sizeof sd_card_p->state.drive_prefix,
                      "%s:", VolumeStr[phy_drv_num]);
#elif FF_STR_VOLUME_ID == 2 /* Unix style drive prefix  */
    // Add '/'
    int rc = snprintf(sd_card_p->state.drive_prefix, sizeof sd_card_p->state.drive_prefix,
                      "/%s", VolumeStr[phy_drv_num]);
#else
#error "Unknown FF_STR_VOLUME_ID"
#endif
    // Notice that only when this returned value is non-negative and less than n,
    // the string has been completely written.
    myASSERT(0 <= rc && (size_t)rc < sizeof sd_card_p->state.drive_prefix);
}

char const *sd_get_drive_prefix(sd_card_t *sd_card_p) {
    myASSERT(driver_initialized);
    myASSERT(sd_card_p);
    if (!sd_card_p) return "";
    return sd_card_p->state.drive_prefix;
}

bool sd_init_driver() {
    auto_init_mutex(initialized_mutex);
    mutex_enter_blocking(&initialized_mutex);
    bool ok = true;
    if (!driver_initialized) {
        myASSERT(sd_get_num());
        for (size_t i = 0; i < sd_get_num(); ++i) {
            sd_card_t *sd_card_p = sd_get_by_num(i);
            if (!sd_card_p) continue;

            myASSERT(sd_card_p->type);

            if (!mutex_is_initialized(&sd_card_p->state.mutex))
                mutex_init(&sd_card_p->state.mutex);
            sd_lock(sd_card_p);

            sd_card_p->state.m_Status = STA_NOINIT;

            sd_set_drive_prefix(sd_card_p, i);

            // Set up Card Detect
            if (sd_card_p->use_card_detect) {
                if (sd_card_p->card_detect_use_pull) {
                    if (sd_card_p->card_detect_pull_hi) {
                        gpio_pull_up(sd_card_p->card_detect_gpio);
                    } else {
                        gpio_pull_down(sd_card_p->card_detect_gpio);
                    }
                }
                gpio_init(sd_card_p->card_detect_gpio);
            }

            switch (sd_card_p->type) {
                case SD_IF_NONE:
                    myASSERT(false);
                    break;
                case SD_IF_SPI:
                    myASSERT(sd_card_p->spi_if_p);  // Must have an interface object
                    myASSERT(sd_card_p->spi_if_p->spi);
                    sd_spi_ctor(sd_card_p);
                    if (!my_spi_init(sd_card_p->spi_if_p->spi)) {
                        ok = false;
                    }
                    /* At power up the SD card CD/DAT3 / CS  line has a 50KOhm pull up enabled
                     * in the card. This resistor serves two functions Card detection and Mode
                     * Selection. For Mode Selection, the host can drive the line high or let it
                     * be pulled high to select SD mode. If the host wants to select SPI mode it
                     * should drive the line low.
                     *
                     * There is an important thing needs to be considered that the MMC/SDC is
                     * initially NOT the SPI device. Some bus activity to access another SPI
                     * device can cause a bus conflict due to an accidental response of the
                     * MMC/SDC. Therefore the MMC/SDC should be initialized to put it into the
                     * SPI mode prior to access any other device attached to the same SPI bus.
                     */
                    sd_go_idle_state(sd_card_p);
                    break;
                case SD_IF_SDIO:
                    myASSERT(sd_card_p->sdio_if_p);
                    sd_sdio_ctor(sd_card_p);
                    break;
                default:
                    myASSERT(false);
            }  // switch (sd_card_p->type)

            sd_unlock(sd_card_p);
        }  // for
        driver_initialized = true;
    }
    mutex_exit(&initialized_mutex);
    return ok;
}

void cidDmp(sd_card_t *sd_card_p, printer_t printer) {
    // +-----------------------+-------+-------+-----------+
    // | Name                  | Field | Width | CID-slice |
    // +-----------------------+-------+-------+-----------+
    // | Manufacturer ID       | MID   | 8     | [127:120] | 15
    (*printer)(
        "\nManufacturer ID: "
        "0x%x\n",
        ext_bits16(sd_card_p->state.CID, 127, 120));
    // | OEM/Application ID    | OID   | 16    | [119:104] | 14
    (*printer)("OEM ID: ");
    {
        char buf[3];
        ext_str(16, sd_card_p->state.CID, 119, 104, sizeof buf, buf);
        (*printer)("%s", buf);
    }
    // | Product name          | PNM   | 40    | [103:64]  | 12
    (*printer)("Product: ");
    {
        char buf[6];
        ext_str(16, sd_card_p->state.CID, 103, 64, sizeof buf, buf);
        (*printer)("%s", buf);
    }
    // | Product revision      | PRV   | 8     | [63:56]   | 7
    (*printer)(
        "\nRevision: "
        "%d.%d\n",
        ext_bits16(sd_card_p->state.CID, 63, 60), ext_bits16(sd_card_p->state.CID, 59, 56));
    // | Product serial number | PSN   | 32    | [55:24]   | 6
    // (*printer)("0x%lx\n", __builtin_bswap32(ext_bits16(sd_card_p->state.CID, 55, 24))
    (*printer)(
        "Serial number: "
        "0x%lx\n",
        ext_bits16(sd_card_p->state.CID, 55, 24));
    // | reserved              | --    | 4     | [23:20]   | 2
    // | Manufacturing date    | MDT   | 12    | [19:8]    |
    // The "m" field [11:8] is the month code. 1 = January.
    // The "y" field [19:12] is the year code. 0 = 2000.
    (*printer)(
        "Manufacturing date: "
        "%d/%d\n",
        ext_bits16(sd_card_p->state.CID, 11, 8),
        ext_bits16(sd_card_p->state.CID, 19, 12) + 2000);
    (*printer)("\n");
    // | CRC7 checksum         | CRC   | 7     | [7:1]     | 0
    // | not used, always 1-   | 1     | [0:0] |           |
    // +-----------------------+-------+-------+-----------+
}
void csdDmp(sd_card_t *sd_card_p, printer_t printer) {
    uint32_t c_size, c_size_mult, read_bl_len;
    uint32_t block_len, mult, blocknr;
    uint32_t hc_c_size;
    uint64_t blocks = 0, capacity = 0;
    bool erase_single_block_enable = 0;
    uint8_t erase_sector_size = 0;

    // csd_structure : CSD[127:126]
    int csd_structure = ext_bits16(sd_card_p->state.CSD, 127, 126);
    switch (csd_structure) {
        case 0:
            c_size = ext_bits16(sd_card_p->state.CSD, 73, 62);  // c_size        : CSD[73:62]
            c_size_mult =
                ext_bits16(sd_card_p->state.CSD, 49, 47);  // c_size_mult   : CSD[49:47]
            read_bl_len =
                ext_bits16(sd_card_p->state.CSD, 83, 80);  // read_bl_len   : CSD[83:80] - the
                                                           // *maximum* read block length
            block_len = 1 << read_bl_len;                  // BLOCK_LEN = 2^READ_BL_LEN
            mult = 1 << (c_size_mult + 2);  // MULT = 2^C_SIZE_MULT+2 (C_SIZE_MULT < 8)
            blocknr = (c_size + 1) * mult;  // BLOCKNR = (C_SIZE+1) * MULT
            capacity = (uint64_t)blocknr * block_len;  // memory capacity = BLOCKNR * BLOCK_LEN
            blocks = capacity / sd_block_size;

            (*printer)("Standard Capacity: c_size: %" PRIu32 "\r\n", c_size);
            (*printer)("Sectors: 0x%llx : %llu\r\n", blocks, blocks);
            (*printer)("Capacity: 0x%llx : %llu MiB\r\n", capacity,
                       (capacity / (1024U * 1024U)));
            break;

        case 1:
            hc_c_size =
                ext_bits16(sd_card_p->state.CSD, 69, 48);  // device size : C_SIZE : [69:48]
            blocks = (hc_c_size + 1) << 10;                // block count = C_SIZE+1) * 1K
                                                           // byte (512B is block size)

            /* ERASE_BLK_EN
            The ERASE_BLK_EN defines the granularity of the unit size of the data to be erased.
            The erase operation can erase either one or multiple units of 512 bytes or one or
            multiple units (or sectors) of SECTOR_SIZE. If ERASE_BLK_EN=0, the host can erase
            one or multiple units of SECTOR_SIZE. If ERASE_BLK_EN=1 the host can erase one or
            multiple units of 512 bytes.
            */
            erase_single_block_enable = ext_bits16(sd_card_p->state.CSD, 46, 46);

            /* SECTOR_SIZE
            The size of an erasable sector. The content of this register is a 7-bit binary coded
            value, defining the number of write blocks. The actual size is computed by
            increasing this number by one. A value of zero means one write block, 127 means 128
            write blocks.
            */
            erase_sector_size = ext_bits16(sd_card_p->state.CSD, 45, 39) + 1;

            (*printer)("SDHC/SDXC Card: hc_c_size: %" PRIu32 "\r\n", hc_c_size);
            (*printer)("Sectors: %llu\r\n", blocks);
            (*printer)("Capacity: %llu MiB (%llu MB)\r\n", blocks / 2048,
                       blocks * sd_block_size / 1000000);
            (*printer)("ERASE_BLK_EN: %s\r\n", erase_single_block_enable
                                                   ? "units of 512 bytes"
                                                   : "units of SECTOR_SIZE");
            (*printer)("SECTOR_SIZE (size of an erasable sector): %d (%lu bytes)\r\n",
                       erase_sector_size,
                       (uint32_t)(erase_sector_size ? 512 : 1) * erase_sector_size);
            break;

        default:
            (*printer)("CSD struct unsupported\r\n");
    };
}

#define KB 1024
#define MB (1024 * 1024)

/* AU (Allocation Unit):
is a physical boundary of the card and consists of one or more blocks and its
size depends on each card. */
bool sd_allocation_unit(sd_card_t *sd_card_p, size_t *au_size_bytes_p) {
    if (SD_IF_SPI == sd_card_p->type) return false;  // SPI can't do full SD Status

    uint8_t status[64] = {0};
    bool ok = rp2040_sdio_get_sd_status(sd_card_p, status);
    if (!ok) return false;
    // 431:428 AU_SIZE
    uint8_t au_size = ext_bits(64, status, 431, 428);
    switch (au_size) {
        // AU_SIZE Value Definition
        case 0x0:
            *au_size_bytes_p = 0;
            break;  // Not Defined
        case 0x1:
            *au_size_bytes_p = 16 * KB;
            break;
        case 0x2:
            *au_size_bytes_p = 32 * KB;
            break;
        case 0x3:
            *au_size_bytes_p = 64 * KB;
            break;
        case 0x4:
            *au_size_bytes_p = 128 * KB;
            break;
        case 0x5:
            *au_size_bytes_p = 256 * KB;
            break;
        case 0x6:
            *au_size_bytes_p = 512 * KB;
            break;
        case 0x7:
            *au_size_bytes_p = 1 * MB;
            break;
        case 0x8:
            *au_size_bytes_p = 2 * MB;
            break;
        case 0x9:
            *au_size_bytes_p = 4 * MB;
            break;
        case 0xA:
            *au_size_bytes_p = 8 * MB;
            break;
        case 0xB:
            *au_size_bytes_p = 12 * MB;
            break;
        case 0xC:
            *au_size_bytes_p = 16 * MB;
            break;
        case 0xD:
            *au_size_bytes_p = 24 * MB;
            break;
        case 0xE:
            *au_size_bytes_p = 32 * MB;
            break;
        case 0xF:
            *au_size_bytes_p = 64 * MB;
            break;
        default:
            myASSERT(false);
    }
    return true;
}

/* [] END OF FILE */
