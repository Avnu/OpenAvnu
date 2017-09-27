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
 * MODULE : AEM - AVDECC Entity Descriptor
 * MODULE SUMMARY : Implements the AVDECC Entity Descriptor IEEE Std 1722.1-2013 clause 7.2.1 
 ******************************************************************
 */

#include "openavb_platform.h"

#include <stdlib.h>

#define	AVB_LOG_COMPONENT	"AEM"
#include "openavb_log.h"

#include "openavb_rawsock.h"
#include "openavb_aem.h"
#include "openavb_descriptor_entity.h"

extern openavb_avdecc_cfg_t gAvdeccCfg;


////////////////////////////////
// Private (internal) functions
////////////////////////////////
openavbRC openavbAemDescriptorEntityToBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf, U16 *descriptorSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_entity_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf || !descriptorSize) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_ENTITY_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	*descriptorSize = 0;

	openavb_aem_descriptor_entity_t *pSrc = pDescriptor;
	U8 *pDst = pBuf;

	OCT_D2BHTONS(pDst, pSrc->descriptor_type);
	OCT_D2BHTONS(pDst, pSrc->descriptor_index);
	OCT_D2BMEMCP(pDst, pSrc->entity_id);
	OCT_D2BMEMCP(pDst, pSrc->entity_model_id);
	OCT_D2BHTONL(pDst, pSrc->entity_capabilities);
	OCT_D2BHTONS(pDst, pSrc->talker_stream_sources);
	OCT_D2BHTONS(pDst, pSrc->talker_capabilities);
	OCT_D2BHTONS(pDst, pSrc->listener_stream_sinks);
	OCT_D2BHTONS(pDst, pSrc->listener_capabilities);
	OCT_D2BHTONL(pDst, pSrc->controller_capabilities);
	OCT_D2BHTONL(pDst, pSrc->available_index);
	OCT_D2BMEMCP(pDst, pSrc->association_id);
	OCT_D2BMEMCP(pDst, pSrc->entity_name);
	BIT_D2BHTONS(pDst, pSrc->vendor_name_string.offset, 3, 0);
	BIT_D2BHTONS(pDst, pSrc->vendor_name_string.index, 0, 2);
	BIT_D2BHTONS(pDst, pSrc->model_name_string.offset, 3, 0);
	BIT_D2BHTONS(pDst, pSrc->model_name_string.index, 0, 2);
	OCT_D2BMEMCP(pDst, pSrc->firmware_version);
	OCT_D2BMEMCP(pDst, pSrc->group_name);
	OCT_D2BMEMCP(pDst, pSrc->serial_number);
	OCT_D2BHTONS(pDst, pSrc->configurations_count);
	OCT_D2BHTONS(pDst, pSrc->current_configuration);

	*descriptorSize = pDst - pBuf;

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorEntityFromBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_entity_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_ENTITY_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	U8 *pSrc = pBuf;
	openavb_aem_descriptor_entity_t *pDst = pDescriptor;

	OCT_B2DNTOHS(pDst->descriptor_type, pSrc);
	OCT_B2DNTOHS(pDst->descriptor_index, pSrc);
	OCT_B2DMEMCP(pDst->entity_id, pSrc);
	OCT_B2DMEMCP(pDst->entity_model_id, pSrc);
	OCT_B2DNTOHL(pDst->entity_capabilities, pSrc);
	OCT_B2DNTOHS(pDst->talker_stream_sources, pSrc);
	OCT_B2DNTOHS(pDst->talker_capabilities, pSrc);
	OCT_B2DNTOHS(pDst->listener_stream_sinks, pSrc);
	OCT_B2DNTOHS(pDst->listener_capabilities, pSrc);
	OCT_B2DNTOHL(pDst->controller_capabilities, pSrc);
	OCT_B2DNTOHL(pDst->available_index, pSrc);
	OCT_B2DMEMCP(pDst->association_id, pSrc);
	OCT_B2DMEMCP(pDst->entity_name, pSrc);
	BIT_B2DNTOHS(pDst->vendor_name_string.offset, pSrc, 0xfff8, 3, 0);
	BIT_B2DNTOHS(pDst->vendor_name_string.index, pSrc, 0x0007, 0, 2);
	BIT_B2DNTOHS(pDst->model_name_string.offset, pSrc, 0xfff8, 3, 0);
	BIT_B2DNTOHS(pDst->model_name_string.index, pSrc, 0x0007, 0, 2);
	OCT_B2DMEMCP(pDst->firmware_version, pSrc);
	OCT_B2DMEMCP(pDst->group_name, pSrc);
	OCT_B2DMEMCP(pDst->serial_number, pSrc);
	OCT_B2DNTOHS(pDst->configurations_count, pSrc);
	OCT_B2DNTOHS(pDst->current_configuration, pSrc);

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorEntityUpdate(void *pVoidDescriptor)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_entity_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	// AVDECC_TODO - Any updates needed?

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT openavb_aem_descriptor_entity_t *openavbAemDescriptorEntityNew()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_entity_t *pDescriptor;

	pDescriptor = malloc(sizeof(*pDescriptor));

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_OUT_OF_MEMORY));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}
	memset(pDescriptor, 0, sizeof(openavb_aem_descriptor_entity_t));

	pDescriptor->descriptorPvtPtr = malloc(sizeof(*pDescriptor->descriptorPvtPtr));
	if (!pDescriptor->descriptorPvtPtr) {
		free(pDescriptor);
		pDescriptor = NULL;
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_OUT_OF_MEMORY));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}
	memset(pDescriptor->descriptorPvtPtr, 0, sizeof(*pDescriptor->descriptorPvtPtr));

	pDescriptor->descriptorPvtPtr->size = sizeof(openavb_aem_descriptor_entity_t);
	pDescriptor->descriptorPvtPtr->bTopLevel = TRUE;
	pDescriptor->descriptorPvtPtr->toBuf = openavbAemDescriptorEntityToBuf;
	pDescriptor->descriptorPvtPtr->fromBuf = openavbAemDescriptorEntityFromBuf;
	pDescriptor->descriptorPvtPtr->update = openavbAemDescriptorEntityUpdate;

	pDescriptor->descriptor_type = OPENAVB_AEM_DESCRIPTOR_ENTITY;
	pDescriptor->descriptor_index = 0;

	// Default to no localized strings.
	pDescriptor->vendor_name_string.offset = OPENAVB_AEM_NO_STRING_OFFSET;
	pDescriptor->vendor_name_string.index = OPENAVB_AEM_NO_STRING_INDEX;
	pDescriptor->model_name_string.offset = OPENAVB_AEM_NO_STRING_OFFSET;
	pDescriptor->model_name_string.index = OPENAVB_AEM_NO_STRING_INDEX;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pDescriptor;
}

extern DLL_EXPORT bool openavbAemDescriptorEntitySet_entity_id(openavb_aem_descriptor_entity_t *pDescriptor, char *aMacAddr, U8 *nMacAddr, U16 id)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	struct ether_addr macAddrBuffer;
	if (aMacAddr) {
		// If an ascii mac address is passed in parse and use it.
		memset(macAddrBuffer.ether_addr_octet, 0, sizeof(macAddrBuffer));

		if (!ether_aton_r(aMacAddr, &macAddrBuffer)) {
			AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_PARSING_MAC_ADDRESS));
			AVB_TRACE_EXIT(AVB_TRACE_AEM);
			return FALSE;
		}
		memcpy(pDescriptor->entity_id, macAddrBuffer.ether_addr_octet, 3);
		memcpy(pDescriptor->entity_id + 3 + 2, macAddrBuffer.ether_addr_octet + 3, 3);
	}
	else if (nMacAddr) {
		// If a network mac address is passed in use it.
		memcpy(pDescriptor->entity_id, nMacAddr, 3);
		memcpy(pDescriptor->entity_id + 3 + 2, nMacAddr + 3, 3);
	}
	else {
		// If no mac address is passed in obtain it from the stack.
		memcpy(pDescriptor->entity_id, gAvdeccCfg.ifmac, 3);
		memcpy(pDescriptor->entity_id + 3 + 2, gAvdeccCfg.ifmac + 3, 3);
	}

	U16 tmpU16 = htons(id);
	memcpy(pDescriptor->entity_id + 3, &tmpU16, sizeof(tmpU16));

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

extern DLL_EXPORT bool openavbAemDescriptorEntitySet_entity_model_id(openavb_aem_descriptor_entity_t *pDescriptor, U8 pData[8])
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor || !pData) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	memcpy(pDescriptor->entity_model_id, pData, 8);

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

extern DLL_EXPORT bool openavbAemDescriptorEntitySet_entity_capabilities(openavb_aem_descriptor_entity_t *pDescriptor, U32 capabilities)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	pDescriptor->entity_capabilities = capabilities;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

extern DLL_EXPORT bool openavbAemDescriptorEntitySet_talker_capabilities(openavb_aem_descriptor_entity_t *pDescriptor, U16 num_sources, U16 capabilities)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	pDescriptor->talker_stream_sources = num_sources;
	pDescriptor->talker_capabilities = capabilities;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

extern DLL_EXPORT bool openavbAemDescriptorEntitySet_listener_capabilities(openavb_aem_descriptor_entity_t *pDescriptor, U16 num_sinks, U16 capabilities)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	pDescriptor->listener_stream_sinks = num_sinks;
	pDescriptor->listener_capabilities = capabilities;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

extern DLL_EXPORT bool openavbAemDescriptorEntitySet_entity_name(openavb_aem_descriptor_entity_t *pDescriptor, char *aName)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor || !aName) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	openavbAemSetString(pDescriptor->entity_name, aName);

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

extern DLL_EXPORT bool openavbAemDescriptorEntitySet_vendor_name(openavb_aem_descriptor_entity_t *pDescriptor, U16 nOffset, U8 nIndex)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	pDescriptor->vendor_name_string.offset = nOffset;
	pDescriptor->vendor_name_string.index = nIndex;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

extern DLL_EXPORT bool openavbAemDescriptorEntitySet_model_name(openavb_aem_descriptor_entity_t *pDescriptor, U16 nOffset, U8 nIndex)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	pDescriptor->model_name_string.offset = nOffset;
	pDescriptor->model_name_string.index = nIndex;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

extern DLL_EXPORT bool openavbAemDescriptorEntitySet_firmware_version(openavb_aem_descriptor_entity_t *pDescriptor, char *aFirmwareVersion)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor || !aFirmwareVersion) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	openavbAemSetString(pDescriptor->firmware_version, aFirmwareVersion);

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

extern DLL_EXPORT bool openavbAemDescriptorEntitySet_group_name(openavb_aem_descriptor_entity_t *pDescriptor, char *aGroupName)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor || !aGroupName) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	openavbAemSetString(pDescriptor->group_name, aGroupName);

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

extern DLL_EXPORT bool openavbAemDescriptorEntitySet_serial_number(openavb_aem_descriptor_entity_t *pDescriptor, char *aSerialNumber)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor || !aSerialNumber) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	openavbAemSetString(pDescriptor->serial_number, aSerialNumber);

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}
