include_directories(
		${AVB_OSAL_DIR}/avdecc
		${AVB_SRC_DIR}/util
		)

# Rules to build the AVB AVDECC
add_executable ( openavb_avdecc openavb_avdecc.c )
target_link_libraries( openavb_avdecc
	avbTl
	${PLATFORM_LINK_LIBRARIES}
	${ALSA_LIBRARIES}
	${GSTRTP_PKG_LIBRARIES}
	${GLIB_PKG_LIBRARIES}
	${GST_PKG_LIBRARIES}
	pthread
	rt
	dl )

# Install rules
install ( TARGETS openavb_avdecc RUNTIME DESTINATION ${AVB_INSTALL_BIN_DIR} )

if (AVB_FEATURE_GSTREAMER)
include_directories( ${GLIB_PKG_INCLUDE_DIRS} ${GST_PKG_INCLUDE_DIRS} )
target_link_libraries( openavb_avdecc ${GLIB_PKG_LIBRARIES} ${GST_PKG_LIBRARIES} ${PLATFORM_LINK_LIBRARIES} )
endif ()
