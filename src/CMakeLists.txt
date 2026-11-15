# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# PicoMEM CMake file

# https://github.com/bgni/minimal-pico-tinyusb-pio-project

# Initialise pico_sdk and tinyusb from installed location
# (note this can come from environment, CMake cache etc)
#set(PICO_SDK_PATH "${CMAKE_SOURCE_DIR}/../pico-sdk")
#set(TINYUSB_PATH "${CMAKE_SOURCE_DIR}/../pico-sdk/lib/tinyusb")

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Declare the Pi Pico Board used
# Use the Pimoroni Pico Plus 2 RP2350 board for PicoMEM > 1.14 and WonderMCA
# Use pico_w otheryise

set(PICO_BOARD pimoroni_pico_plus2_rp2350 CACHE STRING "Board type")
#set(PICO_BOARD pico_w CACHE STRING "Board type")

# Declare the PicoMEM Board

#set(PICOMEM_BOARD "PICOMEM")
#set(PICOMEM_BOARD "PICOMEM_LP")
#set(PICOMEM_BOARD "PICOMEM_13")
#set(PICOMEM_BOARD "PICOMEM_15")
set(PICOMEM_BOARD "PICOMEM_20")
#set(PICOMEM_BOARD "PICOMEM_JR")  # PicoMEM 2 in PC JR Mode
#set(PICOMEM_BOARD "WONDERMCA_10")

# ** UART (Debug) and USB Host definition **
 set(STDIO_UART 0)   # REL
 set(ASYNC_UART 0)
 set(USB_UART 0)

 set(DO_USBHOST 1)
 set(FAST_RAM 0)   # 1 to enable the Fast RAM Mode (No PSRAM based memory)

#set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_BUILD_TYPE "Release")
set(PROJECT_TYPE "TEST")

 # Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
#include(pico_extras_import.cmake)

project(picomem C CXX ASM)


# Tell the Pico SDK to use our local tinyusb instead of the one included in the SDK
set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb)

if (PICOMEM_BOARD STREQUAL "PICOMEM_20" OR PICOMEM_BOARD STREQUAL "PICOMEM_JR")
 set(PICO_FLASH_SIZE_BYTES 16777216)  # 16Mb PicoMEM2
 set(PICO_CYW43_SUPPORTED "1")
 pico_register_common_scope_var(PICO_CYW43_SUPPORTED)
endif()

# WonderMCA RM2 (CYW43439) WiFi: enable cyw43 support BEFORE pico_sdk_init()
# so the SDK creates the pico_cyw43_arch_* targets we link into picomem.
if (PICOMEM_BOARD STREQUAL "WONDERMCA_10")
 set(PICO_CYW43_SUPPORTED "1")
 pico_register_common_scope_var(PICO_CYW43_SUPPORTED)
endif()

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()
  
####### Add the Sub directories containing libraries  ########

add_subdirectory(lib/FatFs_SPI)
# Canonical PicoMEM BIOS images (pmbios.bin / pm2bios.bin) -- built for
# every board variant. The wmbios.bin (+ pmbios_image.h regeneration)
# build lives in the WonderMCA submodule and is loaded only for the
# WONDERMCA_10 board (see the conditional add_subdirectory below).
add_subdirectory(rom)
add_subdirectory(isa_devices)               # all ISA devices IO and MEM code
add_subdirectory(audio_devices)
add_subdirectory(audio_devices/opl)
add_subdirectory(lib/pm_audio/pm_audio_i2s)
add_subdirectory(lib/my_etherdfs)
add_subdirectory(bios_wrappers)
add_subdirectory(pm_libs)
add_subdirectory(pm_boards)
add_subdirectory(lib/picoOled)
add_subdirectory(isa_devices/cdrom)         # CD-ROM core (86Box data layer + image manager)
add_subdirectory(lib/rp2040-psram)          # RP2040 PSRAM with DMA

if (PICO_BOARD STREQUAL "pimoroni_pico_plus2_rp2350" OR PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    add_subdirectory(lib/pico_psram)        # RP2350 PSRAM (QMI/QSPI). INTERFACE lib:
                                            # psram.cpp compiles in the consumer target,
                                            # so it inherits BOARD_WM10 -> RP2350_PSRAM_CS=0.
endif()

if (PICOMEM_BOARD STREQUAL "PICOMEM_20" OR PICOMEM_BOARD STREQUAL "PICOMEM_JR" OR PICO_BOARD STREQUAL "pico_w" OR PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    add_subdirectory(ne2000)               # Add Wifi/ne2000 for pico_w
endif()
if (DO_USBHOST)
    add_subdirectory(lib/tusb_xinput)
    add_subdirectory(USB_Host_Driver2)    
endif()
if (ASYNC_UART)
    add_subdirectory(stdio_async_uart)
endif()

# Add executable. Default name is the project name, version 0.1
add_executable(picomem
               picomem.cpp
               pm_cmd.cpp
               pm_libs/pico_rtc.cpp
#               RTElib/rtedbg.c
               )

target_include_directories(picomem
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/RTElib/Inc
    ${CMAKE_CURRENT_LIST_DIR}/RTElib/Fmt
    ${CMAKE_CURRENT_LIST_DIR}/audio_devices/covox 
)

set(CMAKE_EXE_LINKER_FLAGS "-Wl,--print-memory-usage")
target_compile_options(picomem PRIVATE -save-temps -fverbose-asm -O3) #fast -fmodulo-sched -fmodulo-sched-allow-regmoves) 

# TEMP (cyw43 gSPI diagnostics): force the per-step cyw43 debug prints on.
# Must go via compile_options -- function-like macros are dropped by
# add_compile_definitions. Remove once WiFi bring-up is confirmed.
if (PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    target_compile_options(picomem PRIVATE "-DCYW43_DEBUG(...)=printf(__VA_ARGS__)")
endif()

if (PICOMEM_BOARD STREQUAL "PICOMEM")
      add_compile_definitions(
        BOARD_PM=1
      )
endif()      
if (PICOMEM_BOARD STREQUAL "PICOMEM_LP")
      add_compile_definitions(
        BOARD_PM_LP=1
      )
endif()      
if (PICOMEM_BOARD STREQUAL "PICOMEM_13")
      add_compile_definitions(
        BOARD_PM13=1
        MUX_V2=1
      )
endif()
if (PICOMEM_BOARD STREQUAL "PICOMEM_15")
      set(USE_FRONTPANEL 1)
# PicoMEM 1.5 + Parameters
      add_compile_definitions(
        BOARD_PM15=1
        MUX_V2=1

# Pico Main frequency (280, 340 or 360)
        PM_SYS_CLK=280    # Possible values : 130,133,160,170,...,240,260,270,280
        CYW43_PIO_CLOCK_DIV_INT=4  # For the Wifi Chip PIO clock : 2 for 133MHz

        MAX_PMRAM=16            # Maximum Nb of 16Kb RAM for the Pico emulated RAM (Reduce to allow all, for test) (8 for 128Kb maximum)

        USE_SDIO=0              # To remove SDIO form the SD Code (Save space and RAM)

        USE_SPI_PSRAM=0         # PicoMEM 15 Does not have SPI PSRAM
        USE_SPI_PSRAM_DMA=0     # PicoMEM 15 Does not have SPI PSRAM
        PSRAM_HEAP=1            # Enable the PSRAM HEAP        

        USE_AUDIO_I2S=1
        PICO_AUDIO_I2S_DATA_PIN=8        #DIN
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=9  #BCK
        PICO_AUDIO_I2S_PIO=1
        PICO_AUDIO_I2S_SM=3
        PICO_AUDIO_I2S_DMA_IRQ=1        

# ! ASYNC UART values defined in picomem.cpp        
        PICO_DEFAULT_UART=1
        PICO_DEFAULT_UART_TX_PIN=36
        PICO_DEFAULT_UART_RX_PIN=37

        PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000000

        USE_DEV_IRQ=0       # IRQ Debug display
        DBG_PIN_AUDIO=0
        ENABLE_CDROM_LOG=0  # Comment to disable CD LOG

        ONBOARD_RTC=1
        USE_OLED=1         # Detect and Use OLED (PicoMEM 1.5/2 only)
        USE_HWIRQ=1         # for the PicoMEM 1.5, use the extra interrupt lines
        USE_HWDMA=1
        USE_SBDSP=1
        USE_GUS=0
        USE_MPU=0
        USE_CDROM=0
        USE_CD_AUDIO_FIFO=0
        USE_CD_AUDIO=0
      )
elseif (PICOMEM_BOARD STREQUAL "PICOMEM_20")
      set(USE_FRONTPANEL 1)  # PicoMEM 2.0 + Parameters
      add_compile_definitions(
        BOARD_PM20=1
        MUX_V2=1

# Pico Main frequency (280, 340 or 360)
        PM_SYS_CLK=360    # Possible values : 130,133,160,170,...,240,260,270,280
#        PM_SYS_CLK=280    # Possible values : 130,133,160,170,...,240,260,270,280
        CYW43_PIO_CLOCK_DIV_INT=6  # For the Wifi Chip PIO clock : 2 for 133MHz

        CORE0_WATCHDOG=1        # Watchdog debugger to display the crash location

        MAX_PMRAM=16            # Maximum Nb of 16Kb RAM for the Pico emulated RAM (Reduce to allow all, for test) (8 for 128Kb maximum)

        USE_SDIO=0              # To remove SDIO form the SD Code (Save space and RAM)

        USE_SPI_PSRAM=0         # PicoMEM 15 Does not have SPI PSRAM
        USE_SPI_PSRAM_DMA=0     # PicoMEM 15 Does not have SPI PSRAM
        PSRAM_HEAP=1            # Enable the PSRAM HEAP

#Audio Defines        
        USE_AUDIO_I2S=1
        I2S_CLK_INVERT=1                  # Invert the I2S Clock signals
        PICO_AUDIO_I2S_DATA_PIN=15        #DIN
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=16  #LCK
        PICO_AUDIO_I2S_PIO=1
        PICO_AUDIO_I2S_SM=3
        PICO_AUDIO_I2S_DMA_IRQ=1

# Network Defines
        USE_NE2000=1

# Wifi / CYW43 Defines
        CYW43_DEFAULT_PIN_WL_REG_ON=4
        CYW43_DEFAULT_PIN_WL_DATA_OUT=5
        CYW43_DEFAULT_PIN_WL_DATA_IN=5
        CYW43_DEFAULT_PIN_WL_HOST_WAKE=5
        CYW43_DEFAULT_PIN_WL_CLOCK=7
        CYW43_DEFAULT_PIN_WL_CS=6

        CYW43_PIN_WL_DYNAMIC=0
        CYW43_LOGIC_DEBUG=0

# ! ASYNC UART values defined in picomem.cpp        
        PICO_DEFAULT_UART=0
#        PICO_DEFAULT_UART_TX_PIN=46       # 46/47 For PicoMEM 2.0.0 (Bug)
#        PICO_DEFAULT_UART_RX_PIN=47
        PICO_DEFAULT_UART_TX_PIN=12        # 12/13 For PicoMEM 2.0.1
        PICO_DEFAULT_UART_RX_PIN=13

        PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000000

        USE_DEV_IRQ=0       # IRQ Debug display (out to Port 21h)
        DBG_PIN_AUDIO=0
        ENABLE_CDROM_LOG=1  # Comment to disable CD LOG
        PGDEBUG_CDROM_IMAGE=1
        PGDEBUG_CDROM_BACKEND=1

        ONBOARD_RTC=1
        USE_OLED=1          # Detect and Use OLED (PicoMEM 1.5/2 only)
        USE_HWIRQ=1         # for the PicoMEM 1.5, use the extra interrupt lines
        USE_HWDMA=1
        USE_SBDSP=1
        USE_GUS=1
        USE_MPU=1
        SF2_GMGSx=1         # 1 to use the GMGSx sound font
        USE_CDROM=1
        USE_CD_AUDIO_FIFO=1
        USE_CD_AUDIO=1
        USE_DISK_CACHE=0   # Disk cache to the PSRAM (Does not work for the moment, PSRAM not reliable)
      )
elseif (PICOMEM_BOARD STREQUAL "PICOMEM_JR")
      set(USE_FRONTPANEL 1)  # PicoMEM 2.0 + Parameters
      add_compile_definitions(
        BOARD_PM20=1
        BOARD_PMJR=1   # PCJR is a PicoMEM 2 variant
        MUX_V2=1

# Pico Main frequency (280, 340 or 360)
        PM_SYS_CLK=360    # Possible values : 130,133,160,170,...,240,260,270,280
#        PM_SYS_CLK=280    # Possible values : 130,133,160,170,...,240,260,270,280
        CYW43_PIO_CLOCK_DIV_INT=6  # For the Wifi Chip PIO clock : 2 for 133MHz

        CORE0_WATCHDOG=1        # Watchdog debugger to display the crash location

        MAX_PMRAM=16            # Maximum Nb of 16Kb RAM for the Pico emulated RAM (Reduce to allow all, for test) (8 for 128Kb maximum)

        USE_SDIO=0              # To remove SDIO form the SD Code (Save space and RAM)

        USE_SPI_PSRAM=0         # PicoMEM 15 Does not have SPI PSRAM
        USE_SPI_PSRAM_DMA=0     # PicoMEM 15 Does not have SPI PSRAM
        PSRAM_HEAP=1            # Enable the PSRAM HEAP

#Audio Defines        
        USE_AUDIO_I2S=1
        I2S_CLK_INVERT=1                  # Invert the I2S Clock signals
        PICO_AUDIO_I2S_DATA_PIN=15        #DIN
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=16  #LCK
        PICO_AUDIO_I2S_PIO=1
        PICO_AUDIO_I2S_SM=3
        PICO_AUDIO_I2S_DMA_IRQ=1

# Network Defines
        USE_NE2000=1

# Wifi / CYW43 Defines
        CYW43_DEFAULT_PIN_WL_REG_ON=4
        CYW43_DEFAULT_PIN_WL_DATA_OUT=5
        CYW43_DEFAULT_PIN_WL_DATA_IN=5
        CYW43_DEFAULT_PIN_WL_HOST_WAKE=5
        CYW43_DEFAULT_PIN_WL_CLOCK=7
        CYW43_DEFAULT_PIN_WL_CS=6

        CYW43_PIN_WL_DYNAMIC=0
        CYW43_LOGIC_DEBUG=0

# ! ASYNC UART values defined in picomem.cpp        
        PICO_DEFAULT_UART=0
#        PICO_DEFAULT_UART_TX_PIN=46       # 46/47 For PicoMEM 2.0.0 (Bug)
#        PICO_DEFAULT_UART_RX_PIN=47
        PICO_DEFAULT_UART_TX_PIN=12        # 12/13 For PicoMEM 2.0.1
        PICO_DEFAULT_UART_RX_PIN=13

        PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000000

        ONBOARD_RTC=1
        USE_OLED=1          # Detect and Use OLED (PicoMEM 1.5/2 only)
        USE_HWIRQ=1         # for the PicoMEM 1.5, use the extra interrupt lines
        USE_HWDMA=1
        USE_SBDSP=1
        USE_GUS=1
        USE_MPU=0
        SF2_GMGSx=1         # 1 to use the GMGSx sound font
        
        USE_CDROM=1
        USE_CD_AUDIO_FIFO=0
        USE_CD_AUDIO=1
        USE_DEV_IRQ=0       # IRQ Debug display
        DBG_PIN_AUDIO=0
        ENABLE_CDROM_LOG=0  # Comment to disable CD LOG

        USE_DISK_CACHE=0   # Disk cache to the PSRAM (Does not work for the moment, PSRAM not reliable)
      )      
elseif (PICOMEM_BOARD STREQUAL "WONDERMCA_10")
      set(USE_FRONTPANEL 0)
# PicoMEM 2.0 + Parameters
      add_compile_definitions(

        BOARD_WM10=1
        PICO_PIO_USE_GPIO_BASE=1    # RP2350B: PIO needs gpio_base for GPIO 32-47

        BUS_8BIT=1                 # Firmware  8Bits data bus, CFG Switch 02 OFF
        #BUS_16BIT=1                 # Firmware 16Bits data bus, CFG Switch 02 ON

        SB_TC_INSTR=1               # DIAG: bound the iow_handler_done /CMD-HIGH spin + count timeouts (post-TC hang detector). Surfaced in [wm_dma] trace as spin_to=. Remove once the SBDIAG hang is fixed.

        PM_ENABLE_STDIO=1           # Enable the Serial Display to the defined serial device
        #PM_PRINTF=1                # Define below in the PROJECT_TYPE 1 To enable the Debug Printf

# Pico Main frequency (280, 340 or 360)
        PM_SYS_CLK=280              # Stable on 8557; pcctest passes 100% at this rate.

        MAX_PMRAM=16                # Maximum Nb of 16Kb RAM for the Pico emulated RAM (Reduce to allow all, for test) (8 for 128Kb maximum)
        USE_SDIO=0                  # To remove SDIO form the SD Code (Save space and RAM)
        
        
        USE_SPI_PSRAM=0             # PicoMEM 15 Does not have SPI PSRAM
        USE_SPI_PSRAM_DMA=0         # PicoMEM 15 Does not have SPI PSRAM
        PSRAM_HEAP=1                # Enable the PSRAM HEAP
        
        USE_AUDIO=1                 # Enable AUDIO
        USE_AUDIO_I2S=1             # Enable I2S Audio
        I2S_CLK_INVERT=1            # Invert the I2S Clock signals
        # WonderMCA 1.0 I2S pin assignment (verified working -- OPL2 self-test audible):
        
        
        
        
        PICO_AUDIO_I2S_DATA_PIN=8           #   GPIO 8  = DIN  (data)     -> PCM5102 pin 14
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=9     #   GPIO 9  = BCK  (bit clk)  -> PCM5102 pin 13
        PICO_AUDIO_I2S_PIO=1                #   GPIO 10 = LRCK (word clk) -> PCM5102 pin 15  (= CLOCK_PIN_BASE + 1)
        PICO_AUDIO_I2S_SM=3
        PICO_AUDIO_I2S_DMA_IRQ=1

# ! ASYNC UART values defined in picomem.cpp        
        PICO_DEFAULT_UART=0
        PICO_DEFAULT_UART_TX_PIN=44         # GPIO44 = UART0 TX (TX-only console)
        PICO_DEFAULT_UART_RX_PIN=45         # NOTE: GPIO45 is also /CD-SETUP (POS gate).
                                            # The SDK requires a real UART0-RX-capable pin
                                            # here and there's no spare one, so it stays 45.
                                            # The conflict is mitigated in firmware: stdio is
                                            # init'd ONCE (before the Core1 launch), Core1 then
                                            # reclaims GPIO45 as SIO/CD-SETUP (wm_mca..h:234),
                                            # and we do NOT re-run stdio_init_all() after the
                                            # launch (picomem.cpp). Do not add a post-launch
                                            # stdio_init_all() on WM10 -- it re-grabs GPIO45.

        PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000000

        USE_DEV_IRQ=0                       # IRQ Debug display
        DBG_PIN_AUDIO=0
        ENABLE_CDROM_LOG=0                  # Comment to disable CD LOG

        USE_HWIRQ=1                         # for the PicoMEM 1.5, use the extra interrupt lines
        USE_HWDMA=0                         # ISA-style PIO HWDMA path is not used on MCA;
                                            # MCA slave DMA goes through wm_dma instead.
        
        USE_SBDSP=1                         # SB DSP emulation enabled (MCA slave DMA via wm_dma)
        USE_GUS=1                           # GUS: 3X0 window via CPLD blk_320 (rev 77); DMA via wm_dma (WIP)
        USE_SERMOUSE=1                      # Serial mouse (COM2 0x2F8/IRQ3 via CPLD blk_2F8 rev 78) -- Win+DOS mouse
        USE_MPU=0
        USE_CDROM=1                         # MKE CD-ROM at 0x250 (CPLD blk_250, rev 80); images in 0:/CDROM
        USE_CD_AUDIO_FIFO=0                 # PicoGUS FIFO path off (volctrl/audio_fifo layout differs)
        USE_CD_AUDIO=1                      # CD-DA via cdrom_audio_callback_simple pulled from pm_audio_domix

        USE_NE2000=1
        USE_NE2000_W5500=1                  # Enable W5500 Module
        USE_NE2000_CYW43=1                  # Enable CYW43 WiFi (WonderMCA); gates the WiFi boot-time init
        USE_NE2000_CDCECM=1                 # USB Ethernet (CDC-ECM) host driver -> NE2000 backend
                                            # (WonderMCA/src/wm_devices/usbeth; registered in oldxinput.c)
        USE_NE2000_ASIX=1                   # USB Ethernet ASIX AX88772/A/B vendor driver -> same backend
        USE_NE2000_RTL8150=1                # USB Ethernet Realtek RTL8150 vendor driver -> same backend
                                            # (RTL8152/8153 use their CDC-ECM config via usbeth_ecm.c)
        
        CYW43_DEFAULT_PIN_WL_REG_ON=13
        CYW43_DEFAULT_PIN_WL_DATA_OUT=14
        CYW43_DEFAULT_PIN_WL_DATA_IN=14
        CYW43_DEFAULT_PIN_WL_HOST_WAKE=14
        CYW43_DEFAULT_PIN_WL_CLOCK=15
        CYW43_DEFAULT_PIN_WL_CS=11
        CYW43_PIO_CLOCK_DIV_INT=6           # Match PM20 gSPI rate (~23 MHz PIO clock).

        CYW43_SPI_PROGRAM_NAME=spi_gap0_sample1
      )

      # WonderMCA subdirectories -- pulled in only when targeting WM10 so
      # PicoMEM 1.x / 2.0 builds never depend on the WonderMCA submodule.
      add_subdirectory(wondermca)            # WonderMCA MCA bus and devices
      
      # WonderMCA-specific wmbios build (regenerates pmbios_image.h that
      # wm_mca_rp2350_v10.h embeds and the WM10 bus handler serves).
      # Binary-dir alias "wmbios" keeps the build path stable.
      add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../WonderMCA/wmbios wmbios)

      # wm_mca_rp2350_v10.h includes pmbios_image.h, which is generated by
      # the WonderMCA-side wmbios custom target. Force wmbios to run before
      # any picomem.cpp compile. Non-WM10 builds never load the wmbios
      # subdirectory, so this edge only exists when we're here.
      add_dependencies(picomem wmbios)

      # CMake-time selector consumed by src/ne2000/CMakeLists.txt and
      # src/wondermca/CMakeLists.txt to pick the right backend TU.
      set(USE_NE2000_W5500 1)
else()  # PicoMEM Boards < 1.5
      set(USE_FRONTPANEL 0)
 
      add_compile_definitions(
    #    BOARD_PM=1
    #    MUX_V2=0

# 270 Min for the 5150
        PM_SYS_CLK=280    # Possible values : 130,133,160,170,...,240,260,270,280

        MAX_PMRAM=8             # Maximum Nb of 16Kb RAM for the Pico emulated RAM (Reduce to allow all, for test) (8 for 128Kb maximum)

        USE_SDIO=0              # To remove SDIO form the SD Code (Save space and RAM)

        USE_SPI_PSRAM=1         # 1 To enable the SPI PSRAM (> 0 if Fast RAM=1)  0 for Pimoroni pico plus 2
        USE_SPI_PSRAM_DMA=1     # 1 to use the new code with DMA
        PSRAM_HEAP=0            # Not possible on RP2040       

        USE_AUDIO_I2S=1
        PICO_AUDIO_I2S_DATA_PIN=26
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=27
        PICO_AUDIO_I2S_PIO=1
        PICO_AUDIO_I2S_SM=3
        PICO_AUDIO_I2S_DMA_IRQ=1        

# ! ASYNC UART values defined in picomem.cpp        
        PICO_DEFAULT_UART=0
        PICO_DEFAULT_UART_TX_PIN=28
        PICO_DEFAULT_UART_RX_PIN=-1

        PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000000

        USE_DEV_IRQ=0     # IRQ PIC Debug display
        USE_HWIRQ=0       # Always 0 (not available)
        USE_HWDMA=0       # Always 0 (not available)
        USE_SBDSP=1
# Devices below need PicoMEM 1.5
        USE_GUS=0
        USE_MPU=0        
        USE_CDROM=0
      )

endif()

# qwiic added here as it need USE_FRONTPANEL definition for the OLED code
add_subdirectory(qwiic)

# ****     defines Common to all the PicoMEM version    ******
if (PROJECT_TYPE STREQUAL "TEST")      
    target_sources(picomem PRIVATE)

    target_compile_definitions(picomem PRIVATE
  
    BOARD_PICOMEM=1        # Declare it is a PicoMEM Board

    USE_RTC=1

    USE_SDCARD=1            # 1 To enable SD Support
    SD_CRC_ENABLED=0        # CRC off for speed. Real fix for the SD corruption was HW (short W5500 ribbon + WiFi power-down); CRC was the debug safety net. Set to 1 to make SPI transfer errors fail+retry (visible) instead of silent if SI ever regresses.

# - Defines for the rp2040 psram library (ian scott)
    PSRAM_ASYNC=1
   # PSRAM_SPINLOCK=1
    PSRAM_PIN_CS=5          # Chip Select (SPI0 CS )
    PSRAM_PIN_SCK=6         # Clock       (SPI0 SCK)
    PSRAM_PIN_MOSI=7        # Output      (SPI0 TX )
    PSRAM_PIN_MISO=4        # Input       (SPI0 RX )    

# - Audio Defines -
    USE_AUDIO=1
 
# - Adlib/EMU8950 defs
    EMU8950_ASM=1
    EMU8950_TLL_FLASH=1 # pre-computed TLL table stored in flash via .flashdata section (not copied to RAM)
    EMU8950_NO_FLOAT=1  # RP2040 has no FPU; float only used in TLL init anyway
    EMU8950_NO_TIMER=1  # disable timer which isn't used
    EMU8950_NO_TEST_FLAG=1 # disable test flags (which aren't used)
    USE_ALARM=1         # pic.c used for audio interrupts.

# Debug variables (Debug)
    PM_ENABLE_STDIO=0     # Enable the Serial Display to the defined serial device (ASYNC_UART)
    PM_PRINTF=0           # 1 To enable the Debug Printf

    UART_PIN0=0           # Use PIN0 (IRQ) instead of Pin 28 (PicoMEM 1 Only)
    CFG_TUSB_DEBUG=0      # TinyUSB: 0 :Off / 1:Error / 2:Warning / 3:Info
 
    SD_DEBUG=0            # 1 To enable the DBG_PRINTF in the SD/FatFs spi
    USE_PRINTF=0          # Remove for no SD Debug Printf
    USE_DBG_PRINTF=0      # Remove for no SD Debug Printf
    DISP32=0

# Test Code activation
    CORE1_ALIVETEST=0
    TEST_PSRAM=0
    TEST_SDCARD=0
    TEST_DFS=0    
    TEST_OTHER=0            # Other various test added temporarily
    )

endif()  #(PROJECT_TYPE STREQUAL "TEST")

# *******************  Common / Global definitions **************************

if( PICO_BOARD STREQUAL "pico_w")
        add_compile_definitions( 
          PM_WIFI_LED=1        # Set to 1 only with Pico W for the LED
        )
endif()        

if( PICO_BOARD STREQUAL "pico_w" OR PICOMEM_BOARD STREQUAL "PICOMEM_20" OR PICOMEM_BOARD STREQUAL "PICOMEM_JR")
        add_compile_definitions( 
            PM_WIFI=1
# - NETWORK Defines -
            USE_NE2000=1

# CYW43 resources : One State Machine (Moved to PIO1, ISA too big)
# 2 DMA (Automatic claim) for tx, rx
            CYW43_SPI_PIO_PREFERRED_PIO=1
            DNS_TABLE_SIZE=2        # reduce the RAM taken by the DNS Table (Usefull ?)            
        ) 
    else( 
        add_compile_definitions( 
            PM_WIFI=0
# - NETWORK Defines -
            USE_NE2000=0
         )        
        )        
    endif() # pico_w or PICOMEM_20

if (DO_USBHOST)
        add_compile_definitions( 
            USE_USBHOST=1 
        ) 
    else( 
        add_compile_definitions( 
            USE_USBHOST=0 
        )        
        )        
endif()

if (FAST_RAM)
        add_compile_definitions( 
            DO_FAST_RAM=1 
        ) 
    else( 
        add_compile_definitions( 
            DO_FAST_RAM=0 
        )        
        )        
endif()    

# If ASYNC_UART is set to 1 : STDIO use ASYNC_UART Library, otherwise use USB Serial emulation
if(ASYNC_UART)
        add_compile_definitions(
         ASYNC_UART=1
         )
    else()
        add_compile_definitions(
         ASYNC_UART=0
         )
endif()

if(STDIO_UART)
   pico_enable_stdio_uart(picomem 1)
endif()

if(USB_UART)
        add_compile_definitions(
         USB_UART=1
         )
        pico_enable_stdio_usb(picomem 1)
    else()
        add_compile_definitions(
         USB_UART=0
         )
        pico_enable_stdio_usb(picomem 0)
endif()

# stdio behavior:
# Default Pico SDK printf() can block the caller indefinitely if the
# USB-CDC TX FIFO is full and the host terminal isn't draining bytes.
# On WonderMCA Core 0 that blocks the FIFO-poll loop and stalls every
# CMD coming from PMBIOS.  We previously dropped chars by setting
# PICO_STDIO_DEADLOCK_TIMEOUT_MS=0 -- but that loses log output, which
# we now need back so we can see the CMDs leading up to the failure.
# Use 100ms timeout: blocks briefly to let stdio catch up, but won't
# wait forever on a disconnected terminal.
add_compile_definitions(
    PICO_STDIO_DEADLOCK_TIMEOUT_MS=100
    PICO_STDIO_DEFAULT_CRLF=0
)

pico_set_program_name(picomem "picomem")
pico_set_program_version(picomem "0.5")

# "Compile" the PIO Code    
pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/psram_spi2.pio)          # Old PSRAM code (Without DMA)
pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/isa_iomem_m1.pio)
pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/isa_iomem_m2.pio)        # PicoMEM With Multiplexer V2 
if (PICOMEM_BOARD STREQUAL "PICOMEM_15")
pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/isa_iomem_m2_v15.pio)    # PicoMEM 1.5 With Multiplexer V2
endif()
if (PICOMEM_BOARD STREQUAL "PICOMEM_20" OR PICOMEM_BOARD STREQUAL "PICOMEM_JR")
pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/isa_iomem_m2_v15.pio)    # PicoMEM 1.5 With Multiplexer V2
endif()
if (PICOMEM_BOARD STREQUAL "WONDERMCA_10")
set(WM_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../WonderMCA/src)
set(WM_DIR     ${CMAKE_CURRENT_LIST_DIR}/../WonderMCA)
pico_generate_pio_header(picomem ${WM_SRC_DIR}/i2s.pio)
pico_generate_pio_header(picomem ${WM_SRC_DIR}/mca_iomem.pio)
# bootrom_image.h is pre-generated in WonderMCA/src/ (already in include path)
endif()
#pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/pm_test.pio)

# BUILD Library list

target_link_libraries( picomem

#    pico_cyw43_arch_none    
    pico_stdlib
    pico_multicore
    pico_util
#    hardware_rtc
    hardware_pio
    hardware_pll
    hardware_i2c
    hardware_pwm  # For Joystick

    # ** Other lib
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
    rp2040-psram

    # ** PicoMEM libs
   
    pm_boards
    pm_libs    
    isa_devices
    bios_wrappers
    qwiic

    # ** Audio libs    
    pm_audio_i2s
    audio_devices
    opl_dbopl

    # ** CD-ROM lib (every source is #if USE_CDROM guarded -- empty for
    # boards built with USE_CDROM=0)
    cdrom

) 
 
# Add other libraries based on the needs

if( PICO_BOARD STREQUAL "pimoroni_pico_plus2_rp2350")
    target_link_libraries( picomem
    pico_psram
)
endif()

if (PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    target_link_libraries( picomem
       wondermca ne2000 pico_psram pico_cyw43_arch_none
)
endif()

if (PICOMEM_BOARD STREQUAL "PICOMEM_20" OR PICOMEM_BOARD STREQUAL "PICOMEM_JR")
    target_link_libraries( picomem
    ne2000
    pico_btstack_cyw43
    pico_btstack_ble
)
endif()

if( PICO_BOARD STREQUAL "pico_w")
    target_link_libraries( picomem
       ne2000
)
endif()

if (PICOMEM_BOARD STREQUAL "PICOMEM_15" OR PICOMEM_BOARD STREQUAL "PICOMEM_20" OR PICOMEM_BOARD STREQUAL "PICOMEM_JR")
    target_link_libraries( picomem
       pm2bios
       picoOled
    )
elseif (PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    # WM10 uses original pmbios + picoOled (screenpad)
    target_link_libraries( picomem
       pmbios
       picoOled
    )
else()
    target_link_libraries( picomem
       pmbios
    )
endif()

if (DO_USBHOST)  # Add Libraries used in USB HOST Mode to the picomem project
    target_include_directories(picomem PUBLIC "${CMAKE_SOURCE_DIR}/USB_Host_Driver")
    target_link_libraries( picomem   
        tinyusb_host
        tinyusb_board
        USB_Host_Driver
        xinput_host
     )  
endif()

if(ASYNC_UART)
    target_link_libraries( picomem
        stdio_async_uart
    )
endif()
    
pico_add_extra_outputs(picomem)

add_custom_target(
    program
    DEPENDS picomem
    COMMAND openocd -f interface/picoprobe.cfg -f target/rp2040.cfg -c "program picomem.elf verify reset exit"
)  
 
# To Enable/Disable USB Host :
#  set(DO_USBHOST 0)
#
# To exchange SD Pio and SPI
#
# Exchange hw_config.c and hw_config_pio.c in "target_link_libraries"
# Exchange add_subdirectory(FatFs_SPI_PIO dosbox)
#          and add_subdirectory(FatFs_SPI dosbox)
# Set     SD_SPI_PIO=1            # 1 To use the PIO Code