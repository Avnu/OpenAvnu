# Rules to build the SDK

############
# Install rules for the AVTP Interface Module SDK
# Header files
install ( FILES ../include/openavb_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../include/openavb_types_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../include/openavb_intf_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../include/openavb_log_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../include/openavb_trace_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../include/openavb_platform_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../mcr/openavb_mcr_hal_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../mediaq/openavb_mediaq_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../avtp/openavb_avtp_time_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../map_mjpeg/openavb_map_mjpeg_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../map_mpeg2ts/openavb_map_mpeg2ts_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../map_null/openavb_map_null_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../map_pipe/openavb_map_pipe_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../intf_ctrl/openavb_intf_ctrl_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../platform/${OPENAVB_OSAL}/openavb_osal_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )
install ( FILES ../platform/${OPENAVB_OSAL}/openavb_time_osal_pub.h DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )

# Sample source files
install ( FILES ../intf_echo/openavb_intf_echo.c DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} )

############
# Install rules for the AVTP Mapping Module SDK
# Header files
install ( FILES ../include/openavb_pub.h DESTINATION ${SDK_INSTALL_SDK_MAP_MOD_DIR} )
install ( FILES ../include/openavb_types_pub.h DESTINATION ${SDK_INSTALL_SDK_MAP_MOD_DIR} )
install ( FILES ../include/openavb_map_pub.h DESTINATION ${SDK_INSTALL_SDK_MAP_MOD_DIR} )
install ( FILES ../include/openavb_log_pub.h DESTINATION ${SDK_INSTALL_SDK_MAP_MOD_DIR} )
install ( FILES ../include/openavb_trace_pub.h DESTINATION ${SDK_INSTALL_SDK_MAP_MOD_DIR} )
install ( FILES ../mcr/openavb_mcr_hal_pub.h DESTINATION ${SDK_INSTALL_SDK_MAP_MOD_DIR} )
install ( FILES ../mediaq/openavb_mediaq_pub.h DESTINATION ${SDK_INSTALL_SDK_MAP_MOD_DIR} )
install ( FILES ../avtp/openavb_avtp_time_pub.h DESTINATION ${SDK_INSTALL_SDK_MAP_MOD_DIR} )
install ( FILES ../map_mjpeg/openavb_map_mjpeg_pub.h DESTINATION ${SDK_INSTALL_SDK_MAP_MOD_DIR} )
install ( FILES ../map_mpeg2ts/openavb_map_mpeg2ts_pub.h DESTINATION ${SDK_INSTALL_SDK_MAP_MOD_DIR} )
install ( FILES ../map_null/openavb_map_null_pub.h DESTINATION ${SDK_INSTALL_SDK_MAP_MOD_DIR} )
install ( FILES ../map_pipe/openavb_map_pipe_pub.h DESTINATION ${SDK_INSTALL_SDK_MAP_MOD_DIR} )

# Sample source files
install ( FILES ../map_null/openavb_map_null.c DESTINATION ${SDK_INSTALL_SDK_MAP_MOD_DIR} )


############
# Install rules for the EAVB SDK
# Header files
install ( FILES ../include/openavb_pub.h DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )
install ( FILES ../include/openavb_types_pub.h DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )
install ( FILES ../include/openavb_log_pub.h DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )
install ( FILES ../include/openavb_trace_pub.h DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )
install ( FILES ../tl/openavb_tl_pub.h DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )
install ( FILES ../mediaq/openavb_mediaq_pub.h DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )
install ( FILES ../platform/${OPENAVB_OSAL}/openavb_osal_pub.h DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )
install ( FILES ../platform/${OPENAVB_OSAL}/openavb_time_osal_pub.h DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )
install ( FILES ../include/openavb_platform_pub.h DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )
install ( FILES ../tl/openavb_tl_pub.h DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )
install ( FILES ../include/openavb_map_pub.h DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )
install ( FILES ../avtp/openavb_avtp_time_pub.h DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )
install ( FILES ../include/openavb_intf_pub.h DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )

# CORE_TODO: The avb host example could be moved to the OSAL.
# Sample source files
install ( FILES ../platform/Linux/avb_host/openavb_host.c DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} )

# Add platform specific headers
if ( EXISTS ${AVB_OSAL_DIR}/sdk/CMakeLists.txt )
    add_subdirectory( ${AVB_OSAL_DIR}/sdk ${CMAKE_BINARY_DIR}/${OPENAVB_OSAL}/sdk )
endif()
