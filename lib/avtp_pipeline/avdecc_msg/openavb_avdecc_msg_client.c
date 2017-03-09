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
#include "openavb_listener.h"

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

bool openavbAvdeccMsgClntListenerInitIdentify(int avdeccMsgHandle, const char * friendly_name)
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
	strncpy(pParams->friendly_name, friendly_name, sizeof(pParams->friendly_name));
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
		AVB_LOGF_DEBUG("Stop state requested for Listener %d", avdeccMsgHandle);
		if (pState->pTLState->bRunning) {
			if (openavbTLStop((tl_handle_t) pState->pTLState)) {
				// NOTE:  openavbTLStop() call will cause listener change notification if successful.
				AVB_LOGF_INFO("Listener %d state changed to stopped", desiredState);
				ret = true;
			} else {
				// Notify server of issues.
				openavbAvdeccMsgClntListenerChangeNotification(avdeccMsgHandle, OPENAVB_AVDECC_MSG_UNKNOWN);
			}
		} else {
			// Notify server we are already in this state.
			AVB_LOGF_WARNING("Listener %d state is already at stopped", avdeccMsgHandle);
			openavbAvdeccMsgClntListenerChangeNotification(avdeccMsgHandle, OPENAVB_AVDECC_MSG_STOPPED);
			ret = true;
		}
		break;

	case OPENAVB_AVDECC_MSG_RUNNING:
		// Run requested.
		AVB_LOGF_DEBUG("Run state requested for Listener %d", avdeccMsgHandle);
		if (!(pState->pTLState->bRunning)) {
			if (openavbTLRun((tl_handle_t) pState->pTLState)) {
				// NOTE:  openavbTLRun() call will cause listener change notification if successful.
				AVB_LOGF_INFO("Listener %d state changed to running", desiredState);
				ret = true;
			} else {
				// Notify server of issues.
				openavbAvdeccMsgClntListenerChangeNotification(avdeccMsgHandle, OPENAVB_AVDECC_MSG_UNKNOWN);
			}
		}
		else if (pState->pTLState->bRunning && pState->pTLState->bPaused) {
			openavbTLPauseListener(pState->pTLState, FALSE);
			// NOTE:  openavbTLPauseListener() call will cause listener change notification.
			AVB_LOGF_INFO("Listener %d state changed from paused to running", avdeccMsgHandle);
			ret = true;
		}
		else {
			// Notify server we are already in this state.
			AVB_LOGF_WARNING("Listener %d state is already at running", avdeccMsgHandle);
			openavbAvdeccMsgClntListenerChangeNotification(avdeccMsgHandle, OPENAVB_AVDECC_MSG_RUNNING);
			ret = true;
		}
		break;

	case OPENAVB_AVDECC_MSG_PAUSED:
		// Running with Pause requested.
		AVB_LOGF_DEBUG("Paused state requested for Listener %d", avdeccMsgHandle);
		if (!(pState->pTLState->bRunning)) {
			// Notify server of issues.
			AVB_LOGF_ERROR("Listener %d attempted to pause the stream while not running.", avdeccMsgHandle);
			openavbAvdeccMsgClntListenerChangeNotification(avdeccMsgHandle, OPENAVB_AVDECC_MSG_UNKNOWN);
		}
		else if (pState->pTLState->bRunning && !(pState->pTLState->bPaused)) {
			openavbTLPauseListener(pState->pTLState, TRUE);
			// NOTE:  openavbTLPauseListener() call will cause listener change notification.
			AVB_LOGF_INFO("Listener %d state changed from running to paused", avdeccMsgHandle);
			ret = true;
		}
		else {
			// Notify server we are already in this state.
			AVB_LOGF_WARNING("Listener %d state is already at paused", avdeccMsgHandle);
			openavbAvdeccMsgClntListenerChangeNotification(avdeccMsgHandle, OPENAVB_AVDECC_MSG_PAUSED);
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

		// Do until we are stopped or lose connection to AVDECC Msg server.
		while (pState->pTLState->bAvdeccMsgRunning && pState->bConnected) {

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
		if (!openavbAvdeccMsgClntListenerInitIdentify(pState->avdeccMsgHandle, cfg->friendly_name)) {
			AVB_LOG_ERROR("openavbAvdeccMsgClntListenerInitIdentify() failed");
		}
		else {
			// Let the AVDECC Msg server know our current state.
			if (!openavbAvdeccMsgClntListenerChangeNotification(pState->avdeccMsgHandle,
				(pState->pTLState->bRunning ?
					(pState->pTLState->bPaused ? OPENAVB_AVDECC_MSG_PAUSED : OPENAVB_AVDECC_MSG_RUNNING ) :
					OPENAVB_AVDECC_MSG_STOPPED))) {
				AVB_LOG_ERROR("Initial openavbAvdeccMsgClntListenerChangeNotification() failed");
			}
			else {
				// Do until we are stopped or lose connection to AVDECC Msg.
				while (pState->pTLState->bAvdeccMsgRunning && pState->bConnected) {

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

	avdeccMsgState.avdeccMsgHandle = AVB_AVDECC_MSG_HANDLE_INVALID;
	while (avdeccMsgState.pTLState->bAvdeccMsgRunning) {
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

		// Validate the AVB version for client and server are the same before continuing
		avdeccMsgState.verState = OPENAVB_AVDECC_MSG_VER_UNKNOWN;
		avdeccMsgState.bConnected = openavbAvdeccMsgClntRequestVersionFromServer(avdeccMsgState.avdeccMsgHandle);
		if (avdeccMsgState.pTLState->bAvdeccMsgRunning && avdeccMsgState.bConnected && avdeccMsgState.verState == OPENAVB_AVDECC_MSG_VER_UNKNOWN) {
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
		}

		// Close the AVDECC Msg connection.
		AvdeccMsgStateListRemove(&avdeccMsgState);
		openavbAvdeccMsgClntCloseSrvrConnection(avdeccMsgState.avdeccMsgHandle);
		avdeccMsgState.bConnected = FALSE;
		avdeccMsgState.avdeccMsgHandle = AVB_AVDECC_MSG_HANDLE_INVALID;

		if (avdeccMsgState.pTLState->bAvdeccMsgRunning && avdeccMsgState.verState == OPENAVB_AVDECC_MSG_VER_VALID) {
			SLEEP(1);
		}
	}

	avdeccMsgState.pTLState = NULL;

	// Perform the base cleanup.
	openavbAvdeccMsgCleanup();

	THREAD_JOINABLE(avdeccMsgState.pTLState->avdeccMsgThread);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return NULL;
}


// Client-side helper function.
bool openavbAvdeccMsgClntNotifyCurrentState(tl_state_t *pTLState)
{
	if (!pTLState) { return FALSE; }
	if (!(pTLState->bAvdeccMsgRunning)) { return FALSE; }

	// Find the AVDECC Msg for the supplied tl_state_t pointer.
	int i;
	for (i = 0; i < MAX_AVDECC_MSG_CLIENTS; ++i) {
		avdecc_msg_state_t * pAvdeccMsgState = AvdeccMsgStateListGetIndex(i);
		if (!pAvdeccMsgState) {
			// Out of items.
			break;
		}
		if (pAvdeccMsgState->pTLState == pTLState) {
			// Notify the server regarding the current state.
			if (pTLState->bRunning) {
				openavbAvdeccMsgClntListenerChangeNotification(pAvdeccMsgState->avdeccMsgHandle, (pTLState->bPaused ? OPENAVB_AVDECC_MSG_PAUSED : OPENAVB_AVDECC_MSG_RUNNING));
			} else {
				openavbAvdeccMsgClntListenerChangeNotification(pAvdeccMsgState->avdeccMsgHandle, OPENAVB_AVDECC_MSG_STOPPED);
			}
			return TRUE;
		}
	}

	return FALSE;
}
