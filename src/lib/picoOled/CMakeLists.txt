add_library(picoOled INTERFACE)

target_sources(picoOled INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/picoOled.c        
            )

target_include_directories(picoOled INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/lib/picoOled
        )
