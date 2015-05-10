cmake_minimum_required (VERSION 2.8) 
project (avdeccd)
enable_testing()

include_directories( . "../common" )
file(GLOB AVDECCD_SRC 
   ../common/acmp_controller.c ../common/acmp_controller.h ../common/acmp_listener.c ../common/acmp_listener.h ../common/acmp_talker.c ../common/acmp_talker.h ../common/acmpdu.c ../common/acmpdu.h ../common/adp_adv.c ../common/adp_adv.h ../common/adpdu.c ../common/adpdu.h ../common/avdecc.h ../common/avtp.c ../common/avtp.h ../common/eui64set.c ../common/eui64set.h ../common/frame.c ../common/frame.h ../common/parse.c ../common/parse.h ../common/pdu.c ../common/pdu.h ../common/srp_info.h  ../common/eui48.c ../common/eui48.h ../common/eui64.c ../common/eui64.h
)

if(APPLE)
  add_executable (avdeccd ${AVDECCD_SRC} "avdeccd.c" "avdeccd.h")
elseif(UNIX)
  add_executable (avdeccd ${AVDECCD_SRC} "avdeccd.c" "avdeccd.h")
  target_link_libraries(pthread)
endif()

