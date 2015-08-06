# generic build settings
# just builds linux version of stack on host machine

# Label for messages / build configuration
set ( OPENAVB_HAL      "generic" )
set ( OPENAVB_OSAL     "Linux" )
set ( OPENAVB_TCAL     "GNU" )
set ( OPENAVB_PLATFORM "${OPENAVB_HAL}-${OPENAVB_OSAL}" )

# point to our "proxy" linux/ptp_clock.h include file
# which includes /usr/include/linux/ptp_clock.h
# and adding missing defines just to make everything compile
include_directories ( platform/generic/include )

set ( PLATFORM_DEFINE "AVB_DELAY_TWEAK_USEC=45" )

set ( GSTREAMER_1_0 1 )
