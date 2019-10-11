cmake_minimum_required ( VERSION 2.6 ) 
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
project ( AVB ) 

# Some CMake voodoo to set the default build type
IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
else ()
  IF (NOT (("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
	OR ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	OR ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
	OR ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")))
    MESSAGE ( FATAL_ERROR "Unknown CMAKE_BUILD_TYPE; Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel")
  ENDIF ()
ENDIF()
MESSAGE ("-- Build type is ${CMAKE_BUILD_TYPE}")

# Set a define to signal build to source files
STRING ( TOUPPER "${CMAKE_BUILD_TYPE}_BUILD" BUILD_TYPE_STRING )
SET ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D${BUILD_TYPE_STRING}" )

# CMake automatically adds some compiler flags based on CMAKE_BUILD_TYPE
#  for Debug:		"-g" 
#  for RelWithDebInfo: 	"-O2 -g"
#  for Release:		"-03 -DNDEBUG"
#  for MinSizeRel:	"-0s -DNDEBUG"

# point to AVB SRC directory
set ( AVB_SRC_DIR ${CMAKE_SOURCE_DIR} )

# point to HAL directory
set ( AVB_HAL_DIR ${AVB_SRC_DIR}/platform/${OPENAVB_HAL} )

# point to OSAL directory
set ( AVB_OSAL_DIR ${AVB_SRC_DIR}/platform/${OPENAVB_OSAL} )

# point to TCAL directory
set ( AVB_TCAL_DIR ${AVB_SRC_DIR}/platform/platTCAL/${OPENAVB_TCAL} )

# Directory to install binaries to
set ( AVB_INSTALL_BIN_DIR ${CMAKE_BINARY_DIR}/bin )

# Directory to install static libraries to
set ( AVB_INSTALL_LIB_DIR ${CMAKE_BINARY_DIR}/lib )

# Directory to install AVTP Interface module SDK  to
set ( SDK_INSTALL_SDK_INTF_MOD_DIR ${CMAKE_BINARY_DIR}/sdk_intf_mod )

# Directory to install AVTP Interface module SDK  to
set ( SDK_INSTALL_SDK_MAP_MOD_DIR ${CMAKE_BINARY_DIR}/sdk_map_mod )

# Directory to install EAVB SDK  to
set ( SDK_INSTALL_SDK_EAVB_DIR ${CMAKE_BINARY_DIR}/sdk_eavb )

MESSAGE ("-- CMAKE_SOURCE_DIR              : ${CMAKE_SOURCE_DIR}")
MESSAGE ("-- AVB_SRC_DIR                   : ${AVB_SRC_DIR}")
MESSAGE ("-- AVB_HAL_DIR                   : ${AVB_HAL_DIR}")
MESSAGE ("-- AVB_OSAL_DIR                  : ${AVB_OSAL_DIR}")
MESSAGE ("-- AVB_INSTALL_BIN_DIR           : ${AVB_INSTALL_BIN_DIR}")
MESSAGE ("-- AVB_INSTALL_LIB_DIR           : ${AVB_INSTALL_LIB_DIR}")
MESSAGE ("-- SDK_INSTALL_SDK_INTF_MOD_DIR  : ${SDK_INSTALL_SDK_INTF_MOD_DIR}")
MESSAGE ("-- SDK_INSTALL_SDK_MAP_MOD_DIR   : ${SDK_INSTALL_SDK_MAP_MOD_DIR}")
MESSAGE ("-- SDK_INSTALL_SDK_EAVB_DIR      : ${SDK_INSTALL_SDK_EAVB_DIR}")


# Turn on all build warnings
set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -Wall" )

# Turn off strict aliasing
set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing" )

# Set default visibility of symbols (requires GCC version > 4)
set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden" )

# Need this to use pthread attributes 
set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE" )

# Increase ini parser's max line length
set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DINI_MAX_LINE=1024" )

# check Gstreamer version
if ( GSTREAMER_1_0)
set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGSTREAMER_1_0" )
endif ()

# For AVDECC, disable features we won't need.
if (DEFINED AVB_FEATURE_AVDECC)
	if ( AVB_FEATURE_AVDECC )
		set ( AVB_FEATURE_FQTSS 0 )
		set ( AVB_FEATURE_GSTREAMER 0 )
		set ( AVB_FEATURE_ENDPOINT 0 )
		set ( AVB_FEATURE_IGB 0 )
		set ( AVB_FEATURE_ATL 0 )
		set ( IGB_LAUNCHTIME_ENABLED 0 )
	endif ()
endif ()

# Default feature flags
if (NOT DEFINED AVB_FEATURE_FQTSS)
  set ( AVB_FEATURE_FQTSS 1 )
endif ()
# include GStreamer interfaces if not defined
if (NOT DEFINED AVB_FEATURE_GSTREAMER)
  set ( AVB_FEATURE_GSTREAMER 1 )
endif ()
# Default Endpoint feature
if (NOT DEFINED AVB_FEATURE_ENDPOINT)
  set ( AVB_FEATURE_ENDPOINT 0 )
endif ()
# Default AVDECC feature
if (NOT DEFINED AVB_FEATURE_AVDECC)
  set ( AVB_FEATURE_AVDECC 0 )
endif ()
if (NOT DEFINED AVB_FEATURE_IGB)
  set ( AVB_FEATURE_IGB 1 )
  set ( AVB_FEATURE_ATL 0 )
endif ()

# Default launchtime feature
if (NOT DEFINED IGB_LAUNCHTIME_ENABLED)
  set ( IGB_LAUNCHTIME_ENABLED 0 )
else ()
  MESSAGE ( "-- IGB launch time enabled" )
endif ()
set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DIGB_LAUNCHTIME_ENABLED=${IGB_LAUNCHTIME_ENABLED}" )

# Default launchtime feature for ATL
if (NOT DEFINED ATL_LAUNCHTIME_ENABLED)
  set ( ATL_LAUNCHTIME_ENABLED 1 )
endif ()
set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DATL_LAUNCHTIME_ENABLED=${ATL_LAUNCHTIME_ENABLED}" )


# Export feature flags for sub-builds
if (AVB_FEATURE_FQTSS)
  set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_FQTSS=1" )
endif ()
if (AVB_FEATURE_GSTREAMER)
  set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_GSTREAMER=1" )
endif ()
if (AVB_FEATURE_ENDPOINT)
  set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_ENDPOINT=1" )
endif ()
if (AVB_FEATURE_AVDECC)
  set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_AVDECC=1" )
endif ()
if (AVB_FEATURE_IGB)
	set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_IGB=1" )
	set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_ATL=0" )
else ()
	set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_IGB=0" )
endif()
if (AVB_FEATURE_ATL)
	set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_IGB=0" )
	set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_ATL=1" )
else ()
	set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_ATL=0" )
endif ()

#Export Platform defines
if ( PLATFORM_DEFINE )
  set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D${PLATFORM_DEFINE}" )
endif ()

if (NOT DEFINED SDK_DOC_ONLY)
  if ( NOT DEFINED CROSS_PREFIX )
     MESSAGE ( "-- Native build for ${CMAKE_SYSTEM} on ${CMAKE_SYSTEM_PROCESSOR}" )
     set ( ARCH x86 )
     set ( LINUX_KERNEL_DIR /usr/src/linux-headers-${CMAKE_SYSTEM_VERSION} )
     find_package(PkgConfig REQUIRED)
     pkg_check_modules(GLIB_PKG glib-2.0)
     if (AVB_FEATURE_GSTREAMER)
       if ( GSTREAMER_1_0)
         pkg_check_modules(GST_PKG gstreamer-app-1.0 gstreamer-rtp-1.0)
       else ()
         pkg_check_modules(GST_PKG gstreamer-app-0.10 gstreamer-rtp-0.10)
       endif ()
     endif()
     find_package(ALSA REQUIRED)
     set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUBUNTU=1" )
  else ()
    message ( "-- Cross-compiling for " ${OPENAVB_PLATFORM} " (" ${CROSS_PREFIX} "gcc)" )
  endif ()

  if (NOT DEFINED ARCH)
  MESSAGE ( FATAL_ERROR "Aborting: ARCH not set" )
  endif ()     
  if (NOT DEFINED LINUX_KERNEL_DIR)
  MESSAGE ( FATAL_ERROR "Aborting: LINUX_KERNEL_DIR not set" )
  endif ()     
  if (NOT DEFINED GLIB_PKG_INCLUDE_DIRS OR NOT DEFINED GLIB_PKG_LIBRARIES)
  MESSAGE ( FATAL_ERROR "Aborting: glib-2.0 library not found" )
  endif ()     
  if (AVB_FEATURE_GSTREAMER)
    if (NOT DEFINED GST_PKG_INCLUDE_DIRS OR NOT DEFINED GST_PKG_LIBRARIES)
      MESSAGE ( FATAL_ERROR "Aborting: gstreamer library not found" )
    endif()
  endif ()
  if (NOT DEFINED ALSA_INCLUDE_DIRS)
  MESSAGE ( FATAL_ERROR "Aborting: alsa library not found" )
  endif ()     
endif()

# Add /usr/lib to library search path
link_directories( ${SYSROOT}/usr/lib )
link_directories ( ${PLATFORM_SPECIFIC_DIRECTORIES} )
set ( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_SPECIFIC_LINKER_FLAGS}" )

# Setup platform directories
include_directories ( ${PLATFORM_INCLUDE_DIRECTORIES} )
link_directories ( ${PLATFORM_LINK_DIRECTORIES} )

#find_library(TEMP NAMES ${PLATFORM_LINK_LIBRARIES} PATHS ${PLATFORM_LINK_DIRECTORIES} )
#set ( PLATFORM_LINK_LIBRARIES ${TEMP} )

# Add our include directories
#
# TODO: CMakeLists.txt Cleanup
#    Many of the AVB_SRC_DIR directories listed below
#    need to be listed due to component A including
#    header from componet B which is dependent on
#    component C ...
#
#    These should be cleaned up to limit the dependencies
#    across components.
#
include_directories( 
		${AVB_TCAL_DIR}
		${AVB_HAL_DIR}
		${AVB_HAL_DIR}/mcr
		${AVB_OSAL_DIR}
		${AVB_OSAL_DIR}/include
		${AVB_SRC_DIR}/include
		${AVB_OSAL_DIR}/avtp
		${AVB_OSAL_DIR}/tl
		${AVB_SRC_DIR}/avtp 
		${AVB_SRC_DIR}/avdecc
		${AVB_SRC_DIR}/acmp
		${AVB_SRC_DIR}/adp
		${AVB_SRC_DIR}/aecp
		${AVB_SRC_DIR}/aem
		${AVB_SRC_DIR}/endpoint
		${AVB_SRC_DIR}/srp
		${AVB_SRC_DIR}/maap
		${AVB_SRC_DIR}/shaper
		${AVB_SRC_DIR}/inih
		${AVB_SRC_DIR}/map_mjpeg
		${AVB_SRC_DIR}/map_mpeg2ts
		${AVB_SRC_DIR}/map_null
		${AVB_SRC_DIR}/map_pipe
		${AVB_SRC_DIR}/map_aaf_audio
		${AVB_SRC_DIR}/map_uncmp_audio
		${AVB_SRC_DIR}/map_ctrl
		${AVB_SRC_DIR}/map_h264
		${AVB_SRC_DIR}/intf_ctrl
		${AVB_SRC_DIR}/mcr
		${AVB_SRC_DIR}/mediaq
		${AVB_SRC_DIR}/rawsock
		${AVB_SRC_DIR}/qmgr
		${AVB_SRC_DIR}/tl 
		${AVB_SRC_DIR}/util
		${AVB_SRC_DIR}/avdecc_msg
		${AVB_OSAL_DIR}/avdecc
		${AVB_OSAL_DIR}/endpoint
		${AVB_OSAL_DIR}/avdecc_msg
		${AVB_SRC_DIR}/../common
		${AVB_SRC_DIR}/mcs
		)

# Need include and link directories for GLIB
include_directories(${GLIB_PKG_INCLUDE_DIRS})
link_directories(${GLIB_PKG_LIBRARY_DIRS})

# AVB Core Library
SET ( SRC_FILES "" )

if ( AVB_FEATURE_PCAP )
  find_package ( PCAP REQUIRED )
  set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_PCAP" )
  include_directories ( ${PCAP_INCLUDE_DIR} )
  if ( AVB_FEATURE_IGB )
    set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_IGB" )
  endif ()
  if ( AVB_FEATURE_ATL )
    set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_ATL" )
  endif ()
endif ()

add_subdirectory ( util )
add_subdirectory ( inih )
add_subdirectory ( rawsock )
add_subdirectory ( openavb_common )

# TODO: Make the above avbBase, then make avbTl and avbAvdecc from below

if (AVB_FEATURE_AVDECC)
	add_subdirectory ( avdecc )
	add_subdirectory ( acmp )
	add_subdirectory ( adp )
	add_subdirectory ( aecp )
	add_subdirectory ( aem )
else ()
	add_subdirectory ( avtp )
	add_subdirectory ( mediaq )
	add_subdirectory ( mcr )
	add_subdirectory ( mcs )
	add_subdirectory ( tl )
	add_subdirectory ( qmgr )
	if (AVB_FEATURE_ENDPOINT)
	  add_subdirectory ( endpoint )
	endif ()
endif ()

add_library ( avbTl ${SRC_FILES} )
target_link_libraries ( avbTl dl m )
if ( AVB_FEATURE_PCAP )
   target_link_libraries ( avbTl ${PCAP_LIBRARY} )
endif ()

install ( TARGETS avbTl ARCHIVE DESTINATION ${AVB_INSTALL_LIB_DIR} )

if (AVB_FEATURE_AVDECC)
	# avb_avdecc (openavb_avdecc)
	add_subdirectory ( ${AVB_OSAL_DIR}/avb_avdecc )
else ()
	# avb_host (openavb_host and openavb_harness)
	add_subdirectory ( ${AVB_OSAL_DIR}/avb_host )
endif ()

if (NOT AVB_FEATURE_AVDECC)
	# mapping modules
	macro (add_map_mod MAP_NAME)
		SET ( SRC_FILES "" )
		add_subdirectory ( ${MAP_NAME} )
		add_library ( ${MAP_NAME} ${SRC_FILES} )
		install ( TARGETS ${MAP_NAME} ARCHIVE DESTINATION ${AVB_INSTALL_LIB_DIR} )
	endmacro()
	add_map_mod ( "map_ctrl" )
	add_map_mod ( "map_mjpeg" )
	add_map_mod ( "map_mpeg2ts" )
	add_map_mod ( "map_null" )
	add_map_mod ( "map_pipe" )
	add_map_mod ( "map_aaf_audio" )
	add_map_mod ( "map_uncmp_audio" )
	add_map_mod ( "map_h264" )

	# Interface modules (common)
	macro (add_intf_mod INTF_NAME)
		SET ( SRC_FILES "" )
		add_subdirectory ( ${INTF_NAME} )
		add_library ( ${INTF_NAME} ${SRC_FILES} )
		install ( TARGETS ${INTF_NAME} ARCHIVE DESTINATION ${AVB_INSTALL_LIB_DIR} )
		install ( DIRECTORY "${AVB_SRC_DIR}/${INTF_NAME}/" DESTINATION ${AVB_INSTALL_BIN_DIR} FILES_MATCHING PATTERN "*.ini" )
	endmacro()
	add_intf_mod ( "intf_ctrl" )
	add_intf_mod ( "intf_echo" )
	add_intf_mod ( "intf_logger" )
	add_intf_mod ( "intf_null" )
	add_intf_mod ( "intf_tonegen" )
	add_intf_mod ( "intf_viewer" )

	# Interface modules (platform)
	macro (add_intf_mod_platform INTF_NAME)
		SET ( SRC_FILES "" )
		SET ( INTF_INCLUDE_DIR "")
		SET ( INTF_LIBRARY_DIR "")
		SET ( INTF_LIBRARY "")
		add_subdirectory ( ${AVB_OSAL_DIR}/${INTF_NAME} )
		include_directories ( ${INTF_INCLUDE_DIR} )
		add_library ( ${INTF_NAME} ${SRC_FILES} )
		install ( TARGETS ${INTF_NAME} ARCHIVE DESTINATION ${AVB_INSTALL_LIB_DIR} )
		install ( DIRECTORY "${AVB_OSAL_DIR}/${INTF_NAME}/" DESTINATION ${AVB_INSTALL_BIN_DIR} FILES_MATCHING PATTERN "*.ini" )
	endmacro()

	add_intf_mod_platform ( "intf_alsa" )
	if (AVB_FEATURE_GSTREAMER)
		add_intf_mod_platform ( "intf_mpeg2ts_gst" )
		add_intf_mod_platform ( "intf_mjpeg_gst" )
		add_intf_mod_platform ( "intf_h264_gst" )
	endif ()
	add_intf_mod_platform ( "intf_mpeg2ts_file" )
	add_intf_mod_platform ( "intf_wav_file" )
endif ()

# API documentation
add_subdirectory ( documents )

if (NOT AVB_FEATURE_AVDECC)
	# SDKS
	add_subdirectory ( sdk )

	# rawsock_rx
	add_executable (rawsock_rx ${AVB_OSAL_DIR}/rawsock/rawsock_rx.c)
	target_link_libraries (rawsock_rx avbTl ${GLIB_PKG_LIBRARIES} pthread rt ${PLATFORM_LINK_LIBRARIES} )
	install ( TARGETS rawsock_rx RUNTIME DESTINATION ${AVB_INSTALL_BIN_DIR} )

	# rawsock_tx
	add_executable (rawsock_tx ${AVB_OSAL_DIR}/rawsock/rawsock_tx.c)
	target_link_libraries (rawsock_tx avbTl ${GLIB_PKG_LIBRARIES} pthread rt ${PLATFORM_LINK_LIBRARIES} )
	install ( TARGETS rawsock_tx RUNTIME DESTINATION ${AVB_INSTALL_BIN_DIR} )
endif ()

# Copy additional installation files
if (AVB_FEATURE_ENDPOINT)
	install ( FILES ${AVB_SRC_DIR}/endpoint/endpoint.ini DESTINATION ${AVB_INSTALL_BIN_DIR} )
	install ( PROGRAMS ${AVB_SRC_DIR}/endpoint/shutdown_openavb_endpoint.sh DESTINATION ${AVB_INSTALL_BIN_DIR} )
endif ()
if (AVB_FEATURE_AVDECC)
	install ( FILES ${AVB_SRC_DIR}/avdecc/avdecc.ini DESTINATION ${AVB_INSTALL_BIN_DIR} )
	install ( PROGRAMS ${AVB_SRC_DIR}/avdecc/shutdown_openavb_avdecc.sh DESTINATION ${AVB_INSTALL_BIN_DIR} )
endif ()
