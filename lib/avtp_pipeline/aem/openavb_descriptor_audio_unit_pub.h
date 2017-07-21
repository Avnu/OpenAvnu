/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
Copyright (c) 2016-2017, Harman International Industries, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Attributions: The inih library portion of the source code is licensed from
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt.
Complete license and copyright information can be found at
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

/*
 ******************************************************************
 * MODULE : AEM - AVDECC Audio Unit Descriptor Public Interface
 * MODULE SUMMARY : Public Interface for the Audio Unit Descriptor
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
