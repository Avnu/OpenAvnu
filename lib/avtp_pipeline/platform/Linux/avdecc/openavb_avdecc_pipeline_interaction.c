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

	// Send the Stream ID to the client.
	// The client will stop a running Listener if the settings differ from its current values.
	if (!openavbAvdeccMsgSrvrListenerStreamID(pDescriptorStreamInput->stream->client->avdeccMsgHandle,
			((pListenerStreamInfo->flags & OPENAVB_ACMP_FLAG_CLASS_B) != 0 ? SR_CLASS_B : SR_CLASS_A),
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

	// Sanity tests.
	if (!pDescriptorStreamOutput) {
		AVB_LOG_ERROR("openavbAVDECCRunTalker Invalid descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pTalkerStreamInfo) {
		AVB_LOG_ERROR("openavbAVDECCRunTalker Invalid streaminfo");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pDescriptorStreamOutput->stream) {
		AVB_LOG_ERROR("openavbAVDECCRunTalker Invalid StreamInput descriptor stream");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pDescriptorStreamOutput->stream->client) {
		AVB_LOG_ERROR("openavbAVDECCRunTalker Invalid stream client pointer");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	// Tell the client to start running.
	// Note that that Talker may already be running; this call ensures that it really is.
	if (!openavbAvdeccMsgSrvrChangeRequest(pDescriptorStreamOutput->stream->client->avdeccMsgHandle, OPENAVB_AVDECC_MSG_RUNNING)) {
		AVB_LOG_ERROR("Error requesting Talker change to Running");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	AVB_LOG_INFO("Talker state change to Running requested");

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
		AVB_LOG_INFO("Listener state change to Stopped ignored, as Listener already Stopped");
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

	// Sanity tests.
	if (!pDescriptorStreamOutput) {
		AVB_LOG_ERROR("openavbAVDECCStopTalker Invalid descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pTalkerStreamInfo) {
		AVB_LOG_ERROR("openavbAVDECCStopTalker Invalid streaminfo");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pDescriptorStreamOutput->stream) {
		AVB_LOG_ERROR("openavbAVDECCStopTalker Invalid StreamInput descriptor stream");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pDescriptorStreamOutput->stream->client) {
		AVB_LOG_ERROR("openavbAVDECCStopTalker Invalid stream client pointer");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	// Don't request if already stopped.
	if (pDescriptorStreamOutput->stream->client->lastReportedState == OPENAVB_AVDECC_MSG_STOPPED) {
		AVB_LOG_INFO("Talker state change to Stopped ignored, as Talker already Stopped");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return TRUE;
	}

	// Send the request to the client.
	if (!openavbAvdeccMsgSrvrChangeRequest(pDescriptorStreamOutput->stream->client->avdeccMsgHandle, OPENAVB_AVDECC_MSG_STOPPED)) {
		AVB_LOG_ERROR("Error requesting Talker change to Stopped");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	AVB_LOG_INFO("Talker state change to Stopped requested");

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

bool openavbAVDECCGetTalkerStreamInfo(openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput, U16 configIdx, openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// Sanity tests.
	if (!pDescriptorStreamOutput) {
		AVB_LOG_ERROR("openavbAVDECCGetTalkerStreamInfo Invalid descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pTalkerStreamInfo) {
		AVB_LOG_ERROR("openavbAVDECCGetTalkerStreamInfo Invalid streaminfo");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pDescriptorStreamOutput->stream) {
		AVB_LOG_ERROR("openavbAVDECCGetTalkerStreamInfo Invalid StreamInput descriptor stream");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	// Get the destination MAC Address.
	if (!pDescriptorStreamOutput->stream->dest_addr.mac ||
			memcmp(pDescriptorStreamOutput->stream->dest_addr.buffer.ether_addr_octet, "\x00\x00\x00\x00\x00\x00", ETH_ALEN) == 0) {
		AVB_LOG_DEBUG("openavbAVDECCGetTalkerStreamInfo Invalid stream dest_addr");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	memcpy(pTalkerStreamInfo->stream_dest_mac, pDescriptorStreamOutput->stream->dest_addr.mac, ETH_ALEN);
	AVB_LOGF_DEBUG("Talker stream_dest_mac:  " ETH_FORMAT,
		ETH_OCTETS(pTalkerStreamInfo->stream_dest_mac));

	// Get the Stream ID.
	if (!pDescriptorStreamOutput->stream->stream_addr.mac ||
			memcmp(pDescriptorStreamOutput->stream->stream_addr.buffer.ether_addr_octet, "\x00\x00\x00\x00\x00\x00", ETH_ALEN) == 0) {
		AVB_LOG_ERROR("openavbAVDECCGetTalkerStreamInfo Invalid stream stream_addr");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	memcpy(pTalkerStreamInfo->stream_id, pDescriptorStreamOutput->stream->stream_addr.mac, ETH_ALEN);
	U8 *pStreamUID = pTalkerStreamInfo->stream_id + 6;
	*(U16 *)(pStreamUID) = htons(pDescriptorStreamOutput->stream->stream_uid);
	AVB_LOGF_DEBUG("Talker stream_id:  " ETH_FORMAT "/%u",
		ETH_OCTETS(pTalkerStreamInfo->stream_id),
		(((U16) pTalkerStreamInfo->stream_id[6]) << 8) | (U16) pTalkerStreamInfo->stream_id[7]);

	// Get the VLAN ID.
	pTalkerStreamInfo->stream_vlan_id = pDescriptorStreamOutput->stream->vlan_id;

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

bool openavbAVDECCSetTalkerStreamInfo(openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput,
	U8 sr_class, U8 stream_id_valid, const U8 stream_src_mac[6], U16 stream_uid, U8 stream_dest_valid, const U8 stream_dest_mac[6], U8 stream_vlan_id_valid, U16 stream_vlan_id)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// Sanity tests.
	if (!pDescriptorStreamOutput) {
		AVB_LOG_ERROR("openavbAVDECCSetTalkerStreamInfo Invalid descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pDescriptorStreamOutput->stream) {
		AVB_LOG_ERROR("openavbAVDECCSetTalkerStreamInfo Invalid StreamInput descriptor stream");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (!pDescriptorStreamOutput->stream->client) {
		AVB_LOG_ERROR("openavbAVDECCSetTalkerStreamInfo Invalid stream client pointer");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	// Send the information to the client.
	if (!openavbAvdeccMsgSrvrTalkerStreamID(pDescriptorStreamOutput->stream->client->avdeccMsgHandle,
			sr_class, stream_id_valid, stream_src_mac, stream_uid, stream_dest_valid, stream_dest_mac, stream_vlan_id_valid, stream_vlan_id)) {
		AVB_LOG_ERROR("Error sending stream info updates to Talker");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

openavbAvdeccMsgStateType_t openavbAVDECCGetRequestedState(openavb_aem_descriptor_stream_io_t *pDescriptorStream, U16 configIdx)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// Sanity tests.
	if (!pDescriptorStream) {
		AVB_LOG_ERROR("openavbAVDECCGetRequestedState Invalid descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return OPENAVB_AVDECC_MSG_UNKNOWN;
	}
	if (!pDescriptorStream->stream) {
		AVB_LOG_ERROR("openavbAVDECCGetRequestedState Invalid descriptor stream");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return OPENAVB_AVDECC_MSG_UNKNOWN;
	}
	if (!pDescriptorStream->stream->client) {
		AVB_LOG_DEBUG("openavbAVDECCGetRequestedState Invalid stream client pointer");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return OPENAVB_AVDECC_MSG_UNKNOWN;
	}

	// Return the current state.
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return pDescriptorStream->stream->client->lastRequestedState;
}

openavbAvdeccMsgStateType_t openavbAVDECCGetStreamingState(openavb_aem_descriptor_stream_io_t *pDescriptorStream, U16 configIdx)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// Sanity tests.
	if (!pDescriptorStream) {
		AVB_LOG_ERROR("openavbAVDECCGetStreamingState Invalid descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return OPENAVB_AVDECC_MSG_UNKNOWN;
	}
	if (!pDescriptorStream->stream) {
		AVB_LOG_ERROR("openavbAVDECCGetStreamingState Invalid descriptor stream");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return OPENAVB_AVDECC_MSG_UNKNOWN;
	}
	if (!pDescriptorStream->stream->client) {
		AVB_LOG_DEBUG("openavbAVDECCGetStreamingState Invalid stream client pointer");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return OPENAVB_AVDECC_MSG_UNKNOWN;
	}

	// Return the current state.
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return pDescriptorStream->stream->client->lastReportedState;
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

		if (bPause) {
			// If the client is not running (or already paused), ignore this command.
			if (pDescriptor->stream->client->lastReportedState != OPENAVB_AVDECC_MSG_RUNNING) {
				AVB_LOG_DEBUG("Talker state change to pause ignored, as Talker not running");
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return;
			}

			// Send the request to the client.
			if (!openavbAvdeccMsgSrvrChangeRequest(pDescriptor->stream->client->avdeccMsgHandle, OPENAVB_AVDECC_MSG_PAUSED)) {
				AVB_LOG_ERROR("Error requesting Talker change to Paused");
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return;
			}

			AVB_LOG_INFO("Talker state change from Running to Paused requested");
		}
		else {
			// If the client is not paused, ignore this command.
			if (pDescriptor->stream->client->lastReportedState != OPENAVB_AVDECC_MSG_PAUSED) {
				AVB_LOG_DEBUG("Talker state change to pause ignored, as Talker not paused");
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return;
			}

			// Send the request to the client.
			if (!openavbAvdeccMsgSrvrChangeRequest(pDescriptor->stream->client->avdeccMsgHandle, OPENAVB_AVDECC_MSG_RUNNING)) {
				AVB_LOG_ERROR("Error requesting Talker change to Running");
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return;
			}

			AVB_LOG_INFO("Talker state change from Paused to Running requested");
		}
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
