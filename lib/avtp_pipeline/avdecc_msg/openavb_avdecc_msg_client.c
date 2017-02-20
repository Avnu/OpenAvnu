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

#include "openavb_trace.h"
#include "openavb_avdecc_msg.h"
#include "openavb_tl.h"

#define	AVB_LOG_COMPONENT	"AVDECC Msg Client"
#include "openavb_pub.h"
#include "openavb_log.h"

// forward declarations
static bool openavbAvdeccMsgClntReceiveFromServer(int h, openavbAvdeccMessage_t *msg);

// OSAL specific functions for openavb_avdecc_msg_client.c
#include "openavb_avdecc_msg_client_osal.c"

#define MAX_AVDECC_MSGS 16
avdecc_msg_state_t *gAvdeccMsgStateList[MAX_AVDECC_MSGS] = { 0 };

static bool openavbAvdeccMsgClntReceiveFromServer(int h, openavbAvdeccMessage_t *msg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (!msg || h == AVB_AVDECC_MSG_HANDLE_INVALID) {
		AVB_LOG_ERROR("Client receive; invalid argument passed");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return FALSE;
	}

	switch(msg->type) {
		case OPENAVB_AVDECC_MSG_VERSION_CALLBACK:
			AVB_LOG_DEBUG("Message received:  OPENAVB_AVDECC_MSG_VERSION_CALLBACK");
			openavbAvdeccMsgClntCheckVerMatchesSrvr(h, msg->params.versionCallback.AVBVersion);
			break;
		default:
			AVB_LOG_ERROR("Client receive: unexpected message");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
			return FALSE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return TRUE;
}

bool openavbAvdeccMsgClntRequestVersionFromServer(int h)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);
	openavbAvdeccMessage_t msgBuf;

	memset(&msgBuf, 0, OPENAVB_AVDECC_MSG_LEN);
	msgBuf.type = OPENAVB_AVDECC_MSG_VERSION_REQUEST;
	bool ret = openavbAvdeccMsgClntSendToServer(h, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return ret;
}

void openavbAvdeccMsgClntCheckVerMatchesSrvr(int avdeccMsgHandle, U32 AVBVersion)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	avdecc_msg_state_t *pState = AvdeccMsgStateListGet(avdeccMsgHandle);
	if (!pState) {
		AVB_LOGF_ERROR("avdeccMsgHandle %d not valid", avdeccMsgHandle);
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
			if (!openavbAvdeccMsgClntService(pState->socketHandle, 1000)) {
				AVB_LOG_WARNING("Lost connection to AVDECC Msg");
				pState->bConnected = FALSE;
				pState->socketHandle = AVB_AVDECC_MSG_HANDLE_INVALID;
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

	if (!pState) {
		AVB_LOG_ERROR("Invalid AVDECC Msg State");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return;
	}

	if (pState->bConnected) {

		// Do until we are stopped or loose connection to endpoint
		while (pState->pTLState->bRunning && pState->bConnected) {

			// Look for messages from AVDECC Msg.
			if (!openavbAvdeccMsgClntService(pState->socketHandle, 1000)) {
				AVB_LOG_WARNING("Lost connection to AVDECC Msg");
				pState->bConnected = FALSE;
				pState->socketHandle = AVB_AVDECC_MSG_HANDLE_INVALID;
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

	avdeccMsgState.socketHandle = AVB_AVDECC_MSG_HANDLE_INVALID;
	while (avdeccMsgState.pTLState->bRunning) {
		AVB_TRACE_LINE(AVB_TRACE_AVDECC_MSG_DETAIL);

		if (avdeccMsgState.socketHandle == AVB_AVDECC_MSG_HANDLE_INVALID) {
			avdeccMsgState.socketHandle = openavbAvdeccMsgClntOpenSrvrConnection();
			if (avdeccMsgState.socketHandle == AVB_AVDECC_MSG_HANDLE_INVALID) {
				// error connecting to AVDECC Msg, already logged
				SLEEP(1);
				continue;
			}
		}
		AvdeccMsgStateListAdd(&avdeccMsgState);

		// Validate the AVB version for TL and Endpoint are the same before continuing
		avdeccMsgState.verState = OPENAVB_AVDECC_MSG_VER_UNKNOWN;
		avdeccMsgState.bConnected = openavbAvdeccMsgClntRequestVersionFromServer(avdeccMsgState.socketHandle);
		if (avdeccMsgState.pTLState->bRunning && avdeccMsgState.bConnected && avdeccMsgState.verState == OPENAVB_AVDECC_MSG_VER_UNKNOWN) {
			// Check for AVDECC Msg version message. Timeout in 500 msec.
			if (!openavbAvdeccMsgClntService(avdeccMsgState.socketHandle, 500)) {
				AVB_LOG_WARNING("Lost connection to AVDECC Msg, will retry");
				avdeccMsgState.bConnected = FALSE;
				avdeccMsgState.socketHandle = AVB_AVDECC_MSG_HANDLE_INVALID;
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
			if (avdeccMsgState.cfg.role == AVB_ROLE_TALKER) {
				openavbAvdeccMsgRunTalker(&avdeccMsgState);
			}
			else {
				openavbAvdeccMsgRunListener(&avdeccMsgState);
			}

			// Close the endpoint connection. unless connection already gone in which case the socket could already be reused.
			if (avdeccMsgState.bConnected) {
				AvdeccMsgStateListRemove(&avdeccMsgState);
				openavbAvdeccMsgClntCloseSrvrConnection(avdeccMsgState.socketHandle);
				avdeccMsgState.bConnected = FALSE;
				avdeccMsgState.socketHandle = AVB_AVDECC_MSG_HANDLE_INVALID;
			}
		}

		if (avdeccMsgState.pTLState->bRunning && avdeccMsgState.verState == OPENAVB_AVDECC_MSG_VER_VALID) {
			SLEEP(1);
		}
	}

	// Close the endpoint connection. unless connection already gone in which case the socket could already be reused.
	if (avdeccMsgState.bConnected) {
		AvdeccMsgStateListRemove(&avdeccMsgState);
		openavbAvdeccMsgClntCloseSrvrConnection(avdeccMsgState.socketHandle);
		avdeccMsgState.bConnected = FALSE;
		avdeccMsgState.socketHandle = AVB_AVDECC_MSG_HANDLE_INVALID;
	}

	avdeccMsgState.endpointHandle = 0;
	avdeccMsgState.pTLState = NULL;

	THREAD_JOINABLE(avdeccMsgState.pTLState->avdeccMsgThread);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return NULL;
}


bool AvdeccMsgStateListAdd(avdecc_msg_state_t * pState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (!pState) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return FALSE;
	}

	TL_LOCK();
	int i1;
	for (i1 = 0; i1 < MAX_AVDECC_MSGS; i1++) {
		if (!gAvdeccMsgStateList[i1]) {
			gAvdeccMsgStateList[i1] = pState;
			TL_UNLOCK();
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
			return TRUE;
		}
	}
	TL_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return FALSE;
}

bool AvdeccMsgStateListRemove(avdecc_msg_state_t * pState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (!pState) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return FALSE;
	}

	TL_LOCK();
	int i1;
	for (i1 = 0; i1 < MAX_AVDECC_MSGS; i1++) {
		if (gAvdeccMsgStateList[i1] == pState) {
			gAvdeccMsgStateList[i1] = NULL;
			TL_UNLOCK();
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
			return TRUE;
		}
	}
	TL_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return FALSE;
}

avdecc_msg_state_t * AvdeccMsgStateListGet(int avdeccMsgHandle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (!avdeccMsgHandle) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return NULL;
	}

	TL_LOCK();
	int i1;
	for (i1 = 0; i1 < MAX_AVDECC_MSGS; i1++) {
		if (gAvdeccMsgStateList[i1]) {
			avdecc_msg_state_t *pState = (avdecc_msg_state_t *)gAvdeccMsgStateList[i1];
			if (pState->socketHandle == avdeccMsgHandle) {
				TL_UNLOCK();
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
				return pState;
			}
		}
	}
	TL_LOCK();
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return NULL;
}
