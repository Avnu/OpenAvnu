#Windows specifc build of named pipe test
cmake_minimum_required (VERSION 2.8)
project (named_pipe_test)

include_directories( ".." )
file(GLOB SRC "*.cpp")

add_definitions(-D_CRT_SECURE_NO_WARNINGS )
include_directories( include "../../common" )
add_executable (named_pipe_test ${SRC})
target_link_libraries(named_pipe_test Iphlpapi Ws2_32)
