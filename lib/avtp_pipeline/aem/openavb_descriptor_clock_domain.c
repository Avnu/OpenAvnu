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
 * MODULE : AEM - AVDECC Clock Domain Descriptor
 * MODULE SUMMARY : Implements the AVDECC Clock Domain IEEE Std 1722.1-2013 clause 7.2.32 
 ******************************************************************
 */

#include <stdlib.h>

#define	AVB_LOG_COMPONENT	"AEM"
#include "openavb_log.h"

#include "openavb_rawsock.h"
#include "openavb_aem.h"
#include "openavb_descriptor_clock_domain.h"


////////////////////////////////
// Private (internal) functions
////////////////////////////////
openavbRC openavbAemDescriptorClockDomainToBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf, U16 *descriptorSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_clock_domain_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf || !descriptorSize) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_CLOCK_DOMAIN_BASE_LENGTH + (pDescriptor->clock_sources_count * OPENAVB_DESCRIPTOR_CLOCK_DOMAIN_CLOCK_SOURCE_LENGTH)) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	*descriptorSize = 0;

	openavb_aem_descriptor_clock_domain_t *pSrc = pDescriptor;
	U8 *pDst = pBuf;

	OCT_D2BHTONS(pDst, pSrc->descriptor_type);
	OCT_D2BHTONS(pDst, pSrc->descriptor_index);
	OCT_D2BMEMCP(pDst, pSrc->object_name);
	BIT_D2BHTONS(pDst, pSrc->localized_description.offset, 3, 0);
	BIT_D2BHTONS(pDst, pSrc->localized_description.index, 0, 2);
	OCT_D2BHTONS(pDst, pSrc->clock_source_index);
	OCT_D2BHTONS(pDst, pSrc->clock_sources_offset);
	OCT_D2BHTONS(pDst, pSrc->clock_sources_count);

	int i1;
	for (i1 = 0; i1 < pSrc->clock_sources_count; i1++) {
		OCT_D2BHTONS(pDst, pSrc->clock_sources[i1]);
	}

	*descriptorSize = pDst - pBuf;

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorClockDomainFromBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_clock_domain_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_CLOCK_DOMAIN_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	U8 *pSrc = pBuf;
	openavb_aem_descriptor_clock_domain_t *pDst = pDescriptor;

	OCT_B2DNTOHS(pDst->descriptor_type, pSrc);
	OCT_B2DNTOHS(pDst->descriptor_index, pSrc);
	OCT_B2DMEMCP(pDst->object_name, pSrc);
	BIT_B2DNTOHS(pDst->localized_description.offset, pSrc, 0xfff8, 3, 0);
	BIT_B2DNTOHS(pDst->localized_description.index, pSrc, 0x0007, 0, 2);
	OCT_B2DNTOHS(pDst->clock_source_index, pSrc);
	OCT_B2DNTOHS(pDst->clock_sources_offset, pSrc);
	OCT_B2DNTOHS(pDst->clock_sources_count, pSrc);

	if (bufLength < OPENAVB_DESCRIPTOR_CLOCK_DOMAIN_BASE_LENGTH + (pDescriptor->clock_sources_count * OPENAVB_DESCRIPTOR_CLOCK_DOMAIN_CLOCK_SOURCE_LENGTH)) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	int i1;
	for (i1 = 0; i1 < pDst->clock_sources_count; i1++) {
		OCT_B2DNTOHS(pDst->clock_sources[i1], pSrc);
	}

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorClockDomainUpdate(void *pVoidDescriptor)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_clock_domain_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	// AVDECC_TODO - Any updates needed?

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT openavb_aem_descriptor_clock_domain_t *openavbAemDescriptorClockDomainNew()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_clock_domain_t *pDescriptor;

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

	pDescriptor->descriptorPvtPtr->size = sizeof(openavb_aem_descriptor_clock_domain_t);
	pDescriptor->descriptorPvtPtr->bTopLevel = TRUE;
	pDescriptor->descriptorPvtPtr->toBuf = openavbAemDescriptorClockDomainToBuf;
	pDescriptor->descriptorPvtPtr->fromBuf = openavbAemDescriptorClockDomainFromBuf;
	pDescriptor->descriptorPvtPtr->update = openavbAemDescriptorClockDomainUpdate;

	pDescriptor->descriptor_type = OPENAVB_AEM_DESCRIPTOR_CLOCK_DOMAIN;
	pDescriptor->clock_sources_offset = OPENAVB_DESCRIPTOR_CLOCK_DOMAIN_BASE_LENGTH;

	// Default to no localized strings.
	pDescriptor->localized_description.offset = OPENAVB_AEM_NO_STRING_OFFSET;
	pDescriptor->localized_description.index = OPENAVB_AEM_NO_STRING_INDEX;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pDescriptor;
}

extern DLL_EXPORT bool openavbAemDescriptorClockDomainInitialize(openavb_aem_descriptor_clock_domain_t *pDescriptor, U16 nConfigIdx, const openavb_avdecc_configuration_cfg_t *pConfig)
{
	int i;
	openavb_aem_descriptor_clock_source_t *pClockSourceDescriptor;

	if (!pDescriptor || !pConfig) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	// Specify a name.
	strcpy((char *)(pDescriptor->object_name), "Clock Domain");

	// Add all the current clocks for this configuration to this domain.
	for (i = 0; i < OPENAVB_DESCRIPTOR_CLOCK_DOMAIN_MAX_CLOCK_SOURCES; ++i) {
		pClockSourceDescriptor =
			(openavb_aem_descriptor_clock_source_t *) openavbAemGetDescriptor(nConfigIdx, OPENAVB_AEM_DESCRIPTOR_CLOCK_SOURCE, i);
		if (pClockSourceDescriptor == NULL) { break; }
		pDescriptor->clock_sources[i] = pClockSourceDescriptor->descriptor_index;
		pDescriptor->clock_sources_count = i + 1;
	}

	return TRUE;
}
