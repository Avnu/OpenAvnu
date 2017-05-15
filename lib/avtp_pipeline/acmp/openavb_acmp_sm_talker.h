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
 * MODULE : ACMP - AVDECC Connection Management Protocol : Talker State Machine Interface
 * MODULE SUMMARY : Interface for the ACMP - AVDECC Connection Management Protocol : Talker State Machine
 * IEEE Std 1722.1-2013 clause 8.2.2.6
 ******************************************************************
 */

#ifndef OPENAVB_ACMP_SM_TALKER_H
#define OPENAVB_ACMP_SM_TALKER_H 1

#include "openavb_acmp.h"

// State machine vars IEEE Std 1722.1-2013 clause 8.2.2.6.1
typedef struct {
	openavb_array_t talkerStreamInfos;
	bool rcvdConnectTX;
	bool rcvdDisconnectTX;
	bool rcvdGetTXState;
	bool rcvdGetTXConnection;

	// Not part of spec
	bool doTerminate;
} openavb_acmp_sm_talker_vars_t;

// State machine functions IEEE Std 1722.1-2013 clause 8.2.2.6.2
bool openavbAcmpSMTalker_validTalkerUnique(U16 talkerUniqueId);
U8 openavbAcmpSMTalker_connectTalker(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMTalker_txResponse(U8 messageType, openavb_acmp_ACMPCommandResponse_t *response, U8 error);
U8 openavbAcmpSMTalker_disconnectTalker(openavb_acmp_ACMPCommandResponse_t *command);
U8 openavbAcmpSMTalker_getState(openavb_acmp_ACMPCommandResponse_t *command);
U8 openavbAcmpSMTalker_getConnection(openavb_acmp_ACMPCommandResponse_t *command);

bool openavbAcmpSMTalkerStart(void);
void openavbAcmpSMTalkerStop(void);

void openavbAcmpSMTalkerSet_rcvdConnectTXCmd(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMTalkerSet_rcvdDisconnectTXCmd(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMTalkerSet_rcvdGetTXState(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMTalkerSet_rcvdGetTXConnectionCmd(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMTalkerSet_doTerminate(bool value);

void openavbAcmpSMTalker_updateStreamInfo(openavb_tl_data_cfg_t *pCfg);

#endif // OPENAVB_ACMP_SM_TALKER_H
