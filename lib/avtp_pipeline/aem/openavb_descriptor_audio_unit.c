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
 * MODULE : AEM - AVDECC Audio Unit Descriptor
 * MODULE SUMMARY : Implements the AVDECC Audio Unit IEEE Std 1722.1-2013 clause 7.2.3 
 ******************************************************************
 */

#include <stdlib.h>

#define	AVB_LOG_COMPONENT	"AEM"
#include "openavb_log.h"

#include "openavb_rawsock.h"
#include "openavb_aem.h"
#include "openavb_descriptor_audio_unit.h"


////////////////////////////////
// Private (internal) functions
////////////////////////////////
openavbRC openavbAemDescriptorAudioUnitToBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf, U16 *descriptorSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_audio_unit_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf || !descriptorSize) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_AUDIO_UNIT_BASE_LENGTH + (pDescriptor->sampling_rates_count * OPENAVB_DESCRIPTOR_AUDIO_UNIT_SAMPLING_RATE_LENGTH)) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	*descriptorSize = 0;

	openavb_aem_descriptor_audio_unit_t *pSrc = pDescriptor;
	U8 *pDst = pBuf;

	OCT_D2BHTONS(pDst, pSrc->descriptor_type);
	OCT_D2BHTONS(pDst, pSrc->descriptor_index);
	OCT_D2BMEMCP(pDst, pSrc->object_name);
	BIT_D2BHTONS(pDst, pSrc->localized_description.offset, 3, 0);
	BIT_D2BHTONS(pDst, pSrc->localized_description.index, 0, 2);
	OCT_D2BHTONS(pDst, pSrc->clock_domain_index);
	OCT_D2BHTONS(pDst, pSrc->number_of_stream_input_ports);
	OCT_D2BHTONS(pDst, pSrc->base_stream_input_port);
	OCT_D2BHTONS(pDst, pSrc->number_of_stream_output_ports);
	OCT_D2BHTONS(pDst, pSrc->base_stream_output_port);
	OCT_D2BHTONS(pDst, pSrc->number_of_external_input_ports);
	OCT_D2BHTONS(pDst, pSrc->base_external_input_port);
	OCT_D2BHTONS(pDst, pSrc->number_of_external_output_ports);
	OCT_D2BHTONS(pDst, pSrc->base_external_output_port);
	OCT_D2BHTONS(pDst, pSrc->number_of_internal_input_ports);
	OCT_D2BHTONS(pDst, pSrc->base_internal_input_port);
	OCT_D2BHTONS(pDst, pSrc->number_of_internal_output_ports);
	OCT_D2BHTONS(pDst, pSrc->base_internal_output_port);
	OCT_D2BHTONS(pDst, pSrc->number_of_controls);
	OCT_D2BHTONS(pDst, pSrc->base_control);
	OCT_D2BHTONS(pDst, pSrc->number_of_signal_selectors);
	OCT_D2BHTONS(pDst, pSrc->base_signal_selector);
	OCT_D2BHTONS(pDst, pSrc->number_of_mixers);
	OCT_D2BHTONS(pDst, pSrc->base_mixer);
	OCT_D2BHTONS(pDst, pSrc->number_of_matrices);
	OCT_D2BHTONS(pDst, pSrc->base_matrix);
	OCT_D2BHTONS(pDst, pSrc->number_of_splitters);
	OCT_D2BHTONS(pDst, pSrc->base_splitter);
	OCT_D2BHTONS(pDst, pSrc->number_of_combiners);
	OCT_D2BHTONS(pDst, pSrc->base_combiner);
	OCT_D2BHTONS(pDst, pSrc->number_of_demultiplexers);
	OCT_D2BHTONS(pDst, pSrc->base_demultiplexer);
	OCT_D2BHTONS(pDst, pSrc->number_of_multiplexers);
	OCT_D2BHTONS(pDst, pSrc->base_multiplexer);
	OCT_D2BHTONS(pDst, pSrc->number_of_transcoders);
	OCT_D2BHTONS(pDst, pSrc->base_transcoder);
	OCT_D2BHTONS(pDst, pSrc->number_of_control_blocks);
	OCT_D2BHTONS(pDst, pSrc->base_control_block);
	BIT_D2BHTONL(pDst, pSrc->current_sampling_rate.pull, 29, 0);
	BIT_D2BHTONL(pDst, pSrc->current_sampling_rate.base, 0, 4);
	OCT_D2BHTONS(pDst, pSrc->sampling_rates_offset);
	OCT_D2BHTONS(pDst, pSrc->sampling_rates_count);

	int i1;
	for (i1 = 0; i1 < pSrc->sampling_rates_count; i1++) {
		BIT_D2BHTONL(pDst, pSrc->sampling_rates[i1].pull, 29, 0);
		BIT_D2BHTONL(pDst, pSrc->sampling_rates[i1].base, 0, 4);
	}

	*descriptorSize = pDst - pBuf;

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorAudioUnitFromBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_audio_unit_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_AUDIO_UNIT_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	U8 *pSrc = pBuf;
	openavb_aem_descriptor_audio_unit_t *pDst = pDescriptor;

	OCT_B2DNTOHS(pDst->descriptor_type, pSrc);
	OCT_B2DNTOHS(pDst->descriptor_index, pSrc);
	OCT_B2DMEMCP(pDst->object_name, pSrc);
	BIT_B2DNTOHS(pDst->localized_description.offset, pSrc, 0xfff8, 3, 0);
	BIT_B2DNTOHS(pDst->localized_description.index, pSrc, 0x0007, 0, 2);
	OCT_B2DNTOHS(pDst->clock_domain_index, pSrc);
	OCT_B2DNTOHS(pDst->number_of_stream_input_ports, pSrc);
	OCT_B2DNTOHS(pDst->base_stream_input_port, pSrc);
	OCT_B2DNTOHS(pDst->number_of_stream_output_ports, pSrc);
	OCT_B2DNTOHS(pDst->base_stream_output_port, pSrc);
	OCT_B2DNTOHS(pDst->number_of_external_input_ports, pSrc);
	OCT_B2DNTOHS(pDst->base_external_input_port, pSrc);
	OCT_B2DNTOHS(pDst->number_of_external_output_ports, pSrc);
	OCT_B2DNTOHS(pDst->base_external_output_port, pSrc);
	OCT_B2DNTOHS(pDst->number_of_internal_input_ports, pSrc);
	OCT_B2DNTOHS(pDst->base_internal_input_port, pSrc);
	OCT_B2DNTOHS(pDst->number_of_internal_output_ports, pSrc);
	OCT_B2DNTOHS(pDst->base_internal_output_port, pSrc);
	OCT_B2DNTOHS(pDst->number_of_controls, pSrc);
	OCT_B2DNTOHS(pDst->base_control, pSrc);
	OCT_B2DNTOHS(pDst->number_of_signal_selectors, pSrc);
	OCT_B2DNTOHS(pDst->base_signal_selector, pSrc);
	OCT_B2DNTOHS(pDst->number_of_mixers, pSrc);
	OCT_B2DNTOHS(pDst->base_mixer, pSrc);
	OCT_B2DNTOHS(pDst->number_of_matrices, pSrc);
	OCT_B2DNTOHS(pDst->base_matrix, pSrc);
	OCT_B2DNTOHS(pDst->number_of_splitters, pSrc);
	OCT_B2DNTOHS(pDst->base_splitter, pSrc);
	OCT_B2DNTOHS(pDst->number_of_combiners, pSrc);
	OCT_B2DNTOHS(pDst->base_combiner, pSrc);
	OCT_B2DNTOHS(pDst->number_of_demultiplexers, pSrc);
	OCT_B2DNTOHS(pDst->base_demultiplexer, pSrc);
	OCT_B2DNTOHS(pDst->number_of_multiplexers, pSrc);
	OCT_B2DNTOHS(pDst->base_multiplexer, pSrc);
	OCT_B2DNTOHS(pDst->number_of_transcoders, pSrc);
	OCT_B2DNTOHS(pDst->base_transcoder, pSrc);
	OCT_B2DNTOHS(pDst->number_of_control_blocks, pSrc);
	OCT_B2DNTOHS(pDst->base_control_block, pSrc);
	BIT_B2DNTOHL(pDst->current_sampling_rate.pull, pSrc, 0xe0000000, 29, 0);
	BIT_B2DNTOHL(pDst->current_sampling_rate.base, pSrc, 0x1fffffff, 0, 2);
	OCT_B2DNTOHS(pDst->sampling_rates_offset, pSrc);
	OCT_B2DNTOHS(pDst->sampling_rates_count, pSrc);

	if (bufLength < OPENAVB_DESCRIPTOR_AUDIO_UNIT_BASE_LENGTH + (pDescriptor->sampling_rates_count * OPENAVB_DESCRIPTOR_AUDIO_UNIT_SAMPLING_RATE_LENGTH)) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	int i1;
	for (i1 = 0; i1 < pDst->sampling_rates_count; i1++) {
		BIT_B2DNTOHL(pDst->sampling_rates[i1].pull, pSrc, 0xe0000000, 29, 0);
		BIT_B2DNTOHL(pDst->sampling_rates[i1].base, pSrc, 0x1fffffff, 0, 2);
	}

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorAudioUnitUpdate(void *pVoidDescriptor)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_audio_unit_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	// AVDECC_TODO - Any updates needed?

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT openavb_aem_descriptor_audio_unit_t *openavbAemDescriptorAudioUnitNew()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_audio_unit_t *pDescriptor;

	pDescriptor = malloc(sizeof(*pDescriptor));

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_OUT_OF_MEMORY));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}
	memset(pDescriptor, 0, sizeof(*pDescriptor));

	pDescriptor->descriptorPvtPtr = malloc(sizeof(*pDescriptor->descriptorPvtPtr));
	if (!pDescriptor->descriptorPvtPtr) {
		free(pDescriptor);
		pDescriptor = NULL;
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_OUT_OF_MEMORY));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}

	pDescriptor->descriptorPvtPtr->size = sizeof(openavb_aem_descriptor_audio_unit_t);
	pDescriptor->descriptorPvtPtr->bTopLevel = TRUE;
	pDescriptor->descriptorPvtPtr->toBuf = openavbAemDescriptorAudioUnitToBuf;
	pDescriptor->descriptorPvtPtr->fromBuf = openavbAemDescriptorAudioUnitFromBuf;
	pDescriptor->descriptorPvtPtr->update = openavbAemDescriptorAudioUnitUpdate;

	pDescriptor->descriptor_type = OPENAVB_AEM_DESCRIPTOR_AUDIO_UNIT;
	pDescriptor->sampling_rates_offset = OPENAVB_DESCRIPTOR_AUDIO_UNIT_BASE_LENGTH;

	// Default to no localized strings.
	pDescriptor->localized_description.offset = OPENAVB_AEM_NO_STRING_OFFSET;
	pDescriptor->localized_description.index = OPENAVB_AEM_NO_STRING_INDEX;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pDescriptor;
}

extern DLL_EXPORT bool openavbAemDescriptorAudioUnitInitialize(openavb_aem_descriptor_audio_unit_t *pDescriptor, U16 nConfigIdx, const openavb_avdecc_configuration_cfg_t *pConfig)
{
	(void) nConfigIdx;
	if (!pDescriptor || !pConfig) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	// Specify a name.
	sprintf((char *)(pDescriptor->object_name), "Audio Unit %u", pDescriptor->descriptor_index);

	// Add the port information.
	if (pConfig->stream->role == AVB_ROLE_TALKER) {
		// There will be one stream output port.
		pDescriptor->number_of_stream_output_ports = 1;
		pDescriptor->base_stream_output_port = 0;
	}
	else if (pConfig->stream->role == AVB_ROLE_LISTENER) {
		// There will be one stream input port.
		pDescriptor->number_of_stream_input_ports = 1;
		pDescriptor->base_stream_input_port = 0;
	}
	// AVDECC_TODO - Once implemented, add support for External and Internal ports.

	// AVDECC_TODO - Once implemented, add support for mixers, matrices, splitters, combiners, demultiplexers, multiplexers, transcoders, and control blocks.

	// AVDECC_TODO - Add the sample rate information.  Should this be done in openavbAemDescriptorAudioUnitUpdate() instead?
	// AVDECC_TODO - Merge the configurations instead of overwriting it
	pDescriptor->current_sampling_rate.pull = 0;
	pDescriptor->current_sampling_rate.base = pConfig->stream->current_sampling_rate;
	pDescriptor->sampling_rates_offset = OPENAVB_DESCRIPTOR_AUDIO_UNIT_BASE_LENGTH;
	pDescriptor->sampling_rates_count = pConfig->stream->sampling_rates_count;
	int i = 0;
	while (i < OPENAVB_DESCRIPTOR_AUDIO_UNIT_MAX_SAMPLING_RATES && pConfig->stream->sampling_rates[i] != 0)
	{
		pDescriptor->sampling_rates[i].pull = 0;
		pDescriptor->sampling_rates[i].base = pConfig->stream->sampling_rates[i];
		i++;
	}

	return TRUE;
}
