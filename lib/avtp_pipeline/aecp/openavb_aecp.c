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
 * MODULE : AVDECC Enumeration and control protocol (AECP)
 * MODULE SUMMARY : Implements the AVDECC Enumeration and control protocol (AECP)
 ******************************************************************
 */

#include "openavb_platform.h"

#include <signal.h>

#define	AVB_LOG_COMPONENT	"AECP"
#include "openavb_log.h"

#include "openavb_aem.h"
#include "openavb_aecp.h"
#include "openavb_aecp_sm_entity_model_entity.h"
#include "openavb_aecp_message.h"

openavb_aecp_sm_global_vars_t openavbAecpSMGlobalVars;

MUTEX_HANDLE(openavbAecpMutex);
#define ADP_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAecpMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ADP_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAecpMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

openavbRC openavbAecpStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);

	openavb_avdecc_entity_model_t *pAem = openavbAemGetModel();
	if (!pAem) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING), AVB_TRACE_AECP);
	}

	MUTEX_ATTR_HANDLE(mta);
	MUTEX_ATTR_INIT(mta);
	MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
	MUTEX_ATTR_SET_NAME(mta, "openavbAecpMutex");
	MUTEX_CREATE_ERR();
	MUTEX_CREATE(openavbAecpMutex, mta);
	MUTEX_LOG_ERR("Could not create/initialize 'openavbAecpMutex' mutex");

	// Set AECP global vars
	memcpy(openavbAecpSMGlobalVars.myEntityID, pAem->pDescriptorEntity->entity_id, sizeof(openavbAecpSMGlobalVars.myEntityID));

	openavbRC rc = openavbAecpMessageHandlerStart();
	if (IS_OPENAVB_FAILURE(rc)) {
		openavbAecpStop();
		AVB_RC_TRACE_RET(rc, AVB_TRACE_AECP);
	}

	openavbAecpSMEntityModelEntityStart();

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AECP);
}

void openavbAecpStop()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);

	openavbAecpSMEntityModelEntityStop();
	openavbAecpMessageHandlerStop();

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}


