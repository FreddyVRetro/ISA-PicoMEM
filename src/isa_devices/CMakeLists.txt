# PicoMEM various Devices emulation code
add_library(isa_devices INTERFACE)

target_include_directories(isa_devices INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/isa_devices
    ${CMAKE_CURRENT_LIST_DIR}/serial
    ${CMAKE_CURRENT_LIST_DIR}/cdrom
    ${CMAKE_CURRENT_LIST_DIR}/mke
)

target_sources(isa_devices INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/isa_devices.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dev_memory.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dev_ltemm.cpp    
    ${CMAKE_CURRENT_LIST_DIR}/dev_picomem_io.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dev_post.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dev_irq.cpp    
    ${CMAKE_CURRENT_LIST_DIR}/dev_sbdsp.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dev_adlib.cpp    
    ${CMAKE_CURRENT_LIST_DIR}/dev_cms.cpp     
    ${CMAKE_CURRENT_LIST_DIR}/dev_tandy.cpp     
    ${CMAKE_CURRENT_LIST_DIR}/dev_mindscape.cpp 
    ${CMAKE_CURRENT_LIST_DIR}/dev_covox.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dev_joystick.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dev_mouse.cpp    
    ${CMAKE_CURRENT_LIST_DIR}/dev_dma.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dev_rtc.cpp
    # CD-ROM (Panasonic MKE) -- #if USE_CDROM guarded, so these compile to
    # nothing on boards built with USE_CDROM=0. The data layer (cdrom/*.c)
    # comes from the separate `cdrom` interface library.
    ${CMAKE_CURRENT_LIST_DIR}/dev_cdrom_mke.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mke/mke.c
    ${CMAKE_CURRENT_LIST_DIR}/dev_gus.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dev_mpu.cpp
    # Serial-mouse UART (COM2) -- Windows + DOS mouse. mca_irq calls in
    # dev_sermouse.cpp are BOARD_WM10-guarded, so this links on all boards.
    ${CMAKE_CURRENT_LIST_DIR}/serial/8250uart.cpp
    ${CMAKE_CURRENT_LIST_DIR}/serial/sermouse.cpp
    ${CMAKE_CURRENT_LIST_DIR}/serial/dev_sermouse.cpp
)