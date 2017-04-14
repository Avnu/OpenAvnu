/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : ACMP - AVDECC Connection Management Protocol : Listener State Machine
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       10-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Implements the ACMP - AVDECC Connection Management Protocol : Listener State Machine
 * IEEE Std 1722.1-2013 clause 8.2.2.5
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */
#include "openavb_platform.h"

#include <errno.h>

#define	AVB_LOG_COMPONENT	"ACMP"
#include "openavb_log.h"

#include "openavb_debug.h"
#include "openavb_time.h"
#include "openavb_aem.h"
#include "openavb_acmp.h"
#include "openavb_acmp_message.h"
#include "openavb_acmp_sm_talker.h"
#include "openavb_acmp_sm_listener.h"
#include "openavb_avdecc_pipeline_interaction_pub.h"
#include "openavb_time.h"

typedef enum {
	OPENAVB_ACMP_SM_LISTENER_STATE_WAITING,
	OPENAVB_ACMP_SM_LISTENER_STATE_CONNECT_RX_COMMAND,
	OPENAVB_ACMP_SM_LISTENER_STATE_CONNECT_TX_RESPONSE,
	OPENAVB_ACMP_SM_LISTENER_STATE_GET_STATE,
	OPENAVB_ACMP_SM_LISTENER_STATE_DISCONNECT_RX_COMMAND,
	OPENAVB_ACMP_SM_LISTENER_STATE_DISCONNECT_TX_RESPONSE,
	OPENAVB_ACMP_SM_LISTENER_STATE_CONNECT_TX_TIMEOUT,
	OPENAVB_ACMP_SM_LISTENER_STATE_DISCONNECT_TX_TIMEOUT,
} openavb_acmp_sm_listener_state_t;

extern openavb_acmp_sm_global_vars_t openavbAcmpSMGlobalVars;

static openavb_acmp_ACMPCommandResponse_t rcvdCmdResp;
static openavb_acmp_ACMPCommandResponse_t *pRcvdCmdResp = &rcvdCmdResp;
static openavb_acmp_sm_listener_vars_t openavbAcmpSMListenerVars = {0};
static bool bRunning = FALSE;

extern MUTEX_HANDLE(openavbAcmpMutex);
#define ACMP_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAcmpMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ACMP_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAcmpMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

static MUTEX_HANDLE(openavbAcmpSMListenerMutex);
#define ACMP_SM_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAcmpSMListenerMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ACMP_SM_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAcmpSMListenerMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

SEM_T(openavbAcmpSMListenerSemaphore);
THREAD_TYPE(openavbAcmpSmListenerThread);
THREAD_DEFINITON(openavbAcmpSmListenerThread);

openavb_list_node_t openavbAcmpSMListener_findInflightNodeFromCommand(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavb_list_node_t node = NULL;

	node = openavbListIterFirst(openavbAcmpSMListenerVars.inflight);
	while (node) {
		openavb_acmp_InflightCommand_t *pInFlightCommand = openavbListData(node);
		if (pInFlightCommand) {
			if (memcmp(pInFlightCommand->command.talker_entity_id, command->talker_entity_id, sizeof(pInFlightCommand->command.talker_entity_id)) == 0) {
				if (pInFlightCommand->command.talker_unique_id == command->talker_unique_id ) {
					if (pInFlightCommand->command.sequence_id == command->sequence_id) {
						break;
					}
				}
			}
		}
		node = openavbListIterNext(openavbAcmpSMListenerVars.inflight);
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return node;
}

bool openavbAcmpSMListener_validListenerUnique(U16 listenerUniqueId)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	if (!openavbAemGetDescriptor(openavbAemGetConfigIdx(), OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, listenerUniqueId)) {
		AVB_TRACE_EXIT(AVB_TRACE_ACMP);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return TRUE;
}

bool openavbAcmpSMListener_listenerIsConnected(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	bool bResult = FALSE;

	// Returns TRUE if listener is connected to a stream source other than the one specified in the command.
	openavb_acmp_ListenerStreamInfo_t *pListenerStreamInfo = openavbArrayDataIdx(openavbAcmpSMListenerVars.listenerStreamInfos, command->listener_unique_id);
	if (pListenerStreamInfo) {
		if (pListenerStreamInfo->connected) {
			bResult = TRUE;
			if (memcmp(pListenerStreamInfo->talker_entity_id, command->talker_entity_id, sizeof(command->talker_entity_id)) == 0) {
				if (pListenerStreamInfo->talker_unique_id == command->talker_unique_id) {
					bResult = FALSE;
				}
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return bResult;
}

bool openavbAcmpSMListener_listenerIsConnectedTo(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	bool bResult = FALSE;

	// Returns TRUE if listener is connected to the stream source specified in the command.
	openavb_acmp_ListenerStreamInfo_t *pListenerStreamInfo = openavbArrayDataIdx(openavbAcmpSMListenerVars.listenerStreamInfos, command->listener_unique_id);
	if (pListenerStreamInfo) {
		if (pListenerStreamInfo->connected) {
			if (memcmp(pListenerStreamInfo->talker_entity_id, command->talker_entity_id, sizeof(command->talker_entity_id)) == 0) {
				if (pListenerStreamInfo->talker_unique_id == command->talker_unique_id) {
					bResult = TRUE;
				}
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return bResult;
}

void openavbAcmpSMListener_txCommand(U8 messageType, openavb_acmp_ACMPCommandResponse_t *command, bool retry)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	openavb_list_node_t node = NULL;

	openavbRC rc = openavbAcmpMessageSend(messageType, command, OPENAVB_ACMP_STATUS_SUCCESS);
	if (IS_OPENAVB_SUCCESS(rc)) {
		if (!retry) {
			node = openavbListNew(openavbAcmpSMListenerVars.inflight, sizeof(openavb_acmp_InflightCommand_t));
			if (node) {
				openavb_acmp_InflightCommand_t *pInFlightCommand = openavbListData(node);
				if (pInFlightCommand) {

					memcpy(&pInFlightCommand->command, command, sizeof(pInFlightCommand->command));
					pInFlightCommand->retried = FALSE;
					pInFlightCommand->original_sequence_id = command->sequence_id;	// AVDECC_TODO - is this correct?

					CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &pInFlightCommand->timer);
					switch (messageType) {
						case OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_RX_COMMAND:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_CONNECT_RX_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						case OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_RX_COMMAND:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_DISCONNECT_RX_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						case OPENAVB_ACMP_MESSAGE_TYPE_GET_RX_STATE_COMMAND:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_GET_RX_STATE_COMMAND * MICROSECONDS_PER_MSEC);
							break;
					}
				}
			}
		}
		else {
			// Retry case
			node = openavbAcmpSMListener_findInflightNodeFromCommand(command);
			if (node) {
				openavb_acmp_InflightCommand_t *pInFlightCommand = openavbListData(node);
				if (pInFlightCommand) {
					pInFlightCommand->retried = TRUE;

					CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &pInFlightCommand->timer);
					switch (messageType) {
						case OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_RX_COMMAND:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_CONNECT_RX_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						case OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_RX_COMMAND:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_DISCONNECT_RX_COMMAND * MICROSECONDS_PER_MSEC);
							break;
						case OPENAVB_ACMP_MESSAGE_TYPE_GET_RX_STATE_COMMAND:
							openavbTimeTimespecAddUsec(&pInFlightCommand->timer, OPENAVB_ACMP_COMMAND_TIMEOUT_GET_RX_STATE_COMMAND * MICROSECONDS_PER_MSEC);
							break;
					}
				}
			}
		}
	}
	else {
		// Failed to send command
		openavbAcmpSMListener_txResponse(messageType + 1, command, OPENAVB_ACMP_STATUS_COULD_NOT_SEND_MESSAGE);
		if (retry) {
			node = openavbAcmpSMListener_findInflightNodeFromCommand(command);
			if (node) {
				openavbListDelete(openavbAcmpSMListenerVars.inflight, node);
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMListener_txResponse(U8 messageType, openavb_acmp_ACMPCommandResponse_t *response, U8 error)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	openavbAcmpMessageSend(messageType, response, error);
	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

U8 openavbAcmpSMListener_connectListener(openavb_acmp_ACMPCommandResponse_t *response)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	U8 retStatus;

	// AVDECC_TODO:  What do we do if the Listener is already connected?

	openavb_acmp_ListenerStreamInfo_t *pListenerStreamInfo = openavbArrayDataIdx(openavbAcmpSMListenerVars.listenerStreamInfos, response->listener_unique_id);
	if (pListenerStreamInfo) {
		memcpy(pListenerStreamInfo->talker_entity_id, response->talker_entity_id, sizeof(pListenerStreamInfo->talker_entity_id));
		pListenerStreamInfo->talker_unique_id = response->talker_unique_id;
		pListenerStreamInfo->connected = TRUE; 
		memcpy(pListenerStreamInfo->stream_id, response->stream_id, sizeof(pListenerStreamInfo->stream_id));
		memcpy(pListenerStreamInfo->stream_dest_mac, response->stream_dest_mac, sizeof(pListenerStreamInfo->stream_dest_mac));
		memcpy(pListenerStreamInfo->controller_entity_id, response->controller_entity_id, sizeof(pListenerStreamInfo->controller_entity_id));
		pListenerStreamInfo->flags = response->flags;
		pListenerStreamInfo->stream_vlan_id = response->stream_vlan_id;

		U16 configIdx = openavbAemGetConfigIdx();
		openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput = openavbAemGetDescriptor(configIdx, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, response->listener_unique_id);

		// AVDECC_TODO:  Verify that the pDescriptorStreamInput->stream details match that of pListenerStreamInfo.

		if (!pDescriptorStreamInput) {
			retStatus = OPENAVB_ACMP_STATUS_LISTENER_UNKNOWN_ID;
		}
		else if (openavbAVDECCRunListener(pDescriptorStreamInput, configIdx, pListenerStreamInfo)) {
			retStatus = OPENAVB_ACMP_STATUS_SUCCESS;
		}
		else {
			retStatus = OPENAVB_ACMP_STATUS_LISTENER_MISBEHAVING;
		}
	}
	else {
		retStatus = OPENAVB_ACMP_STATUS_LISTENER_UNKNOWN_ID;
	}

	// AVDECC_TODO - any flags need to be updated in the response?

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return retStatus;
}

U8 openavbAcmpSMListener_disconnectListener(openavb_acmp_ACMPCommandResponse_t *response)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	U8 retStatus; 

	openavb_acmp_ListenerStreamInfo_t *pListenerStreamInfo = openavbArrayDataIdx(openavbAcmpSMListenerVars.listenerStreamInfos, response->listener_unique_id);
	if (pListenerStreamInfo) {
		U16 configIdx = openavbAemGetConfigIdx();
		openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput = openavbAemGetDescriptor(configIdx, OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT, response->listener_unique_id);

		// AVDECC_TODO:  Verify that the pDescriptorStreamInput->stream details match that of pListenerStreamInfo.

		if (!pDescriptorStreamInput) {
			retStatus = OPENAVB_ACMP_STATUS_LISTENER_UNKNOWN_ID;
		}
		else if (openavbAVDECCStopListener(pDescriptorStreamInput, configIdx, pListenerStreamInfo)) {
			retStatus = OPENAVB_ACMP_STATUS_SUCCESS;
		}
		else {
			retStatus = OPENAVB_ACMP_STATUS_LISTENER_MISBEHAVING;
		}
		memset(pListenerStreamInfo, 0, sizeof(*pListenerStreamInfo));
	}
	else {
		retStatus = OPENAVB_ACMP_STATUS_LISTENER_UNKNOWN_ID;
	}

	// AVDECC_TODO - any flags need to be updated in the response?

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return retStatus;
}

void openavbAcmpSMListener_cancelTimeout(openavb_acmp_ACMPCommandResponse_t *commandResponse)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	// Nothing to do since the timer is reset during the wait state and the inFlight entry will have already been removed.
	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMListener_removeInflight(openavb_acmp_ACMPCommandResponse_t *commandResponse)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	openavb_list_node_t node = openavbAcmpSMListener_findInflightNodeFromCommand(commandResponse);
	if (node) {
		openavbListDelete(openavbAcmpSMListenerVars.inflight, node);
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

U8 openavbAcmpSMListener_getState(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	U8 retStatus = OPENAVB_ACMP_STATUS_LISTENER_MISBEHAVING;

	openavb_acmp_ListenerStreamInfo_t *pListenerStreamInfo = openavbArrayDataIdx(openavbAcmpSMListenerVars.listenerStreamInfos, command->listener_unique_id);
	if (pListenerStreamInfo) {
		memcpy(command->stream_id, pListenerStreamInfo->stream_id, sizeof(command->stream_id));
		memcpy(command->stream_dest_mac, pListenerStreamInfo->stream_dest_mac, sizeof(command->stream_dest_mac));
		command->stream_vlan_id = pListenerStreamInfo->stream_vlan_id;
		command->connection_count = pListenerStreamInfo->connected ? 1 : 0;			// AVDECC_TODO - questionable information in spec.
		command->flags = pListenerStreamInfo->flags;
		memcpy(command->talker_entity_id, pListenerStreamInfo->talker_entity_id, sizeof(command->talker_entity_id));
		//command->listener_unique_id = command->listener_unique_id;				// Overwriting data in passed in structure
		retStatus = OPENAVB_ACMP_STATUS_SUCCESS;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return retStatus;
}

void openavbAcmpSMListenerStateMachine()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavb_acmp_InflightCommand_t *pInflightActive = NULL;		// The inflight command for the current state
	openavb_acmp_sm_listener_state_t state = OPENAVB_ACMP_SM_LISTENER_STATE_WAITING;

	// Lock such that the mutex is held unless waiting for a semaphore. Synchronous processing of command responses.
	ACMP_SM_LOCK();
	while (bRunning) {
		switch (state) {
			case OPENAVB_ACMP_SM_LISTENER_STATE_WAITING:
				AVB_TRACE_LINE(AVB_TRACE_ACMP);
				AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_LISTENER_STATE_WAITING");

				openavbAcmpSMListenerVars.rcvdConnectRXCmd = FALSE;
				openavbAcmpSMListenerVars.rcvdDisconnectRXCmd = FALSE;
				openavbAcmpSMListenerVars.rcvdConnectTXResp = FALSE;
				openavbAcmpSMListenerVars.rcvdDisconnectTXResp = FALSE;
				openavbAcmpSMListenerVars.rcvdGetRXState = FALSE;

				// Wait for a change in state
				while (state == OPENAVB_ACMP_SM_LISTENER_STATE_WAITING && bRunning) {
					AVB_TRACE_LINE(AVB_TRACE_ACMP);

					// Calculate timeout for inflight commands
					// Start is a arbitrary large time out.
					struct timespec timeout;
					CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &timeout);
					timeout.tv_sec += 60;	// At most will timeout after 60 seconds

					// Look for soonest inflight command timeout
					openavb_list_node_t node = openavbListIterFirst(openavbAcmpSMListenerVars.inflight);
					while (node) {
						openavb_acmp_InflightCommand_t *pInflight = openavbListData(node);
						if ((openavbTimeTimespecCmp(&pInflight->timer, &timeout) < 0)) {
							timeout.tv_sec = pInflight->timer.tv_sec;
							timeout.tv_nsec = pInflight->timer.tv_nsec;
						}
						node = openavbListIterNext(openavbAcmpSMListenerVars.inflight);
					}

					ACMP_SM_UNLOCK();
					U32 timeoutMSec = 0;
					struct timespec now;
					CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &now);
					timeoutMSec = openavbTimeUntilMSec(&now, &timeout);
					SEM_ERR_T(err);
					SEM_TIMEDWAIT(openavbAcmpSMListenerSemaphore, timeoutMSec, err);
					ACMP_SM_LOCK();

					if (!SEM_IS_ERR_NONE(err)) {
						if (SEM_IS_ERR_TIMEOUT(err)) {
							struct timespec now;
							CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &now);

							// Look for a timed out inflight command
							openavb_list_node_t node = openavbListIterFirst(openavbAcmpSMListenerVars.inflight);
							while (node) {
								openavb_acmp_InflightCommand_t *pInflight = openavbListData(node);
								if ((openavbTimeTimespecCmp(&now, &pInflight->timer) >= 0)) {
									// Found a timed out command
									if (pInflight->command.message_type == OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_TX_COMMAND) {
										state = OPENAVB_ACMP_SM_LISTENER_STATE_CONNECT_TX_TIMEOUT;
										pInflightActive = pInflight;
									}
									else if (pInflight->command.message_type == OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_TX_COMMAND) {
										state = OPENAVB_ACMP_SM_LISTENER_STATE_DISCONNECT_TX_TIMEOUT;
										pInflightActive = pInflight;
									}
									break;
								}
								node = openavbListIterNext(openavbAcmpSMListenerVars.inflight);
							}
						}
					}
					else {
						if (openavbAcmpSMListenerVars.doTerminate) {
							bRunning = FALSE;
						}
						else if (memcmp(pRcvdCmdResp->listener_entity_id, openavbAcmpSMGlobalVars.my_id, sizeof(openavbAcmpSMGlobalVars.my_id)) == 0) {
							if (openavbAcmpSMListenerVars.rcvdConnectRXCmd) {
								state = OPENAVB_ACMP_SM_LISTENER_STATE_CONNECT_RX_COMMAND;
							}
							else if (openavbAcmpSMListenerVars.rcvdConnectTXResp) {
								state = OPENAVB_ACMP_SM_LISTENER_STATE_CONNECT_TX_RESPONSE;
							}
							else if (openavbAcmpSMListenerVars.rcvdGetRXState) {
								state = OPENAVB_ACMP_SM_LISTENER_STATE_GET_STATE;
							}
							else if (openavbAcmpSMListenerVars.rcvdDisconnectRXCmd) {
								state = OPENAVB_ACMP_SM_LISTENER_STATE_DISCONNECT_RX_COMMAND;
							}
							else if (openavbAcmpSMListenerVars.rcvdDisconnectTXResp) {
								state = OPENAVB_ACMP_SM_LISTENER_STATE_DISCONNECT_TX_RESPONSE;
							}
						}
					}
				}
				break;

			case OPENAVB_ACMP_SM_LISTENER_STATE_CONNECT_RX_COMMAND:
				{
					AVB_TRACE_LINE(AVB_TRACE_ACMP);
					AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_LISTENER_STATE_CONNECT_RX_COMMAND");

					if (openavbAcmpSMListener_validListenerUnique(pRcvdCmdResp->listener_unique_id)) {
						if (!openavbAcmpSMListener_listenerIsConnected(pRcvdCmdResp)) {
							openavbAcmpSMListener_txCommand(OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_TX_COMMAND, pRcvdCmdResp, FALSE);
						}
						else {
							openavbAcmpSMListener_txResponse(OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_TX_RESPONSE, pRcvdCmdResp, OPENAVB_ACMP_STATUS_LISTENER_EXCLUSIVE);
						}
					}
					else {
						openavbAcmpSMListener_txResponse(OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_TX_RESPONSE, pRcvdCmdResp, OPENAVB_ACMP_STATUS_LISTENER_UNKNOWN_ID);
					}
					state = OPENAVB_ACMP_SM_LISTENER_STATE_WAITING;
				}
				break;
			case OPENAVB_ACMP_SM_LISTENER_STATE_CONNECT_TX_RESPONSE:
				{
					AVB_TRACE_LINE(AVB_TRACE_ACMP);
					AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_LISTENER_STATE_CONNECT_TX_RESPONSE");

					U8 status;
					if (openavbAcmpSMListener_validListenerUnique(pRcvdCmdResp->listener_unique_id)) {
						openavb_acmp_ACMPCommandResponse_t response;
						memcpy(&response, pRcvdCmdResp, sizeof(response));
						if (pRcvdCmdResp->status == OPENAVB_ACMP_STATUS_SUCCESS) {
							status = openavbAcmpSMListener_connectListener(&response);
						}
						else {
							status = pRcvdCmdResp->status;
						}
						
						openavb_list_node_t node = openavbAcmpSMListener_findInflightNodeFromCommand(pRcvdCmdResp);
						if (node) {
							openavb_acmp_InflightCommand_t *pInFlightCommand = openavbListData(node);
							if (pInFlightCommand) {
								response.sequence_id = pInFlightCommand->original_sequence_id;
							}
						}
						openavbAcmpSMListener_cancelTimeout(pRcvdCmdResp);
						openavbAcmpSMListener_removeInflight(pRcvdCmdResp);
						openavbAcmpSMListener_txResponse(OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_RX_RESPONSE, &response, status);
					}
					state = OPENAVB_ACMP_SM_LISTENER_STATE_WAITING;
				}
				break;
			case OPENAVB_ACMP_SM_LISTENER_STATE_GET_STATE:
				{
					AVB_TRACE_LINE(AVB_TRACE_ACMP);
					AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_LISTENER_STATE_GET_STATE");

					U8 error;

					openavb_acmp_ACMPCommandResponse_t response;
					memcpy(&response, pRcvdCmdResp, sizeof(response));
					if (openavbAcmpSMListener_validListenerUnique(pRcvdCmdResp->listener_unique_id)) {
						error = openavbAcmpSMListener_getState(pRcvdCmdResp);
					}
					else {
						error = OPENAVB_ACMP_STATUS_LISTENER_UNKNOWN_ID;
					}
					openavbAcmpSMListener_txResponse(OPENAVB_ACMP_MESSAGE_TYPE_GET_RX_STATE_RESPONSE, &response, error);
					state = OPENAVB_ACMP_SM_LISTENER_STATE_WAITING;
				}
				break;
			case OPENAVB_ACMP_SM_LISTENER_STATE_DISCONNECT_RX_COMMAND:
				{
					AVB_TRACE_LINE(AVB_TRACE_ACMP);
					AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_LISTENER_STATE_DISCONNECT_RX_COMMAND");

					if (openavbAcmpSMListener_validListenerUnique(pRcvdCmdResp->listener_unique_id)) {
						U8 status;

						openavb_acmp_ACMPCommandResponse_t response;
						memcpy(&response, pRcvdCmdResp, sizeof(response));
						status = openavbAcmpSMListener_disconnectListener(pRcvdCmdResp);
						if (status == OPENAVB_ACMP_STATUS_SUCCESS) {
							openavbAcmpSMListener_txCommand(OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_TX_COMMAND, pRcvdCmdResp, FALSE);
						}
						else {
							openavbAcmpSMListener_txResponse(OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_RX_RESPONSE, &response, status);
						}
					}
					else {
						openavbAcmpSMListener_txResponse(OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_RX_RESPONSE, pRcvdCmdResp, OPENAVB_ACMP_STATUS_NOT_CONNECTED);
					}
					state = OPENAVB_ACMP_SM_LISTENER_STATE_WAITING;
				}
				break;
			case OPENAVB_ACMP_SM_LISTENER_STATE_DISCONNECT_TX_RESPONSE:
				{
					AVB_TRACE_LINE(AVB_TRACE_ACMP);
					AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_LISTENER_STATE_DISCONNECT_TX_RESPONSE");

					if (openavbAcmpSMListener_validListenerUnique(pRcvdCmdResp->listener_unique_id)) {
						openavb_acmp_ACMPCommandResponse_t response;
						memcpy(&response, pRcvdCmdResp, sizeof(response));
						U8 status = pRcvdCmdResp->status;

						openavb_list_node_t node = openavbAcmpSMListener_findInflightNodeFromCommand(pRcvdCmdResp);
						if (node) {
							openavb_acmp_InflightCommand_t *pInFlightCommand = openavbListData(node);
							if (pInFlightCommand) {
								response.sequence_id = pInFlightCommand->original_sequence_id;
							}
						}
						openavbAcmpSMListener_cancelTimeout(pRcvdCmdResp);
						openavbAcmpSMListener_removeInflight(pRcvdCmdResp);
						openavbAcmpSMListener_txResponse(OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_RX_RESPONSE, &response, status);

						state = OPENAVB_ACMP_SM_LISTENER_STATE_WAITING;
					}
				}
				break;
			case OPENAVB_ACMP_SM_LISTENER_STATE_CONNECT_TX_TIMEOUT:
				{
					AVB_TRACE_LINE(AVB_TRACE_ACMP);
					AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_LISTENER_STATE_CONNECT_TX_TIMEOUT");

					if (pInflightActive) {
						if (pInflightActive->retried) {
							openavb_acmp_ACMPCommandResponse_t response;
							memcpy(&response, &pInflightActive->command, sizeof(response));
							response.sequence_id = pInflightActive->original_sequence_id;
							openavbAcmpSMListener_txResponse(OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_RX_RESPONSE, &response, OPENAVB_ACMP_STATUS_LISTENER_TALKER_TIMEOUT);
							openavbAcmpSMListener_removeInflight(&pInflightActive->command);
						}
						else {
							openavbAcmpSMListener_txCommand(OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_TX_COMMAND, &pInflightActive->command, TRUE);
						}
					}
					state = OPENAVB_ACMP_SM_LISTENER_STATE_WAITING;
				}
				break;
			case OPENAVB_ACMP_SM_LISTENER_STATE_DISCONNECT_TX_TIMEOUT:
				{
					AVB_TRACE_LINE(AVB_TRACE_ACMP);
					AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_LISTENER_STATE_DISCONNECT_TX_TIMEOUT");

					if (pInflightActive) {
						if (pInflightActive->retried) {
							openavb_acmp_ACMPCommandResponse_t response;
							memcpy(&response, &pInflightActive->command, sizeof(response));
							response.sequence_id = pInflightActive->original_sequence_id;
							openavbAcmpSMListener_txResponse(OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_RX_RESPONSE, &response, OPENAVB_ACMP_STATUS_LISTENER_TALKER_TIMEOUT);
							openavbAcmpSMListener_removeInflight(&pInflightActive->command);
						}
						else {
							openavbAcmpSMListener_txCommand(OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_TX_COMMAND, &pInflightActive->command, TRUE);
						}
					}
					state = OPENAVB_ACMP_SM_LISTENER_STATE_WAITING;
				}
				break;
		}
	}
	ACMP_SM_UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void* openavbAcmpSmListenerThreadFn(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	openavbAcmpSMListenerStateMachine();
	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return NULL;
}

bool openavbAcmpSMListenerStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavbAcmpSMListenerVars.inflight = openavbListNewList();
	if (!openavbAcmpSMListenerVars.inflight) {
		AVB_LOG_ERROR("Unable to create inflight list. ACMP protocol not started.");
		AVB_TRACE_EXIT(AVB_TRACE_ACMP);
		return FALSE;
	}
	openavbAcmpSMListenerVars.listenerStreamInfos = openavbArrayNewArray(sizeof(openavb_acmp_ListenerStreamInfo_t));
	if (!openavbAcmpSMListenerVars.listenerStreamInfos) {
		AVB_LOG_ERROR("Unable to create listenerStreamInfos array. ACMP protocol not started.");
		AVB_TRACE_EXIT(AVB_TRACE_ACMP);
		return FALSE;
	}
	openavb_array_t streamInputDescriptorArray = openavbAemGetDescriptorArray(openavbAemGetConfigIdx(), OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT);
	if (streamInputDescriptorArray) {
		openavbArrayMultiNew(openavbAcmpSMListenerVars.listenerStreamInfos, openavbArrayCount(streamInputDescriptorArray));
	}

	MUTEX_ATTR_HANDLE(mta);
	MUTEX_ATTR_INIT(mta);
	MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
	MUTEX_ATTR_SET_NAME(mta, "openavbAcmpSMListenerMutex");
	MUTEX_CREATE_ERR();
	MUTEX_CREATE(openavbAcmpSMListenerMutex, mta);
	MUTEX_LOG_ERR("Could not create/initialize 'openavbAcmpSMListenerMutex' mutex");

	SEM_ERR_T(err);
	SEM_INIT(openavbAcmpSMListenerSemaphore, 1, err);
	SEM_LOG_ERR(err);

	// Start the State Machine
	bool errResult;
	bRunning = TRUE;
	THREAD_CREATE(openavbAcmpSmListenerThread, openavbAcmpSmListenerThread, NULL, openavbAcmpSmListenerThreadFn, NULL);
	THREAD_CHECK_ERROR(openavbAcmpSmListenerThread, "Thread / task creation failed", errResult);
	if (errResult) {
		bRunning = FALSE;
		AVB_TRACE_EXIT(AVB_TRACE_ACMP);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return TRUE;
}

void openavbAcmpSMListenerStop()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	if (bRunning) {
		openavbAcmpSMListenerSet_doTerminate(TRUE);

		THREAD_JOIN(openavbAcmpSmListenerThread, NULL);
	}

	SEM_ERR_T(err);
	SEM_DESTROY(openavbAcmpSMListenerSemaphore, err);
	SEM_LOG_ERR(err);

	openavbListDeleteList(openavbAcmpSMListenerVars.inflight);
	openavbArrayDeleteArray(openavbAcmpSMListenerVars.listenerStreamInfos);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMListenerSet_rcvdConnectRXCmd(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	ACMP_SM_LOCK();
	memcpy(pRcvdCmdResp, command, sizeof(*command));
	ACMP_SM_UNLOCK();
	openavbAcmpSMListenerVars.rcvdConnectRXCmd = TRUE;
	SEM_ERR_T(err);
	SEM_POST(openavbAcmpSMListenerSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMListenerSet_rcvdDisconnectRXCmd(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	ACMP_SM_LOCK();
	memcpy(pRcvdCmdResp, command, sizeof(*command));
	ACMP_SM_UNLOCK();
	openavbAcmpSMListenerVars.rcvdDisconnectRXCmd = TRUE;
	SEM_ERR_T(err);
	SEM_POST(openavbAcmpSMListenerSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMListenerSet_rcvdConnectTXResp(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	ACMP_SM_LOCK();
	memcpy(pRcvdCmdResp, command, sizeof(*command));
	ACMP_SM_UNLOCK();
	openavbAcmpSMListenerVars.rcvdConnectTXResp = TRUE;
	SEM_ERR_T(err);
	SEM_POST(openavbAcmpSMListenerSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMListenerSet_rcvdDisconnectTXResp(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	ACMP_SM_LOCK();
	memcpy(pRcvdCmdResp, command, sizeof(*command));
	ACMP_SM_UNLOCK();
	openavbAcmpSMListenerVars.rcvdDisconnectTXResp = TRUE;
	SEM_ERR_T(err);
	SEM_POST(openavbAcmpSMListenerSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMListenerSet_rcvdGetRXState(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	ACMP_SM_LOCK();
	memcpy(pRcvdCmdResp, command, sizeof(*command));
	ACMP_SM_UNLOCK();
	openavbAcmpSMListenerVars.rcvdGetRXState = TRUE;
	SEM_ERR_T(err);
	SEM_POST(openavbAcmpSMListenerSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMListenerSet_doTerminate(bool value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	openavbAcmpSMListenerVars.doTerminate = value;
	SEM_ERR_T(err);
	SEM_POST(openavbAcmpSMListenerSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}




