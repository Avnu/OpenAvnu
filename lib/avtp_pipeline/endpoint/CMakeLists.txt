include_directories(
	 ${AVB_OSAL_DIR}/endpoint
	 ${AVB_OSAL_DIR}/maap
	 ${AVB_OSAL_DIR}/tl
	 ${AVB_SRC_DIR}/maap
	 ${AVB_SRC_DIR}/endpoint
	 ${AVB_SRC_DIR}/srp
	 ${AVB_SRC_DIR}/tl
	 ${AVB_OSAL_DIR}/fqtss/qmgr
	 ${AVB_OSAL_DIR}/srp
	 )

# File sets are curreintly the same for AVB_FEATURE_ENDPOINT or not. Needs code changes to properly allow linux to run with and without SRP/MAPP
if (AVB_FEATURE_ENDPOINT)
	SET (SRC_FILES ${SRC_FILES}
	   ${AVB_SRC_DIR}/endpoint/openavb_endpoint.c 
	   ${AVB_OSAL_DIR}/endpoint/openavb_endpoint_osal.c 
	   ${AVB_SRC_DIR}/endpoint/openavb_endpoint_server.c
	   ${AVB_OSAL_DIR}/endpoint/openavb_endpoint_cfg.c
	   PARENT_SCOPE
	)
else(AVB_FEATURE_ENDPOINT)
	SET (SRC_FILES ${SRC_FILES}
	   ${AVB_SRC_DIR}/endpoint/openavb_endpoint.c 
	   ${AVB_OSAL_DIR}/endpoint/openavb_endpoint_osal.c 
	   ${AVB_SRC_DIR}/endpoint/openavb_endpoint_server.c
	   ${AVB_OSAL_DIR}/endpoint/openavb_endpoint_cfg.c
	   PARENT_SCOPE
	)
endif (AVB_FEATURE_ENDPOINT)

# If FQTSS is enabled...
if ( AVB_FEATURE_FQTSS )
   # set define for ifdefs
   set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_FQTSS=1" )
   # and add library to link list
   set ( AVB_ENDPOINT_LIBRARIES "${AVB_ENDPOINT_LIBRARIES}" qmgr )
endif ( AVB_FEATURE_FQTSS )

