
    add_library(opl INTERFACE)
    target_sources(opl INTERFACE
#emu8950    
            ${CMAKE_CURRENT_LIST_DIR}/emu8950.c
            ${CMAKE_CURRENT_LIST_DIR}/slot_render.cpp
#            ${CMAKE_CURRENT_LIST_DIR}/opl_emu8950.c
#DosBox OPL
            ${CMAKE_CURRENT_LIST_DIR}/dbopl/dbopl.cpp            
            ${CMAKE_CURRENT_LIST_DIR}/opl_dbopl.cpp
            )
    target_compile_options(opl INTERFACE -fms-extensions) # want OPL_SLOT_RENDER to be unnamed within OPL_SLOT
    if (APPLE)
        target_compile_options(opl INTERFACE -Wno-microsoft-anon-tag)
    endif() 
    target_compile_definitions(opl INTERFACE
            EMU8950_NO_RATECONV
    )
    target_include_directories(opl INTERFACE ${CMAKE_CURRENT_LIST_DIR})
    target_link_libraries(opl INTERFACE pm_audio_i2s hardware_gpio)
    if (PICO_ON_DEVICE)
        target_sources(opl INTERFACE
                ${CMAKE_CURRENT_LIST_DIR}/slot_render_pico.S
        )
        target_link_libraries(opl INTERFACE hardware_interp)
    endif()