cmake_minimum_required (VERSION 2.8) 
project (maap_tests)
enable_testing()

set (ComDir "../common" )

include_directories( ${ComDir} )

if(UNIX)
  add_executable( test_intervals "test_intervals.c" "${ComDir}/intervals.c" )
  add_executable( test_maap_packet_packing "test_maap_packet_packing.c" "${ComDir}/maap_packet.c" )
endif()

add_test(IntervalTreeWorks test_intervals )
add_test(PacketPackingWorks test_maap_packet_packing )
