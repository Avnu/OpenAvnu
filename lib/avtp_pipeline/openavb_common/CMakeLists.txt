if (AVB_FEATURE_IGB)
	message("-- common IGB library included")
	SET (IGB_FILES
		${AVB_SRC_DIR}/../common/avb_igb.c
	)
endif ()
if (AVB_FEATURE_ATL)
	message("-- common ATL library included")
	SET (ATL_FILES
		${AVB_SRC_DIR}/../common/avb_atl.c
	)
endif ()
SET (SRC_FILES ${SRC_FILES}
	${AVB_SRC_DIR}/../common/avb_gptp.c
	${AVB_SRC_DIR}/openavb_common/mrp_client.c
	${IGB_FILES}
	${ATL_FILES}
	PARENT_SCOPE
)
