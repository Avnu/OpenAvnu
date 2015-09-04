/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
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
* "endpoint" process to create a reservation for their traffic.
* This code provides the means for them to do so.
*
* This source file is compiled into the streamer executable.
* 
* It provides proxy functions for the streamer to call.  The arguments
* for those calls are packed into messages, which are unpacked in the
* endpoint process and then used to call the real functions.
*
* Current IPC uses unix sockets.  Can change this by creating a new
* implementations in openavb_enpoint_client.c and openavb_endpoint_server.c
*/

#include <stdlib.h>
#include <string.h>

#include "openavb_platform.h"

#include "openavb_trace.h"
#include "openavb_endpoint.h"

#define	AVB_LOG_COMPONENT	"Endpoint"
#include "openavb_pub.h"
#include "openavb_log.h"

// forward declarations
static bool openavbEptClntReceiveFromServer(int h, openavbEndpointMessage_t *msg);

// OSAL specific functions for openavb_endpoint_client.c
#include "openavb_endpoint_client_osal.c"

static bool openavbEptClntReceiveFromServer(int h, openavbEndpointMessage_t *msg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	if (!msg || h == AVB_ENDPOINT_HANDLE_INVALID) {
		AVB_LOG_ERROR("Client receive; invalid argument passed");
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return FALSE;
	}

	switch(msg->type) {
		case OPENAVB_ENDPOINT_TALKER_CALLBACK:
			openavbEptClntNotifyTlkrOfSrpCb(h,
			                           &msg->streamID,
			                            msg->params.talkerCallback.ifname,
			                            msg->params.talkerCallback.destAddr,
			                            msg->params.talkerCallback.lsnrDecl,
			                            msg->params.talkerCallback.classRate,
			                            msg->params.talkerCallback.vlanID,
			                            msg->params.talkerCallback.priority,
			                            msg->params.talkerCallback.fwmark);
			break;
		case OPENAVB_ENDPOINT_LISTENER_CALLBACK:
			openavbEptClntNotifyLstnrOfSrpCb(h, 
			                            &msg->streamID,
			                             msg->params.listenerCallback.ifname,
			                             msg->params.listenerCallback.destAddr,
			                             msg->params.listenerCallback.tlkrDecl,
			                            &msg->params.listenerCallback.tSpec,
			                             msg->params.listenerCallback.srClass,
			                             msg->params.listenerCallback.latency,
			                            &msg->params.listenerCallback.failInfo);
			break;
		case OPENAVB_ENDPOINT_VERSION_CALLBACK:
			openavbEptClntCheckVerMatchesSrvr(h, msg->params.versionCallback.AVBVersion);
			break;
		default:
			AVB_LOG_ERROR("Client receive: unexpected message");
			AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
			return FALSE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return TRUE;
}

bool openavbEptClntRegisterStream(int h,
                              AVBStreamID_t *streamID,
                              U8 destAddr[],
                              AVBTSpec_t *tSpec,
                              U8 srClass,
                              U8 srRank,
                              U32 latency)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	openavbEndpointMessage_t msgBuf;

	// Check for valid parameters. destAddr is optional and checked later in this function before using.
	if (!streamID || !tSpec) {
		AVB_LOG_ERROR("Client register: invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return FALSE;
	}

	memset(&msgBuf, 0, OPENAVB_ENDPOINT_MSG_LEN);
	msgBuf.type = OPENAVB_ENDPOINT_TALKER_REGISTER;
	memcpy(&(msgBuf.streamID), streamID, sizeof(AVBStreamID_t));
	if (destAddr)
		memcpy(msgBuf.params.talkerRegister.destAddr, destAddr, ETH_ALEN);
	msgBuf.params.talkerRegister.tSpec.maxFrameSize = tSpec->maxFrameSize;
	msgBuf.params.talkerRegister.tSpec.maxIntervalFrames = tSpec->maxIntervalFrames;
	msgBuf.params.talkerRegister.srClass = srClass;
	msgBuf.params.talkerRegister.srRank = srRank;
	msgBuf.params.talkerRegister.latency = latency;
	bool ret = openavbEptClntSendToServer(h, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return ret;
}

bool openavbEptClntAttachStream(int h, AVBStreamID_t *streamID,
                            openavbSrpLsnrDeclSubtype_t ld)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	openavbEndpointMessage_t msgBuf;

	if (!streamID) {
		AVB_LOG_ERROR("Client attach: invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return FALSE;
	}
	
	memset(&msgBuf, 0, OPENAVB_ENDPOINT_MSG_LEN);
	msgBuf.type = OPENAVB_ENDPOINT_LISTENER_ATTACH;
	memcpy(&(msgBuf.streamID), streamID, sizeof(AVBStreamID_t));
	msgBuf.params.listenerAttach.lsnrDecl = ld;
	bool ret = openavbEptClntSendToServer(h, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return ret;
}

bool openavbEptClntStopStream(int h, AVBStreamID_t *streamID)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	openavbEndpointMessage_t msgBuf;

	if (!streamID) {
		AVB_LOG_ERROR("Client stop: invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return FALSE;
	}

	memset(&msgBuf, 0, OPENAVB_ENDPOINT_MSG_LEN);
	msgBuf.type = OPENAVB_ENDPOINT_CLIENT_STOP;
	memcpy(&(msgBuf.streamID), streamID, sizeof(AVBStreamID_t));
	bool ret = openavbEptClntSendToServer(h, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return ret;
}
 
bool openavbEptClntRequestVersionFromServer(int h)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	openavbEndpointMessage_t msgBuf;

	memset(&msgBuf, 0, OPENAVB_ENDPOINT_MSG_LEN);
	msgBuf.type = OPENAVB_ENDPOINT_VERSION_REQUEST;
	bool ret = openavbEptClntSendToServer(h, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return ret;
}
 
