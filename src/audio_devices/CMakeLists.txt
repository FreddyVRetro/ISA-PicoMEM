# PicoMEM Audio devices emulation code
add_library(audio_devices INTERFACE)

target_include_directories(audio_devices INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/audio_devices
)

# OPL is added as an independant library
target_sources(audio_devices INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/pico_pic.c
    ${CMAKE_CURRENT_LIST_DIR}/dev_audiomix.cpp
    ${CMAKE_CURRENT_LIST_DIR}/square/square.cpp
    ${CMAKE_CURRENT_LIST_DIR}/emu2149/emu2149.c
    ${CMAKE_CURRENT_LIST_DIR}/sbdsp/sbdsp.cpp
)
