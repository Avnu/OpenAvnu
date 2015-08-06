IF (NOT GSTREAMER_1_0)
    SET (SRC_FILES ${SRC_FILES}
	    ${AVB_OSAL_DIR}/intf_mjpeg_gst/openavb_intf_mjpeg_gst.c
        ${AVB_OSAL_DIR}/gst_al/gst_al_01.c
        PARENT_SCOPE
    )
ELSE ()
    SET (SRC_FILES ${SRC_FILES}
	    ${AVB_OSAL_DIR}/intf_mjpeg_gst/openavb_intf_mjpeg_gst.c
        ${AVB_OSAL_DIR}/gst_al/gst_al_10.c
        PARENT_SCOPE
    )
ENDIF ()

# Need include and link directories for GST
SET (INTF_INCLUDE_DIR 
        ${INTF_INCLUDE_DIR} ${GLIB_PKG_INCLUDE_DIRS} ${GST_PKG_INCLUDE_DIRS} 
        ${AVB_OSAL_DIR}/gst_al
        PARENT_SCOPE
)
SET (INTF_LIBRARY_DIR ${INTF_LIBRARY_DIR} PARENT_SCOPE)
SET (INTF_LIBRARY ${GLIB_PKG_LIBRARIES} ${GST_PKG_LIBRARIES} ${GSTRTP_PKG_LIBRARIES} PARENT_SCOPE)

