cmake_minimum_required(VERSION 3.12)

project(emu2149)

if (NOT BUILD_SHARED_LIBS)
 add_definitions(-DVR_EMU_2149_STATIC)
endif()

add_library(emu2149)

if (WIN32)
  if (BUILD_SHARED_LIBS)
     add_definitions(-DVR_EMU_2149_COMPILING_DLL)
   endif()
endif()

target_include_directories (emu2149 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(emu2149 PRIVATE emu2149.c)
