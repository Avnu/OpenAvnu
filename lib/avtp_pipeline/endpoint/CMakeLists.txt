SET (SRC_FILES ${SRC_FILES}
   ${AVB_SRC_DIR}/endpoint/openavb_endpoint.c
   ${AVB_OSAL_DIR}/endpoint/openavb_endpoint_osal.c
   ${AVB_SRC_DIR}/endpoint/openavb_endpoint_server.c
   ${AVB_OSAL_DIR}/endpoint/openavb_endpoint_cfg.c
   ${AVB_OSAL_DIR}/endpoint/openavb_endpoint_avdecc_cfg.c
   PARENT_SCOPE
)
