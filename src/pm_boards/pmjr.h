// File for the PicoMEM PCjr variant (discrete custom RP2350 board)
// Derived from pm20.h (PicoMEM 2.0). The PCjr I/O Channel has no DMA controller,
// so DRQ/DACK/TC are not fitted and GPIO 36-39 and 42 are unconnected.
// The three I/O Channel interrupt levels (IRQ1, IRQ2, IRQ7) replace ISA IRQ3/5/6/7.

#define MAX_PMRAM 16  // 256KB "Fast RAM"

#define USE_RP2350_PSRAM 1   // Use the RP2350 internal PSRAM

#define BOARD_ID     12

#define ISA_SM_INCLUDE "isa_iomem_m2_v15.pio.h"        // The ISA PIO code to use for this board

#define LED_PIN 3

// PCjr I/O Channel interrupt levels, jumper selected on J8.
// Each pin drives a 74LVC2G04 inverter, so 0 = IRQ asserted (same as the other boards).
//
// Only IRQ7 is defined. IRQ1's vector (INT 09h) is the keyboard - the PCjr NMI handler
// dispatches through it - and the prebuilt option ROM probes IRQ 3/5/7 only, so it cannot
// detect levels 1 or 2 anyway. Leaving GPIO 35/40 undefined keeps them as inputs, which
// the inverter pull-ups hold at the idle level.
// #define PIN_IRQ1  40  // I/O Channel (Reserved) - INT 09h is the keyboard vector
// #define PIN_IRQ2  35  // I/O Channel - needs a ROM that hooks INT 0Ah
#define PIN_IRQ7  41  // I/O Channel (shared with the Parallel Printer sidecar)

// No PIN_DRQ / PIN_DMAW / PIN_TC : the PCjr has no DMA controller.

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

#define FPH_IO1    2   // C_IRQ
#define FPH_IO2    1   // SW1
#define FPH_IO3    43  // SW2
#define FPH_IO4    46  // SW3
#define FPH_IO5    47  // SW4
