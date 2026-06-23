#pragma once

#include <stdint.h>
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
+-----------------------+-------+-------+-----------+
| Name                  | Field | Width | CID-slice |
+-----------------------+-------+-------+-----------+
| Manufacturer ID       | MID   | 8     | [127:120] |
| OEM/Application ID    | OID   | 16    | [119:104] |
| Product name          | PNM   | 40    | [103:64]  |
| Product revision      | PRV   | 8     | [63:56]   |
| Product serial number | PSN   | 32    | [55:24]   |
| reserved              | --    | 4     | [23:20]   |
| Manufacturing date    | MDT   | 12    | [19:8]    |
| CRC7 checksum         | CRC   | 7     | [7:1]     |
| not used, always 1-   | 1     | [0:0] |           |
+-----------------------+-------+-------+-----------+
*/
// Table 5-2: The CID Fields
typedef uint8_t CID_t[16];

/*
+---------------+-----------------------+-------------------------------------+
| CSD_STRUCTURE | CSD structure version | Card Capacity                       |
+---------------+-----------------------+-------------------------------------+
| 0             | CSD Version 1.0       | Standard Capacity                   |
| 1             | CSD Version 2.0       | High Capacity and Extended Capacity |
| 2-3           | reserved              |                                     |
+---------------+-----------------------+-------------------------------------+
*/
// Table 5-3: CSD Register Structure

/*
+--------------------------------------------------+--------------------+-------+---------------+-----------+-----------+
| Name                                             | Field              | Width | Value         | Cell Type | CSD-slice |
+--------------------------------------------------+--------------------+-------+---------------+-----------+-----------+
| CSD structure                                    | CSD_STRUCTURE      | 2     | 00b           | R         | [127:126] |
| reserved                                         | -                  | 6     | 00 0000b      | R         | [125:120] |
| data read access-time-1                          | TAAC               | 8     | xxh           | R         | [119:112] |
| data read access-time-2 in CLK cycles (NSAC*100) | NSAC               | 8     | xxh           | R         | [111:104] |
| max. data transfer rate                          | TRAN_SPEED         | 8     | 32h or 5Ah    | R         | [103:96]  |
| card command classes                             | CCC                | 12    | 01x110110101b | R         | [95:84]   |
| max. read data block length                      | READ_BL_LEN        | 4     | xh            | R         | [83:80]   |
| partial blocks for read allowed                  | READ_BL_PARTIAL    | 1     | 1b            | R         | [79:79]   |
| write block misalignment                         | WRITE_BLK_MISALIGN | 1     | xb            | R         | [78:78]   |
| read block misalignment                          | READ_BLK_MISALIGN  | 1     | xb            | R         | [77:77]   |
| DSR implemented                                  | DSR_IMP            | 1     | xb            | R         | [76:76]   |
| reserved                                         | -                  | 2     | 00b           | R         | [75:74]   |
| device size                                      | C_SIZE             | 12    | xxxh          | R         | [73:62]   |
| max. read current @VDD min                       | VDD_R_CURR_MIN     | 3     | xxxb          | R         | [61:59]   |
| max. read current @VDD max                       | VDD_R_CURR_MAX     | 3     | xxxb          | R         | [58:56]   |
| max. write current @VDD min                      | VDD_W_CURR_MIN     | 3     | xxxb          | R         | [55:53]   |
| max. write current @VDD max                      | VDD_W_CURR_MAX     | 3     | xxxb          | R         | [52:50]   |
| device size multiplier                           | C_SIZE_MULT        | 3     | xxxb          | R         | [49:47]   |
| erase single block enable                        | ERASE_BLK_EN       | 1     | xb            | R         | [46:46]   |
| erase sector size                                | SECTOR_SIZE        | 7     | xxxxxxxb      | R         | [45:39]   |
| write protect group size                         | WP_GRP_SIZE        | 7     | xxxxxxxb      | R         | [38:32]   |
| write protect group enable                       | WP_GRP_ENABLE      | 1     | xb            | R         | [31:31]   |
| reserved                                         | (Do not use)       | 2     | 00b           | R         | [30:29]   |
| write speed factor                               | R2W_FACTOR         | 3     | xxxb          | R         | [28:26]   |
| max. write data block length                     | WRITE_BL_LEN       | 4     | xxxxb         | R         | [25:22]   |
| partial blocks for write allowed                 | WRITE_BL_PARTIAL   | 1     | xb            | R         | [21:21]   |
| reserved                                         | -                  | 5     | 00000b        | R         | [20:16]   |
| File format group                                | FILE_FORMAT_GRP    | 1     | xb            | R/W(1)    | [15:15]   |
| copy flag                                        | COPY               | 1     | xb            | R/W(1)    | [14:14]   |
| permanent write protection                       | PERM_WRITE_PROTECT | 1     | xb            | R/W(1)    | [13:13]   |
| temporary write protection                       | TMP_WRITE_PROTECT  | 1     | xb            | R/W       | [12:12]   |
| File format                                      | FILE_FORMAT        | 2     | xxb           | R/W(1)    | [11:10]   |
| reserved                                         |                    | 2     | 00b           | R/W       | [9:8]     |
| CRC                                              | CRC                | 7     | xxxxxxxb      | R/W       | [7:1]     |
| not used, always'1'                              | -                  | 1     | 1b            | -         | [0:0]     |
+--------------------------------------------------+--------------------+-------+---------------+-----------+-----------+
*/
// Table 5-4: The CSD Register Fields (CSD Version 1.0)

/*
+------------------------------------------------+----------------------+-------+----------------------+-----------+-----------+
| Name                                           | Field                | Width | Value                | Cell Type | CSD-slice |
+------------------------------------------------+----------------------+-------+----------------------+-----------+-----------+
| CSD structure                                  | CSD_STRUCTURE        | 2     | 01b                  | R         | [127:126] |
| reserved                                       | -                    | 6     | 00 0000b             | R         | [125:120] |
| data read access-time                          | (TAAC)               | 8     | 0Eh                  | R         | [119:112] |
| data read access-time in CLK cycles (NSAC*100) | (NSAC)               | 8     | 00h                  | R         | [111:104] |
| max. data transfer rate                        | (TRAN_SPEED)         | 8     | 32h, 5Ah, 0Bh or 2Bh | R         | [103:96]  |
| card command classes                           | CCC                  | 12    | 01x110110101b        | R         | [95:84]   |
| max. read data block length                    | (READ_BL_LEN)        | 4     | 9                    | R         | [83:80]   |
| partial blocks for read allowed                | (READ_BL_PARTIAL)    | 1     | 0                    | R         | [79:79]   |
| write block misalignment                       | (WRITE_BLK_MISALIGN) | 1     | 0                    | R         | [78:78]   |
| read block misalignment                        | (READ_BLK_MISALIGN)  | 1     | 0                    | R         | [77:77]   |
| DSR implemented                                | DSR_IMP              | 1     | x                    | R         | [76:76]   |
| reserved                                       | -                    | 6     | 00 0000b             | R         | [75:70]   |
| device size                                    | C_SIZE               | 22    | xxxxxxh              | R         | [69:48]   |
| reserved                                       | -                    | 1     | 0                    | R         | [47:47]   |
| erase single block enable                      | (ERASE_BLK_EN)       | 1     | 1                    | R         | [46:46]   |
| erase sector size                              | (SECTOR_SIZE)        | 7     | 7Fh                  | R         | [45:39]   |
| write protect group size                       | (WP_GRP_SIZE)        | 7     | 0000000b             | R         | [38:32]   |
| write protect group enable                     | (WP_GRP_ENABLE)      | 1     | 0                    | R         | [31:31]   |
| reserved                                       |                      | 2     | 00b                  | R         | [30:29]   |
| write speed factor                             | (R2W_FACTOR)         | 3     | 010b                 | R         | [28:26]   |
| max. write data block length                   | (WRITE_BL_LEN)       | 4     | 9                    | R         | [25:22]   |
| partial blocks for write allowed               | (WRITE_BL_PARTIAL)   | 1     | 0                    | R         | [21:21]   |
| reserved                                       | -                    | 5     | 00000b               | R         | [20:16]   |
| File format group                              | (FILE_FORMAT_GRP)    | 1     | 0                    | R         | [15:15]   |
| copy flag                                      | COPY                 | 1     | x                    | R/W(1)    | [14:14]   |
| permanent write protection                     | PERM_WRITE_PROTECT   | 1     | x                    | R/W(1)    | [13:13]   |
| temporary write protection                     | TMP_WRITE_PROTECT    | 1     | x                    | R/W       | [12:12]   |
| File format                                    | (FILE_FORMAT)        | 2     | 00b                  | R         | [11:10]   |
| reserved                                       | -                    | 2     | 00b                  | R         | [9:8]     |
| CRC                                            | CRC                  | 7     | xxxxxxxb             | R/W       | [7:1]     |
| not used, always'1'                            | -                    | 1     | 1                    | -         | [0:0]     |
+------------------------------------------------+----------------------+-------+----------------------+-----------+-----------+
*/
// Table 5-16: The CSD Register Fields (CSD Version 2.0)
typedef uint8_t CSD_t[16];

/* return Capacity in sectors */
static inline uint32_t CSD_sectors(CSD_t csd) /*  const  */ {
    uint32_t c_size;
    // +--------------------------------------------------+--------------------+-------+---------------+-----------+-----------+
    // | Name                                             | Field              | Width | Value         | Cell Type | CSD-slice |
    // +--------------------------------------------------+--------------------+-------+---------------+-----------+-----------+
    // | CSD structure                                    | CSD_STRUCTURE      | 2     | 00b           | R         | [127:126] |
    uint8_t ver = ext_bits16(csd, 127, 126);
    if (ver == 0) {
        // | device size                                      | C_SIZE             | 12    | xxxh          | R         | [73:62]   |
        c_size = ext_bits16(csd, 73, 62);
        // | device size multiplier                           | C_SIZE_MULT        | 3     | xxxb          | R         | [49:47]   |
        uint8_t c_size_mult = ext_bits16(csd, 49, 47);
        //  MULT = 2^(C_SIZE_MULT+2)
        uint32_t mult = 1UL << (c_size_mult + 2);
        //  BLOCKNR = (C_SIZE+1) * MULT
        return (c_size + 1) * mult;
    } else if (ver == 1) {
        // | device size                                    | C_SIZE               | 22    | xxxxxxh              | R         | [69:48]   |
        c_size = ext_bits16(csd, 69, 48);
        /* The user data area capacity is calculated from C_SIZE as follows:
        memory capacity = (C_SIZE+1) * 512K byte */
        return (c_size + 1) * 1024; // sectors
    } else {
        return 0;
    }
}

#ifdef __cplusplus
}
#endif
