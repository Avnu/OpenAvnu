cmake_minimum_required (VERSION 2.8)
project (maap)

set (ComDir "./common")
set (ComSource
  "${ComDir}/intervals.c"
  "${ComDir}/maap.c"
  "${ComDir}/maap_net.c"
  "${ComDir}/maap_packet.c"
  "${ComDir}/maap_parse.c"
)
include_directories( ${ComDir} )

if(UNIX)
  set (OsDir "./linux/src")
  set (OsSource
    "${OsDir}/maap_daemon.c"
    "${OsDir}/maap_timer_linux.c"
  )
  include_directories( ${OsDir} )
  add_executable(maap_daemon ${ComSource} ${OsSource})
  target_link_libraries(maap_daemon rt)
endif()

add_subdirectory("test")
add_subdirectory("tests")
