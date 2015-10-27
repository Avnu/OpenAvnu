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
* MODULE SUMMARY : Common implementation for the talker and listener
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "openavb_tl.h"
#include "openavb_trace.h"
//#include "openavb_mediaq.h"
#include "openavb_talker.h"
#include "openavb_listener.h"
#include "openavb_avtp.h"
#include "openavb_endpoint.h"

#include "openavb_platform.h"

#define	AVB_LOG_COMPONENT	"Talker / Listener"
#include "openavb_pub.h"
#include "openavb_log.h"

void openavbEptClntCheckVerMatchesSrvr(int endpointHandle, U32 AVBVersion)
{
}


// Talker Listener thread function
void* openavbTLThreadFn(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = (tl_state_t *)pv;
	
	openavbTLThreadFnOsal(pTLState);

	TL_LOCK();
	// Assign a unique endpoint handle
	static int gEndpointHandle = 1;
	pTLState->endpointHandle = gEndpointHandle++;
	TL_UNLOCK();

	while (pTLState->bRunning) {
		AVB_TRACE_LINE(AVB_TRACE_TL_DETAIL);

		if (pTLState->cfg.role == AVB_ROLE_TALKER) {
			openavbTLRunTalker(pTLState);
		}
		else {
			openavbTLRunListener(pTLState);
		}

		// Close the endpoint connection. unless connection already gone in which case the socket could already be reused.
		if (pTLState->bConnected) {
			pTLState->bConnected = FALSE;
			pTLState->endpointHandle = 0;
		}

		if (pTLState->bRunning) {
			SLEEP(1);
		}
	}

	THREAD_JOINABLE(pTLState->TLThread);

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return NULL;
}

tl_handle_t TLHandleListGet(int endpointHandle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!endpointHandle || !gTLHandleList) {
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return NULL;
	}

	TL_LOCK();
	int i1;
	for (i1 = 0; i1 < gMaxTL; i1++) {
		if (gTLHandleList[i1]) {
			tl_state_t *pTLState = (tl_state_t *)gTLHandleList[i1];
			if (pTLState->endpointHandle == endpointHandle) {
				TL_UNLOCK();
				AVB_TRACE_EXIT(AVB_TRACE_TL);
				return pTLState;
			}
		}
	}
	TL_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return NULL;
}

bool openavbEptClntStopStream(int h, AVBStreamID_t *streamID)
{
	return TRUE;
}

bool openavbEptClntService(int h, int timeout)
{
	return TRUE;
}

