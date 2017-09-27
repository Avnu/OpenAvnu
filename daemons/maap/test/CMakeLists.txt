cmake_minimum_required (VERSION 2.8) 
project (maap_tests)
enable_testing()

set (ComDir "../common" )

include_directories( ${ComDir} )

if(UNIX)
  add_executable( test_intervals "test_intervals.c" "${ComDir}/intervals.c" )
elseif(WIN32)
  add_executable( test_intervals "test_intervals.c" "${ComDir}/intervals.c" )
endif()

add_test(IntervalTreeWorks test_intervals )
