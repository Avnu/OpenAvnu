set ( GSTREAMER_1_0 0 )
set ( AVB_FEATURE_PCAP 1 )
set ( AVB_FEATURE_IGB 0 )
set ( IGB_LAUNCHTIME_ENABLED 0 )

# and another kernel sources
#set ( LINUX_KERNEL_DIR "/usr/src/kernel" )

# build configuration
set ( OPENAVB_HAL      "arm_im6sx" )
set ( OPENAVB_OSAL     "Linux" )
set ( OPENAVB_TCAL     "GNU" )
set ( OPENAVB_PLATFORM "${OPENAVB_HAL}-${OPENAVB_OSAL}" )

# Platform Additions
set ( PLATFORM_INCLUDE_DIRECTORIES
	${CMAKE_SOURCE_DIR}/platform/arm_imx6x/include
	${CMAKE_SOURCE_DIR}/openavb_common
	${CMAKE_SOURCE_DIR}/../../daemons/common
	${CMAKE_SOURCE_DIR}/../../daemons/mrpd
	${CMAKE_SOURCE_DIR}/../../daemons/maap/common
)

# TODO_OPENAVB : need this?
# Set platform specific define
#set ( PLATFORM_DEFINE "AVB_DELAY_TWEAK_USEC=15" )
