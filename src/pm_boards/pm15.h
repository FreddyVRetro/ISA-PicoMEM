// File for PicoMEM 1.5

#define MAX_PMRAM 16        // 128KB "Fast RAM"

#define ISA_SM_INCLUDE "isa_iomem_noaen.pio.h"        // The ISA PIO code to use for this board

#define PIN_IRQ3  35  //
#define PIN_IRQ5  40  //
#define PIN_IRQ7  41  //

//MicroSD Connector
#define SD_SPI_CS    5 // SD SPI Chip select
#define SD_SPI_SCK   6 // SD SPI Clock  (And PSRAM)
#define SD_SPI_MOSI  7 // SD SPI Output (And PSRAM)
#define SD_SPI_MISO  4 // SD SPI Input  (And PSRAM)

// I2S as spare
#define PIN_GP26  8  //
#define PIN_GP27  9  //
#define PIN_GP28  10  //

//I2C for QwiiC, RTC, DP
#define PIN_QWIIC_SDA 2
#define PIN_QWIIC_SCL 3

//#define PIN_DEBUG 27  // Used for debug