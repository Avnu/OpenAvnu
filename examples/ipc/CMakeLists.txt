cmake_minimum_required (VERSION 2.8) 
project (ipcex)
enable_testing()

file(GLOB IPCEX_SRC "ipcex.c")

if(APPLE)
elseif(UNIX)
elseif(WIN32)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
 add_executable (exipc ${IPCEX_SRC})


