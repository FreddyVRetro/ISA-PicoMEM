# PicoMEM various Devices emulation code
add_library(pm_boards INTERFACE)

target_include_directories(pm_boards INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/pm_boards
)

target_sources(pm_boards INTERFACE
#    ${CMAKE_CURRENT_LIST_DIR}/isa_devices.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pmdev_pico.cpp
)