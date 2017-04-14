/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : ACMP - AVDECC Connection Management Protocol : Talker State Machine
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       10-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Implements the ACMP - AVDECC Connection Management Protocol : Talker State Machine
 * IEEE Std 1722.1-2013 clause 8.2.2.6
 * 
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */
#include "openavb_platform.h"

#include <signal.h>

#define	AVB_LOG_COMPONENT	"ACMP"
#include "openavb_log.h"

#include "openavb_aem.h"
#include "openavb_acmp_sm_talker.h"
#include "openavb_acmp_sm_listener.h"
#include "openavb_acmp_message.h"
#include "openavb_avdecc_pipeline_interaction_pub.h"

typedef enum {
	OPENAVB_ACMP_SM_TALKER_STATE_WAITING,
	OPENAVB_ACMP_SM_TALKER_STATE_CONNECT,
	OPENAVB_ACMP_SM_TALKER_STATE_DISCONNECT,
	OPENAVB_ACMP_SM_TALKER_STATE_GET_STATE,
	OPENAVB_ACMP_SM_TALKER_STATE_GET_CONNECTION
} openavb_acmp_sm_talker_state_t;

extern openavb_acmp_sm_global_vars_t openavbAcmpSMGlobalVars;

static openavb_acmp_ACMPCommandResponse_t rcvdCmdResp;
static openavb_acmp_ACMPCommandResponse_t *pRcvdCmdResp = &rcvdCmdResp;
static openavb_acmp_sm_talker_vars_t openavbAcmpSMTalkerVars = {0};
static bool bRunning = FALSE;

extern MUTEX_HANDLE(openavbAcmpMutex);
#define ACMP_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAcmpMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ACMP_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAcmpMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

static MUTEX_HANDLE(openavbAcmpSMTalkerMutex);
#define ACMP_SM_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAcmpSMTalkerMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ACMP_SM_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAcmpSMTalkerMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

SEM_T(openavbAcmpSMTalkerSemaphore);
THREAD_TYPE(openavbAcmpSmTalkerThread);
THREAD_DEFINITON(openavbAcmpSmTalkerThread);


openavb_list_node_t openavbAcmpSMTalker_findListenerPairNodeFromCommand(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavb_list_node_t node = NULL;

	openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo = openavbArrayDataIdx(openavbAcmpSMTalkerVars.talkerStreamInfos, command->talker_unique_id);
	if (pTalkerStreamInfo) {
		node = openavbListIterFirst(pTalkerStreamInfo->connected_listeners);
		while (node) {
			openavb_acmp_ListenerPair_t *pListenerPair = openavbListData(node);
			if (pListenerPair) {
				if (memcmp(pListenerPair->listener_entity_id, command->listener_entity_id, sizeof(pListenerPair->listener_entity_id)) == 0) {
					if (pListenerPair->listener_unique_id == command->listener_unique_id) {
						break;
					}
				}
			}
			node = openavbListIterNext(pTalkerStreamInfo->connected_listeners);
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return node;
}


bool openavbAcmpSMTalker_validTalkerUnique(U16 talkerUniqueId)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	if (!openavbAemGetDescriptor(openavbAemGetConfigIdx(), OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT, talkerUniqueId)) {
		AVB_TRACE_EXIT(AVB_TRACE_ACMP);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return TRUE;
}

U8 openavbAcmpSMTalker_connectTalker(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	U8 retStatus = OPENAVB_ACMP_STATUS_TALKER_MISBEHAVING;

	openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo = openavbArrayDataIdx(openavbAcmpSMTalkerVars.talkerStreamInfos, command->talker_unique_id);
	if (pTalkerStreamInfo) {
		openavb_list_node_t node = openavbAcmpSMTalker_findListenerPairNodeFromCommand(command);
		if (node) {
			// Already connected, so return the current status.
			memcpy(command->stream_id, pTalkerStreamInfo->stream_id, sizeof(command->stream_id));
			memcpy(command->stream_dest_mac, pTalkerStreamInfo->stream_dest_mac, sizeof(command->stream_dest_mac));
			command->stream_vlan_id = pTalkerStreamInfo->stream_vlan_id;
			command->connection_count = pTalkerStreamInfo->connection_count;
			retStatus = OPENAVB_ACMP_STATUS_SUCCESS;
		}
		else {
			if (!pTalkerStreamInfo->connected_listeners) {
				pTalkerStreamInfo->connected_listeners = openavbListNewList();
			}
			node = openavbListNew(pTalkerStreamInfo->connected_listeners, sizeof(openavb_acmp_ListenerPair_t));
			if (node) {
				openavb_acmp_ListenerPair_t *pListenerPair = openavbListData(node);
				memcpy(pListenerPair->listener_entity_id, command->listener_entity_id, sizeof(pListenerPair->listener_entity_id));
				pListenerPair->listener_unique_id = command->listener_unique_id;
				pTalkerStreamInfo->connection_count++;

				U16 configIdx = openavbAemGetConfigIdx();
				openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput = openavbAemGetDescriptor(configIdx, OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT, command->talker_unique_id);
				if (!pDescriptorStreamOutput) {
					AVB_TRACE_EXIT(AVB_TRACE_ACMP);
					return retStatus;
				}
				openavbAVDECCGetTalkerStreamInfo(pDescriptorStreamOutput, configIdx, pTalkerStreamInfo);

				if (openavbAVDECCRunTalker(pDescriptorStreamOutput, configIdx, pTalkerStreamInfo)) {

					memcpy(command->stream_id, pTalkerStreamInfo->stream_id, sizeof(command->stream_id));
					memcpy(command->stream_dest_mac, pTalkerStreamInfo->stream_dest_mac, sizeof(command->stream_dest_mac));
					command->stream_vlan_id = pTalkerStreamInfo->stream_vlan_id;
					command->connection_count = pTalkerStreamInfo->connection_count;
					retStatus = OPENAVB_ACMP_STATUS_SUCCESS;
				}
			}
		}
	}
	else {
		retStatus = OPENAVB_ACMP_STATUS_TALKER_UNKNOWN_ID;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return retStatus;
}

void openavbAcmpSMTalker_txResponse(U8 messageType, openavb_acmp_ACMPCommandResponse_t *response, U8 error)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	openavbAcmpMessageSend(messageType, response, error);
	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

U8 openavbAcmpSMTalker_disconnectTalker(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	U8 retStatus = OPENAVB_ACMP_STATUS_TALKER_MISBEHAVING;

	openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo = openavbArrayDataIdx(openavbAcmpSMTalkerVars.talkerStreamInfos, command->talker_unique_id);
	if (pTalkerStreamInfo) {
		openavb_list_node_t node = openavbAcmpSMTalker_findListenerPairNodeFromCommand(command);
		if (!node) {
			// Already disconnected, so return the current status.
			memcpy(command->stream_id, pTalkerStreamInfo->stream_id, sizeof(command->stream_id));
			memcpy(command->stream_dest_mac, pTalkerStreamInfo->stream_dest_mac, sizeof(command->stream_dest_mac));
			command->connection_count = pTalkerStreamInfo->connection_count;
			retStatus = OPENAVB_ACMP_STATUS_SUCCESS;
		}
		else {
			openavbListDelete(pTalkerStreamInfo->connected_listeners, node);
			pTalkerStreamInfo->connection_count--;

			U16 configIdx = openavbAemGetConfigIdx();
			openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput = openavbAemGetDescriptor(configIdx, OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT, command->talker_unique_id);
			if (!pDescriptorStreamOutput) {
				AVB_TRACE_EXIT(AVB_TRACE_ACMP);
				return retStatus;
			}
			openavbAVDECCGetTalkerStreamInfo(pDescriptorStreamOutput, configIdx, pTalkerStreamInfo);

			// Stop the Talker if connection_count is 0.
			if (pTalkerStreamInfo->connection_count > 0 ||
					openavbAVDECCStopTalker(pDescriptorStreamOutput, configIdx, pTalkerStreamInfo)) {
				memcpy(command->stream_id, pTalkerStreamInfo->stream_id, sizeof(command->stream_id));
				memcpy(command->stream_dest_mac, pTalkerStreamInfo->stream_dest_mac, sizeof(command->stream_dest_mac));
				command->connection_count = pTalkerStreamInfo->connection_count;
				retStatus = OPENAVB_ACMP_STATUS_SUCCESS;
			}
		}
	}
	else {
		retStatus = OPENAVB_ACMP_STATUS_TALKER_UNKNOWN_ID;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return retStatus;
}

U8 openavbAcmpSMTalker_getState(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	U8 retStatus = OPENAVB_ACMP_STATUS_TALKER_MISBEHAVING;

	openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo = openavbArrayDataIdx(openavbAcmpSMTalkerVars.talkerStreamInfos, command->talker_unique_id);
	if (pTalkerStreamInfo) {
		U16 configIdx = openavbAemGetConfigIdx();
		openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput = openavbAemGetDescriptor(configIdx, OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT, command->talker_unique_id);
		if (!pDescriptorStreamOutput) {
			AVB_TRACE_EXIT(AVB_TRACE_ACMP);
			return retStatus;
		}
		openavbAVDECCGetTalkerStreamInfo(pDescriptorStreamOutput, configIdx, pTalkerStreamInfo);

		memcpy(command->stream_id, pTalkerStreamInfo->stream_id, sizeof(command->stream_id));
		memcpy(command->stream_dest_mac, pTalkerStreamInfo->stream_dest_mac, sizeof(command->stream_dest_mac));
		command->stream_vlan_id = pTalkerStreamInfo->stream_vlan_id;
		command->connection_count = pTalkerStreamInfo->connection_count;
		retStatus = OPENAVB_ACMP_STATUS_SUCCESS;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return retStatus;
}

U8 openavbAcmpSMTalker_getConnection(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	U8 retStatus = OPENAVB_ACMP_STATUS_TALKER_MISBEHAVING;

	openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo = openavbArrayDataIdx(openavbAcmpSMTalkerVars.talkerStreamInfos, command->talker_unique_id);
	if (pTalkerStreamInfo) {
		int count = command->connection_count;

		openavb_list_node_t node = openavbListIterFirst(pTalkerStreamInfo->connected_listeners);
		while (node && count-- > 0) {
			node = openavbListIterNext(pTalkerStreamInfo->connected_listeners);
		}

		openavb_acmp_ListenerPair_t *pListenerPair = openavbListData(node);
		if (pListenerPair) {
			memcpy(command->stream_id, pTalkerStreamInfo->stream_id, sizeof(command->stream_id));
			memcpy(command->stream_dest_mac, pTalkerStreamInfo->stream_dest_mac, sizeof(command->stream_dest_mac));
			command->stream_vlan_id = pTalkerStreamInfo->stream_vlan_id;
			command->connection_count = pTalkerStreamInfo->connection_count;

			memcpy(command->listener_entity_id, pListenerPair->listener_entity_id, sizeof(command->listener_entity_id));
			command->listener_unique_id = pListenerPair->listener_unique_id;
			retStatus = OPENAVB_ACMP_STATUS_SUCCESS;
		}
		else {
			retStatus = OPENAVB_ACMP_STATUS_NO_SUCH_CONNECTION;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return retStatus;
}

void openavbAcmpSMTalkerStateMachine()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavb_acmp_sm_talker_state_t state = OPENAVB_ACMP_SM_TALKER_STATE_WAITING;

	// Lock such that the mutex is held unless waiting for a semaphore. Synchronous processing of command responses.
	ACMP_SM_LOCK();
	while (bRunning) {
		switch (state) {
			case OPENAVB_ACMP_SM_TALKER_STATE_WAITING:
				AVB_TRACE_LINE(AVB_TRACE_ACMP);
				AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_TALKER_STATE_WAITING");

				openavbAcmpSMTalkerVars.rcvdConnectTX = FALSE;
				openavbAcmpSMTalkerVars.rcvdDisconnectTX = FALSE;
				openavbAcmpSMTalkerVars.rcvdGetTXState = FALSE;
				openavbAcmpSMTalkerVars.rcvdGetTXConnection = FALSE;

				// Wait for a change in state
				while (state == OPENAVB_ACMP_SM_TALKER_STATE_WAITING && bRunning) {
					AVB_TRACE_LINE(AVB_TRACE_ACMP);

					ACMP_SM_UNLOCK();
					SEM_ERR_T(err);
					SEM_WAIT(openavbAcmpSMTalkerSemaphore, err);
					ACMP_SM_LOCK();

					if (SEM_IS_ERR_NONE(err)) {
						if (openavbAcmpSMTalkerVars.doTerminate) {
							bRunning = FALSE;
						}
						else if (memcmp(pRcvdCmdResp->talker_entity_id, openavbAcmpSMGlobalVars.my_id, sizeof(openavbAcmpSMGlobalVars.my_id)) == 0) {

							if (openavbAcmpSMTalkerVars.rcvdConnectTX) {
								state = OPENAVB_ACMP_SM_TALKER_STATE_CONNECT;
							}
							else if (openavbAcmpSMTalkerVars.rcvdDisconnectTX) {
								state = OPENAVB_ACMP_SM_TALKER_STATE_DISCONNECT;
							}
							else if (openavbAcmpSMTalkerVars.rcvdGetTXState) {
								state = OPENAVB_ACMP_SM_TALKER_STATE_GET_STATE;
							}
							else if (openavbAcmpSMTalkerVars.rcvdGetTXConnection) {
								state = OPENAVB_ACMP_SM_TALKER_STATE_GET_CONNECTION;
							}
						}
					}
				}
				break;

			case OPENAVB_ACMP_SM_TALKER_STATE_CONNECT:
				{
					AVB_TRACE_LINE(AVB_TRACE_ACMP);
					AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_TALKER_STATE_CONNECT");

					U8 error;
					openavb_acmp_ACMPCommandResponse_t response;
					memcpy(&response, pRcvdCmdResp, sizeof(response));
					if (openavbAcmpSMTalker_validTalkerUnique(pRcvdCmdResp->talker_unique_id)) {
						error = openavbAcmpSMTalker_connectTalker(&response);
					}
					else {
						error = OPENAVB_ACMP_STATUS_TALKER_UNKNOWN_ID;
					}
					openavbAcmpSMTalker_txResponse(OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_TX_RESPONSE, &response, error);

					state = OPENAVB_ACMP_SM_TALKER_STATE_WAITING;
				}
				break;

			case OPENAVB_ACMP_SM_TALKER_STATE_DISCONNECT:
				{
					AVB_TRACE_LINE(AVB_TRACE_ACMP);
					AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_TALKER_STATE_DISCONNECT");

					U8 error;
					openavb_acmp_ACMPCommandResponse_t response;
					memcpy(&response, pRcvdCmdResp, sizeof(response));
					if (openavbAcmpSMTalker_validTalkerUnique(pRcvdCmdResp->talker_unique_id)) {
						error = openavbAcmpSMTalker_disconnectTalker(&response);
					}
					else {
						error = OPENAVB_ACMP_STATUS_TALKER_UNKNOWN_ID;
					}
					openavbAcmpSMTalker_txResponse(OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_TX_RESPONSE, &response, error);

					state = OPENAVB_ACMP_SM_TALKER_STATE_WAITING;
				}
				break;

			case OPENAVB_ACMP_SM_TALKER_STATE_GET_STATE:
				{
					AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_TALKER_STATE_GET_STATE");

					U8 error;
					openavb_acmp_ACMPCommandResponse_t response;
					memcpy(&response, pRcvdCmdResp, sizeof(response));
					if (openavbAcmpSMTalker_validTalkerUnique(pRcvdCmdResp->talker_unique_id)) {
						error = openavbAcmpSMTalker_getState(&response);
					}
					else {
						error = OPENAVB_ACMP_STATUS_TALKER_UNKNOWN_ID;
					}
					openavbAcmpSMTalker_txResponse(OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_STATE_RESPONSE, &response, error);

					state = OPENAVB_ACMP_SM_TALKER_STATE_WAITING;
				}
				break;

			case OPENAVB_ACMP_SM_TALKER_STATE_GET_CONNECTION:
				{
					AVB_LOG_DEBUG("State:  OPENAVB_ACMP_SM_TALKER_STATE_GET_CONNECTION");

					U8 error;
					openavb_acmp_ACMPCommandResponse_t response;
					memcpy(&response, pRcvdCmdResp, sizeof(response));
					if (openavbAcmpSMTalker_validTalkerUnique(pRcvdCmdResp->talker_unique_id)) {
						error = openavbAcmpSMTalker_getConnection(&response);
					}
					else {
						error = OPENAVB_ACMP_STATUS_TALKER_UNKNOWN_ID;
					}
					openavbAcmpSMTalker_txResponse(OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_CONNECTION_RESPONSE, &response, error);

					state = OPENAVB_ACMP_SM_TALKER_STATE_WAITING;
				}
				break;
		}
	}
	ACMP_SM_UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void* openavbAcmpSMTalkerThreadFn(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	openavbAcmpSMTalkerStateMachine();
	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return NULL;
}

bool openavbAcmpSMTalkerStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavbAcmpSMTalkerVars.talkerStreamInfos = openavbArrayNewArray(sizeof(openavb_acmp_TalkerStreamInfo_t));
	if (!openavbAcmpSMTalkerVars.talkerStreamInfos) {
		AVB_LOG_ERROR("Unable to create talkerStreamInfos array. ACMP protocol not started.");
		AVB_TRACE_EXIT(AVB_TRACE_ACMP);
		return FALSE;
	}
	openavb_array_t streamOutputDescriptorArray = openavbAemGetDescriptorArray(openavbAemGetConfigIdx(), OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT);
	if (streamOutputDescriptorArray) {
		openavbArrayMultiNew(openavbAcmpSMTalkerVars.talkerStreamInfos, openavbArrayCount(streamOutputDescriptorArray));
	}

	MUTEX_ATTR_HANDLE(mta);
	MUTEX_ATTR_INIT(mta);
	MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
	MUTEX_ATTR_SET_NAME(mta, "openavbAcmpSMTalkerMutex");
	MUTEX_CREATE_ERR();
	MUTEX_CREATE(openavbAcmpSMTalkerMutex, mta);
	MUTEX_LOG_ERR("Could not create/initialize 'openavbAcmpSMTalkerMutex' mutex");

	SEM_ERR_T(err);
	SEM_INIT(openavbAcmpSMTalkerSemaphore, 1, err);
	SEM_LOG_ERR(err);

	// Start the State Machine
	bool errResult;
	bRunning = TRUE;
	THREAD_CREATE(openavbAcmpSmTalkerThread, openavbAcmpSmTalkerThread, NULL, openavbAcmpSMTalkerThreadFn, NULL);
	THREAD_CHECK_ERROR(openavbAcmpSmTalkerThread, "Thread / task creation failed", errResult);
	if (errResult) {
		bRunning = FALSE;
		AVB_TRACE_EXIT(AVB_TRACE_ACMP);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return TRUE;
}

void openavbAcmpSMTalkerStop()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	if (bRunning) {
		openavbAcmpSMTalkerSet_doTerminate(TRUE);

		THREAD_JOIN(openavbAcmpSmTalkerThread, NULL);
	}

	SEM_ERR_T(err);
	SEM_DESTROY(openavbAcmpSMTalkerSemaphore, err);
	SEM_LOG_ERR(err);

	openavb_array_elem_t node = openavbArrayIterFirst(openavbAcmpSMTalkerVars.talkerStreamInfos);
	if (node) {
		openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo = openavbArrayData(node);
		if (pTalkerStreamInfo != NULL) {
			openavbListDeleteList(pTalkerStreamInfo->connected_listeners);
			node = openavbArrayIterNext(openavbAcmpSMTalkerVars.talkerStreamInfos);
		}
	}
	openavbArrayDeleteArray(openavbAcmpSMTalkerVars.talkerStreamInfos);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMTalkerSet_rcvdConnectTXCmd(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	ACMP_SM_LOCK();
	memcpy(pRcvdCmdResp, command, sizeof(*command));
	ACMP_SM_UNLOCK();
	openavbAcmpSMTalkerVars.rcvdConnectTX = TRUE;

	SEM_ERR_T(err);
	SEM_POST(openavbAcmpSMTalkerSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMTalkerSet_rcvdDisconnectTXCmd(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	ACMP_SM_LOCK();
	memcpy(pRcvdCmdResp, command, sizeof(*command));
	ACMP_SM_UNLOCK();
	openavbAcmpSMTalkerVars.rcvdDisconnectTX = TRUE;

	SEM_ERR_T(err);
	SEM_POST(openavbAcmpSMTalkerSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMTalkerSet_rcvdGetTXState(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	ACMP_SM_LOCK();
	memcpy(pRcvdCmdResp, command, sizeof(*command));
	ACMP_SM_UNLOCK();
	openavbAcmpSMTalkerVars.rcvdGetTXState = TRUE;

	SEM_ERR_T(err);
	SEM_POST(openavbAcmpSMTalkerSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMTalkerSet_rcvdGetTXConnectionCmd(openavb_acmp_ACMPCommandResponse_t *command)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	ACMP_SM_LOCK();
	memcpy(pRcvdCmdResp, command, sizeof(*command));
	ACMP_SM_UNLOCK();
	openavbAcmpSMTalkerVars.rcvdGetTXConnection = TRUE;

	SEM_ERR_T(err);
	SEM_POST(openavbAcmpSMTalkerSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpSMTalkerSet_doTerminate(bool value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	openavbAcmpSMTalkerVars.doTerminate = value;

	SEM_ERR_T(err);
	SEM_POST(openavbAcmpSMTalkerSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}


