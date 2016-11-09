/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Audio Map Descriptor
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       26-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Implements the AVDECC Audio Map IEEE Std 1722.1-2013 clause 7.2.19 
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
#include "openavb_descriptor_audio_map.h"


////////////////////////////////
// Private (internal) functions
////////////////////////////////
openavbRC openavbAemDescriptorAudioMapToBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf, U16 *descriptorSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_audio_map_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf || !descriptorSize) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_AUDIO_MAP_BASE_LENGTH + (pDescriptor->number_of_mappings * OPENAVB_DESCRIPTOR_AUDIO_MAP_AUDIO_MAPPING_FORMAT_LENGTH)) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	*descriptorSize = 0;

	openavb_aem_descriptor_audio_map_t *pSrc = pDescriptor;
	U8 *pDst = pBuf;

	OCT_D2BHTONS(pDst, pSrc->descriptor_type);
	OCT_D2BHTONS(pDst, pSrc->descriptor_index);
	OCT_D2BHTONS(pDst, pSrc->mappings_offset);
	OCT_D2BHTONS(pDst, pSrc->number_of_mappings);

	int i1;
	for (i1 = 0; i1 < pSrc->number_of_mappings; i1++) {
		OCT_D2BHTONS(pDst, pSrc->mapping_formats[i1].mapping_stream_index);
		OCT_D2BHTONS(pDst, pSrc->mapping_formats[i1].mapping_stream_channel);
		OCT_D2BHTONS(pDst, pSrc->mapping_formats[i1].mapping_cluster_offset);
		OCT_D2BHTONS(pDst, pSrc->mapping_formats[i1].mapping_cluster_channel);
	}

	*descriptorSize = pDst - pBuf;

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorAudioMapFromBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_audio_map_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_AUDIO_MAP_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	U8 *pSrc = pBuf;
	openavb_aem_descriptor_audio_map_t *pDst = pDescriptor;

	OCT_B2DNTOHS(pDst->descriptor_type, pSrc);
	OCT_B2DNTOHS(pDst->descriptor_index, pSrc);
	OCT_B2DNTOHS(pDst->mappings_offset, pSrc);
	OCT_B2DNTOHS(pDst->number_of_mappings, pSrc);

	if (bufLength < OPENAVB_DESCRIPTOR_AUDIO_MAP_BASE_LENGTH + (pDescriptor->number_of_mappings * OPENAVB_DESCRIPTOR_AUDIO_MAP_AUDIO_MAPPING_FORMAT_LENGTH)) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	int i1;
	for (i1 = 0; i1 < pDst->number_of_mappings; i1++) {
		OCT_B2DNTOHS(pDst->mapping_formats[i1].mapping_stream_index, pSrc);
		OCT_B2DNTOHS(pDst->mapping_formats[i1].mapping_stream_channel, pSrc);
		OCT_B2DNTOHS(pDst->mapping_formats[i1].mapping_cluster_offset, pSrc);
		OCT_B2DNTOHS(pDst->mapping_formats[i1].mapping_cluster_channel, pSrc);
	}

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT openavb_aem_descriptor_audio_map_t *openavbAemDescriptorAudioMapNew()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_audio_map_t *pDescriptor;

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

	pDescriptor->descriptorPvtPtr->size = sizeof(openavb_aem_descriptor_audio_map_t);
	pDescriptor->descriptorPvtPtr->bTopLevel = FALSE;
	pDescriptor->descriptorPvtPtr->toBuf = openavbAemDescriptorAudioMapToBuf;
	pDescriptor->descriptorPvtPtr->fromBuf = openavbAemDescriptorAudioMapFromBuf;

	pDescriptor->descriptor_type = OPENAVB_AEM_DESCRIPTOR_AUDIO_MAP;
	pDescriptor->mappings_offset = OPENAVB_DESCRIPTOR_AUDIO_MAP_BASE_LENGTH;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pDescriptor;
}

