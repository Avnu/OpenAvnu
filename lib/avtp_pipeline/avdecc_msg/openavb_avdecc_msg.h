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
* HEADER SUMMARY : Declarations used by the AVDECC message code.
*/

#ifndef OPENAVB_AVDECC_MSG_H
#define OPENAVB_AVDECC_MSG_H

#include "openavb_types.h"

#define AVB_AVDECC_MSG_HANDLE_INVALID	(-1)
#define AVDECC_MSG_RECONNECT_SECONDS	10
#define AVB_AVDECC_MSG_UNIX_PATH		"/tmp/avdecc_msg"
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

struct _avdecc_msg_state;
typedef struct _avdecc_msg_state avdecc_msg_state_t;


bool openavbAvdeccMsgInitialize(void);
bool openavbAvdeccMsgCleanup();

// Functions to save the list of AVDECC Msg clients
bool AvdeccMsgStateListAdd(avdecc_msg_state_t * pState);
bool AvdeccMsgStateListRemove(avdecc_msg_state_t * pState);
avdecc_msg_state_t * AvdeccMsgStateListGet(int avdeccMsgHandle);
avdecc_msg_state_t * AvdeccMsgStateListGetIndex(int nIndex);

bool openavbAvdeccMsgClntService(int socketHandle, int timeout);
void openavbAvdeccMsgSrvrService(void);


typedef enum {
	OPENAVB_AVDECC_MSG_UNKNOWN = 0,
	OPENAVB_AVDECC_MSG_STOPPED,
	OPENAVB_AVDECC_MSG_STOPPED_UNEXPECTEDLY,
	OPENAVB_AVDECC_MSG_RUNNING,
	OPENAVB_AVDECC_MSG_PAUSED,
} openavbAvdeccMsgStateType_t;

typedef enum {
	// Client-to-Server messages
	OPENAVB_AVDECC_MSG_VERSION_REQUEST,
	OPENAVB_AVDECC_MSG_CLIENT_INIT_IDENTIFY,
	OPENAVB_AVDECC_MSG_C2S_TALKER_STREAM_ID,
	OPENAVB_AVDECC_MSG_CLIENT_CHANGE_NOTIFICATION,

	// Server-to-Client messages
	OPENAVB_AVDECC_MSG_VERSION_CALLBACK,
	OPENAVB_AVDECC_MSG_LISTENER_STREAM_ID,
	OPENAVB_AVDECC_MSG_S2C_TALKER_STREAM_ID,
	OPENAVB_AVDECC_MSG_CLIENT_CHANGE_REQUEST,
} openavbAvdeccMsgType_t;

//////////////////////////////
// Client-to-Server message parameters
//////////////////////////////
typedef struct {
} openavbAvdeccMsgParams_VersionRequest_t;

typedef struct {
	char friendly_name[FRIENDLY_NAME_SIZE];
	U8 talker;
} openavbAvdeccMsgParams_ClientInitIdentify_t;

typedef struct {
	U8 sr_class; // SR_CLASS_A or SR_CLASS_B
	U8 stream_src_mac[6]; // MAC Address for the Talker
	U16 stream_uid; // Stream ID value
	U8 stream_dest_mac[6]; // Multicast address for the stream
	U16 stream_vlan_id; // VLAN ID of the stream
} openavbAvdeccMsgParams_C2S_TalkerStreamID_t;

typedef struct {
	U8 current_state; // Convert to openavbAvdeccMsgStateType_t
} openavbAvdeccMsgParams_ClientChangeNotification_t;

//////////////////////////////
// Server-to-Client messages parameters
//////////////////////////////
typedef struct {
	U32 AVBVersion;
} openavbAvdeccMsgParams_VersionCallback_t;

typedef struct {
	U8 sr_class; // SR_CLASS_A or SR_CLASS_B
	U8 stream_src_mac[6]; // MAC Address for the Talker
	U16 stream_uid; // Stream ID value
	U8 stream_dest_mac[6]; // Multicast address for the stream
	U16 stream_vlan_id; // VLAN ID of the stream
} openavbAvdeccMsgParams_ListenerStreamID_t;

typedef struct {
	U8 sr_class; // SR_CLASS_A or SR_CLASS_B
	U8 stream_id_valid; // The stream_src_mac and stream_uid values were supplied
	U8 stream_src_mac[6]; // MAC Address for the Talker
	U16 stream_uid; // Stream ID value
	U8 stream_dest_valid; // The stream_dest_mac value was supplied
	U8 stream_dest_mac[6]; // Multicast address for the stream
	U8 stream_vlan_id_valid; // The stream_vlan_id value was supplied
	U16 stream_vlan_id; // VLAN ID of the stream
} openavbAvdeccMsgParams_S2C_TalkerStreamID_t;

typedef struct {
	U8 desired_state; // Convert to openavbAvdeccMsgStateType_t
} openavbAvdeccMsgParams_ClientChangeRequest_t;

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
bool openavbAvdeccMsgClntInitIdentify(int avdeccMsgHandle, const char * friendly_name, U8 talker);
bool openavbAvdeccMsgSrvrHndlInitIdentifyFromClient(int avdeccMsgHandle, char * friendly_name, U8 talker);

// Client notify the server of the stream values the Talker will be using.
bool openavbAvdeccMsgClntTalkerStreamID(int avdeccMsgHandle, U8 sr_class, const U8 stream_src_mac[6], U16 stream_uid, const U8 stream_dest_mac[6], U16 stream_vlan_id);
bool openavbAvdeccMsgSrvrHndlTalkerStreamIDFromClient(int avdeccMsgHandle, U8 sr_class, const U8 stream_src_mac[6], U16 stream_uid, const U8 stream_dest_mac[6], U16 stream_vlan_id);

// Server notify the client of the stream values for the Listener to use.
bool openavbAvdeccMsgSrvrListenerStreamID(int avdeccMsgHandle, U8 sr_class, const U8 stream_src_mac[6], U16 stream_uid, const U8 stream_dest_mac[6], U16 stream_vlan_id);
bool openavbAvdeccMsgClntHndlListenerStreamIDFromServer(int avdeccMsgHandle, U8 sr_class, const U8 stream_src_mac[6], U16 stream_uid, const U8 stream_dest_mac[6], U16 stream_vlan_id);

// Server notify the client of the stream values for the Talker to use (supplied by the AVDECC controller).
bool openavbAvdeccMsgSrvrTalkerStreamID(
	int avdeccMsgHandle,
	U8 sr_class,
	U8 stream_id_valid, const U8 stream_src_mac[6], U16 stream_uid,
	U8 stream_dest_valid, const U8 stream_dest_mac[6],
	U8 stream_vlan_id_valid, U16 stream_vlan_id);
bool openavbAvdeccMsgClntHndlTalkerStreamIDFromServer(
	int avdeccMsgHandle,
	U8 sr_class,
	U8 stream_id_valid, const U8 stream_src_mac[6], U16 stream_uid,
	U8 stream_dest_valid, const U8 stream_dest_mac[6],
	U8 stream_vlan_id_valid, U16 stream_vlan_id);

// Server state change requests, and client notifications of state changes.
bool openavbAvdeccMsgSrvrChangeRequest(int avdeccMsgHandle, openavbAvdeccMsgStateType_t desiredState);
bool openavbAvdeccMsgClntHndlChangeRequestFromServer(int avdeccMsgHandle, openavbAvdeccMsgStateType_t desiredState);
bool openavbAvdeccMsgClntChangeNotification(int avdeccMsgHandle, openavbAvdeccMsgStateType_t currentState);
bool openavbAvdeccMsgSrvrHndlChangeNotificationFromClient(int avdeccMsgHandle, openavbAvdeccMsgStateType_t currentState);

#endif // OPENAVB_AVDECC_MSG_H
