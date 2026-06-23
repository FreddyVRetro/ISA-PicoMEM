# PicoMEM Audio devices emulation code
add_library(midi_mpu INTERFACE)

target_include_directories(midi_mpu INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/midi_mpu
)

# OPL is added as an independant library
target_sources(midi_mpu INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/VWMPU.C
)
