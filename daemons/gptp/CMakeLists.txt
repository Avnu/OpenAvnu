cmake_minimum_required (VERSION 2.8)
project (gptp)

include_directories( "./common" )
file(GLOB GPTP_COMMON "./common/*.cpp" "./common/*.c")

if(UNIX)
  include_directories( include "./linux/src" )
  file(GLOB GPTP_OS "./linux/src/*.cpp")
  target_link_libraries(gptp pthread)
elseif(WIN32)
  if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    link_directories($ENV{WPCAP_DIR}/Lib/x64)
  elseif( CMAKE_SIZEOF_VOID_P EQUAL 4 )
    link_directories($ENV{WPCAP_DIR}/Lib)
  endif()

  # HAVE_REMOTE change pcap include options
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -DHAVE_REMOTE)
  include_directories( include "./windows/daemon_cl" $ENV{WPCAP_DIR}/Include )
  file(GLOB GPTP_OS "./windows/daemon_cl/*.cpp")
  add_executable (gptp ${GPTP_COMMON} ${GPTP_OS})
  target_link_libraries(gptp wpcap Iphlpapi Ws2_32)
endif()

#add_subdirectory("tests/simple")
