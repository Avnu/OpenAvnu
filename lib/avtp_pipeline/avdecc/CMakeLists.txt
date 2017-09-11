SET (SRC_FILES ${SRC_FILES}
	${AVB_SRC_DIR}/avdecc/openavb_avdecc.c
	${AVB_OSAL_DIR}/avdecc/openavb_avdecc_osal.c
	${AVB_OSAL_DIR}/avdecc/openavb_avdecc_cfg.c
	${AVB_OSAL_DIR}/avdecc/openavb_avdecc_read_ini.c
	${AVB_OSAL_DIR}/avdecc/openavb_avdecc_pipeline_interaction.c
	${AVB_OSAL_DIR}/avdecc/openavb_avdecc_save_state.c
	${AVB_OSAL_DIR}/openavb_osal_avdecc.c
	${AVB_OSAL_DIR}/openavb_grandmaster_osal.c
	${AVB_SRC_DIR}/avdecc_msg/openavb_avdecc_msg_server.c
	PARENT_SCOPE
)
