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
 * MODULE SUMMARY :
 *
 * Stream clients (talkers or listeners) must connect to the central
 * "avdecc_msg" process to create a reservation for their traffic.
 *
 * This code implements the server side of the IPC.
 *
 * It provides proxy functions for the avdecc_msg to call.  The arguments
 * for those calls are packed into messages, which are unpacked in the
 * process and then used to call the real functions.
 *
 * Current IPC uses unix sockets.  Can change this by creating a new
 * implementations in openavb_avdecc_msg_client.c and openavb_avdecc_msg_server.c
 */

#include <stdlib.h>
#include <string.h>

//#define AVB_LOG_LEVEL  AVB_LOG_LEVEL_DEBUG
#define	AVB_LOG_COMPONENT	"AVDECC Msg"
#include "openavb_pub.h"
#include "openavb_log.h"
#include "openavb_avdecc_pub.h"
#include "openavb_adp.h"
#include "openavb_acmp_sm_talker.h"
#include "openavb_acmp_sm_listener.h"

#include "openavb_avdecc_msg_server.h"
#include "openavb_trace.h"

// forward declarations
static bool openavbAvdeccMsgSrvrReceiveFromClient(int avdeccMsgHandle, openavbAvdeccMessage_t *msg);

// OSAL specific functions
#include "openavb_avdecc_msg_server_osal.c"

// AvdeccMsgStateListGet() support.
#include "openavb_avdecc_msg.c"

// the following are from openavb_avdecc.c
extern openavb_avdecc_cfg_t gAvdeccCfg;
extern openavb_tl_data_cfg_t * streamList;


static bool openavbAvdeccMsgSrvrReceiveFromClient(int avdeccMsgHandle, openavbAvdeccMessage_t *msg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (!msg) {
		AVB_LOG_ERROR("Receiving message; invalid argument passed");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return FALSE;
	}

	bool ret = FALSE;
	switch (msg->type) {
		case OPENAVB_AVDECC_MSG_VERSION_REQUEST:
			AVB_LOG_DEBUG("Message received:  OPENAVB_AVDECC_MSG_VERSION_REQUEST");
			ret = openavbAvdeccMsgSrvrHndlVerRqstFromClient(avdeccMsgHandle);
			break;
		case OPENAVB_AVDECC_MSG_CLIENT_INIT_IDENTIFY:
			AVB_LOG_DEBUG("Message received:  OPENAVB_AVDECC_MSG_CLIENT_INIT_IDENTIFY");
			ret = openavbAvdeccMsgSrvrHndlInitIdentifyFromClient(avdeccMsgHandle,
				msg->params.clientInitIdentify.friendly_name,
				msg->params.clientInitIdentify.talker);
			break;
		case OPENAVB_AVDECC_MSG_C2S_TALKER_STREAM_ID:
			AVB_LOG_DEBUG("Message received:  OPENAVB_AVDECC_MSG_C2S_TALKER_STREAM_ID");
			ret = openavbAvdeccMsgSrvrHndlTalkerStreamIDFromClient(avdeccMsgHandle,
				msg->params.c2sTalkerStreamID.sr_class,
				msg->params.c2sTalkerStreamID.stream_src_mac,
				ntohs(msg->params.c2sTalkerStreamID.stream_uid),
				msg->params.c2sTalkerStreamID.stream_dest_mac,
				ntohs(msg->params.c2sTalkerStreamID.stream_vlan_id));
			break;
		case OPENAVB_AVDECC_MSG_CLIENT_CHANGE_NOTIFICATION:
			AVB_LOG_DEBUG("Message received:  OPENAVB_AVDECC_MSG_CLIENT_CHANGE_NOTIFICATION");
			ret = openavbAvdeccMsgSrvrHndlChangeNotificationFromClient(avdeccMsgHandle, (openavbAvdeccMsgStateType_t) msg->params.clientChangeNotification.current_state);
			break;
		default:
			AVB_LOG_ERROR("Unexpected message received at server");
			break;
	}

	AVB_LOGF_VERBOSE("Message handled, ret=%d", ret);
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return ret;
}

void openavbAvdeccMsgSrvrSendServerVersionToClient(int avdeccMsgHandle, U32 AVBVersion)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	openavbAvdeccMessage_t  msgBuf;
	memset(&msgBuf, 0, OPENAVB_AVDECC_MSG_LEN);
	msgBuf.type = OPENAVB_AVDECC_MSG_VERSION_CALLBACK;
	msgBuf.params.versionCallback.AVBVersion = htonl(AVBVersion);
	openavbAvdeccMsgSrvrSendToClient(avdeccMsgHandle, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
}

/* Client version request
 */
bool openavbAvdeccMsgSrvrHndlVerRqstFromClient(int avdeccMsgHandle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	openavbAvdeccMsgSrvrSendServerVersionToClient(avdeccMsgHandle, AVB_CORE_VER_FULL);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return TRUE;
}

bool openavbAvdeccMsgSrvrHndlInitIdentifyFromClient(int avdeccMsgHandle, char * friendly_name, U8 talker)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);
	openavb_tl_data_cfg_t * currentStream;

	avdecc_msg_state_t *pState = AvdeccMsgStateListGet(avdeccMsgHandle);
	if (pState) {
		// The handle was already specified.  Something has gone terribly wrong!
		AVB_LOGF_ERROR("avdeccMsgHandle %d already used", avdeccMsgHandle);
		AvdeccMsgStateListRemove(pState);
		free(pState);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return false;
	}

	// Make sure the supplied string is nil-terminated.
	friendly_name[FRIENDLY_NAME_SIZE - 1] = '\0';

	// Create a structure to hold the client information.
	pState = (avdecc_msg_state_t *) calloc(1, sizeof(avdecc_msg_state_t));
	if (!pState) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return false;
	}
	pState->avdeccMsgHandle = avdeccMsgHandle;
	pState->bTalker = (talker != 0);
	pState->lastRequestedState = pState->lastReportedState = OPENAVB_AVDECC_MSG_UNKNOWN;

	// Find the state information matching this item.
	for (currentStream = streamList; currentStream != NULL; currentStream = currentStream->next) {
		if (strncmp(currentStream->friendly_name, friendly_name, FRIENDLY_NAME_SIZE) == 0)
		{
			break;
		}
	}
	if (!currentStream) {
		AVB_LOGF_WARNING("Ignoring unexpected client %d, friendly_name:  %s",
			avdeccMsgHandle, friendly_name);
		free(pState);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return false;
	}

	// Keep track of this new state item.
	if (!AvdeccMsgStateListAdd(pState)) {
		AVB_LOGF_ERROR("Error saving client identity information %d", avdeccMsgHandle);
		free(pState);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return false;
	}

	// Associate this Listener instance with the stream information.
	pState->stream = currentStream;
	currentStream->client = pState;

	AVB_LOGF_INFO("Client %d Detected, friendly_name:  %s",
		avdeccMsgHandle, friendly_name);

	// Enable ADP support, now that we have at least one Talker/Listener.
	openavbAdpHaveTL(true);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return true;
}

bool openavbAvdeccMsgSrvrHndlTalkerStreamIDFromClient(int avdeccMsgHandle, U8 sr_class, const U8 stream_src_mac[6], U16 stream_uid, const U8 stream_dest_mac[6], U16 stream_vlan_id)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	avdecc_msg_state_t *pState = AvdeccMsgStateListGet(avdeccMsgHandle);
	if (!pState) {
		AVB_LOGF_ERROR("avdeccMsgHandle %d not valid", avdeccMsgHandle);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return false;
	}

	openavb_tl_data_cfg_t *pCfg = pState->stream;
	if (!pCfg) {
		AVB_LOGF_ERROR("avdeccMsgHandle %d stream not valid", avdeccMsgHandle);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return false;
	}

	// Update the stream information supplied by the client.
	pCfg->sr_class = sr_class;
	memcpy(pCfg->stream_addr.buffer.ether_addr_octet, stream_src_mac, 6);
	pCfg->stream_addr.mac = &(pCfg->stream_addr.buffer); // Indicate that the MAC Address is valid.
	pCfg->stream_uid = stream_uid;
	memcpy(pCfg->dest_addr.buffer.ether_addr_octet, stream_dest_mac, 6);
	pCfg->dest_addr.mac = &(pCfg->dest_addr.buffer); // Indicate that the MAC Address is valid.
	pCfg->vlan_id = stream_vlan_id;
	AVB_LOGF_DEBUG("Talker-supplied sr_class:  %u", pCfg->sr_class);
	AVB_LOGF_DEBUG("Talker-supplied stream_id:  " ETH_FORMAT "/%u",
		ETH_OCTETS(pCfg->stream_addr.buffer.ether_addr_octet), pCfg->stream_uid);
	AVB_LOGF_DEBUG("Talker-supplied dest_addr:  " ETH_FORMAT,
		ETH_OCTETS(pCfg->dest_addr.buffer.ether_addr_octet));
	AVB_LOGF_DEBUG("Talker-supplied vlan_id:  %u", pCfg->vlan_id);

	// Notify the state machine that we received this information.
	openavbAcmpSMTalker_updateStreamInfo(pCfg);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return true;
}

bool openavbAvdeccMsgSrvrListenerStreamID(int avdeccMsgHandle, U8 sr_class, const U8 stream_src_mac[6], U16 stream_uid, const U8 stream_dest_mac[6], U16 stream_vlan_id)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);
	openavbAvdeccMessage_t msgBuf;

	avdecc_msg_state_t *pState = AvdeccMsgStateListGet(avdeccMsgHandle);
	if (!pState) {
		AVB_LOGF_ERROR("avdeccMsgHandle %d not valid", avdeccMsgHandle);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return false;
	}

	memset(&msgBuf, 0, OPENAVB_AVDECC_MSG_LEN);
	msgBuf.type = OPENAVB_AVDECC_MSG_LISTENER_STREAM_ID;
	openavbAvdeccMsgParams_ListenerStreamID_t * pParams =
		&(msgBuf.params.listenerStreamID);
	pParams->sr_class = sr_class;
	memcpy(pParams->stream_src_mac, stream_src_mac, 6);
	pParams->stream_uid = htons(stream_uid);
	memcpy(pParams->stream_dest_mac, stream_dest_mac, 6);
	pParams->stream_vlan_id = htons(stream_vlan_id);
	bool ret = openavbAvdeccMsgSrvrSendToClient(avdeccMsgHandle, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return ret;
}

bool openavbAvdeccMsgSrvrTalkerStreamID(int avdeccMsgHandle,
	U8 sr_class, U8 stream_id_valid, const U8 stream_src_mac[6], U16 stream_uid, U8 stream_dest_valid, const U8 stream_dest_mac[6], U8 stream_vlan_id_valid, U16 stream_vlan_id)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);
	openavbAvdeccMessage_t msgBuf;

	avdecc_msg_state_t *pState = AvdeccMsgStateListGet(avdeccMsgHandle);
	if (!pState) {
		AVB_LOGF_ERROR("avdeccMsgHandle %d not valid", avdeccMsgHandle);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return false;
	}

	// Send the stream information to the client.
	memset(&msgBuf, 0, OPENAVB_AVDECC_MSG_LEN);
	msgBuf.type = OPENAVB_AVDECC_MSG_S2C_TALKER_STREAM_ID;
	openavbAvdeccMsgParams_S2C_TalkerStreamID_t * pParams =
		&(msgBuf.params.s2cTalkerStreamID);
	pParams->sr_class = sr_class;
	pParams->stream_id_valid = stream_id_valid;
	memcpy(pParams->stream_src_mac, stream_src_mac, 6);
	pParams->stream_uid = htons(stream_uid);
	pParams->stream_dest_valid = stream_dest_valid;
	memcpy(pParams->stream_dest_mac, stream_dest_mac, 6);
	pParams->stream_vlan_id_valid = stream_vlan_id_valid;
	pParams->stream_vlan_id = htons(stream_vlan_id);
	bool ret = openavbAvdeccMsgSrvrSendToClient(avdeccMsgHandle, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return ret;
}

bool openavbAvdeccMsgSrvrChangeRequest(int avdeccMsgHandle, openavbAvdeccMsgStateType_t desiredState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);
	openavbAvdeccMessage_t msgBuf;

	avdecc_msg_state_t *pState = AvdeccMsgStateListGet(avdeccMsgHandle);
	if (!pState) {
		AVB_LOGF_ERROR("avdeccMsgHandle %d not valid", avdeccMsgHandle);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return false;
	}

	memset(&msgBuf, 0, OPENAVB_AVDECC_MSG_LEN);
	msgBuf.type = OPENAVB_AVDECC_MSG_CLIENT_CHANGE_REQUEST;
	openavbAvdeccMsgParams_ClientChangeRequest_t * pParams =
		&(msgBuf.params.clientChangeRequest);
	pParams->desired_state = (U8) desiredState;
	bool ret = openavbAvdeccMsgSrvrSendToClient(avdeccMsgHandle, &msgBuf);
	if (ret) {
		// Save the requested state for future reference.
		pState->lastRequestedState = desiredState;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return ret;
}

static const char * GetStateString(openavbAvdeccMsgStateType_t state)
{
	switch (state) {
	case OPENAVB_AVDECC_MSG_UNKNOWN:
		return "Unknown";
	case OPENAVB_AVDECC_MSG_STOPPED:
		return "Stopped";
	case OPENAVB_AVDECC_MSG_RUNNING:
		return "Running";
	case OPENAVB_AVDECC_MSG_PAUSED:
		return "Paused";
	case OPENAVB_AVDECC_MSG_STOPPED_UNEXPECTEDLY:
		return "Stopped_Unexpectedly";
	default:
		return "ERROR";
	}
}

bool openavbAvdeccMsgSrvrHndlChangeNotificationFromClient(int avdeccMsgHandle, openavbAvdeccMsgStateType_t currentState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	avdecc_msg_state_t *pState = AvdeccMsgStateListGet(avdeccMsgHandle);
	if (!pState) {
		AVB_LOGF_ERROR("avdeccMsgHandle %d not valid", avdeccMsgHandle);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return false;
	}

	// Save the updated state.
	if (currentState != pState->lastReportedState) {
		openavbAvdeccMsgStateType_t previousState = pState->lastReportedState;
		pState->lastReportedState = currentState;
		AVB_LOGF_INFO("Notified of client %d state change from %s to %s (Last requested:  %s)",
				avdeccMsgHandle, GetStateString(previousState), GetStateString(currentState), GetStateString(pState->lastRequestedState));

		// If AVDECC did not yet set the client state, set the client state to the desired state.
		if (pState->lastRequestedState == OPENAVB_AVDECC_MSG_UNKNOWN) {
			if (pState->stream->initial_state == TL_INIT_STATE_RUNNING && pState->lastReportedState != OPENAVB_AVDECC_MSG_RUNNING) {
				// Have the client be running if the user explicitly requested it to be running.
				openavbAvdeccMsgSrvrChangeRequest(avdeccMsgHandle, OPENAVB_AVDECC_MSG_RUNNING);
			}
			else if (pState->stream->initial_state != TL_INIT_STATE_RUNNING && pState->lastReportedState == OPENAVB_AVDECC_MSG_RUNNING) {
				// Have the client not be running if the user didn't explicitly request it to be running.
				openavbAvdeccMsgSrvrChangeRequest(avdeccMsgHandle, OPENAVB_AVDECC_MSG_STOPPED);
			}
			else if (gAvdeccCfg.bFastConnectSupported &&
					!(pState->bTalker) &&
					pState->lastReportedState == OPENAVB_AVDECC_MSG_STOPPED) {
				// Listener started as not running, and is not configured to start in the running state.
				// See if we should do a fast connect using the saved state.
				openavb_tl_data_cfg_t *pCfg = pState->stream;
				if (pCfg) {
					U16 flags, talker_unique_id;
					U8 talker_entity_id[8], controller_entity_id[8];
					bool bAvailable = openavbAvdeccGetSaveStateInfo(pCfg, &flags, &talker_unique_id, &talker_entity_id, &controller_entity_id);
					if (bAvailable) {
						openavbAcmpSMListenerSet_doFastConnect(pCfg, flags, talker_unique_id, talker_entity_id, controller_entity_id);
					}
				}
			}
		}
		else if (currentState == OPENAVB_AVDECC_MSG_STOPPED_UNEXPECTEDLY) {
			if (previousState != OPENAVB_AVDECC_MSG_STOPPED_UNEXPECTEDLY &&
					pState->lastRequestedState == OPENAVB_AVDECC_MSG_RUNNING &&
					gAvdeccCfg.bFastConnectSupported) {
				// The Talker disappeared.  Use fast connect to assist with reconnecting if it reappears.
				openavb_tl_data_cfg_t *pCfg = pState->stream;
				if (pCfg) {
					U16 flags, talker_unique_id;
					U8 talker_entity_id[8], controller_entity_id[8];
					bool bAvailable = openavbAvdeccGetSaveStateInfo(pCfg, &flags, &talker_unique_id, &talker_entity_id, &controller_entity_id);
					if (bAvailable) {
						// Set the timed-out status for the Listener's descriptor.
						// The next time the Talker advertises itself, openavbAcmpSMListenerSet_talkerTestFastConnect() should try to reconnect.
						openavb_aem_descriptor_stream_io_t *pDescriptor;
						U16 listenerUniqueId;
						for (listenerUniqueId = 0; listenerUniqueId < 0xFFFF; ++listenerUniqueId) {
							pDescriptor = openavbAemGetDescriptor(openavbAemGetConfigIdx(), OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, listenerUniqueId);
							if (pDescriptor && pDescriptor->stream &&
									strcmp(pDescriptor->stream->friendly_name, pCfg->friendly_name) == 0) {
								// We found a match.
								AVB_LOGF_INFO("Listener %s waiting to fast connect to flags=0x%04x, talker_unique_id=0x%04x, talker_entity_id=" ENTITYID_FORMAT ", controller_entity_id=" ENTITYID_FORMAT,
										pCfg->friendly_name,
										flags,
										talker_unique_id,
										ENTITYID_ARGS(talker_entity_id),
										ENTITYID_ARGS(controller_entity_id));
								pDescriptor->fast_connect_status = OPENAVB_FAST_CONNECT_STATUS_TIMED_OUT;
								memcpy(pDescriptor->fast_connect_talker_entity_id, talker_entity_id, sizeof(pDescriptor->fast_connect_talker_entity_id));
								CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &pDescriptor->fast_connect_start_time);
								pDescriptor->fast_connect_start_time.tv_sec += 5;	// Give the Talker some time to shutdown or stabilize
								break;
							}
						}
					}
				}
			}

			// Now that we have handled this, treat this state as a normal stop.
			pState->lastReportedState = OPENAVB_AVDECC_MSG_STOPPED;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return true;
}


/* Called if a client closes their end of the IPC
 */
void openavbAvdeccMsgSrvrCloseClientConnection(int avdeccMsgHandle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	// Free the state for this handle.
	avdecc_msg_state_t *pState = AvdeccMsgStateListGet(avdeccMsgHandle);
	if (pState) {
		AvdeccMsgStateListRemove(pState);
		if (streamList && pState->stream) {
			// Clear the stream pointer to this object.
			pState->stream = NULL;
		}
		free(pState);

		// If there are no more Talkers or Listeners, stop ADP.
		if (AvdeccMsgStateListGetIndex(0) == NULL) {
			openavbAdpHaveTL(false);
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
}
