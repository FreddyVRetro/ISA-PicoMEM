

#ifdef PIMORONI_PICO_PLUS2_RP2350
#define MAX_PMRAM 8         // 256Kb "Fast RAM"
#else
#define MAX_PMRAM 16        // 128KB "Fast RAM"
#endif

// PSRAM SPI - defined in the CMakeLists.txt
/*
#define PSRAM_PIN_CS   5       // Chip Select (SPI0 CS )
#define PSRAM_PIN_SCK  6 // Clock       (SPI0 SCK)
#define PSRAM_PIN_MOSI 7       // Output      (SPI0 TX )
#define PSRAM_PIN_MISO 4       // Input       (SPI0 RX )
*/

// pm_gpio_defs.h :

//MicroSD Connector
#define SD_SPI_CS    3 // SD SPI Chip select
#define SD_SPI_SCK   6 // SD SPI Clock  (And PSRAM)
#define SD_SPI_MOSI  7 // SD SPI Output (And PSRAM)
#define SD_SPI_MISO  4 // SD SPI Input  (And PSRAM)

#define PIN_GP26  26  // 
#define PIN_GP27  27  //
#define PIN_GP28  28  //

#define PIN_QWIIC_SDA 26
#define PIN_QWIIC_SCL 27

#define PIN_DEBUG 27  // Used for debug
