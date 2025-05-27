add_library(etherdfs INTERFACE)

target_include_directories(etherdfs INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}    
)

target_sources(etherdfs INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ethersrv-linux.c
    ${CMAKE_CURRENT_LIST_DIR}/fs.c
)

target_link_libraries(etherdfs INTERFACE    
    etherdfs
    FatFs_SPI
) 