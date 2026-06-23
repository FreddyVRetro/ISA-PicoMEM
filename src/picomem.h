// Variables in picomem.cpp and used by pm_cmd.cpp

#if USE_SPI_PSRAM
#if USE_SPI_PSRAM_DMA
extern psram_spi_inst_t psram_spi;
#else
extern pio_spi_inst_t psram_spi;
#endif
#endif