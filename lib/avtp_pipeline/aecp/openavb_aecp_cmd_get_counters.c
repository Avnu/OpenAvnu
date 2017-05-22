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

#include "openavb_platform.h"

#include <errno.h>

#define	AVB_LOG_COMPONENT	"AECP"
#include "openavb_log.h"

#include "openavb_aem.h"
#include "openavb_aecp.h"
#include "openavb_aecp_message.h"
#include "openavb_aecp_cmd_get_counters.h"
#include "openavb_avdecc_pipeline_interaction_pub.h"


static void GetEntitySpecificCounters(void * pDescriptor, U16 descriptorType, openavb_aecp_response_data_get_counters_t *pRsp)
{
	U32 retValue;

	// Code assumes that ENTITY_SPECIFIC flags and offsets are the same regardless of the Descriptor Type.
	if (openavbAVDECCGetCounterValue(pDescriptor, descriptorType, OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_1, &retValue)) {
		pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_1;
		*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_OFFSET_ENTITY_SPECIFIC_1]) = htonl(retValue);
	}
	if (openavbAVDECCGetCounterValue(pDescriptor, descriptorType, OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_2, &retValue)) {
		pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_2;
		*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_OFFSET_ENTITY_SPECIFIC_2]) = htonl(retValue);
	}
	if (openavbAVDECCGetCounterValue(pDescriptor, descriptorType, OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_3, &retValue)) {
		pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_3;
		*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_OFFSET_ENTITY_SPECIFIC_3]) = htonl(retValue);
	}
	if (openavbAVDECCGetCounterValue(pDescriptor, descriptorType, OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_4, &retValue)) {
		pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_4;
		*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_OFFSET_ENTITY_SPECIFIC_4]) = htonl(retValue);
	}
	if (openavbAVDECCGetCounterValue(pDescriptor, descriptorType, OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_5, &retValue)) {
		pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_5;
		*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_OFFSET_ENTITY_SPECIFIC_5]) = htonl(retValue);
	}
	if (openavbAVDECCGetCounterValue(pDescriptor, descriptorType, OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_6, &retValue)) {
		pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_6;
		*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_OFFSET_ENTITY_SPECIFIC_6]) = htonl(retValue);
	}
	if (openavbAVDECCGetCounterValue(pDescriptor, descriptorType, OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_7, &retValue)) {
		pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_7;
		*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_OFFSET_ENTITY_SPECIFIC_7]) = htonl(retValue);
	}
	if (openavbAVDECCGetCounterValue(pDescriptor, descriptorType, OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_8, &retValue)) {
		pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_COUNTER_ENTITY_SPECIFIC_8;
		*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_ENTITY_OFFSET_ENTITY_SPECIFIC_8]) = htonl(retValue);
	}
}

U8 openavbAecpCommandGetCountersHandler(
	openavb_aecp_command_data_get_counters_t *pCmd,
	openavb_aecp_response_data_get_counters_t *pRsp )
{
	U8 result = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;
	U32 value = 0;

	if (!pCmd || !pRsp) {
		return OPENAVB_AEM_COMMAND_STATUS_NO_RESOURCES;
	}

	// Default to no counters.
	pRsp->counters_valid = 0;
	pRsp->counters_block_length = 128;
	memset(pRsp->counters_block, 0, sizeof(pRsp->counters_block));

	if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_ENTITY) {
		openavb_aem_descriptor_entity_t *pDescriptorEntity = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
		if (pDescriptorEntity) {
			// Get the supported counter values.
			GetEntitySpecificCounters(pDescriptorEntity, OPENAVB_AEM_DESCRIPTOR_ENTITY, pRsp);

			result = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
		}
	}
	else if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_AVB_INTERFACE) {
		openavb_aem_descriptor_avb_interface_t *pDescriptorAvbInterface = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
		if (pDescriptorAvbInterface) {
			// Get the supported counter values.
			if (openavbAVDECCGetCounterValue(pDescriptorAvbInterface, OPENAVB_AEM_DESCRIPTOR_AVB_INTERFACE, OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_COUNTER_LINK_UP, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_COUNTER_LINK_UP;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_OFFSET_LINK_UP]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorAvbInterface, OPENAVB_AEM_DESCRIPTOR_AVB_INTERFACE, OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_COUNTER_LINK_DOWN, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_COUNTER_LINK_DOWN;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_OFFSET_LINK_DOWN]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorAvbInterface, OPENAVB_AEM_DESCRIPTOR_AVB_INTERFACE, OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_COUNTER_FRAMES_TX, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_COUNTER_FRAMES_TX;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_OFFSET_FRAMES_TX]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorAvbInterface, OPENAVB_AEM_DESCRIPTOR_AVB_INTERFACE, OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_COUNTER_FRAMES_RX, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_COUNTER_FRAMES_RX;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_OFFSET_FRAMES_RX]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorAvbInterface, OPENAVB_AEM_DESCRIPTOR_AVB_INTERFACE, OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_COUNTER_RX_CRC_ERROR, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_COUNTER_RX_CRC_ERROR;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_OFFSET_RX_CRC_ERROR]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorAvbInterface, OPENAVB_AEM_DESCRIPTOR_AVB_INTERFACE, OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_COUNTER_GPTP_GM_CHANGED, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_COUNTER_GPTP_GM_CHANGED;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_AVB_INTERFACE_OFFSET_GPTP_GM_CHANGED]) = htonl(value);
			}
			GetEntitySpecificCounters(pDescriptorAvbInterface, OPENAVB_AEM_DESCRIPTOR_AVB_INTERFACE, pRsp);

			result = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
		}
	}
	else if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_CLOCK_DOMAIN) {
		openavb_aem_descriptor_clock_domain_t *pDescriptorClockDomain = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
		if (pDescriptorClockDomain) {
			// Get the supported counter values.
			if (openavbAVDECCGetCounterValue(pDescriptorClockDomain, OPENAVB_AEM_DESCRIPTOR_CLOCK_DOMAIN, OPENAVB_AEM_GET_COUNTERS_COMMAND_CLOCK_DOMAIN_COUNTER_LOCKED, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_CLOCK_DOMAIN_COUNTER_LOCKED;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_CLOCK_DOMAIN_OFFSET_LOCKED]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorClockDomain, OPENAVB_AEM_DESCRIPTOR_CLOCK_DOMAIN, OPENAVB_AEM_GET_COUNTERS_COMMAND_CLOCK_DOMAIN_COUNTER_UNLOCKED, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_CLOCK_DOMAIN_COUNTER_UNLOCKED;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_CLOCK_DOMAIN_OFFSET_UNLOCKED]) = htonl(value);
			}
			GetEntitySpecificCounters(pDescriptorClockDomain, OPENAVB_AEM_DESCRIPTOR_CLOCK_DOMAIN, pRsp);

			result = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
		}
	}
	else if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT) {
		openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
		if (pDescriptorStreamInput) {
			// Get the supported counter values.
			if (openavbAVDECCGetCounterValue(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_MEDIA_LOCKED, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_MEDIA_LOCKED;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_OFFSET_MEDIA_LOCKED]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_MEDIA_UNLOCKED, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_MEDIA_UNLOCKED;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_OFFSET_MEDIA_UNLOCKED]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_STREAM_RESET, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_STREAM_RESET;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_OFFSET_STREAM_RESET]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_SEQ_NUM_MISMATCH, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_SEQ_NUM_MISMATCH;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_OFFSET_SEQ_NUM_MISMATCH]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_MEDIA_RESET, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_MEDIA_RESET;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_OFFSET_MEDIA_RESET]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_TIMESTAMP_UNCERTAIN, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_TIMESTAMP_UNCERTAIN;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_OFFSET_TIMESTAMP_UNCERTAIN]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_TIMESTAMP_VALID, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_TIMESTAMP_VALID;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_OFFSET_TIMESTAMP_VALID]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_TIMESTAMP_NOT_VALID, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_TIMESTAMP_NOT_VALID;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_OFFSET_TIMESTAMP_NOT_VALID]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_UNSUPPORTED_FORMAT, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_UNSUPPORTED_FORMAT;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_OFFSET_UNSUPPORTED_FORMAT]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_LATE_TIMESTAMP, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_LATE_TIMESTAMP;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_OFFSET_LATE_TIMESTAMP]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_EARLY_TIMESTAMP, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_EARLY_TIMESTAMP;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_OFFSET_EARLY_TIMESTAMP]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_FRAMES_RX, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_FRAMES_RX;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_OFFSET_FRAMES_RX]) = htonl(value);
			}
			if (openavbAVDECCGetCounterValue(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_FRAMES_TX, &value)) {
				pRsp->counters_valid |= OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_COUNTER_FRAMES_TX;
				*(U32 *)&(pRsp->counters_block[OPENAVB_AEM_GET_COUNTERS_COMMAND_STREAM_INPUT_OFFSET_FRAMES_TX]) = htonl(value);
			}
			GetEntitySpecificCounters(pDescriptorStreamInput, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, pRsp);

			result = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
		}
	}

	return result;
}
