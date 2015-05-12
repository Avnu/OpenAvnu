cmake_minimum_required (VERSION 2.8) 
project (avdeccd)
enable_testing()

set( CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-Wall -Wpedantic" )


include_directories(
    .
    "../common"
    "../../thirdparty/jdksavdecc-c/include"
    "../../thirdparty/libuv/include"
    "../../thirdparty/uvrawpkt/include")

file(GLOB AVDECCD_SRC 
   ./acmp_controller.c 
   ./acmp_controller.h 
   ./acmp_listener.c 
   ./acmp_listener.h 
   ./acmp_talker.c 
   ./acmp_talker.h 
   ./adp_adv.c 
   ./adp_adv.h 
   ./srp_info.h
   ./srp_info.c
   ./srp_encoding.h
   ./srp_encoding.c
   ./srp.h
   ../common/parse.c 
   ../common/parse.h 
)

add_executable (avdeccd ${AVDECCD_SRC} "avdeccd.c" "avdeccd.h")

target_link_libraries( avdeccd jdksavdecc-c uvrawpkt uv)

if(APPLE)
  target_link_libraries(avdeccd pcap)
elseif(UNIX)
  target_link_libraries(avdeccd pcap pthread)
endif()



