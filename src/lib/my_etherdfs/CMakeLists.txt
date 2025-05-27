add_library(pm_etherdfs INTERFACE)

target_sources(pm_etherdfs INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/ethersrv-pico.c
            ${CMAKE_CURRENT_LIST_DIR}/fs.c
#            ${CMAKE_CURRENT_LIST_DIR}/lock.c
            )

target_include_directories(pm_etherdfs INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/my_etherdfs
        )