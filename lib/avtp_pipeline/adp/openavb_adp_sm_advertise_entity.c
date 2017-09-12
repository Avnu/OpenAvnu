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
 * MODULE : ADP - AVDECC Discovery Protocol : Advertise Entity State Machine
 * MODULE SUMMARY : Implements the AVDECC Discovery Protocol : Advertise Entity State Machine
 * IEEE Std 1722.1-2013 clause 6.2.4.
 ******************************************************************
 */

#include "openavb_platform.h"

#include <errno.h>

#define	AVB_LOG_COMPONENT	"ADP"
#include "openavb_log.h"

#include "openavb_time.h"
#include "openavb_adp.h"
#include "openavb_adp_sm_advertise_interface.h"
#include "openavb_adp_sm_advertise_entity.h"

typedef enum {
	OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_INITIALIZE,
	OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_ADVERTISE,
	OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_RESET_WAIT,
	OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_WAITING,
	OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_TERMINATE,
} openavb_adp_sm_advertise_entity_state_t;

extern openavb_adp_sm_global_vars_t openavbAdpSMGlobalVars;
extern openavb_adp_sm_advertise_interface_vars_t openavbAdpSMAdvertiseInterfaceVars;
openavb_adp_sm_advertise_entity_vars_t openavbAdpSMAdvertiseEntityVars;

extern MUTEX_HANDLE(openavbAdpMutex);
#define ADP_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAdpMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ADP_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAdpMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

SEM_T(openavbAdpSMAdvertiseEntitySemaphore);
THREAD_TYPE(openavbAdpSmAdvertiseEntityThread);
THREAD_DEFINITON(openavbAdpSmAdvertiseEntityThread);

void openavbAdpSMAdvertiseEntity_sendAvailable()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);
	openavbAdpSMAdvertiseInterfaceSet_doAdvertise(TRUE);
	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void openavbAdpSMAdvertiseEntityStateMachine()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);
	bool bRunning = TRUE;

	openavb_adp_sm_advertise_entity_state_t state = OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_INITIALIZE;

	while (bRunning) {
		switch (state) {
			case OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_INITIALIZE:
				{
					AVB_TRACE_LINE(AVB_TRACE_ADP);
					AVB_LOG_DEBUG("State:  OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_INITIALIZE");

					ADP_LOCK();
					openavbAdpSMGlobalVars.entityInfo.pdu.available_index = 0;
					ADP_UNLOCK();

					// The advertise interface will send the first advertisement on startup.
					// This entity will be responsible for sending subsequent advertisements.
					// This allows the advertise entity and advertise interface to startup without any inter-dependencies.
					state = OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_RESET_WAIT;
				}
				break;
			case OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_ADVERTISE:
				{
					AVB_TRACE_LINE(AVB_TRACE_ADP);
					AVB_LOG_DEBUG("State:  OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_ADVERTISE");

					openavbAdpSMAdvertiseEntity_sendAvailable();
					ADP_LOCK();
					openavbAdpSMAdvertiseEntityVars.needsAdvertise = FALSE;
					ADP_UNLOCK();
					state = OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_RESET_WAIT;
				}
				break;
			case OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_RESET_WAIT:
				{
					AVB_TRACE_LINE(AVB_TRACE_ADP);
					AVB_LOG_DEBUG("State:  OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_RESET_WAIT");

					ADP_LOCK();
					CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &openavbAdpSMAdvertiseEntityVars.reannounceTimerTimeout);
					/* The advertisements should be sent at intervals of 1/4 valid_time, where valid_time is in 2-second units.
					 * See IEEE Std 1722.1-2013 clauses 6.2.1.6 and 6.2.4. */
					U32 advDelayUsec = openavbAdpSMGlobalVars.entityInfo.header.valid_time / 2 * MICROSECONDS_PER_SECOND;
					if (advDelayUsec < MICROSECONDS_PER_SECOND) {
						advDelayUsec = MICROSECONDS_PER_SECOND;
					}
					openavbTimeTimespecAddUsec(&openavbAdpSMAdvertiseEntityVars.reannounceTimerTimeout, advDelayUsec);
					ADP_UNLOCK();
					state = OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_WAITING;
				}
				break;
			case OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_WAITING:
				{
					AVB_TRACE_LINE(AVB_TRACE_ADP);
					AVB_LOG_DEBUG("State:  OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_WAITING");

					ADP_LOCK();
					openavbAdpSMAdvertiseInterfaceSet_rcvdDiscover(FALSE);
					openavbAdpSMGlobalVars.entityInfo.pdu.available_index++;
					ADP_UNLOCK();

					// Wait for change in state
					while (state == OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_WAITING && bRunning) {
						U32 timeoutMSec;
						struct timespec now;
						CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &now);
						timeoutMSec = openavbTimeUntilMSec(&now, &openavbAdpSMAdvertiseEntityVars.reannounceTimerTimeout);

						if (timeoutMSec == 0) {
							/* No need to wait. */
							state = OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_ADVERTISE;
						}
						else {
							SEM_ERR_T(err);
							SEM_TIMEDWAIT(openavbAdpSMAdvertiseEntitySemaphore, timeoutMSec, err);
							if (!SEM_IS_ERR_NONE(err) && !SEM_IS_ERR_TIMEOUT(err)) { AVB_LOGF_WARNING("Semaphore error %d", err); }

							if (openavbAdpSMAdvertiseEntityVars.doTerminate) {
								bRunning = FALSE;
							} else if (SEM_IS_ERR_TIMEOUT(err) ||
								openavbAdpSMAdvertiseEntityVars.needsAdvertise) {
								state = OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_ADVERTISE;
							}
						}
					}
				}
				break;

			default:
				AVB_LOG_ERROR("State:  Unexpected!");
				bRunning = FALSE;	// Unexpected
				break;
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void* openavbAdpSMAdvertiseEntityThreadFn(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);
	openavbAdpSMAdvertiseEntityStateMachine();
	AVB_TRACE_EXIT(AVB_TRACE_ADP);
	return NULL;
}

void openavbAdpSMAdvertiseEntityStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	SEM_ERR_T(err);
	SEM_INIT(openavbAdpSMAdvertiseEntitySemaphore, 1, err);
	SEM_LOG_ERR(err);

	ADP_LOCK();
	openavbAdpSMAdvertiseEntityVars.needsAdvertise = FALSE;
	openavbAdpSMAdvertiseEntityVars.doTerminate = FALSE;
	ADP_UNLOCK();

	// Start the Advertise Entity State Machine
	bool errResult;
	THREAD_CREATE(openavbAdpSmAdvertiseEntityThread, openavbAdpSmAdvertiseEntityThread, NULL, openavbAdpSMAdvertiseEntityThreadFn, NULL);
	THREAD_CHECK_ERROR(openavbAdpSmAdvertiseEntityThread, "Thread / task creation failed", errResult);
	if (errResult);		// Already reported

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void openavbAdpSMAdvertiseEntityStop()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	openavbAdpSMAdvertiseEntitySet_doTerminate(TRUE);
	THREAD_JOIN(openavbAdpSmAdvertiseEntityThread, NULL);

	SEM_ERR_T(err);
	SEM_DESTROY(openavbAdpSMAdvertiseEntitySemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void openavbAdpSMAdvertiseEntitySet_needsAdvertise(bool value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	openavbAdpSMAdvertiseEntityVars.needsAdvertise = value;

	SEM_ERR_T(err);
	SEM_POST(openavbAdpSMAdvertiseEntitySemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void openavbAdpSMAdvertiseEntitySet_doTerminate(bool value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	openavbAdpSMAdvertiseEntityVars.doTerminate = value;

	SEM_ERR_T(err);
	SEM_POST(openavbAdpSMAdvertiseEntitySemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}





