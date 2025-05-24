# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
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
          
# for TinyUSB
#set(FAMILY rp2040)
#set(BOARD pico_sdk)

# Declare the Pi Pico Board used
set(PICO_BOARD pico_w CACHE STRING "Board type")
#set(PICO_BOARD PICO)
#set(PICO_BOARD PICO2)
#set(PICO_BOARD PIMORONI_PICO_PLUS2_RP2350)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
#include(pico_extras_import.cmake)

 # ** UART (Debug) and USB Host definition **
 set(STDIO_UART 1)
 set(USB_UART 0)
 set(ASYNC_UART 0)
 set(DO_USBHOST 1)
 
project(picomem C CXX ASM)
  
#set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_BUILD_TYPE "Release")
set(PROJECT_TYPE "TEST")

# Tell the Pico SDK to use our local tinyusb instead of the one included in the SDK
# This is needed to support tusb_xinput
set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()
  
# Add the Sub directories containing libraries

add_subdirectory(lib/FatFs_SPI)
add_subdirectory(rom)                       # PicoMEM BIOS binary
add_subdirectory(isa_devices)               # all ISA devices IO and MEM code
add_subdirectory(qwiic)
add_subdirectory(audio_devices)
add_subdirectory(audio_devices/opl)
add_subdirectory(lib/pm_audio/pm_audio_i2s)

add_subdirectory(lib/rp2040-psram)          # rp2040 PSRAM with DMA

if (PICO_BOARD STREQUAL "pimoroni_pico_plus2_rp2350")
    add_subdirectory(lib/pico_psram)            # rp2350 PSRAM
endif()

if (PICO_BOARD STREQUAL "pico_w")
    add_subdirectory(ne2000)
endif()
if (DO_USBHOST)
    add_subdirectory(lib/tusb_xinput)
    add_subdirectory(USB_Host_driver)    
endif()
if (ASYNC_UART)
    add_subdirectory(stdio_async_uart)
endif()

include_directories(
    ${TINYUSB_PATH}/hw
    ${TINYUSB_PATH}/src
#    ${CMAKE_CURRENT_LIST_DIR}/src/pm_libs    
#    ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_arch/include
#    ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_driver/include
#    ${PICO_SDK_PATH}/lib/cyw43-driver/src 
)


# Add executable. Default name is the project name, version 0.1
add_executable(picomem picomem.cpp
               pm_cmd.cpp
               pm_pccmd.cpp 
               pm_libs/pm_libs.cpp 
               dosbox/bios_disk.cpp
               pm_disk/pm_disk.cpp      # Disk code
               pm_disk/hw_config.c      # For uSD Driver in SPI
#               hw_config_pio.c         # For uSD Driver in PIO
               )

target_compile_options(picomem PRIVATE -save-temps -fverbose-asm) 

target_compile_definitions(picomem PRIVATE

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
)
    
if (PROJECT_TYPE STREQUAL "TEST")
    target_sources(picomem PRIVATE)

    target_compile_definitions(picomem PRIVATE

#   PM_PROTO=1       # Remove for final Board
    TIMING_DEBUG=0
  
# 270 Min for the 5150
    PM_SYS_CLK=280    # Possible values : 130,133,160,170,...,240,260,270,280

# Define the BIOS default base address
# Don't use C000 Segment (0xC0000u) if the PC is configured to use an external video BIOS
# To Configure the ROM, Be carefull that the XTIDE ROM is 32Kb, even if only 16Kb is used
    USEROM0=1
    ROM_BIOS=0x0D0000u     # @ of the PM BIOS           12Kb

    MAX_PMRAM=8             # Maximum Nb of 16Kb RAM for the Pico emulated RAM (Reduce to allow all, for test) (8 for 128Kb maximum)

    MUX_V2=0
    
    USE_SDCARD=1            # 1 To enable SD Support   
    USE_SDIO=0              # To remove SDIO form the SD Code (Save space and RAM)
    SD_SPI_PIO=0            # 1 To use the uSD driver with PIO
    SD_CRC_ENABLED=0        # Disable CRC checking for SPI-attached cards.

    DO_FAST_RAM=0           # Fast RAM disable all the RAM emulation based on PSRAM

    USE_PSRAM=1             # 1 To enable PSRAM Support ! (Put to 0 if Fast RAM)  0 for Pimoroni pico plus 2
    USE_PSRAM_DMA=1

    # defines for the rp2040 psram library (ian scott)
    PSRAM_ASYNC=1
   # PSRAM_SPINLOCK=1
    PSRAM_PIN_CS=5          # Chip Select (SPI0 CS )
    PSRAM_PIN_SCK=6         # Clock       (SPI0 SCK)
    PSRAM_PIN_MOSI=7        # Output      (SPI0 TX )
    PSRAM_PIN_MISO=4        # Input       (SPI0 RX )    

# - Audio Defines -    
    USE_AUDIO=1
 
# Adlib defs  
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
    PM_ENABLE_STDIO=1      # Enable the Serial Display to the defined serial device (ASYNC_UART)
    UART_PIN0=0            # Use PIN0 (IRQ) instead of Pin 28

    CFG_TUSB_DEBUG=0      # TinyUSB: 0 :Off / 1:Error / 2:Warning / 3:Info
    SD_DEBUG=0            # 1 To enable the DBG_PRINTF in the SD/FatFs spi
    USE_PRINTF=0          # Remove for no SD Debug Printf
    USE_DBG_PRINTF=0      # Remove for no SD Debug Printf
    PM_PRINTF=1           # 1 To enable the Debug Printf
    DISP32=0
    
    # Test Code activation
    TEST_PSRAM=0
    TEST_SDCARD=0
    TEST_OTHER=0            # Other various test added temporarily
 
    )
   
    if( PICO_BOARD STREQUAL "pico_w") 
        add_compile_definitions( 
            PM_PICO_W=1 
# - NETWORK Defines -
            USE_NE2000=1

# CYW43 resources : One State Machine (Moved to PIO1, ISA too big)
# 2 DMA (Automatic claim) for tx, rx

            CYW43_PIO_CLOCK_DIV_INT=4  # For the Wifi Chip PIO clock : 2 for 133MHz
            CYW43_SPI_PIO_PREFERRED_PIO=1
            DNS_TABLE_SIZE=2        # reduce the RAM taken by the DNS Table (Usefull ?)            

        ) 
    else( 
        add_compile_definitions( 
            PM_PICO_W=0
# - NETWORK Defines -
            USE_NE2000=0
         )        
        )        
    endif()

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

#    pico_enable_stdio_uart(picomem 0)
#    pico_enable_stdio_semihosting(picomem 0)
#    pico_enable_stdio_usb(picomem 0)
#    pico_enable_stdio_uart(picomem 1)

endif()  #(PROJECT_TYPE STREQUAL "TEST")

pico_set_program_name(picomem "picomem")
pico_set_program_version(picomem "0.1")

# "Compile" the PIO Code    
pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/psram_spi2.pio)      # Old PSRAM code (Without DMA)
pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/isa_iomem.pio)
pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/isa_iomem_v2.pio)    # PicoMEM With Multiplexer V2 
pico_generate_pio_header(picomem ${CMAKE_CURRENT_LIST_DIR}/pm_test.pio)

# BUILD Library list

# Add the standard library to the build
target_link_libraries( picomem

#    pico_cyw43_arch_none    
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_pll
    hardware_i2c
    hardware_pwm  # For Joystick

    # ** Other lib
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
    rp2040-psram

    # ** PicoMEM libs
    pmbios
    isa_devices
    qwiic

# ** Audio libs    
    pm_audio_i2s
    audio_devices
    opl  
) 
 
# Add other libraries based on the needs

if( PICO_BOARD STREQUAL "pimoroni_pico_plus2_rp2350")
    target_link_libraries( picomem
    pico_psram
)
endif()

if( PICO_BOARD STREQUAL "pico_w")
    target_link_libraries( picomem
       ne2000
)
endif()

if (DO_USBHOST)  # Add Libraries used in USB HOST Mode to the picomem project
    target_include_directories(picomem PUBLIC "${CMAKE_SOURCE_DIR}/USB_Host_driver")
    target_link_libraries( picomem   
        tinyusb_host
        tinyusb_board
        USB_Host_driver
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