/*
 ******************************************************************
 * COPYRIGHT (C) Harman International Industries, Inc.
 * All Rights Reserved
 ******************************************************************
 */
#include "openavb_platform.h"

#include <errno.h>

#define	AVB_LOG_COMPONENT	"AECP"
#include "openavb_log.h"

#include "openavb_aem.h"
#include "openavb_aecp.h"
#include "openavb_aecp_message.h"
#include "openavb_aecp_cmd_get_counters.h"


U8 openavbAecpCommandGetCountersHandler(
	openavb_aecp_command_data_get_counters_t *pCmd,
	openavb_aecp_response_data_get_counters_t *pRsp )
{
	U8 result = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;

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
			result = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
		}
	}
	else if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_AVB_INTERFACE) {
		openavb_aem_descriptor_avb_interface_t *pDescriptorAvbInterface = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
		if (pDescriptorAvbInterface) {
			// AVDECC_TODO - Get the LINK_UP, LINK_DOWN, FRAMES_TX, FRAMES_RX, RX_CRC_ERROR. and GPTP_GM_CHANGE counts from the gPTP daemon.
			result = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
		}
	}
	else if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_CLOCK_DOMAIN) {
		openavb_aem_descriptor_clock_domain_t *pDescriptorClockDomain = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
		if (pDescriptorClockDomain) {
			// AVDECC_TODO - Get the LOCKED and UNLOCKED counts.
			result = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
		}
	}
	else if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT) {
		openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
		if (pDescriptorStreamInput) {
			// AVDECC_TODO - Get the MEDIA_LOCKED, MEDIA_UNLOCKED, STREAM_RESET, SEQ_NUM_MISMATCH, MEDIA_RESET,
			//     TIMESTAMP_UNCERTAIN, TIMESTAMP_VALID, TIMESTAMP_NOT_VALID, UNSUPPORTED_FORMAT,
			//     LATE_TIMESTAMP, EARLY_TIMESTAMP, FRAMES_RX, and FRAMES_TX counts.
			result = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
		}
	}

	return result;
}
