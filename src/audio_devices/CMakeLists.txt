# PicoMEM Audio devices emulation code
add_library(audio_devices INTERFACE)

target_include_directories(audio_devices INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/audio_devices
)

# OPL is added as an independant library
target_sources(audio_devices INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/pico_pic.c
    ${CMAKE_CURRENT_LIST_DIR}/audio_fifo8.c    
    ${CMAKE_CURRENT_LIST_DIR}/audio_fifo.c
    ${CMAKE_CURRENT_LIST_DIR}/dev_audiomix.cpp
    ${CMAKE_CURRENT_LIST_DIR}/square/square.cpp
    ${CMAKE_CURRENT_LIST_DIR}/emu2149/emu2149.c
    ${CMAKE_CURRENT_LIST_DIR}/covox/covox_dac.cpp
#    ${CMAKE_CURRENT_LIST_DIR}/gus/gus-x.cpp
    ${CMAKE_CURRENT_LIST_DIR}/midi/ardsynth_renderer.cpp
#    ${CMAKE_CURRENT_LIST_DIR}/volctrl.cpp    
)

if (PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    target_sources(audio_devices INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/sbdsp/wm_sbdsp.cpp   # WM10 "Merged (Aug 2026) Version"
    ${CMAKE_CURRENT_LIST_DIR}/gus/gus-x.cpp
    )
else()
    target_sources(audio_devices INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/sbdsp/sbdsp.cpp     # PicoMEM Version    
    ${CMAKE_CURRENT_LIST_DIR}/gus/pm_gus-x.cpp
    )
endif()

target_include_directories(audio_devices INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/midi
)
