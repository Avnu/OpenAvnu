if (AVB_FEATURE_PCAP)
	message("-- Rawsock PCAP enabled")
	SET (PCAP_FILES
		${AVB_OSAL_DIR}/rawsock/pcap_rawsock.c
	)
	if (AVB_FEATURE_IGB)
		message("-- Rawsock IGB enabled")
		SET (IGB_FILES
			${AVB_OSAL_DIR}/rawsock/igb_rawsock.c
			${AVB_HAL_DIR}/openavb_igb.c
		)
	endif ()
	if (AVB_FEATURE_ATL)
		message("-- Rawsock ATL enabled")
		SET (ATL_FILES
			${AVB_OSAL_DIR}/rawsock/atl_rawsock.c
			${AVB_HAL_DIR}/openavb_atl.c
		)
	endif ()
endif ()
SET (SRC_FILES ${SRC_FILES}
	${AVB_SRC_DIR}/rawsock/rawsock_impl.c
	${AVB_OSAL_DIR}/rawsock/openavb_rawsock.c
	${AVB_OSAL_DIR}/rawsock/simple_rawsock.c
	${AVB_OSAL_DIR}/rawsock/ring_rawsock.c
	${AVB_OSAL_DIR}/rawsock/sendmmsg_rawsock.c
	${PCAP_FILES}
	${IGB_FILES}
	${ATL_FILES}
	PARENT_SCOPE
)
