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
 * MODULE : ACMP - AVDECC Connection Management Protocol
 * MODULE SUMMARY : Implements the 1722.1 (AVDECC) connection management protocol
 ******************************************************************
 */
#include "openavb_platform.h"

#include <signal.h>

#define	AVB_LOG_COMPONENT	"ACMP"
#include "openavb_log.h"

#include "openavb_aem.h"
#include "openavb_acmp.h"
#include "openavb_acmp_sm_listener.h"
#include "openavb_acmp_sm_talker.h"
#include "openavb_acmp_sm_controller.h"
#include "openavb_acmp_message.h"

extern openavb_avdecc_cfg_t gAvdeccCfg;

openavb_acmp_sm_global_vars_t openavbAcmpSMGlobalVars;

MUTEX_HANDLE(openavbAcmpMutex);
#define ACMP_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAcmpMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ACMP_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAcmpMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

static bool bListenerStarted = FALSE;
static bool bTalkerStarted = FALSE;

openavbRC openavbAcmpStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavb_avdecc_entity_model_t *pAem = openavbAemGetModel();
	if (!pAem) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING), AVB_TRACE_ACMP);
	}

	{
		MUTEX_ATTR_HANDLE(mta);
		MUTEX_ATTR_INIT(mta);
		MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
		MUTEX_ATTR_SET_NAME(mta, "openavbAcmpMutex");
		MUTEX_CREATE_ERR();
		MUTEX_CREATE(openavbAcmpMutex, mta);
		MUTEX_LOG_ERR("Could not create/initialize 'openavbAcmpMutex' mutex");
	}

	ACMP_LOCK();
	{
		// Populate global ACMP vars
		openavb_acmp_control_header_t *pHeader = &openavbAcmpSMGlobalVars.controlHeader;

		pHeader->cd = 1;
		pHeader->subtype = OPENAVB_ACMP_AVTP_SUBTYPE;
		pHeader->sv = 0;
		pHeader->version = 0;
		//pHeader->message_type = 0;				// Set later depending on message type
		//pHeader->status = 0;						// Set later depending on message
		pHeader->control_data_length = 44;
		//pHeader->stream_id;						// Set later depending on stream
	}

	{
		memcpy(openavbAcmpSMGlobalVars.my_id, pAem->pDescriptorEntity->entity_id, sizeof(openavbAcmpSMGlobalVars.my_id));
	}
	ACMP_UNLOCK();

	openavbRC rc = openavbAcmpMessageHandlerStart();
	if (IS_OPENAVB_FAILURE(rc)) {
		openavbAcmpStop();
		AVB_RC_TRACE_RET(rc, AVB_TRACE_ACMP);
	}

	if (gAvdeccCfg.bListener) {
		if (!openavbAcmpSMListenerStart()) {
			AVB_RC_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_GENERIC), AVB_TRACE_ACMP);
		}
		bListenerStarted = TRUE;
	}

	if (gAvdeccCfg.bTalker) {
		if (!openavbAcmpSMTalkerStart()) {
			AVB_RC_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_GENERIC), AVB_TRACE_ACMP);
		}
		bTalkerStarted = TRUE;
	}

	if (!openavbAcmpSMControllerStart()) {
		AVB_RC_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_GENERIC), AVB_TRACE_ACMP);
	}

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_ACMP);
}

void openavbAcmpStop()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavbAcmpSMControllerStop();

	if (bTalkerStarted) {
		openavbAcmpSMTalkerStop();
	}

	if (bListenerStarted) {
		openavbAcmpSMListenerStop();
	}

	openavbAcmpMessageHandlerStop();

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

