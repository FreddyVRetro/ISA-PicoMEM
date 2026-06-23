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

# FINAL /RELEASE Firmware settings PicoMEM 1.5:
#    set(PICO_BOARD pico_w CACHE STRING "Board type")
#    set(STDIO_UART 1)
#    PM_ENABLE_STDIO=0
#    PM_PRINTF=0
#    USE_AUDIO_I2S=1
#    DO_FAST_RAM=0
#    USE_SPI_PSRAM=1
#    USE_SPI_PSRAM_DMA=1

# Declare the Pi Pico Board used

set(PICO_BOARD pimoroni_pico_plus2_rp2350 CACHE STRING "Board type")
#set(PICO_BOARD pico_w CACHE STRING "Board type")

# Declare the PicoMEM Board

#set(PICOMEM_BOARD "PICOMEM")
#set(PICOMEM_BOARD "PICOMEM_LP")
#set(PICOMEM_BOARD "PICOMEM_13")
#set(PICOMEM_BOARD "PICOMEM_15")
set(PICOMEM_BOARD "PICOMEM_20")
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

if (PICOMEM_BOARD STREQUAL "PICOMEM_20")
 set(PICO_FLASH_SIZE_BYTES 16777216)  # 16Mb PicoMEM2
 set(PICO_CYW43_SUPPORTED "1")
 pico_register_common_scope_var(PICO_CYW43_SUPPORTED)
endif()

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()
  
####### Add the Sub directories containing libraries  ########

add_subdirectory(lib/FatFs_SPI)
add_subdirectory(rom)                       # PicoMEM BIOS binary
add_subdirectory(isa_devices)               # all ISA devices IO and MEM code
add_subdirectory(audio_devices)
add_subdirectory(audio_devices/opl)
add_subdirectory(lib/pm_audio/pm_audio_i2s)
add_subdirectory(lib/my_etherdfs)
add_subdirectory(pm_libs)
add_subdirectory(pm_boards)
add_subdirectory(lib/picoOled)
#add_subdirectory(isa_devices/cdrom)
add_subdirectory(lib/rp2040-psram)          # RP2040 PSRAM with DMA

if (PICO_BOARD STREQUAL "pimoroni_pico_plus2_rp2350")
    add_subdirectory(lib/pico_psram)        # RP2350 PSRAM
endif()

if (PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    add_subdirectory(wondermca)            # WonderMCA MCA bus and devices
endif()

if (PICOMEM_BOARD STREQUAL "PICOMEM_20" OR PICO_BOARD STREQUAL "pico_w")
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
               dosbox/bios_disk.cpp
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
      )
elseif (PICOMEM_BOARD STREQUAL "PICOMEM_20")
      set(USE_FRONTPANEL 1)
# PicoMEM 2.0 + Parameters
      add_compile_definitions(
        BOARD_PM20=1
        MUX_V2=1

# Pico Main frequency (280, 340 or 360)
        PM_SYS_CLK=360    # Possible values : 130,133,160,170,...,240,260,270,280
#        PM_SYS_CLK=280    # Possible values : 130,133,160,170,...,240,260,270,280
        CYW43_PIO_CLOCK_DIV_INT=6  # For the Wifi Chip PIO clock : 2 for 133MHz

        MAX_PMRAM=16            # Maximum Nb of 16Kb RAM for the Pico emulated RAM (Reduce to allow all, for test) (8 for 128Kb maximum)

        USE_SDIO=0              # To remove SDIO form the SD Code (Save space and RAM)

        USE_SPI_PSRAM=0         # PicoMEM 15 Does not have SPI PSRAM
        USE_SPI_PSRAM_DMA=0     # PicoMEM 15 Does not have SPI PSRAM
        PSRAM_HEAP=1            # Enable the PSRAM HEAP

        USE_AUDIO_I2S=1
        I2S_CLK_INVERT=1                  # Invert the I2S Clock signals
        PICO_AUDIO_I2S_DATA_PIN=15        #DIN
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=16  #LCK
        PICO_AUDIO_I2S_PIO=1
        PICO_AUDIO_I2S_SM=3
        PICO_AUDIO_I2S_DMA_IRQ=1

# Wifi / CYW43
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

        USE_DEV_IRQ=0       # IRQ Debug display
        DBG_PIN_AUDIO=0
        ENABLE_CDROM_LOG=0  # Comment to disable CD LOG

        ONBOARD_RTC=1
        USE_OLED=1          # Detect and Use OLED (PicoMEM 1.5/2 only)
        USE_HWIRQ=1         # for the PicoMEM 1.5, use the extra interrupt lines
        USE_HWDMA=1
        USE_SBDSP=1
        USE_GUS=1
        USE_MPU=1
        SF2_GMGSx=1        # 1 to use the GMGSx sound font
        USE_CDROM=0
        USE_CD_AUDIO_FIFO=0
      )
elseif (PICOMEM_BOARD STREQUAL "WONDERMCA_10")
      set(USE_FRONTPANEL 0)
# PicoMEM 2.0 + Parameters
      add_compile_definitions(

        BOARD_WM10=1

        PM_ENABLE_STDIO=1     # Enable the Serial Display to the defined serial device (ASYNC_UART)
        PM_PRINTF=1           # 1 To enable the Debug Printf

# Pico Main frequency (280, 340 or 360)
        PM_SYS_CLK=320    # Temporary: 280 to bypass QMI flash timing code for debug

        MAX_PMRAM=16            # Maximum Nb of 16Kb RAM for the Pico emulated RAM (Reduce to allow all, for test) (8 for 128Kb maximum)

        USE_SDIO=0              # To remove SDIO form the SD Code (Save space and RAM)

        USE_SPI_PSRAM=0         # PicoMEM 15 Does not have SPI PSRAM
        USE_SPI_PSRAM_DMA=0     # PicoMEM 15 Does not have SPI PSRAM
        PSRAM_HEAP=1            # Enable the PSRAM HEAP

        USE_AUDIO_I2S=1
        I2S_CLK_INVERT=1                  # Invert the I2S Clock signals
        PICO_AUDIO_I2S_DATA_PIN=15        #DIN
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=16  #LCK
        PICO_AUDIO_I2S_PIO=1
        PICO_AUDIO_I2S_SM=3
        PICO_AUDIO_I2S_DMA_IRQ=1

# ! ASYNC UART values defined in picomem.cpp        
        PICO_DEFAULT_UART=0
        PICO_DEFAULT_UART_TX_PIN=44       # 44/45
        PICO_DEFAULT_UART_RX_PIN=45

        PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000000

        USE_DEV_IRQ=0       # IRQ Debug display
        DBG_PIN_AUDIO=0
        ENABLE_CDROM_LOG=0  # Comment to disable CD LOG

        USE_HWIRQ=1         # for the PicoMEM 1.5, use the extra interrupt lines
        USE_HWDMA=1
        USE_SBDSP=1
        USE_GUS=1
        USE_MPU=0
        USE_CDROM=0
        USE_CD_AUDIO_FIFO=0
      )      
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
  
    BOARD_PICOMEM=1        # Declare it is a PicoMEMP Board

# - Define the BIOS default base address
# Don't use C000 Segment (0xC0000u) if the PC is configured to use an external video BIOS
# To Configure the ROM, Be carefull that the XTIDE ROM is 32Kb, even if only 16Kb is used
    ROM_BIOS=0x0D0000u      # @ of the PM BIOS           12Kb

    USE_RTC=1

    USE_SDCARD=1            # 1 To enable SD Support   
    SD_CRC_ENABLED=0        # Disable CRC checking for SPI-attached cards.

# - Defines for the rp2040 psram library (ian scott)
    PSRAM_ASYNC=1
   # PSRAM_SPINLOCK=1
    PSRAM_PIN_CS=5          # Chip Select (SPI0 CS )
    PSRAM_PIN_SCK=6         # Clock       (SPI0 SCK)
    PSRAM_PIN_MOSI=7        # Output      (SPI0 TX )
    PSRAM_PIN_MISO=4        # Input       (SPI0 RX )    

# - Audio Defines -
    USE_AUDIO=1
 
# - Adlib defs  
    USE_EMU8950_OPL=1
    EMU8950_ASM=1
    EMU8950_NO_TLL=1               # Free 128Kb of RAM, but maybe slower
#    EMU8950_NO_WAVE_TABLE_MAP=1   # Free 8Kb ?
#    EMU8950_LINEAR=1
#    EMU8950_NO_PERCUSSION_MODE=1
    EMU8950_NO_TEST_FLAG=1
    USE_ALARM=1                    # pic.c used for audio interrupts.
# To compile with EMU8950_LINEAR, Needed to comment this in OPL Code (2 Times) if (hack_ch == 0 && s == 12)

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
   


#    pico_enable_stdio_uart(picomem 0)
#    pico_enable_stdio_semihosting(picomem 0)
#    pico_enable_stdio_usb(picomem 0)
#    pico_enable_stdio_uart(picomem 1)

endif()  #(PROJECT_TYPE STREQUAL "TEST")

# *******************  Common / Global definitions **************************

if( PICO_BOARD STREQUAL "pico_w")
        add_compile_definitions( 
          PM_WIFI_LED=1        # Set to 1 only with Pico W for the LED
        )
endif()        

if( PICO_BOARD STREQUAL "pico_w" OR PICOMEM_BOARD STREQUAL "PICOMEM_20")
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
        if(STDIO_UART)
          pico_enable_stdio_uart(picomem 1)
        endif()
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

pico_set_program_name(picomem "picomem")
pico_set_program_version(picomem "0.5")

# "Compile" the PIO Code    
pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/psram_spi2.pio)          # Old PSRAM code (Without DMA)
pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/isa_iomem_m1.pio)
pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/isa_iomem_m2.pio)        # PicoMEM With Multiplexer V2 
if (PICOMEM_BOARD STREQUAL "PICOMEM_15")
pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/isa_iomem_m2_v15.pio)    # PicoMEM 1.5 With Multiplexer V2
endif()
if (PICOMEM_BOARD STREQUAL "PICOMEM_20")
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
    qwiic

    # ** Audio libs    
    pm_audio_i2s
    audio_devices
    opl  

    # ** CD-ROM lib
#    cdrom

    # ** etherdfs server
    pm_etherdfs

) 
 
# Add other libraries based on the needs

if( PICO_BOARD STREQUAL "pimoroni_pico_plus2_rp2350")
    target_link_libraries( picomem
    pico_psram
)
endif()

if (PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    target_link_libraries( picomem
       wondermca
)
endif()

if (PICOMEM_BOARD STREQUAL "PICOMEM_20")
    target_link_libraries( picomem
       ne2000
)
endif()

if( PICO_BOARD STREQUAL "pico_w")
    target_link_libraries( picomem
       ne2000
)
endif()

if (PICOMEM_BOARD STREQUAL "PICOMEM_15" OR PICOMEM_BOARD STREQUAL "PICOMEM_20" OR PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    target_link_libraries( picomem
       pm2bios
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