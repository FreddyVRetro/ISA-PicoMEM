

#define ISA_SM_INCLUDE "isa_iomem_m1.pio.h"        // The ISA PIO code to use for this board

// PSRAM SPI

#define I2S_QWIIC_SHARES 1  // When I2S and QwiiC are shared, Stop all QwiiC access when Audio is ON

#define BOARD_ID       1

#define PSRAM_PIN_CS   1 // Chip Select
#define PSRAM_PIN_SCK  2 // Clock
#define PSRAM_PIN_MOSI 3 // Output
#define PSRAM_PIN_MISO 0 // Input


