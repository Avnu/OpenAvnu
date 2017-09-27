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
 * MODULE : ADP - AVDECC Discovery Protocol : Advertise interface State Machine
 * MODULE SUMMARY : Implements the AVDECC Discovery Protocol : Advertise Interface State Machine
 * IEEE Std 1722.1-2013 clause 6.2.5.
 * 
 * Note: 1722.1 allows for multiple Interface state machines,
 *  one per AVB Interface descriptor. This implementation
 *  assumes one AVB Interface descriptor exists in the Entity Model.
 ******************************************************************
 */

#include "openavb_platform.h"

#include <signal.h>

#define	AVB_LOG_COMPONENT	"ADP"
#include "openavb_log.h"

#include "openavb_adp_sm_advertise_interface.h"
#include "openavb_adp_sm_advertise_entity.h"
#include "openavb_adp_message.h"

#define OPENAVB_ADP_SM_ADVERTISE_INTERFACE_WAIT_TIME_USEC 10000

typedef enum {
	OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_INITIALIZE,
	OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_WAITING,
	OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_DEPARTING,
	OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_ADVERTISE,
	OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_RECEIVED_DISCOVER,
	OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_UPDATE_GM,
	OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_LINK_STATE_CHANGE,
} openavb_adp_sm_advertise_interface_state_t;

extern openavb_avdecc_cfg_t gAvdeccCfg;
extern openavb_adp_sm_global_vars_t openavbAdpSMGlobalVars;
extern openavb_adp_sm_advertise_entity_vars_t openavbAdpSMAdvertiseEntityVars;
openavb_adp_sm_advertise_interface_vars_t openavbAdpSMAdvertiseInterfaceVars;

extern MUTEX_HANDLE(openavbAdpMutex);
#define ADP_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAdpMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ADP_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAdpMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

SEM_T(openavbAdpSMAdvertiseInterfaceSemaphore);
THREAD_TYPE(openavbAdpSmAdvertiseInterfaceThread);
THREAD_DEFINITON(openavbAdpSmAdvertiseInterfaceThread);

void openavbAdpSMAdvertiseInterface_txEntityAvailable()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);
	ADP_LOCK();
	openavbAdpSMGlobalVars.entityInfo.header.valid_time = gAvdeccCfg.valid_time;
	ADP_UNLOCK();
	openavbAdpMessageSend(OPENAVB_ADP_MESSAGE_TYPE_ENTITY_AVAILABLE);
	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void openavbAdpSMAdvertiseInterface_txEntityDeparting()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);
	ADP_LOCK();
	openavbAdpSMGlobalVars.entityInfo.header.valid_time = 0;
	ADP_UNLOCK();
	openavbAdpMessageSend(OPENAVB_ADP_MESSAGE_TYPE_ENTITY_DEPARTING);
	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void openavbAdpSMAdvertiseInterfaceStateMachine()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);
	bool bRunning = TRUE;

	openavb_adp_sm_advertise_interface_state_t state = OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_INITIALIZE;

	while (bRunning) {
		switch (state) {
			case OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_INITIALIZE:
				{
					AVB_TRACE_LINE(AVB_TRACE_ADP);
					AVB_LOG_DEBUG("State:  OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_INITIALIZE");

					ADP_LOCK();
					openavbAdpSMAdvertiseInterfaceVars.lastLinkIsUp = FALSE;
					openavbAdpSMAdvertiseInterfaceVars.doAdvertise = FALSE;			// Per 1722.1 What's next notes
					memcpy(openavbAdpSMAdvertiseInterfaceVars.advertisedGrandmasterID,
						openavbAdpSMGlobalVars.entityInfo.pdu.gptp_grandmaster_id,
						sizeof(openavbAdpSMGlobalVars.entityInfo.pdu.gptp_grandmaster_id));
					ADP_UNLOCK();

					// This interface will send the first advertisement on startup.
					// The advertise entity will be responsible for sending subsequent advertisements.
					// This allows the advertise entity and advertise interface to startup without any inter-dependencies.
					state = OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_ADVERTISE;
				}
				break;
			case OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_WAITING:
				{
					AVB_TRACE_LINE(AVB_TRACE_ADP);
					AVB_LOG_DEBUG("State:  OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_WAITING");

					// openavbAdpSMAdvertiseInterfaceVars.rcvdDiscover = FALSE;		// Per 1722.1 What's next notes. Note: setting this elsewhere otherwise incorrect behavior.
					// openavbAdpSMGlobalVars.entityInfo.pdu.available_index++;		// Per 1722.1 What's next notes

					// Wait for a change in state
					while (state == OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_WAITING && bRunning) {
						SEM_ERR_T(err);
						SEM_WAIT(openavbAdpSMAdvertiseInterfaceSemaphore, err);
						if (!SEM_IS_ERR_NONE(err)) { AVB_LOGF_WARNING("Semaphore error %d", err); }

						ADP_LOCK();
						if (openavbAdpSMAdvertiseInterfaceVars.doTerminate)
							state = OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_DEPARTING;
						else if (openavbAdpSMAdvertiseInterfaceVars.doAdvertise)
							state = OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_ADVERTISE;
						else if (openavbAdpSMAdvertiseInterfaceVars.rcvdDiscover)
							state = OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_RECEIVED_DISCOVER;
						else if (memcmp(openavbAdpSMAdvertiseInterfaceVars.advertisedGrandmasterID,
							openavbAdpSMGlobalVars.entityInfo.pdu.gptp_grandmaster_id,
							sizeof(openavbAdpSMGlobalVars.entityInfo.pdu.gptp_grandmaster_id)))
							state = OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_UPDATE_GM;
						else if (openavbAdpSMAdvertiseInterfaceVars.lastLinkIsUp != openavbAdpSMAdvertiseInterfaceVars.linkIsUp)
							state = OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_LINK_STATE_CHANGE;
						ADP_UNLOCK();
					}
				}
				break;
			case OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_DEPARTING:
				{
					AVB_TRACE_LINE(AVB_TRACE_ADP);
					AVB_LOG_DEBUG("State:  OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_DEPARTING");

					openavbAdpSMAdvertiseInterface_txEntityDeparting();
					bRunning = FALSE;
				}
				break;
			case OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_ADVERTISE:
				{
					AVB_TRACE_LINE(AVB_TRACE_ADP);
					AVB_LOG_DEBUG("State:  OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_ADVERTISE");

					ADP_LOCK();
					openavbAdpSMAdvertiseInterfaceVars.doAdvertise = FALSE;			// Per 1722.1 What's next notes
					ADP_UNLOCK();
					openavbAdpSMAdvertiseInterface_txEntityAvailable();
					state = OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_WAITING;
				}
				break;
			case OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_RECEIVED_DISCOVER:
				{
					AVB_TRACE_LINE(AVB_TRACE_ADP);
					AVB_LOG_DEBUG("State:  OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_RECEIVED_DISCOVER");

					ADP_LOCK();
					openavbAdpSMAdvertiseInterfaceVars.rcvdDiscover = FALSE;		// Not per spec but is needed.
					if (((U64)*openavbAdpSMAdvertiseInterfaceVars.entityID == 0x0000000000000000L) ||
						((U64)*openavbAdpSMAdvertiseInterfaceVars.entityID == (U64)*openavbAdpSMGlobalVars.entityInfo.header.entity_id)) {
						openavbAdpSMAdvertiseEntitySet_needsAdvertise(TRUE);
					}
					ADP_UNLOCK();
					state = OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_WAITING;
				}
				break;
			case OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_UPDATE_GM:
				{
					AVB_TRACE_LINE(AVB_TRACE_ADP);
					AVB_LOG_DEBUG("State:  OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_UPDATE_GM");

					openavbAdpSMAdvertiseEntitySet_needsAdvertise(TRUE);
					state = OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_WAITING;
				}
				break;
			case OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_LINK_STATE_CHANGE:
				{
					AVB_TRACE_LINE(AVB_TRACE_ADP);
					AVB_LOG_DEBUG("State:  OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_LINK_STATE_CHANGE");

					openavbAdpSMAdvertiseInterfaceVars.lastLinkIsUp = openavbAdpSMAdvertiseInterfaceVars.linkIsUp;
					if (openavbAdpSMAdvertiseInterfaceVars.linkIsUp) {
						openavbAdpSMAdvertiseEntitySet_needsAdvertise(TRUE);
					}
					state = OPENAVB_ADP_SM_ADVERTISE_INTERFACE_STATE_WAITING;
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

void* openavbAdpSMAdvertiseInterfaceThreadFn(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);
	openavbAdpSMAdvertiseInterfaceStateMachine();
	AVB_TRACE_EXIT(AVB_TRACE_ADP);
	return NULL;
}


void openavbAdpSMAdvertiseInterfaceStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	SEM_ERR_T(err);
	SEM_INIT(openavbAdpSMAdvertiseInterfaceSemaphore, 1, err);
	SEM_LOG_ERR(err);

	ADP_LOCK();
	memset(&openavbAdpSMAdvertiseInterfaceVars, 0, sizeof(openavbAdpSMAdvertiseInterfaceVars));
	ADP_UNLOCK();

	// Start the Advertise Entity State Machine
	bool errResult;
	THREAD_CREATE(openavbAdpSmAdvertiseInterfaceThread, openavbAdpSmAdvertiseInterfaceThread, NULL, openavbAdpSMAdvertiseInterfaceThreadFn, NULL);
	THREAD_CHECK_ERROR(openavbAdpSmAdvertiseInterfaceThread, "Thread / task creation failed", errResult);
	if (errResult);		// Already reported

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void openavbAdpSMAdvertiseInterfaceStop()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	openavbAdpSMAdvertiseInterfaceSet_doTerminate(TRUE);
	THREAD_JOIN(openavbAdpSmAdvertiseInterfaceThread, NULL);

	SEM_ERR_T(err);
	SEM_DESTROY(openavbAdpSMAdvertiseInterfaceSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void openavbAdpSMAdvertiseInterfaceSet_advertisedGrandmasterID(U8 *pValue)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);
	if (pValue) {
		memcpy(openavbAdpSMAdvertiseInterfaceVars.advertisedGrandmasterID, pValue, sizeof(openavbAdpSMAdvertiseInterfaceVars.advertisedGrandmasterID));
	}

	SEM_ERR_T(err);
	SEM_POST(openavbAdpSMAdvertiseInterfaceSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void openavbAdpSMAdvertiseInterfaceSet_rcvdDiscover(bool value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);
	openavbAdpSMAdvertiseInterfaceVars.rcvdDiscover = value;

	SEM_ERR_T(err);
	SEM_POST(openavbAdpSMAdvertiseInterfaceSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void openavbAdpSMAdvertiseInterfaceSet_entityID(U8 *pValue)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);
	if (pValue) {
		memcpy(openavbAdpSMAdvertiseInterfaceVars.entityID, pValue, sizeof(openavbAdpSMAdvertiseInterfaceVars.entityID));
	}
	// sem post is done when the rcvdDiscover is set.
	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void openavbAdpSMAdvertiseInterfaceSet_doTerminate(bool value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);
	openavbAdpSMAdvertiseInterfaceVars.doTerminate = value;

	SEM_ERR_T(err);
	SEM_POST(openavbAdpSMAdvertiseInterfaceSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void openavbAdpSMAdvertiseInterfaceSet_doAdvertise(bool value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);
	openavbAdpSMAdvertiseInterfaceVars.doAdvertise = value;

	SEM_ERR_T(err);
	SEM_POST(openavbAdpSMAdvertiseInterfaceSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void openavbAdpSMAdvertiseInterfaceSet_linkIsUp(bool value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);
	openavbAdpSMAdvertiseInterfaceVars.linkIsUp = value;

	SEM_ERR_T(err);
	SEM_POST(openavbAdpSMAdvertiseInterfaceSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}


