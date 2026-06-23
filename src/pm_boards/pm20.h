// File for PicoMEM 1.5

#define MAX_PMRAM 16  // 256KB "Fast RAM"

#define USE_RP2350_PSRAM 1   // Use the RP2350 internal PSRAM

#define BOARD_ID     11

#define ISA_SM_INCLUDE "isa_iomem_m2_v15.pio.h"        // The ISA PIO code to use for this board

#define LED_PIN 3

#define PIN_IRQ3  35  //
#define PIN_IRQ5  40  //
#define PIN_IRQ6  42  //
#define PIN_IRQ7  41  //

#define PIN_DRQ  38         // Init the DMA Request and Acknowledge Pins
#define PIN_DMAW 39
#define PIN_TC 37     // Terminal Count pin for DMA

//I2C to the Front Panel connector
#define PIN_I2C0_SDA 44
#define PIN_I2C0_SCL 45
//I2C to the RTC and QwiiC connector
#define PIN_I2C1_SDA 18
#define PIN_I2C1_SCL 19

//MicroSD Connector
#define SD_SPI       spi1 
#define SD_SPI_CS    9  // SD SPI Chip select
#define SD_SPI_SCK   10 // SD SPI Clock
#define SD_SPI_MOSI  11 // SD SPI Output
#define SD_SPI_MISO  8  // SD SPI Input

// I2C/buttons front pannel connector definitions
#define FP_HEADER 1
#define FP_I2C    i2c0
#define FP_SDA    PIN_I2C0_SDA
#define FP_SCL    PIN_I2C0_SCL

#define FPH_IO1    2   // Button 1
#define FPH_IO2    1   // Button 2
#define FPH_IO3    43  // Button 3
#define FPH_IO4    46  // Button 4
#define FPH_IO5    47  // Button 5 or ENC

//#define PIN_DEBUG 27  // Used for debug