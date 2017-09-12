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
 * MODULE : AVDECC Enumeration and control protocol (AECP) : Entity Model Entity State Machine Interface
 * MODULE SUMMARY : Interface for the AVDECC Enumeration and control protocol (AECP) : Entity Model Entity State Machine
 * IEEE Std 1722.1-2013 clause 9.2.2.3
 ******************************************************************
 */

#ifndef OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_H
#define OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_H 1

#include "openavb_aecp.h"

// State machine vars IEEE Std 1722.1-2013 clause 9.2.2.3.1.2
typedef struct {
	openavb_aecp_AEMCommandResponse_t *rcvdCommand;
	bool rcvdAEMCommand;
	openavb_aecp_AEMCommandResponse_t unsolicited;
	bool doUnsolicited;

	// Not part of spec
	bool doTerminate;
} openavb_aecp_sm_entity_model_entity_vars_t;

// State machine functions IEEE Std 1722.1-2013 clause 9.2.2.3.1.3
void acquireEntity(void);
void lockEntity(void);
void processCommand(void);

void openavbAecpSMEntityModelEntityStart(void);
void openavbAecpSMEntityModelEntityStop(void);

// Accessors
void openavbAecpSMEntityModelEntitySet_rcvdCommand(openavb_aecp_AEMCommandResponse_t *rcvdCommand);
	// Note:  rcvdAEMCommand is set during the call to openavbAecpSMEntityModelEntitySet_rcvdCommand()
void openavbAecpSMEntityModelEntitySet_unsolicited(openavb_aecp_AEMCommandResponse_t *unsolicited);
	// Note:  doUnsolicited is set during the call to openavbAecpSMEntityModelEntitySet_unsolicited()
void openavbAecpSMEntityModelEntitySet_doTerminate(bool value);

#endif // OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_H
