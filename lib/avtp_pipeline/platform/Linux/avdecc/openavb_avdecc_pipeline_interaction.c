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
#include "openavb_avdecc_msg_server.h"


bool openavbAVDECCRunListener(openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput, U16 configIdx, openavb_acmp_ListenerStreamInfo_t *pListenerStreamInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// Sanity tests.
	if (!pDescriptorStreamInput) {
		AVB_LOG_ERROR("openavbAVDECCRunListener Invalid descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pListenerStreamInfo) {
		AVB_LOG_ERROR("openavbAVDECCRunListener Invalid streaminfo");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pDescriptorStreamInput->stream) {
		AVB_LOG_ERROR("openavbAVDECCRunListener Invalid StreamInput descriptor stream");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pDescriptorStreamInput->stream->client) {
		AVB_LOG_ERROR("openavbAVDECCRunListener Invalid stream client pointer");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	// Stop the Listener if it is currently running.
	if (pDescriptorStreamInput->stream->client->lastReportedState != OPENAVB_AVDECC_MSG_STOPPED) {
		if (!openavbAvdeccMsgSrvrChangeRequest(pDescriptorStreamInput->stream->client->avdeccMsgHandle, OPENAVB_AVDECC_MSG_STOPPED)) {
			AVB_LOG_ERROR("Error requesting Listener change to Stopped");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}
		AVB_LOG_INFO("Listener state change to Stopped requested");
	}

	// Send the Stream ID to the client.
	if (!openavbAvdeccMsgSrvrListenerStreamID(pDescriptorStreamInput->stream->client->avdeccMsgHandle,
			pListenerStreamInfo->stream_id, /* The first 6 bytes of the steam_id are the source MAC Address */
			(((U16) pListenerStreamInfo->stream_id[6]) << 8 | (U16) pListenerStreamInfo->stream_id[7]),
			pListenerStreamInfo->stream_dest_mac,
			pListenerStreamInfo->stream_vlan_id)) {
		AVB_LOG_ERROR("Error send Stream ID to Listener");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	// Tell the client to start running.
	if (!openavbAvdeccMsgSrvrChangeRequest(pDescriptorStreamInput->stream->client->avdeccMsgHandle, OPENAVB_AVDECC_MSG_RUNNING)) {
		AVB_LOG_ERROR("Error requesting Listener change to Running");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	AVB_LOG_INFO("Listener state change to Running requested");

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

bool openavbAVDECCRunTalker(openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput, U16 configIdx, openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// AVDECC_TODO:  Fill this in.
	AVB_LOG_ERROR("openavbAVDECCRunTalker Not Implemented!");

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

bool openavbAVDECCStopListener(openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput, U16 configIdx, openavb_acmp_ListenerStreamInfo_t *pListenerStreamInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// Sanity tests.
	if (!pDescriptorStreamInput) {
		AVB_LOG_ERROR("openavbAVDECCStopListener Invalid descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pListenerStreamInfo) {
		AVB_LOG_ERROR("openavbAVDECCStopListener Invalid streaminfo");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pDescriptorStreamInput->stream) {
		AVB_LOG_ERROR("openavbAVDECCStopListener Invalid StreamInput descriptor stream");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pDescriptorStreamInput->stream->client) {
		AVB_LOG_ERROR("openavbAVDECCStopListener Invalid stream client pointer");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	// Don't request if already stopped.
	if (pDescriptorStreamInput->stream->client->lastReportedState == OPENAVB_AVDECC_MSG_STOPPED) {
		AVB_LOG_INFO("Listener state change to running ignored, as Listener already Stopped");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return TRUE;
	}

	// Send the request to the client.
	if (!openavbAvdeccMsgSrvrChangeRequest(pDescriptorStreamInput->stream->client->avdeccMsgHandle, OPENAVB_AVDECC_MSG_STOPPED)) {
		AVB_LOG_ERROR("Error requesting Listener change to Stopped");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	AVB_LOG_INFO("Listener state change to Stopped requested");

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

bool openavbAVDECCStopTalker(openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput, U16 configIdx, openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// AVDECC_TODO:  Fill this in.
	AVB_LOG_ERROR("openavbAVDECCStopTalker Not Implemented!");

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

bool openavbAVDECCGetTalkerStreamInfo(openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput, U16 configIdx, openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// AVDECC_TODO:  Fill this in.
	AVB_LOG_ERROR("openavbAVDECCGetTalkerStreamInfo Not Implemented!");

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

bool openavbAVDECCListenerIsStreaming(openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput, U16 configIdx)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// Sanity tests.
	if (!pDescriptorStreamInput) {
		AVB_LOG_ERROR("openavbAVDECCStopListener Invalid descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pDescriptorStreamInput->stream) {
		AVB_LOG_ERROR("openavbAVDECCStopListener Invalid StreamInput descriptor stream");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pDescriptorStreamInput->stream->client) {
		AVB_LOG_ERROR("openavbAVDECCStopListener Invalid stream client pointer");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	// Return the current Listener state.
	// If the state is not known, assume the Listener is not running.
	if (pDescriptorStreamInput->stream->client->lastReportedState == OPENAVB_AVDECC_MSG_RUNNING) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return TRUE;
	}
	if (pDescriptorStreamInput->stream->client->lastReportedState != OPENAVB_AVDECC_MSG_STOPPED) {
		AVB_LOG_WARNING("Listener state unknown");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return FALSE;
}

bool openavbAVDECCTalkerIsStreaming(openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput, U16 configIdx)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// AVDECC_TODO:  Fill this in.
	AVB_LOG_ERROR("openavbAVDECCTalkerIsStreaming Not Implemented!");

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return FALSE;
}

void openavbAVDECCPauseStream(openavb_aem_descriptor_stream_io_t *pDescriptor, bool bPause)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// Sanity test.
	if (!pDescriptor) {
		AVB_LOG_ERROR("openavbAVDECCPauseStream Invalid descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return;
	}
	if (!pDescriptor->stream) {
		AVB_LOG_ERROR("openavbAVDECCPauseStream Invalid StreamInput descriptor stream");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return;
	}
	if (!pDescriptor->stream->client) {
		AVB_LOG_ERROR("openavbAVDECCPauseStream Invalid stream client pointer");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return;
	}

	if (pDescriptor->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT) {

		if (bPause) {
			// If the client is not running (or already paused), ignore this command.
			if (pDescriptor->stream->client->lastReportedState != OPENAVB_AVDECC_MSG_RUNNING) {
				AVB_LOG_DEBUG("Listener state change to pause ignored, as Listener not running");
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return;
			}

			// Send the request to the client.
			if (!openavbAvdeccMsgSrvrChangeRequest(pDescriptor->stream->client->avdeccMsgHandle, OPENAVB_AVDECC_MSG_PAUSED)) {
				AVB_LOG_ERROR("Error requesting Listener change to Paused");
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return;
			}

			AVB_LOG_INFO("Listener state change from Running to Paused requested");
		}
		else {
			// If the client is not paused, ignore this command.
			if (pDescriptor->stream->client->lastReportedState != OPENAVB_AVDECC_MSG_PAUSED) {
				AVB_LOG_DEBUG("Listener state change to pause ignored, as Listener not paused");
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return;
			}

			// Send the request to the client.
			if (!openavbAvdeccMsgSrvrChangeRequest(pDescriptor->stream->client->avdeccMsgHandle, OPENAVB_AVDECC_MSG_RUNNING)) {
				AVB_LOG_ERROR("Error requesting Listener change to Running");
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return;
			}

			AVB_LOG_INFO("Listener state change from Paused to Running requested");
		}
	}
	else if (pDescriptor->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT) {
		// AVDECC_TODO:  Implement this feature.
		AVB_LOG_ERROR("openavbAVDECCPauseStream Talker Not Implemented!");
	}
	else {
		AVB_LOG_ERROR("openavbAVDECCPauseStream unsupported descriptor");
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
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
