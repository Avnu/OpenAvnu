SET (SRC_FILES_TL 
	${AVB_SRC_DIR}/tl/openavb_tl.c
	${AVB_OSAL_DIR}/tl/openavb_tl_osal.c
	${AVB_SRC_DIR}/tl/openavb_listener.c
	${AVB_SRC_DIR}/tl/openavb_talker.c
	)

if(AVB_FEATURE_ENDPOINT)
	#Additional Files for Endpoint
	MESSAGE ("-- TL with Endpoint")
	SET (SRC_FILES_TL_EXTRA 
		${AVB_SRC_DIR}/endpoint/openavb_endpoint_client.c
		${AVB_SRC_DIR}/tl/openavb_tl_endpoint.c
		${AVB_SRC_DIR}/tl/openavb_talker_endpoint.c
		${AVB_SRC_DIR}/tl/openavb_listener_endpoint.c
	    )
else()
	#Additional Files for No Endpoint
	MESSAGE ("-- TL without Endpoint")
	SET (SRC_FILES_TL_EXTRA
		${AVB_SRC_DIR}/tl/openavb_tl_no_endpoint.c
		${AVB_SRC_DIR}/tl/openavb_talker_no_endpoint.c
		${AVB_SRC_DIR}/tl/openavb_listener_no_endpoint.c
	    )
endif()

SET ( SRC_FILES ${SRC_FILES} ${SRC_FILES_TL} ${SRC_FILES_TL_EXTRA} PARENT_SCOPE)


