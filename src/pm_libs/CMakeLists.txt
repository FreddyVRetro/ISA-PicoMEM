add_library(pm_libs INTERFACE)

target_include_directories(pm_libs INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/pm_libs
)

target_sources(pm_libs INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/pm_libs.cpp
    ${CMAKE_CURRENT_LIST_DIR}/isa_dma.cpp
    ${CMAKE_CURRENT_LIST_DIR}/isa_irq.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pico_rtc.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hw_config.c    
    ${CMAKE_CURRENT_LIST_DIR}/pm_disk.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pm_apps.cpp
)


