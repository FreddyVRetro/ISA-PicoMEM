// File for PicoMEM 1.3 Proto (Multiplexer v2)
// Use only with a Pico2 !!

#ifdef PIMORONI_PICO_PLUS2_RP2350
#define MAX_PMRAM 16       // 256Kb "Fast RAM"
#else
#define MAX_PMRAM 8        // 128KB "Fast RAM"
#endif

#define BOARD_ID     4

#define I2S_QWIIC_SHARES 1  // When I2S and QwiiC are shared, Stop all QwiiC access when Audio is ON

#if PM_WIFI_LED
#else
#define LED_PIN 25
#endif

#ifdef PIMORONI_PICO_PLUS2_RP2350
#define USE_RP2350_PSRAM 1   // Use the RP2350 internal PSRAM
#endif

#define ISA_SM_INCLUDE "isa_iomem_m2.pio.h"        // The ISA PIO code to use for this board

//MicroSD Connector
#define SD_SPI       spi0 
#define SD_SPI_CS    3 // SD SPI Chip select
#define SD_SPI_SCK   6 // SD SPI Clock  (And PSRAM)
#define SD_SPI_MOSI  7 // SD SPI Output (And PSRAM)
#define SD_SPI_MISO  4 // SD SPI Input  (And PSRAM)

#define PIN_I2C1_SDA 26
#define PIN_I2C1_SCL 27

#define PIN_DEBUG 27  // Used for debug