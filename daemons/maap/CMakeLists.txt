cmake_minimum_required (VERSION 2.8)
project (maap)

set (ComDir "./common")
set (ComSource
  "${ComDir}/intervals.c"
  "${ComDir}/maap.c"
  "${ComDir}/maap_net.c"
  "${ComDir}/maap_packet.c"
  "${ComDir}/maap_parse.c"
  "${ComDir}/maap_log_queue.c"
)
include_directories( ${ComDir} )

if(UNIX)
  set (OsDir "./linux/src")
  set (OsSource
    "${OsDir}/maap_daemon.c"
    "${OsDir}/maap_log_linux.c"
    "${OsDir}/maap_timer_linux.c"
  )
  include_directories( ${OsDir} )
  add_executable(maap_daemon ${ComSource} ${OsSource})
  target_link_libraries(maap_daemon rt pthread)
elseif(WIN32)
  set (OsDir "./windows/src")
  set (OsSource
    "${OsDir}/maap_main.c"
    "${OsDir}/maap_log_windows.c"
    "${OsDir}/maap_timer_windows.c"
  )
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -DHAVE_REMOTE)
  include_directories( ${OsDir} )
  add_executable(maap_daemon ${ComSource} ${OsSource})
  target_link_libraries(maap_daemon ws2_32)
endif()

add_subdirectory("test")
add_subdirectory("tests")
