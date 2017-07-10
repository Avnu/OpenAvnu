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
 * MODULE : ACMP - AVDECC Connection Management Protocol : Controller State Machine
 * MODULE SUMMARY : Implements the ACMP - AVDECC Connection Management Protocol : Controller State Machine
 * IEEE Std 1722.1-2013 clause 8.2.2.4
 ******************************************************************
 */

#define	AVB_LOG_COMPONENT	"ACMP"
#include "openavb_log.h"

#include "openavb_debug.h"
#include "openavb_time.h"
#include "openavb_acmp_sm_controller.h"
#include "openavb_acmp_message.h"
#include "openavb_avdecc_pipeline_interaction_pub.h"

typedef enum {
	OPENAVB_ACMP_SM_CONTROLLER_STATE_WAITING,
	OPENAVB_ACMP_SM_CONTROLLER_STATE_TIMEOUT,
	OPENAVB_ACMP_SM_CONTROLLER_STATE_RESPONSE
} openavb_acmp_sm_controller_state_t;

extern openavb_acmp_sm_global_vars_t openavbAcmpSMGlobalVars;

static openavb_acmp_ACMPCommandResponse_t rcvdCmdResp;
static openavb_acmp_ACMPCommandResponse_t *pRcvdCmdResp = &rcvdCmdResp;
static openavb_acmp_sm_controller_vars_t openavbAcmpSMControllerVars = {0};
static bool bRunning = FALSE;

extern MUTEX_HANDLE(openavbAcmpMutex);
#define ACMP_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAcmpMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ACMP_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAcmpMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

static MUTEX_HANDLE(openavbAcmpSMControllerMutex);
#define ACMP_SM_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAcmpSMControllerMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ACMP_SM_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAcmpSMControllerMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

SEM_T(openavbAcmpSMControllerSemaphore);
THREAD_TYPE(openavbAcmpSmControllerThread);
THREAD_DEFINITON(openavbAcmpSmControllerThread);


static openavb_list_node_t openavbAcmpSMController_findInflightNodeFromCommand(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavb_list_node_t node = openavbListIterFirst(openavbAcmpSMControllerVars.inflight);
	while (node) {
		openavb_acmp_InflightCommand_t *pInFlightCommand = openavbListData(node);
		if (pInFlightCommand) {
			if (memcmp(pInFlightCommand->command.controller_entity_id, command->controller_entity_id, sizeof(pInFlightCommand->command.controller_entity_id)) == 0 &&
					pInFlightCommand->command.sequence_id == command->sequence_id) {
				break;
			}
		}
		node = openavbListIterNext(openavbAcmpSMControllerVars.inflight);
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return node;
}


void openavbAcmpSMController_txCommand(U8 messageType, openavb_acmp_ACMPCommandResponse_t *command, bool retry)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	openavb_list_node_t node = NULL;

	openavbRC rc = openavbAcmpMessageSend(messageType, command, OPENAVB_ACMP_STATUS_SUCCESS);
	if (IS_OPENAVB_SUCCESS(rc)) {
		if (!retry) {
			node = openavbListNew(openavbAcmpSMControllerVars.inflight, sizeof(openavb_acmp_InflightCommand_t));
			if (node) {
				openavb_acmp_InflightCommand_t *pInFlightCommand = openavbListData(node);
				if (pInFlightCommand) {

					memcpy(&pInFlightCommand->command, command, sizeof(pInFlightCommand->command));
					pInFlightCommand->command.message_type = messageType;
					pInFlightCommand->retried = FALSE;
					pInFlightCommand->original_sequence_id = command->sequence_id;	// AVDECC_TODO - is this correct?

					CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &pInFlightCommand->timer);
					switch (messageType) {
						case OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_STATE_RESPONSE:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_GET_TX_STATE_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						case OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_RX_RESPONSE:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_CONNECT_RX_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						case OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_RX_RESPONSE:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_DISCONNECT_RX_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						case OPENAVB_ACMP_MESSAGE_TYPE_GET_RX_STATE_RESPONSE:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_GET_RX_STATE_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						case OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_CONNECTION_RESPONSE:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_GET_TX_CONNECTION_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						default:
							AVB_LOGF_ERROR("Unsupported command %u in openavbAcmpSMController_txCommand", messageType);
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_CONNECT_RX_COMMAND * MICROSECONDS_PER_MSEC);
							break;
					}
				}
			}
		}
		else {
			// Retry case
			node = openavbAcmpSMController_findInflightNodeFromCommand(command);
			if (node) {
				openavb_acmp_InflightCommand_t *pInFlightCommand = openavbListData(node);
				if (pInFlightCommand) {
					pInFlightCommand->retried = TRUE;

					CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &pInFlightCommand->timer);
					switch (messageType) {
						case OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_STATE_RESPONSE:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_GET_TX_STATE_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						case OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_RX_RESPONSE:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_CONNECT_RX_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						case OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_RX_RESPONSE:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_DISCONNECT_RX_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						case OPENAVB_ACMP_MESSAGE_TYPE_GET_RX_STATE_RESPONSE:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_GET_RX_STATE_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						case OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_CONNECTION_RESPONSE:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_GET_TX_CONNECTION_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						default:
							AVB_LOGF_ERROR("Unsupported command %u in openavbAcmpSMController_txCommand", messageType);
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_CONNECT_RX_COMMAND * MICROSECONDS_PER_MSEC);
							break;
					}
				}
			}
		}
	}
	else {
		// Failed to send command
		openavbAcmpMessageSend(messageType, command, OPENAVB_ACMP_STATUS_COULD_NOT_SEND_MESSAGE);
		if (retry) {
			node = openavbAcmpSMController_findInflightNodeFromCommand(command);
			if (node) {
				openavbListDelete(openavbAcmpSMControllerVars.inflight, node);
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMController_cancelTimeout(openavb_acmp_ACMPCommandResponse_t *commandResponse)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	// Nothing to do since the timer is reset during the wait state and the inFlight entry will have already been removed.
	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMController_removeInflight(openavb_acmp_ACMPCommandResponse_t *commandResponse)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavb_list_node_t node = openavbAcmpSMController_findInflightNodeFromCommand(commandResponse);
	if (node) {
		openavbListDelete(openavbAcmpSMControllerVars.inflight, node);
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMController_processResponse(openavb_acmp_ACMPCommandResponse_t *commandResponse)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	// AVDECC_TODO:  Is there anything we need to do here, such as updating the saved state?
	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMController_makeCommand(openavb_acmp_ACMPCommandParams_t *param)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	// AVDECC_TODO:  Fill this in if implementing the doCommand support.
	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMControllerStateMachine()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavb_acmp_InflightCommand_t *pInflightActive = NULL;		// The inflight command for the current state
	openavb_acmp_sm_controller_state_t state = OPENAVB_ACMP_SM_CONTROLLER_STATE_WAITING;

	// Lock such that the mutex is held unless waiting for a semaphore. Synchronous processing of command responses.
	ACMP_SM_LOCK();
	while (bRunning) {
		switch (state) {
			case OPENAVB_ACMP_SM_CONTROLLER_STATE_WAITING:
				AVB_TRACE_LINE(AVB_TRACE_ACMP);
				AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_CONTROLLER_STATE_WAITING");

				openavbAcmpSMControllerVars.rcvdResponse = FALSE;

				// Calculate timeout for inflight commands
				// Start is a arbitrary large time out.
				struct timespec timeout;
				CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &timeout);
				timeout.tv_sec += 60;	// At most will timeout after 60 seconds

				// Look for soonest inflight command timeout
				openavb_list_node_t node = openavbListIterFirst(openavbAcmpSMControllerVars.inflight);
				while (node) {
					openavb_acmp_InflightCommand_t *pInflight = openavbListData(node);
					if ((openavbTimeTimespecCmp(&pInflight->timer, &timeout) < 0)) {
						timeout.tv_sec = pInflight->timer.tv_sec;
						timeout.tv_nsec = pInflight->timer.tv_nsec;
					}
					node = openavbListIterNext(openavbAcmpSMControllerVars.inflight);
				}

				ACMP_SM_UNLOCK();
				U32 timeoutMSec = 0;
				struct timespec now;
				CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &now);
				timeoutMSec = openavbTimeUntilMSec(&now, &timeout);
				SEM_ERR_T(err);
				SEM_TIMEDWAIT(openavbAcmpSMControllerSemaphore, timeoutMSec, err);
				ACMP_SM_LOCK();

				if (!SEM_IS_ERR_NONE(err)) {
					if (SEM_IS_ERR_TIMEOUT(err)) {
						struct timespec now;
						CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &now);

						// Look for a timed out inflight command
						openavb_list_node_t node = openavbListIterFirst(openavbAcmpSMControllerVars.inflight);
						while (node) {
							openavb_acmp_InflightCommand_t *pInflight = openavbListData(node);
							if ((openavbTimeTimespecCmp(&now, &pInflight->timer) >= 0)) {
								// Found a timed out command
								state = OPENAVB_ACMP_SM_CONTROLLER_STATE_TIMEOUT;
								pInflightActive = pInflight;
								break;
							}
							node = openavbListIterNext(openavbAcmpSMControllerVars.inflight);
						}
					}
				}
				else {
					if (openavbAcmpSMControllerVars.doTerminate) {
						bRunning = FALSE;
					}
					else if (openavbAcmpSMControllerVars.rcvdResponse &&
							memcmp(pRcvdCmdResp->controller_entity_id, openavbAcmpSMGlobalVars.my_id, sizeof(openavbAcmpSMGlobalVars.my_id)) == 0) {
						// Look for a corresponding inflight command
						openavb_list_node_t node = openavbListIterFirst(openavbAcmpSMControllerVars.inflight);
						while (node) {
							openavb_acmp_InflightCommand_t *pInflight = openavbListData(node);
							if (pRcvdCmdResp->sequence_id == pInflight->command.sequence_id &&
									pRcvdCmdResp->message_type == pInflight->command.message_type + 1) {
								// Found a corresponding command
								state = OPENAVB_ACMP_SM_CONTROLLER_STATE_RESPONSE;
								pInflightActive = pInflight;
								break;
							}
							node = openavbListIterNext(openavbAcmpSMControllerVars.inflight);
						}
					}
				break;

			case OPENAVB_ACMP_SM_CONTROLLER_STATE_TIMEOUT:
				{
					AVB_TRACE_LINE(AVB_TRACE_ACMP);
					AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_CONTROLLER_STATE_TIMEOUT");

					if (pInflightActive) {
						if (pInflightActive->retried) {
							openavbAcmpSMController_removeInflight(&pInflightActive->command);
						}
						else {
							openavbAcmpSMController_txCommand(pInflightActive->command.message_type, &pInflightActive->command, TRUE);
						}
					}
					pInflightActive = NULL;
					state = OPENAVB_ACMP_SM_CONTROLLER_STATE_WAITING;
				}
				break;

			case OPENAVB_ACMP_SM_CONTROLLER_STATE_RESPONSE:
				{
					AVB_TRACE_LINE(AVB_TRACE_ACMP);
					AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_CONTROLLER_STATE_RESPONSE");

					if (pInflightActive) {
						openavbAcmpSMController_cancelTimeout(pRcvdCmdResp);
						openavbAcmpSMController_processResponse(pRcvdCmdResp);
						openavbAcmpSMController_removeInflight(pRcvdCmdResp);
					}
					pInflightActive = NULL;
					state = OPENAVB_ACMP_SM_CONTROLLER_STATE_WAITING;
				}
				break;
			}
		}
	}
	ACMP_SM_UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void* openavbAcmpSMControllerThreadFn(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	openavbAcmpSMControllerStateMachine();
	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return NULL;
}

bool openavbAcmpSMControllerStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavbAcmpSMControllerVars.inflight = openavbListNewList();
	if (!openavbAcmpSMControllerVars.inflight) {
		AVB_LOG_ERROR("Unable to create inflight list. ACMP protocol not started.");
		AVB_TRACE_EXIT(AVB_TRACE_ACMP);
		return FALSE;
	}

	MUTEX_ATTR_HANDLE(mta);
	MUTEX_ATTR_INIT(mta);
	MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
	MUTEX_ATTR_SET_NAME(mta, "openavbAcmpSMControllerMutex");
	MUTEX_CREATE_ERR();
	MUTEX_CREATE(openavbAcmpSMControllerMutex, mta);
	MUTEX_LOG_ERR("Could not create/initialize 'openavbAcmpSMControllerMutex' mutex");

	SEM_ERR_T(err);
	SEM_INIT(openavbAcmpSMControllerSemaphore, 1, err);
	SEM_LOG_ERR(err);

	// Start the State Machine
	bool errResult;
	bRunning = TRUE;
	THREAD_CREATE(openavbAcmpSmControllerThread, openavbAcmpSmControllerThread, NULL, openavbAcmpSMControllerThreadFn, NULL);
	THREAD_CHECK_ERROR(openavbAcmpSmControllerThread, "Thread / task creation failed", errResult);
	if (errResult) {
		bRunning = FALSE;
		AVB_TRACE_EXIT(AVB_TRACE_ACMP);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return TRUE;
}

void openavbAcmpSMControllerStop()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	if (bRunning) {
		openavbAcmpSMControllerSet_doTerminate(TRUE);

		THREAD_JOIN(openavbAcmpSmControllerThread, NULL);
	}

	SEM_ERR_T(err);
	SEM_DESTROY(openavbAcmpSMControllerSemaphore, err);
	SEM_LOG_ERR(err);

	openavbListDeleteList(openavbAcmpSMControllerVars.inflight);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMControllerSet_rcvdResponse(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	ACMP_SM_LOCK();

	memcpy(pRcvdCmdResp, command, sizeof(*command));
	openavbAcmpSMControllerVars.rcvdResponse = TRUE;

	SEM_ERR_T(err);
	SEM_POST(openavbAcmpSMControllerSemaphore, err);
	SEM_LOG_ERR(err);

	ACMP_SM_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMControllerSet_doTerminate(bool value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	openavbAcmpSMControllerVars.doTerminate = value;

	SEM_ERR_T(err);
	SEM_POST(openavbAcmpSMControllerSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}
