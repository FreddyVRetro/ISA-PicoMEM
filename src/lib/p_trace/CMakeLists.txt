add_library(p_trace INTERFACE)

target_sources(p_trace INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/p_trace.c          
            )

target_include_directories(p_trace INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/lib/p_trace
        )