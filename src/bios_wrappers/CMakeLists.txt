# BIOS-backed storage emulation wrappers.
# HDD/FDD INT 13 services and EtherDFS are exposed through one target so the
# firmware executable does not need to know where each backend is implemented.
add_library(bios_wrappers INTERFACE)

target_include_directories(bios_wrappers INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../dosbox
    ${CMAKE_CURRENT_LIST_DIR}/../lib/my_etherdfs
)

target_sources(bios_wrappers INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/../dosbox/bios_disk.cpp
    ${CMAKE_CURRENT_LIST_DIR}/bios_int13h.cpp
    ${CMAKE_CURRENT_LIST_DIR}/bios_int21h.cpp
    ${CMAKE_CURRENT_LIST_DIR}/bios_int2Fh.cpp
)

target_link_libraries(bios_wrappers INTERFACE
    pm_etherdfs
)
