add_library(opl INTERFACE)
target_sources(opl INTERFACE
        # ${CMAKE_CURRENT_LIST_DIR}/opl_api.c
        ${CMAKE_CURRENT_LIST_DIR}/emu8950.c
        ${CMAKE_CURRENT_LIST_DIR}/tll_table_flash.c
        ${CMAKE_CURRENT_LIST_DIR}/slot_render.cpp
        ${CMAKE_CURRENT_LIST_DIR}/opl_pico.c)
target_compile_options(opl INTERFACE -fms-extensions) # want OPL_SLOT_RENDER to be unnamed within OPL_SLOT
target_compile_definitions(opl INTERFACE
        EMU8950_NO_RATECONV
)
target_include_directories(opl INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(opl INTERFACE hardware_gpio)
target_sources(opl INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/slot_render_pico.S
)
target_link_libraries(opl INTERFACE hardware_interp)

# ymfm OPL3 backend — alternative to the emu8950 library above.
# Link opl_ymfm instead of opl to use ymf262 (OPL3) emulation.
add_library(opl_ymfm INTERFACE)
target_sources(opl_ymfm INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/opl_ymfm.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ymfm/src/ymfm_opl.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ymfm/src/ymfm_misc.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ymfm/src/ymfm_adpcm.cpp
)
target_include_directories(opl_ymfm INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/ymfm/src
)
target_compile_options(opl_ymfm INTERFACE -O3 -Wno-stringop-overflow)
target_link_libraries(opl_ymfm INTERFACE pico_audio_i2s hardware_gpio hardware_timer)

# DOSBox OPL3 backend
add_library(opl_dbopl INTERFACE)
target_sources(opl_dbopl INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/opl_dbopl.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dbopl/dbopl.cpp
)
target_include_directories(opl_dbopl INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/dbopl
)
target_compile_options(opl_dbopl INTERFACE -O3 -Wno-stringop-overflow)
target_link_libraries(opl_dbopl INTERFACE hardware_gpio hardware_timer)
