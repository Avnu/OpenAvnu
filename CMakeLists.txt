cmake_minimum_required (VERSION 2.8) 
project (open-avb)
enable_testing()

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

add_subdirectory("thirdparty/cpputest")
add_subdirectory("daemons/common/tests")
add_subdirectory("daemons/mrpd")
add_subdirectory("daemons/maap")

message("
-------------------------------------------------------
OpenAvnu Build information

Current compiler options:
    CC:                                 ${CMAKE_C_COMPILER}
    CXX:                                ${CMAKE_CXX_COMPILER}
    OpenAvnu CFLAGS:                    ${CMAKE_C_FLAGS}
    OpenAvnu CXXFLAGS:                  ${CMAKE_CXX_FLAGS}
    OpenAvnu LDFLAGS:                   ${CMAKE_LD_FLAGS}

Features configured in OpenAvnu root CMakeFile.txt:
    Memory Leak Detection:              ${MEMORY_LEAK_DETECTION}
    Compiling Extensions:               ${EXTENSIONS}
    Support Long Long:                  ${LONGLONG}
    Use OpenAvnu flags:                 ${CMAKE_FLAGS}

    Using Standard C library:           ${STD_C}
    Using Standard C++ library:         ${STD_CPP}
    Using C++11 library:                ${C++11}

-------------------------------------------------------
")
