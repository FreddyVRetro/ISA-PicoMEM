add_library(ne2000 INTERFACE)

target_include_directories(ne2000 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/ne2000
)

# Core DP8390 emulator -- transport-agnostic.
target_sources(ne2000 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ne2000.c
)

# Pick exactly ONE backend TU per build. Selection is driven by the
# active board (PICOMEM_BOARD / PICO_BOARD) to avoid dependency on the
# order in which CMake variables are set.
# - WonderMCA -> W5500 backend TU is added in src/wondermca/CMakeLists.txt
# - Anything else -> default CYW43 (Pico W) backend
if(PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    # W5500 backend lives in the WonderMCA submodule (see
    # src/wondermca/CMakeLists.txt). Do NOT link CYW43.
else()
    target_sources(ne2000 INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/backends/cyw43_backend.c
    )
    target_link_libraries(ne2000 INTERFACE
        pico_cyw43_arch_none
    )
endif()