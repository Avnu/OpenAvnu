SET (SRC_FILES ${SRC_FILES}
	${AVB_SRC_DIR}/aem/openavb_aem.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_entity.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_configuration.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_audio_unit.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_locale.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_strings.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_stream_io.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_jack_io.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_avb_interface.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_clock_source.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_stream_port_io.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_external_port_io.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_audio_cluster.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_audio_map.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_control.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_clock_domain.c
	${AVB_SRC_DIR}/aem/openavb_descriptor_locale_strings_handler.c
	PARENT_SCOPE
)

