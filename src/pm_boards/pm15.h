// File for PicoMEM 1.5

#define MAX_PMRAM 16  // 256KB "Fast RAM"

#define USE_RP2350_PSRAM 1   // Use the RP2350 internal PSRAM

#define BOARD_ID     10

#define ISA_SM_INCLUDE "isa_iomem_m2_v15.pio.h"        // The ISA PIO code to use for this board

#define LED_PIN 11

#define PIN_IRQ3  35  //
#define PIN_IRQ5  40  //
#define PIN_IRQ7  41  //

#define PIN_DRQ  38         // Init the DMA Request and Acknowledge Pins
#define PIN_DMAW 39

//I2C to the Front Panel connector
#define PIN_I2C0_SDA 44
#define PIN_I2C0_SCL 45
//I2C to the RTC and QwiiC connector
#define PIN_I2C1_SDA 2
#define PIN_I2C1_SCL 3

//MicroSD Connector
#define SD_SPI       spi0 
#define SD_SPI_CS    5 // SD SPI Chip select
#define SD_SPI_SCK   6 // SD SPI Clock  (And PSRAM)
#define SD_SPI_MOSI  7 // SD SPI Output (And PSRAM)
#define SD_SPI_MISO  4 // SD SPI Input  (And PSRAM)

// I2C/buttons front pannel connector definitions
#define FP_HEADER 1
#define FP_I2C    i2c0
#define FP_SDA    PIN_I2C0_SDA
#define FP_SCL    PIN_I2C0_SCL

#define FPH_IO1    42   // Button 1
#define FPH_IO2    43   // Button 2
#define FPH_IO3    46  // Button 3
#define FPH_IO4    47  // Button 4
#define FPH_IO5    1   // Button 5 or ENC

//#define PIN_DEBUG 27  // Used for debug