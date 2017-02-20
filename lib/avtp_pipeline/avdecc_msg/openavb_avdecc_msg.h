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
* HEADER SUMMARY : Declarations used by the AVDECC message code.
*/

#ifndef OPENAVB_AVDECC_MSG_H
#define OPENAVB_AVDECC_MSG_H

#include "openavb_types.h"
#include "openavb_tl.h"

#define AVB_AVDECC_MSG_HANDLE_INVALID	(-1)
#define AVDECC_MSG_RECONNECT_SECONDS 	10
#define AVB_AVDECC_MSG_UNIX_PATH 		"/tmp/avdecc_msg"
#define MAX_AVDECC_MSG_CLIENTS			16


////////////////
// AVDECC Msg state mutex
////////////////
extern MUTEX_HANDLE(gAvdeccMsgStateMutex);
#define AVDECC_MSG_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(gAvdeccMsgStateMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define AVDECC_MSG_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(gAvdeccMsgStateMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }


typedef enum OPENAVB_AVDECC_MSG_VER_STATE
{
	OPENAVB_AVDECC_MSG_VER_UNKNOWN = 0,
	OPENAVB_AVDECC_MSG_VER_INVALID,
	OPENAVB_AVDECC_MSG_VER_VALID,
} openavbAvdeccMsgVerState_t;

typedef struct {

	// Connected to AVDECC Msg flag. (assumed atomic)
	bool bConnected;

	// The status of the version check to make sure AVDECC Msg and TL are running the same version.
	openavbAvdeccMsgVerState_t verState;

	// Configuration settings. (Values are set once from a single thread no lock needed).
	openavb_tl_cfg_t cfg;

	// Handle to the endpoint linked to this AVDECC Msg instance.
	int endpointHandle;

	// Pointer to the talker/listener state linked to the AVDECC Msg instance.
	tl_state_t *pTLState;

	// Handle to the AVDECC Msg socket for the connection to the server.
	int socketHandle;

} avdecc_msg_state_t;


bool openavbAvdeccMsgInitialize(void);
bool openavbAvdeccMsgCleanup();

// Functions to save the list of AVDECC Msg clients (client-side support only)
bool AvdeccMsgStateListAdd(avdecc_msg_state_t * pState);
bool AvdeccMsgStateListRemove(avdecc_msg_state_t * pState);
avdecc_msg_state_t * AvdeccMsgStateListGet(int avdeccMsgHandle);

bool openavbAvdeccMsgClntService(int h, int timeout);
void openavbAvdeccMsgSrvrService(void);


typedef enum {
	OPENAVB_AVDECC_MSG_PLAY,
	OPENAVB_AVDECC_MSG_PAUSE,
	OPENAVB_AVDECC_MSG_CLOSE,
} openavbAvdeccMsgStateType_t;

typedef enum {
	// client-to-server messages
	OPENAVB_AVDECC_MSG_VERSION_REQUEST,
	OPENAVB_AVDECC_MSG_LISTENER_INIT_IDENTIFY,
	OPENAVB_AVDECC_MSG_LISTENER_CHANGE_NOTIFICATION,

	// server-to-client messages
	OPENAVB_AVDECC_MSG_VERSION_CALLBACK,
	OPENAVB_AVDECC_MSG_LISTENER_CHANGE_REQUEST,
} openavbAvdeccMsgType_t;

//////////////////////////////
// Client-to-Server message parameters
//////////////////////////////
typedef struct {
} openavbAvdeccMsgParams_VersionRequest_t;

typedef struct {
	U8 stream_src_mac[6]; // MAC Address for the Talker
	U8 stream_dest_mac[6]; // Multicast address for the stream
	U16 stream_uid; // Stream ID value
	U16 stream_vlan_id; // VLAN ID of the stream
} openavbAvdeccMsgParams_ListenerInitIdentify_t;

typedef struct {
	openavbAvdeccMsgStateType_t current_state;
} openavbAvdeccMsgParams_ListenerChangeNotification_t;

//////////////////////////////
// Server-to-Client messages parameters
//////////////////////////////
typedef struct {
	U32 		AVBVersion;
} openavbAvdeccMsgParams_VersionCallback_t;

typedef struct {
	openavbAvdeccMsgStateType_t desired_state;
} openavbAvdeccMsgParams_ListenerChangeRequest_t;

#define OPENAVB_AVDECC_MSG_LEN sizeof(openavbAvdeccMessage_t)

bool openavbAvdeccMsgServerOpen(void);
void openavbAvdeccMsgServerClose(void);

#include "openavb_avdecc_msg_osal.h"


/**************************  AVDECC Msg Client-Server  *************************/
// There is a single AVDECC server for the entire talker/listener device
// There is an AVDECC client for each stream.

// AVDECC client open a connection to AVDECC server.
// (Note that the server never opens a connection to the client.)
int  openavbAvdeccMsgClntOpenSrvrConnection(void);
// AVDECC client close its connection to the server.
void openavbAvdeccMsgClntCloseSrvrConnection(int h);
// AVDECC server close its connection to the client.
void openavbAvdeccMsgSrvrCloseClientConnection(int h);

// Immediately after establishing connection with the AVDECC Msg server,
// the AVDECC Msg client checks that the server's version is the same as its own.
// The client sends a request to the client for its version.
// The server handles the request and sends its version to the requesting client.
// The client compares the received version to its own.
bool openavbAvdeccMsgClntRequestVersionFromServer(int h);
bool openavbAvdeccMsgSrvrHndlVerRqstFromClient(int h);
void openavbAvdeccMsgSrvrSendServerVersionToClient(int h, U32 AVBVersion);
void openavbAvdeccMsgClntCheckVerMatchesSrvr(int h, U32 AVBVersion);

// Client notify the server of identity (so AVDECC Msg knows client identity)
bool openavbAvdeccMsgClientInitListenerIdentify(int avdeccMsgHandle, U8 stream_src_mac[6], U8 stream_dest_mac[6], U16 stream_uid, U16 stream_vlan_id);
bool openavbAvdeccMsgSrvrHndlListenerInitIdentifyFromClient(int avdeccMsgHandle, U8 stream_src_mac[6], U8 stream_dest_mac[6], U16 stream_uid, U16 stream_vlan_id);

// Server state change requests, and client notifications of state changes.
bool openavbAvdeccMsgSrvrListenerChangeRequest(int avdeccMsgHandle, openavbAvdeccMsgStateType_t desiredState);
bool openavbAvdeccMsgClntHndlListenerChangeRequestFromServer(int avdeccMsgHandle, openavbAvdeccMsgStateType_t desiredState);
bool openavbAvdeccMsgClntListenerChangeNotification(int avdeccMsgHandle, openavbAvdeccMsgStateType_t currentState);
bool openavbAvdeccMsgSrvrHndlListenerChangeNotificationFromClient(int avdeccMsgHandle, openavbAvdeccMsgStateType_t currentState);

#endif // OPENAVB_AVDECC_MSG_H
