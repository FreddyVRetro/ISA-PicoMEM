add_library(pm_audio_i2s INTERFACE)

pico_generate_pio_header(pm_audio_i2s ${CMAKE_CURRENT_LIST_DIR}/audio_i2s.pio)

target_sources(pm_audio_i2s INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/audio_i2s.c
            ${CMAKE_CURRENT_LIST_DIR}/pm_audio.cpp
            )

target_include_directories(pm_audio_i2s INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/lib/pm_audio/pm_audio_i2s
        )