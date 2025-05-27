// Variables in picomem.cpp and used by pm_cmd.cpp

// Lotech like EMS Regs
extern uint8_t  EMS_Bank[4];
extern uint32_t EMS_Base[4];

#if USE_PSRAM
#if USE_PSRAM_DMA
extern psram_spi_inst_t psram_spi;
#else
extern pio_spi_inst_t psram_spi;
#endif
#endif
