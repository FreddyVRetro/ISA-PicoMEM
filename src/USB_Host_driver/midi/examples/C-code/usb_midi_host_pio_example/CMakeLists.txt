cmake_minimum_required(VERSION 3.13)

set(BOARD pico_sdk)
include(pico_sdk_import.cmake)

project(usb_midi_host_pio_example C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../.. usb_midi_host)
add_executable(usb_midi_host_pio_example
    usb_midi_host_pio_example.c
)

pico_enable_stdio_uart(usb_midi_host_pio_example 1)
#pico_enable_stdio_usb(usb_midi_host_pio_example 1)

target_include_directories(usb_midi_host_pio_example PRIVATE
 ${CMAKE_CURRENT_LIST_DIR}
)

target_link_options(usb_midi_host_pio_example PRIVATE -Xlinker --print-memory-usage)
target_compile_options(usb_midi_host_pio_example PRIVATE -Wall -Wextra)
# uncomment the next line if you are using the Adafruit Feather with USB A Host board
#target_compile_definitions(usb_midi_host_pio_example PRIVATE PICO_DEFAULT_LED_PIN=13)
target_link_libraries(usb_midi_host_pio_example tinyusb_host tinyusb_board usb_midi_host_app_driver
  pico_multicore tinyusb_pico_pio_usb pico_stdlib)

if(DEFINED PICO_BOARD)
if(${PICO_BOARD} MATCHES "pico_w")
message("Adding pico_cyw43_arch_none library for pico_w")
# add additional compile and link options
target_link_libraries(usb_midi_host_pio_example pico_cyw43_arch_none)
endif()
else()
message("PICO_BOARD is not defined")
endif()

pico_add_extra_outputs(usb_midi_host_pio_example)

