/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Audio Unit Descriptor
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       21-Nov-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Implements the AVDECC Audio Unit IEEE Std 1722.1-2013 clause 7.2.3 
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
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
	BIT_D2BHTONS(pDst, pSrc->current_sampling_rate.pull, 29, 0);
	BIT_D2BHTONS(pDst, pSrc->current_sampling_rate.base, 0, 4);
	OCT_D2BHTONS(pDst, pSrc->sampling_rates_offset);
	OCT_D2BHTONS(pDst, pSrc->sampling_rates_count);

	int i1;
	for (i1 = 0; i1 < pSrc->sampling_rates_count; i1++) {
		BIT_D2BHTONS(pDst, pSrc->sampling_rates[i1].pull, 29, 0);
		BIT_D2BHTONS(pDst, pSrc->sampling_rates[i1].base, 0, 4);
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
	BIT_B2DNTOHS(pDst->current_sampling_rate.pull, pSrc, 0xe0000000, 29, 0);
	BIT_B2DNTOHS(pDst->current_sampling_rate.base, pSrc, 0x1fffffff, 0, 2);
	OCT_B2DNTOHS(pDst->sampling_rates_offset, pSrc);
	OCT_B2DNTOHS(pDst->sampling_rates_count, pSrc);

	if (bufLength < OPENAVB_DESCRIPTOR_AUDIO_UNIT_BASE_LENGTH + (pDescriptor->sampling_rates_count * OPENAVB_DESCRIPTOR_AUDIO_UNIT_SAMPLING_RATE_LENGTH)) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	int i1;
	for (i1 = 0; i1 < pDst->sampling_rates_count; i1++) {
		BIT_B2DNTOHS(pDst->sampling_rates[i1].pull, pSrc, 0xe0000000, 29, 0);
		BIT_B2DNTOHS(pDst->sampling_rates[i1].base, pSrc, 0x1fffffff, 0, 2);
	}

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

	pDescriptor->descriptor_type = OPENAVB_AEM_DESCRIPTOR_AUDIO_UNIT;
	pDescriptor->sampling_rates_offset = OPENAVB_DESCRIPTOR_AUDIO_UNIT_BASE_LENGTH;

	// Default to no localized strings.
	pDescriptor->localized_description.offset = OPENAVB_AEM_NO_STRING_OFFSET;
	pDescriptor->localized_description.index = OPENAVB_AEM_NO_STRING_INDEX;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pDescriptor;
}

