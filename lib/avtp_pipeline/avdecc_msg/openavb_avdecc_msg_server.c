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

#include "openavb_avdecc_msg.h"
#include "openavb_trace.h"

//#define AVB_LOG_LEVEL  AVB_LOG_LEVEL_DEBUG
#define	AVB_LOG_COMPONENT	"AVDECC Msg Server"
#include "openavb_pub.h"
#include "openavb_log.h"
#include "openavb_avdecc_pub.h"

// forward declarations
static bool openavbAvdeccMsgSrvrReceiveFromClient(int h, openavbAvdeccMessage_t *msg);

#include "openavb_avdecc_msg_server_osal.c"

// the following are from openavb_avdecc.c
extern openavb_avdecc_cfg_t gAvdeccCfg;
extern openavb_tl_data_cfg_t * streamList;


static bool openavbAvdeccMsgSrvrReceiveFromClient(int h, openavbAvdeccMessage_t *msg)
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
			AVB_LOG_DEBUG("Version request from client");
			ret = openavbAvdeccMsgSrvrHndlVerRqstFromClient(h);
			break;
		default:
			AVB_LOG_ERROR("Unexpected message received at server");
			break;
	}

	AVB_LOGF_VERBOSE("Message handled, ret=%d", ret);
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return ret;
}

void openavbAvdeccMsgSrvrSendServerVersionToClient(int h, U32 AVBVersion)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	openavbAvdeccMessage_t  msgBuf;
	memset(&msgBuf, 0, OPENAVB_AVDECC_MSG_LEN);
	msgBuf.type = OPENAVB_AVDECC_MSG_VERSION_CALLBACK;
	msgBuf.params.versionCallback.AVBVersion = AVBVersion;
	openavbAvdeccMsgSrvrSendToClient(h, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
}

/* Client version request
 */
bool openavbAvdeccMsgSrvrHndlVerRqstFromClient(int h)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	openavbAvdeccMsgSrvrSendServerVersionToClient(h, AVB_CORE_VER_FULL);

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return TRUE;
}

/* Called if a client closes their end of the IPC
 */
void openavbAvdeccMsgSrvrCloseClientConnection(int h)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	// AVDECC_TODO:  Should we do anything here?

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
}
