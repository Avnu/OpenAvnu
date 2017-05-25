cmake_minimum_required (VERSION 2.8) 
project (open-avb)
enable_testing()

set(C++11 ON CACHE BOOL "Compile with C++11 support" FORCE)
add_subdirectory("thirdparty/cpputest")
add_subdirectory("daemons/common/tests")
add_subdirectory("daemons/mrpd")
add_subdirectory("daemons/maap")
