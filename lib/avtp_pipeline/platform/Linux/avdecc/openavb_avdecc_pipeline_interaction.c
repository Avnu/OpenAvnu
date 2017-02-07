/*
 ******************************************************************
 * COPYRIGHT (C) Harman International
 * All Rights Reserved
 ******************************************************************
 */

#define	AVB_LOG_COMPONENT	"AVDECC"
#include "openavb_log.h"
#include "openavb_trace_pub.h"

#include "openavb_aem_types_pub.h"
#include "openavb_avdecc_pipeline_interaction_pub.h"


bool openavbAVDECCRunListener(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 configIdx, U16 descriptorType, U16 descriptorIdx, void *pVoidListenerStreamInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	AVB_LOG_ERROR("openavbAVDECCRunListener Not Implemented!");

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

bool openavbAVDECCRunTalker(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 configIdx, U16 descriptorType, U16 descriptorIdx, void *pVoidTalkerStreamInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	AVB_LOG_ERROR("openavbAVDECCRunTalker Not Implemented!");

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

bool openavbAVDECCStopListener(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 configIdx, void *pVoidListenerStreamInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	AVB_LOG_ERROR("openavbAVDECCStopListener Not Implemented!");

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

bool openavbAVDECCStopTalker(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 configIdx, void *pVoidTalkerStreamInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	AVB_LOG_ERROR("openavbAVDECCStopTalker Not Implemented!");

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

bool openavbAVDECCGetTalkerStreamInfo(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 configIdx, void *pVoidTalkerStreamInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	AVB_LOG_ERROR("openavbAVDECCGetTalkerStreamInfo Not Implemented!");

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

// Get the current counter value in pValue.  Returns TRUE if the counter is supported, FALSE otherwise.
bool openavbAVDECCGetCounterValue(void *pDescriptor, U16 descriptorType, U32 counterFlag, U32 *pValue)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	if (!pDescriptor) {
		/* Asked for a non-existing descriptor. */
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	switch (descriptorType) {
	case OPENAVB_AEM_DESCRIPTOR_ENTITY:
		// The only counters are entity-specific.
		break;

	case OPENAVB_AEM_DESCRIPTOR_AVB_INTERFACE:
		// AVDECC_TODO - Get the LINK_UP, LINK_DOWN, FRAMES_TX, FRAMES_RX, RX_CRC_ERROR. and GPTP_GM_CHANGED counts from the gPTP daemon.
		break;

	case OPENAVB_AEM_DESCRIPTOR_CLOCK_DOMAIN:
		{
			switch (counterFlag) {
			case OPENAVB_AEM_GET_COUNTERS_COMMAND_CLOCK_DOMAIN_COUNTER_LOCKED:
				AVB_LOG_ERROR("OPENAVB_AEM_GET_COUNTERS_COMMAND_CLOCK_DOMAIN_COUNTER_LOCKED Not Implemented!");
				if (pValue) { *pValue = 1; }
				return TRUE;

			case OPENAVB_AEM_GET_COUNTERS_COMMAND_CLOCK_DOMAIN_COUNTER_UNLOCKED:
				AVB_LOG_ERROR("OPENAVB_AEM_GET_COUNTERS_COMMAND_CLOCK_DOMAIN_COUNTER_UNLOCKED Not Implemented!");
				if (pValue) { *pValue = 1; }
				return TRUE;

			default:
				break;
			}
			break;
		}

	case OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT:
		// AVDECC_TODO - Get the MEDIA_LOCKED, MEDIA_UNLOCKED, STREAM_RESET, SEQ_NUM_MISMATCH, MEDIA_RESET,
		//     TIMESTAMP_UNCERTAIN, TIMESTAMP_VALID, TIMESTAMP_NOT_VALID, UNSUPPORTED_FORMAT,
		//     LATE_TIMESTAMP, EARLY_TIMESTAMP, FRAMES_RX, and FRAMES_TX counts.
		break;

	default:
		break;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return FALSE;
}
