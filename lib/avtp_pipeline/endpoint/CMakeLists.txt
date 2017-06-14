SET (SRC_FILES ${SRC_FILES}
   ${AVB_SRC_DIR}/endpoint/openavb_endpoint.c
   ${AVB_OSAL_DIR}/endpoint/openavb_endpoint_osal.c
   ${AVB_OSAL_DIR}/endpoint/openavb_endpoint_osal_maap.c
   ${AVB_OSAL_DIR}/endpoint/openavb_endpoint_osal_ptp.c
   ${AVB_OSAL_DIR}/endpoint/openavb_endpoint_osal_shaper.c
   ${AVB_OSAL_DIR}/endpoint/openavb_endpoint_osal_srp.c
   ${AVB_SRC_DIR}/endpoint/openavb_endpoint_server.c
   ${AVB_OSAL_DIR}/endpoint/openavb_endpoint_cfg.c
   PARENT_SCOPE
)
