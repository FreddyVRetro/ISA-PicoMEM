# WonderMCA MCA devices and bus code
set(WM_SRC ${CMAKE_CURRENT_LIST_DIR}/../../WonderMCA/src)

add_library(wondermca INTERFACE)

target_include_directories(wondermca INTERFACE
    ${WM_SRC}
    ${WM_SRC}/mca_devices
    ${WM_SRC}/wm_boards
    ${WM_SRC}/wm_devices
    ${CMAKE_CURRENT_LIST_DIR}/../../WonderMCA/include
)

target_sources(wondermca INTERFACE
    ${WM_SRC}/wm_board.c
    ${WM_SRC}/mca_device.c
    ${WM_SRC}/mca_devices/dev_mca_pos.cpp
    ${WM_SRC}/mca_devices/dev_mca_tst.cpp
    ${WM_SRC}/mca_devices/dev_mca_post.cpp   # PS/2 POST-code capture
    ${WM_SRC}/mca_irq.c                 # MCA hardware IRQ delivery (WM10)
    ${WM_SRC}/wm_devices/wm_dma.c
    ${WM_SRC}/utils.c
)

# Screenpad UI -- own subfolder + CMakeLists. Attaches its own sources
# and include path to the wondermca interface target.
add_subdirectory(${WM_SRC}/wm_devices/screenpad ${CMAKE_CURRENT_BINARY_DIR}/screenpad)

# W5500 NE2000 backend (hardware Ethernet over SPI). Lives under
# wm_devices/W5500/ with its own CMakeLists. Included whenever the
# active board is WonderMCA -- the W5500 backend is the only NE2000
# transport the WM10 board supports. Picked up here so the selection
# cannot skew relative to src/ne2000/CMakeLists.txt's board-driven
# choice.
if(PICOMEM_BOARD STREQUAL "WONDERMCA_10")
    add_subdirectory(${WM_SRC}/wm_devices/W5500 ${CMAKE_CURRENT_BINARY_DIR}/W5500)

    # usbeth — USB Ethernet (CDC-ECM) NE2000 backend. Coexists with the
    # W5500: the W5500 TU still provides the link-time ne2000_backend
    # default; usbeth reassigns the vtable at runtime when attached
    # (same pattern as the CYW43 WiFi selection). Gated by USE_NE2000_CDCECM
    # (defined in src/CMakeLists.txt's WM10 block).
    add_subdirectory(${WM_SRC}/wm_devices/usbeth ${CMAKE_CURRENT_BINARY_DIR}/usbeth)
endif()
