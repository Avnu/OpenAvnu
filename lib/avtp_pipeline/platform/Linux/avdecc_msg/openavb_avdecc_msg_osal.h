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

#ifndef OSAL_AVDECC_MSG_H
#define OSAL_AVDECC_MSG_H

// should only be included from openavb_avdecc_msg.h

#include <linux/un.h>
#include <net/if.h>
#include <unistd.h>
#include <signal.h>

typedef struct {
	openavbAvdeccMsgType_t	type;
	AVBStreamID_t			streamID;
	union {
		// Client-to-Server messages
		openavbAvdeccMsgParams_VersionRequest_t				versionRequest;
		openavbAvdeccMsgParams_ClientInitIdentify_t			clientInitIdentify;
		openavbAvdeccMsgParams_C2S_TalkerStreamID_t			c2sTalkerStreamID;
		openavbAvdeccMsgParams_ClientChangeNotification_t	clientChangeNotification;

		// Server-to-Client messages
		openavbAvdeccMsgParams_VersionCallback_t			versionCallback;
		openavbAvdeccMsgParams_ListenerStreamID_t			listenerStreamID;
		openavbAvdeccMsgParams_S2C_TalkerStreamID_t			s2cTalkerStreamID;
		openavbAvdeccMsgParams_ClientChangeRequest_t		clientChangeRequest;
	} params;
} openavbAvdeccMessage_t;


bool startAvdeccMsg(int mode, int ifindex, const char* ifname, unsigned mtu, unsigned link_kbit, unsigned nsr_kbit);
void stopAvdeccMsg();

#endif // OSAL_AVDECC_MSG_H
