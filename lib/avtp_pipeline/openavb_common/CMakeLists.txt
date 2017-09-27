if (AVB_FEATURE_IGB)
	message("-- common IGB library included")
	SET (IGB_FILES
		${AVB_SRC_DIR}/../common/avb_igb.c
	)
endif ()
SET (SRC_FILES ${SRC_FILES}
	${AVB_SRC_DIR}/../common/avb_gptp.c
	${AVB_SRC_DIR}/openavb_common/mrp_client.c
	${IGB_FILES}
	PARENT_SCOPE
)
