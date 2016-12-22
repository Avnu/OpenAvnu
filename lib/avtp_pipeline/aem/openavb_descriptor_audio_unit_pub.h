/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Audio Unit Descriptor Public Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       3-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Pulbic Interface for the Audio Unit Desciptor
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_AUDIO_UNIT_PUB_H
#define OPENAVB_DESCRIPTOR_AUDIO_UNIT_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"

#define OPENAVB_DESCRIPTOR_AUDIO_UNIT_MAX_SAMPLING_RATES (91)

// AUDIO UNIT Descriptor IEEE Std 1722.1-2013 clause 7.2.3
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;											// Set internally
	U16 descriptor_index;											// Set internally
	U8 object_name[OPENAVB_AEM_STRLEN_MAX];
	openavb_aem_string_ref_t localized_description;
	U16 clock_domain_index;
	U16 number_of_stream_input_ports;
	U16 base_stream_input_port;
	U16 number_of_stream_output_ports;
	U16 base_stream_output_port;
	U16 number_of_external_input_ports;
	U16 base_external_input_port;
	U16 number_of_external_output_ports;
	U16 base_external_output_port;
	U16 number_of_internal_input_ports;
	U16 base_internal_input_port;
	U16 number_of_internal_output_ports;
	U16 base_internal_output_port;
	U16 number_of_controls;
	U16 base_control;
	U16 number_of_signal_selectors;
	U16 base_signal_selector;
	U16 number_of_mixers;
	U16 base_mixer;
	U16 number_of_matrices;
	U16 base_matrix;
	U16 number_of_splitters;
	U16 base_splitter;
	U16 number_of_combiners;
	U16 base_combiner;
	U16 number_of_demultiplexers;
	U16 base_demultiplexer;
	U16 number_of_multiplexers;
	U16 base_multiplexer;
	U16 number_of_transcoders;
	U16 base_transcoder;
	U16 number_of_control_blocks;
	U16 base_control_block;
	openavb_aem_sampling_rate_t current_sampling_rate;
	U16 sampling_rates_offset;
	U16 sampling_rates_count;
	openavb_aem_sampling_rate_t sampling_rates[OPENAVB_DESCRIPTOR_AUDIO_UNIT_MAX_SAMPLING_RATES];
} openavb_aem_descriptor_audio_unit_t;

openavb_aem_descriptor_audio_unit_t *openavbAemDescriptorAudioUnitNew(void);

bool openavbAemDescriptorAudioUnitInitialize(openavb_aem_descriptor_audio_unit_t *pDescriptor, U16 nConfigIdx, const openavb_avdecc_configuration_cfg_t *pConfig);

#endif // OPENAVB_DESCRIPTOR_AUDIO_UNIT_PUB_H
