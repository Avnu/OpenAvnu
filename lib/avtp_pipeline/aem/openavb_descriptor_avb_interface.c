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
 * MODULE : AEM - AVDECC AVB Interface Descriptor
 * MODULE SUMMARY : Implements the AVDECC AVB Interface IEEE Std 1722.1-2013 clause 7.2.8 
 ******************************************************************
 */

#include "openavb_platform.h"

#include <stdlib.h>

#define	AVB_LOG_COMPONENT	"AEM"
#include "openavb_log.h"

#include "openavb_rawsock.h"
#include "openavb_aem.h"
#include "openavb_descriptor_avb_interface.h"

#ifndef AVB_PTP_AVAILABLE
#include "openavb_grandmaster_osal_pub.h"
#endif // !AVB_PTP_AVAILABLE

extern openavb_avdecc_cfg_t gAvdeccCfg;


////////////////////////////////
// Private (internal) functions
////////////////////////////////
openavbRC openavbAemDescriptorAvbInterfaceToBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf, U16 *descriptorSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_avb_interface_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf || !descriptorSize) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_AVB_INTERFACE_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	*descriptorSize = 0;

	openavb_aem_descriptor_avb_interface_t *pSrc = pDescriptor;
	U8 *pDst = pBuf;

	OCT_D2BHTONS(pDst, pSrc->descriptor_type);
	OCT_D2BHTONS(pDst, pSrc->descriptor_index);
	OCT_D2BMEMCP(pDst, pSrc->object_name);
	BIT_D2BHTONS(pDst, pSrc->localized_description.offset, 3, 0);
	BIT_D2BHTONS(pDst, pSrc->localized_description.index, 0, 2);
	OCT_D2BMEMCP(pDst, pSrc->mac_address);
	OCT_D2BHTONS(pDst, pSrc->interface_flags);
	OCT_D2BMEMCP(pDst, pSrc->clock_identity);
	OCT_D2BHTONB(pDst, pSrc->priority1);
	OCT_D2BHTONB(pDst, pSrc->clock_class);
	OCT_D2BHTONS(pDst, pSrc->offset_scaled_log_variance);
	OCT_D2BHTONB(pDst, pSrc->clock_accuracy);
	OCT_D2BHTONB(pDst, pSrc->priority2);
	OCT_D2BHTONB(pDst, pSrc->domain_number);
	OCT_D2BHTONB(pDst, pSrc->log_sync_interval);
	OCT_D2BHTONB(pDst, pSrc->log_announce_interval);
	OCT_D2BHTONB(pDst, pSrc->log_pdelay_interval);
	OCT_D2BHTONS(pDst, pSrc->port_number);

	*descriptorSize = pDst - pBuf;

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorAvbInterfaceFromBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_avb_interface_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_AVB_INTERFACE_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	U8 *pSrc = pBuf;
	openavb_aem_descriptor_avb_interface_t *pDst = pDescriptor;

	OCT_B2DNTOHS(pDst->descriptor_type, pSrc);
	OCT_B2DNTOHS(pDst->descriptor_index, pSrc);
	OCT_B2DMEMCP(pDst->object_name, pSrc);
	BIT_B2DNTOHS(pDst->localized_description.offset, pSrc, 0xfff8, 3, 0);
	BIT_B2DNTOHS(pDst->localized_description.index, pSrc, 0x0007, 0, 2);
	OCT_B2DMEMCP(pDst->mac_address, pSrc);
	OCT_B2DNTOHS(pDst->interface_flags, pSrc);
	OCT_B2DMEMCP(pDst->clock_identity, pSrc);
	OCT_B2DNTOHB(pDst->priority1, pSrc);
	OCT_B2DNTOHB(pDst->clock_class, pSrc);
	OCT_B2DNTOHS(pDst->offset_scaled_log_variance, pSrc);
	OCT_B2DNTOHB(pDst->clock_accuracy, pSrc);
	OCT_B2DNTOHB(pDst->priority2, pSrc);
	OCT_B2DNTOHB(pDst->domain_number, pSrc);
	OCT_B2DNTOHB(pDst->log_sync_interval, pSrc);
	OCT_B2DNTOHB(pDst->log_announce_interval, pSrc);
	OCT_B2DNTOHB(pDst->log_pdelay_interval, pSrc);
	OCT_B2DNTOHS(pDst->port_number, pSrc);

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorAvbInterfaceUpdate(void *pVoidDescriptor)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_avb_interface_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	// Make the Clock Identity the same as the Entity Id.
	// Just use the first entity descriptor from the first configuration, as they should all be the same.
	openavb_aem_descriptor_entity_t *pEntityDescriptor =
		(openavb_aem_descriptor_entity_t *) openavbAemGetDescriptor(0, OPENAVB_AEM_DESCRIPTOR_ENTITY, 0);
	if (pEntityDescriptor) {
		memcpy(pDescriptor->clock_identity, pEntityDescriptor->entity_id, sizeof(pDescriptor->clock_identity));
	}

#ifndef AVB_PTP_AVAILABLE
	// Get the current grandmaster information.
	if (!osalClockGrandmasterGetInterface(
		pDescriptor->clock_identity,
		&pDescriptor->priority1,
		&pDescriptor->clock_class,
		&pDescriptor->offset_scaled_log_variance,
		&pDescriptor->clock_accuracy,
		&pDescriptor->priority2,
		&pDescriptor->domain_number,
		&pDescriptor->log_sync_interval,
		&pDescriptor->log_announce_interval,
		&pDescriptor->log_pdelay_interval,
		&pDescriptor->port_number))
	{
		AVB_LOG_ERROR("osalClockGrandmasterGetInterface failure");
	}
#endif // !AVB_PTP_AVAILABLE

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT openavb_aem_descriptor_avb_interface_t *openavbAemDescriptorAvbInterfaceNew()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_avb_interface_t *pDescriptor;

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

	pDescriptor->descriptorPvtPtr->size = sizeof(openavb_aem_descriptor_avb_interface_t);
	pDescriptor->descriptorPvtPtr->bTopLevel = TRUE;
	pDescriptor->descriptorPvtPtr->toBuf = openavbAemDescriptorAvbInterfaceToBuf;
	pDescriptor->descriptorPvtPtr->fromBuf = openavbAemDescriptorAvbInterfaceFromBuf;
	pDescriptor->descriptorPvtPtr->update = openavbAemDescriptorAvbInterfaceUpdate;

	strncpy((char *) pDescriptor->object_name, gAvdeccCfg.ifname, sizeof(pDescriptor->object_name));
	memcpy(pDescriptor->mac_address, gAvdeccCfg.ifmac, sizeof(pDescriptor->mac_address));

	pDescriptor->descriptor_type = OPENAVB_AEM_DESCRIPTOR_AVB_INTERFACE;

	// Default to supporting all the interfaces.
	pDescriptor->interface_flags =
		OPENAVB_AEM_INTERFACE_TYPE_GPTP_GRANDMASTER_SUPPORTED |
		OPENAVB_AEM_INTERFACE_TYPE_GPTP_SUPPORTED |
		OPENAVB_AEM_INTERFACE_TYPE_SRP_SUPPORTED;

	// Default to no localized strings.
	pDescriptor->localized_description.offset = OPENAVB_AEM_NO_STRING_OFFSET;
	pDescriptor->localized_description.index = OPENAVB_AEM_NO_STRING_INDEX;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pDescriptor;
}

extern DLL_EXPORT bool openavbAemDescriptorAvbInterfaceInitialize(openavb_aem_descriptor_avb_interface_t *pDescriptor, U16 nConfigIdx, const openavb_avdecc_configuration_cfg_t *pConfig)
{
	(void) nConfigIdx;
	if (!pDescriptor || !pConfig) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	// AVDECC_TODO - Any updates needed?

	return TRUE;
}
