add_library(pm_libs INTERFACE)

target_include_directories(pm_libs INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/pm_libs
    ${CMAKE_CURRENT_LIST_DIR}/pm_libs/pm_watcom
)

# WonderMCA screenpad host glue (pm_screenpad_host.cpp) needs the
# WonderMCA screenpad headers.
if(PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    target_include_directories(pm_libs INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/../../WonderMCA/src/wm_devices/screenpad
    )
target_sources(pm_libs INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/pm_screenpad_host.cpp
)    
endif()

target_sources(pm_libs INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/pm_libs.cpp
    ${CMAKE_CURRENT_LIST_DIR}/isa_dma.cpp 
    ${CMAKE_CURRENT_LIST_DIR}/pico_rtc.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hw_config.c    
    ${CMAKE_CURRENT_LIST_DIR}/pm_disk.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pm_cache.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pm_apps.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pm_bluetooth_scan.c
    ${CMAKE_CURRENT_LIST_DIR}/pm_pccmd.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pm_bios_menu.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pm_watcom/w_bios.cpp
    ${CMAKE_CURRENT_LIST_DIR}/inputs/players/player.c
    ${CMAKE_CURRENT_LIST_DIR}/inputs/pm_inputs.c
    ${CMAKE_CURRENT_LIST_DIR}/inputs/pm_keyboard.c
)

if (PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    target_sources(audio_devices INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/wm_irq.cpp
    )
else()
    target_sources(audio_devices INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/isa_irq.cpp
    )
endif()

# sp_buttons.c uses FPH_IO* pins (PICOMEM_20 front-panel header). Skip for
# boards that don't have those pins (e.g. WonderMCA).
if(NOT PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    target_sources(pm_libs INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/inputs/buttons/sp_buttons.c
    )
endif()