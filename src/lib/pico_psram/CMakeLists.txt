add_library(pico_psram INTERFACE)

target_sources(pico_psram INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/psram.cpp          
            )

target_include_directories(pico_psram INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/lib/pico_psram
        )