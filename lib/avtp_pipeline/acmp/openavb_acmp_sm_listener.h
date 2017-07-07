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
 ******************************************************************
 * MODULE : ACMP - AVDECC Connection Management Protocol : Listener State Machine Interface
 * MODULE SUMMARY : Interface for the ACMP - AVDECC Connection Management Protocol : Listener State Machine
 * IEEE Std 1722.1-2013 clause 8.2.2.5
 ******************************************************************
 */

#ifndef OPENAVB_ACMP_SM_LISTENER_H
#define OPENAVB_ACMP_SM_LISTENER_H 1

#include "openavb_list.h"
#include "openavb_acmp.h"

// State machine vars IEEE Std 1722.1-2013 clause 8.2.2.5.1
typedef struct {
	openavb_list_t inflight;
	openavb_array_t listenerStreamInfos;
	bool rcvdConnectRXCmd;
	bool rcvdDisconnectRXCmd;
	bool rcvdConnectTXResp;
	bool rcvdDisconnectTXResp;
	bool rcvdGetRXState;

	// Not part of spec
	bool doTerminate;
} openavb_acmp_sm_listener_vars_t;

// State machine functions IEEE Std 1722.1-2013 clause 8.2.2.5.2
bool openavbAcmpSMListener_validListenerUnique(U16 listenerUniqueId);
bool openavbAcmpSMListener_listenerIsConnected(openavb_acmp_ACMPCommandResponse_t *command);
bool openavbAcmpSMListener_listenerIsConnectedTo(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMListener_txCommand(U8 messageType, openavb_acmp_ACMPCommandResponse_t *command, bool retry);
void openavbAcmpSMListener_txResponse(U8 messageType, openavb_acmp_ACMPCommandResponse_t *response, U8 error);
U8 openavbAcmpSMListener_connectListener(openavb_acmp_ACMPCommandResponse_t *response);
U8 openavbAcmpSMListener_disconnectListener(openavb_acmp_ACMPCommandResponse_t *response);
void openavbAcmpSMListener_cancelTimeout(openavb_acmp_ACMPCommandResponse_t *commandResponse);
void openavbAcmpSMListener_removeInflight(openavb_acmp_ACMPCommandResponse_t *commandResponse);
U8 openavbAcmpSMListener_getState(openavb_acmp_ACMPCommandResponse_t *command);

bool openavbAcmpSMListenerStart(void);
void openavbAcmpSMListenerStop(void);

void openavbAcmpSMListenerSet_rcvdConnectRXCmd(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMListenerSet_rcvdDisconnectRXCmd(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMListenerSet_rcvdConnectTXResp(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMListenerSet_rcvdDisconnectTXResp(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMListenerSet_rcvdGetRXState(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMListenerSet_doTerminate(bool value);

// Special command to initiate the fast connect support.
void openavbAcmpSMListenerSet_doFastConnect(
	const openavb_tl_data_cfg_t *pListener,
	U16 flags,
	U16 talker_unique_id,
	const U8 talker_entity_id[8],
	const U8 controller_entity_id[8]);

// Assist function to detect if Talker available for fast connect
void openavbAcmpSMListenerSet_talkerTestFastConnect(
	const U8 entity_id[8]);

#endif // OPENAVB_ACMP_SM_LISTENER_H
