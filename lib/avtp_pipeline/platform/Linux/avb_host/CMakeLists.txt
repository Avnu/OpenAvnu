include_directories(
		${AVB_OSAL_DIR}/tl
		${AVB_OSAL_DIR}/endpoint
		${AVB_SRC_DIR}/util
		${AVB_SRC_DIR}/tl
		${AVB_SRC_DIR}/srp
		)

# Rules to build the AVB host
add_executable ( openavb_host openavb_host.c )
target_link_libraries( openavb_host
	map_ctrl
	map_mjpeg
	map_mpeg2ts
	map_null
	map_pipe
	map_aaf_audio 
	map_uncmp_audio 
	map_h264 
	intf_ctrl
	intf_echo
	intf_logger
	intf_null
	intf_tonegen
	intf_viewer
	intf_alsa
	intf_mpeg2ts_gst
	intf_mjpeg_gst
	intf_mpeg2ts_file
	intf_wav_file
	intf_h264_gst
	avbTl
	${PLATFORM_LINK_LIBRARIES}
	${ALSA_LIBRARIES}
	${GSTRTP_PKG_LIBRARIES}
	${GLIB_PKG_LIBRARIES}
	${GST_PKG_LIBRARIES}
	pthread 
	rt 
	dl 
	pci )


# Rules to build the AVB harness
add_executable ( openavb_harness openavb_harness.c )
target_link_libraries( openavb_harness 
	map_ctrl
	map_mjpeg
	map_mpeg2ts
	map_null
	map_pipe
	map_aaf_audio 
	map_uncmp_audio 
	map_h264 
	intf_ctrl
	intf_echo
	intf_logger
	intf_null
	intf_tonegen
	intf_viewer
	intf_alsa
	intf_mpeg2ts_gst
	intf_mjpeg_gst
	intf_mpeg2ts_file
	intf_wav_file
	intf_h264_gst
	avbTl
	${PLATFORM_LINK_LIBRARIES}
	${ALSA_LIBRARIES}
	${GSTRTP_PKG_LIBRARIES}
	${GLIB_PKG_LIBRARIES}
	${GST_PKG_LIBRARIES}
	pthread 
	rt 
	dl 
	pci )

# Install rules 
install ( TARGETS openavb_host RUNTIME DESTINATION ${AVB_INSTALL_BIN_DIR} )
install ( TARGETS openavb_harness RUNTIME DESTINATION ${AVB_INSTALL_BIN_DIR} )

if (AVB_FEATURE_GSTREAMER)
include_directories( ${GLIB_PKG_INCLUDE_DIRS} ${GST_PKG_INCLUDE_DIRS} )
target_link_libraries( openavb_host ${GLIB_PKG_LIBRARIES} ${GST_PKG_LIBRARIES} ${PLATFORM_LINK_LIBRARIES} )
target_link_libraries( openavb_harness ${GLIB_PKG_LIBRARIES} ${GST_PKG_LIBRARIES} ${PLATFORM_LINK_LIBRARIES} )
endif ()
