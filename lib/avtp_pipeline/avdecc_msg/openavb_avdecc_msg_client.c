/*************************************************************************************************************
Copyright (c) 2012-2017, Harman International Industries, Incorporated
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
* "avdecc_msg" process to support AVDECC control.
* This code provides the means for them to do so.
*
* It provides proxy functions for the process to call.  The arguments
* for those calls are packed into messages, which are unpacked in the
* avdecc_msg process and then used to call the real functions.
*
* Current IPC uses unix sockets.  Can change this by creating a new
* implementations in openavb_avdecc_msg_client.c and openavb_avdecc_msg_server.c
*/

#include <stdlib.h>
#include <string.h>

#include "openavb_platform.h"

#define	AVB_LOG_COMPONENT	"AVDECC Msg"
#include "openavb_pub.h"
#include "openavb_log.h"

#include "openavb_trace.h"
#include "openavb_avdecc_msg_client.h"
#include "openavb_tl.h"

// forward declarations
static bool openavbAvdeccMsgClntReceiveFromServer(int avdeccMsgHandle, openavbAvdeccMessage_t *msg);

// OSAL specific functions
#include "openavb_avdecc_msg_client_osal.c"

// AvdeccMsgStateListGet() support.
#include "openavb_avdecc_msg.c"

static bool openavbAvdeccMsgClntReceiveFromServer(int avdeccMsgHandle, openavbAvdeccMessage_t *msg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (!msg || avdeccMsgHandle == AVB_AVDECC_MSG_HANDLE_INVALID) {
		AVB_LOG_ERROR("Client receive; invalid argument passed");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return FALSE;
	}

	switch(msg->type) {
		case OPENAVB_AVDECC_MSG_VERSION_CALLBACK:
			AVB_LOG_DEBUG("Message received:  OPENAVB_AVDECC_MSG_VERSION_CALLBACK");
			openavbAvdeccMsgClntCheckVerMatchesSrvr(avdeccMsgHandle, msg->params.versionCallback.AVBVersion);
			break;
		case OPENAVB_AVDECC_MSG_LISTENER_CHANGE_REQUEST:
			AVB_LOG_DEBUG("Message received:  OPENAVB_AVDECC_MSG_LISTENER_CHANGE_REQUEST");
			openavbAvdeccMsgClntHndlListenerChangeRequestFromServer(avdeccMsgHandle, msg->params.listenerChangeRequest.desired_state);
			break;
		default:
			AVB_LOG_ERROR("Client receive: unexpected message");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
			return FALSE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return TRUE;
}

bool openavbAvdeccMsgClntRequestVersionFromServer(int avdeccMsgHandle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);
	openavbAvdeccMessage_t msgBuf;

	memset(&msgBuf, 0, OPENAVB_AVDECC_MSG_LEN);
	msgBuf.type = OPENAVB_AVDECC_MSG_VERSION_REQUEST;
	bool ret = openavbAvdeccMsgClntSendToServer(avdeccMsgHandle, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return ret;
}

void openavbAvdeccMsgClntCheckVerMatchesSrvr(int avdeccMsgHandle, U32 AVBVersion)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	avdecc_msg_state_t *pState = AvdeccMsgStateListGet(avdeccMsgHandle);
	if (!pState) {
		AVB_LOGF_ERROR("avdeccMsgHandle %d not valid", avdeccMsgHandle);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return;
	}

	if (AVBVersion == AVB_CORE_VER_FULL) {
		pState->verState = OPENAVB_AVDECC_MSG_VER_VALID;
		AVB_LOG_DEBUG("AVDECC Versions Match");
	}
	else {
		pState->verState = OPENAVB_AVDECC_MSG_VER_INVALID;
		AVB_LOG_WARNING("AVDECC Versions Do Not Match");
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
}

bool openavbAvdeccMsgClntInitListenerIdentify(int avdeccMsgHandle, U8 stream_src_mac[6], U8 stream_dest_mac[6], U16 stream_uid, U16 stream_vlan_id)
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
	msgBuf.type = OPENAVB_AVDECC_MSG_LISTENER_INIT_IDENTIFY;
	openavbAvdeccMsgParams_ListenerInitIdentify_t * pParams =
		&(msgBuf.params.listenerInitIdentify);
	memcpy(pParams->stream_src_mac, stream_src_mac, sizeof(pParams->stream_src_mac));
	memcpy(pParams->stream_dest_mac, stream_dest_mac, sizeof(pParams->stream_dest_mac));
	pParams->stream_uid = stream_uid;
	pParams->stream_vlan_id = stream_vlan_id;
	bool ret = openavbAvdeccMsgClntSendToServer(avdeccMsgHandle, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return ret;
}

bool openavbAvdeccMsgClntHndlListenerChangeRequestFromServer(int avdeccMsgHandle, openavbAvdeccMsgStateType_t desiredState)
{
	bool ret = false;
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	avdecc_msg_state_t *pState = AvdeccMsgStateListGet(avdeccMsgHandle);
	if (!pState || !pState->pTLState) {
		AVB_LOGF_ERROR("avdeccMsgHandle %d not valid", avdeccMsgHandle);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return false;
	}

	switch (desiredState) {
	case OPENAVB_AVDECC_MSG_STOPPED:
		// Stop requested.
		AVB_LOGF_INFO("Stop state requested for Listener %d", desiredState);
		if (pState->pTLState->bRunning) {
			if (openavbTLStop((tl_handle_t) pState->pTLState)) {
				// Notify server of state change.
				openavbAvdeccMsgClntListenerChangeNotification(avdeccMsgHandle, OPENAVB_AVDECC_MSG_STOPPED);
				ret = true;
			} else {
				// Notify server of issues.
				openavbAvdeccMsgClntListenerChangeNotification(avdeccMsgHandle, OPENAVB_AVDECC_MSG_UNKNOWN);
			}
		} else {
			// Notify server we are already in this state.
			openavbAvdeccMsgClntListenerChangeNotification(avdeccMsgHandle, OPENAVB_AVDECC_MSG_STOPPED);
			ret = true;
		}
		break;
	case OPENAVB_AVDECC_MSG_RUNNING:
		// Run requested.
		AVB_LOGF_INFO("Run state requested for Listener %d", desiredState);
		if (!(pState->pTLState->bRunning)) {
			if (openavbTLRun((tl_handle_t) pState->pTLState)) {
				// Notify server of state change.
				AVB_LOGF_INFO("Listener %d state changed to running", desiredState);
				openavbAvdeccMsgClntListenerChangeNotification(avdeccMsgHandle, OPENAVB_AVDECC_MSG_RUNNING);
				ret = true;
			} else {
				// Notify server of issues.
				openavbAvdeccMsgClntListenerChangeNotification(avdeccMsgHandle, OPENAVB_AVDECC_MSG_UNKNOWN);
			}
		} else {
			// Notify server we are already in this state.
			AVB_LOGF_WARNING("Listener %d state is already at running", desiredState);
			openavbAvdeccMsgClntListenerChangeNotification(avdeccMsgHandle, OPENAVB_AVDECC_MSG_RUNNING);
			ret = true;
		}
		break;
	default:
		AVB_LOGF_ERROR("openavbAvdeccMsgClntHndlListenerChangeRequestFromServer invalid state %d", desiredState);
		break;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return ret;
}

bool openavbAvdeccMsgClntListenerChangeNotification(int avdeccMsgHandle, openavbAvdeccMsgStateType_t currentState)
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
	msgBuf.type = OPENAVB_AVDECC_MSG_LISTENER_CHANGE_NOTIFICATION;
	openavbAvdeccMsgParams_ListenerChangeNotification_t * pParams =
		&(msgBuf.params.listenerChangeNotification);
	pParams->current_state = currentState;
	bool ret = openavbAvdeccMsgClntSendToServer(avdeccMsgHandle, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return ret;
}


// Called from openavbAvdeccMsgThreadFn() which is started from openavbTLRun()
void openavbAvdeccMsgRunTalker(avdecc_msg_state_t *pState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (!pState) {
		AVB_LOG_ERROR("Invalid AVDECC Msg State");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return;
	}

	if (pState->bConnected) {

		// Do until we are stopped or loose connection to endpoint
		while (pState->pTLState->bRunning && pState->bConnected) {

			// Look for messages from AVDECC Msg.
			if (!openavbAvdeccMsgClntService(pState->avdeccMsgHandle, 1000)) {
				AVB_LOG_WARNING("Lost connection to AVDECC Msg");
				pState->bConnected = FALSE;
				pState->avdeccMsgHandle = AVB_AVDECC_MSG_HANDLE_INVALID;
			}
		}
	}
	else {
		AVB_LOG_WARNING("Failed to connect to AVDECC Msg");
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
}

// Called from openavbAvdeccMsgThreadFn() which is started from openavbTLRun()
void openavbAvdeccMsgRunListener(avdecc_msg_state_t *pState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (!pState && !pState->pTLState) {
		AVB_LOG_ERROR("Invalid AVDECC Msg State");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return;
	}

	openavb_tl_cfg_t * cfg = &(pState->pTLState->cfg);
	if (cfg->role != AVB_ROLE_LISTENER) {
		AVB_LOG_ERROR("openavbAvdeccMsgRunListener() passed a non-Listener state");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return;
	}

	if (pState->bConnected) {

		// Let the AVDECC Msg server know our identity.
		if (!openavbAvdeccMsgClntInitListenerIdentify(pState->avdeccMsgHandle,
			cfg->stream_addr.buffer.ether_addr_octet, cfg->dest_addr.buffer.ether_addr_octet,
			cfg->stream_uid, cfg->vlan_id)) {
			AVB_LOG_ERROR("openavbAvdeccMsgClntInitListenerIdentify() failed");
		}
		else {
			// Let the AVDECC Msg server know our current state.
			if (!openavbAvdeccMsgClntListenerChangeNotification(pState->avdeccMsgHandle,
				(pState->pTLState->bRunning ? OPENAVB_AVDECC_MSG_RUNNING : OPENAVB_AVDECC_MSG_STOPPED))) {
				AVB_LOG_ERROR("Initial openavbAvdeccMsgClntListenerChangeNotification() failed");
			}
			else {
				// Do until we are stopped or loose connection to endpoint
				while (pState->pTLState->bRunning && pState->bConnected) {

					// Look for messages from AVDECC Msg.
					if (!openavbAvdeccMsgClntService(pState->avdeccMsgHandle, 1000)) {
						AVB_LOG_WARNING("Lost connection to AVDECC Msg");
						pState->bConnected = FALSE;
						pState->avdeccMsgHandle = AVB_AVDECC_MSG_HANDLE_INVALID;
					}
				}
			}
		}
	}
	else {
		AVB_LOG_WARNING("Failed to connect to AVDECC Msg");
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
}

// AVDECC Msg thread function
void* openavbAvdeccMsgThreadFn(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);
	avdecc_msg_state_t avdeccMsgState;

	// Perform the base initialization.
	openavbAvdeccMsgInitialize();

	// Initialize our local state structure.
	memset(&avdeccMsgState, 0, sizeof(avdeccMsgState));
	avdeccMsgState.pTLState = (tl_state_t *)pv;

	// Wait until the TLState endpointHandle value is assigned, and then use the same handle value.
	// endpointHandle will be assigned by openavbTLThreadFn().
	while (!avdeccMsgState.endpointHandle && avdeccMsgState.pTLState->bRunning) {
		SLEEP_MSEC(1);

		TL_LOCK();
		avdeccMsgState.endpointHandle = avdeccMsgState.pTLState->endpointHandle;
		TL_UNLOCK();
	}

	avdeccMsgState.avdeccMsgHandle = AVB_AVDECC_MSG_HANDLE_INVALID;
	while (avdeccMsgState.pTLState->bRunning) {
		AVB_TRACE_LINE(AVB_TRACE_AVDECC_MSG_DETAIL);

		if (avdeccMsgState.avdeccMsgHandle == AVB_AVDECC_MSG_HANDLE_INVALID) {
			avdeccMsgState.avdeccMsgHandle = openavbAvdeccMsgClntOpenSrvrConnection();
			if (avdeccMsgState.avdeccMsgHandle == AVB_AVDECC_MSG_HANDLE_INVALID) {
				// error connecting to AVDECC Msg, already logged
				SLEEP(1);
				continue;
			}
		}
		AvdeccMsgStateListAdd(&avdeccMsgState);

		// Validate the AVB version for TL and Endpoint are the same before continuing
		avdeccMsgState.verState = OPENAVB_AVDECC_MSG_VER_UNKNOWN;
		avdeccMsgState.bConnected = openavbAvdeccMsgClntRequestVersionFromServer(avdeccMsgState.avdeccMsgHandle);
		if (avdeccMsgState.pTLState->bRunning && avdeccMsgState.bConnected && avdeccMsgState.verState == OPENAVB_AVDECC_MSG_VER_UNKNOWN) {
			// Check for AVDECC Msg version message. Timeout in 500 msec.
			if (!openavbAvdeccMsgClntService(avdeccMsgState.avdeccMsgHandle, 500)) {
				AVB_LOG_WARNING("Lost connection to AVDECC Msg, will retry");
				avdeccMsgState.bConnected = FALSE;
				avdeccMsgState.avdeccMsgHandle = AVB_AVDECC_MSG_HANDLE_INVALID;
			}
		}
		if (avdeccMsgState.verState == OPENAVB_AVDECC_MSG_VER_UNKNOWN) {
			AVB_LOG_ERROR("AVB core version not reported by AVDECC Msg server.  Will reconnect to AVDECC Msg and check again.");
		} else if (avdeccMsgState.verState == OPENAVB_AVDECC_MSG_VER_INVALID) {
			AVB_LOG_ERROR("AVB core version is different than AVDECC Msg AVB core version.  Will reconnect to AVDECC Msg and check again.");
		} else {
			AVB_LOG_DEBUG("AVB core version matches AVDECC Msg AVB core version.");
		}

		if (avdeccMsgState.bConnected && avdeccMsgState.verState == OPENAVB_AVDECC_MSG_VER_VALID) {
			if (avdeccMsgState.pTLState->cfg.role == AVB_ROLE_TALKER) {
				openavbAvdeccMsgRunTalker(&avdeccMsgState);
			}
			else {
				openavbAvdeccMsgRunListener(&avdeccMsgState);
			}

			// Close the endpoint connection. unless connection already gone in which case the socket could already be reused.
			if (avdeccMsgState.bConnected) {
				AvdeccMsgStateListRemove(&avdeccMsgState);
				openavbAvdeccMsgClntCloseSrvrConnection(avdeccMsgState.avdeccMsgHandle);
				avdeccMsgState.bConnected = FALSE;
				avdeccMsgState.avdeccMsgHandle = AVB_AVDECC_MSG_HANDLE_INVALID;
			}
		}

		if (avdeccMsgState.pTLState->bRunning && avdeccMsgState.verState == OPENAVB_AVDECC_MSG_VER_VALID) {
			SLEEP(1);
		}
	}

	// Close the endpoint connection. unless connection already gone in which case the socket could already be reused.
	if (avdeccMsgState.bConnected) {
		AvdeccMsgStateListRemove(&avdeccMsgState);
		openavbAvdeccMsgClntCloseSrvrConnection(avdeccMsgState.avdeccMsgHandle);
		avdeccMsgState.bConnected = FALSE;
		avdeccMsgState.avdeccMsgHandle = AVB_AVDECC_MSG_HANDLE_INVALID;
	}

	avdeccMsgState.endpointHandle = 0;
	avdeccMsgState.pTLState = NULL;

	// Perform the base cleanup.
	openavbAvdeccMsgCleanup();

	THREAD_JOINABLE(avdeccMsgState.pTLState->avdeccMsgThread);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return NULL;
}
