# and another kernel sources
#set ( LINUX_KERNEL_DIR "/usr/src/kernel" )

# build configuration
set ( OPENAVB_HAL      "x86_i210" )
set ( OPENAVB_OSAL     "Linux" )
set ( OPENAVB_TCAL     "GNU" )
set ( OPENAVB_PLATFORM "${OPENAVB_HAL}-${OPENAVB_OSAL}" )

# Platform Additions
set ( PLATFORM_INCLUDE_DIRECTORIES 	
	${CMAKE_SOURCE_DIR}/platform/x86_i210/include 
	${CMAKE_SOURCE_DIR}/../igb 
	${CMAKE_SOURCE_DIR}/openavb_common
	${CMAKE_SOURCE_DIR}/../../daemons/common
	${CMAKE_SOURCE_DIR}/../../daemons/mrpd
)

set ( PLATFORM_LINK_DIRECTORIES
	${CMAKE_SOURCE_DIR}/../igb
)

set ( PLATFORM_LINK_LIBRARIES
	igb
)


# TODO_OPENAVB : need this?
# Set platform specific define
#set ( PLATFORM_DEFINE "AVB_DELAY_TWEAK_USEC=15" )

set ( GSTREAMER_1_0 0 )
set ( AVB_FEATURE_PCAP 1 )
