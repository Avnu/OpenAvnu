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
 * MODULE : AVDECC Enumeration and control protocol (AECP) : Entity Model Entity State Machine
 * MODULE SUMMARY : Implements the AVDECC Enumeration and control protocol (AECP) : Entity Model Entity State Machine
 * IEEE Std 1722.1-2013 clause 9.2.2.3
 ******************************************************************
 */

#include "openavb_platform.h"

#include <errno.h>

#define	AVB_LOG_COMPONENT	"AECP"
#include "openavb_log.h"

#include "openavb_aem.h"
#include "openavb_aecp.h"
#include "openavb_aecp_message.h"
#include "openavb_aecp_sm_entity_model_entity.h"
#include "openavb_list.h"

#include "openavb_avdecc_pipeline_interaction_pub.h"
#include "openavb_aecp_cmd_get_counters.h"

typedef enum {
	OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_WAITING,
	OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_UNSOLICITED_RESPONSE,
	OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_RECEIVED_COMMAND,
} openavb_aecp_sm_entity_model_entity_state_t;

extern openavb_aecp_sm_global_vars_t openavbAecpSMGlobalVars;
openavb_aecp_sm_entity_model_entity_vars_t openavbAecpSMEntityModelEntityVars;

extern MUTEX_HANDLE(openavbAemMutex);
#define AEM_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAemMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define AEM_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAemMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

MUTEX_HANDLE(openavbAecpQueueMutex);
#define AECP_QUEUE_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAecpQueueMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define AECP_QUEUE_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAecpQueueMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

MUTEX_HANDLE(openavbAecpSMMutex);
#define AECP_SM_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAecpSMMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define AECP_SM_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAecpSMMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

SEM_T(openavbAecpSMEntityModelEntityWaitingSemaphore);
THREAD_TYPE(openavbAecpSMEntityModelEntityThread);
THREAD_DEFINITON(openavbAecpSMEntityModelEntityThread);


static openavb_list_t s_commandQueue = NULL;

// Returns 1 if the queue was not empty before adding the new command,
//  0 if the queue was empty before adding the new command,
//  or -1 if an error occurred.
static int addCommandToQueue(openavb_aecp_AEMCommandResponse_t *command)
{
	int returnVal;

	if (!s_commandQueue) { return -1; }
	if (!command) { return -1; }

	AECP_QUEUE_LOCK();
	// Determine if the queue has something in it.
	returnVal = (openavbListFirst(s_commandQueue) != NULL ? 1 : 0);

	// Add the command to the end of the linked list.
	if (openavbListAdd(s_commandQueue, command) == NULL) {
		returnVal = -1;
	}
	AECP_QUEUE_UNLOCK();
	return (returnVal);
}

static openavb_aecp_AEMCommandResponse_t * getNextCommandFromQueue(void)
{
	openavb_aecp_AEMCommandResponse_t *item = NULL;
	openavb_list_node_t node;

	if (!s_commandQueue) { return NULL; }

	AECP_QUEUE_LOCK();
	node = openavbListFirst(s_commandQueue);
	if (node) {
		item = openavbListData(node);
		openavbListDelete(s_commandQueue, node);
	}
	AECP_QUEUE_UNLOCK();
	return item;
}


void acquireEntity()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);

	openavb_aecp_AEMCommandResponse_t *pCommand = openavbAecpSMEntityModelEntityVars.rcvdCommand;
	if (!pCommand) {
		AVB_LOG_ERROR("acquireEntity called without command");
		AVB_TRACE_EXIT(AVB_TRACE_AECP);
		return;
	}

	pCommand->headers.message_type = OPENAVB_AECP_MESSAGE_TYPE_AEM_RESPONSE;

	if (pCommand->entityModelPdu.command_data.acquireEntityCmd.descriptor_type != OPENAVB_AEM_DESCRIPTOR_ENTITY) {
		pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_SUPPORTED;
	}
	else {
		openavb_avdecc_entity_model_t *pAem = openavbAemGetModel();
		if (!pAem) {
			AVB_TRACE_EXIT(AVB_TRACE_AECP);
			return;
		}

		AEM_LOCK();
		if (pCommand->entityModelPdu.command_data.acquireEntityCmd.flags & OPENAVB_AEM_ACQUIRE_ENTITY_COMMAND_FLAG_RELEASE) {
			// AVDECC_TODO - need to add mutex for the entity model
			AVB_LOG_DEBUG("Entity Released");
			pAem->entityAcquired = FALSE;
		}
		else {
			if (pAem->entityAcquired) {
				if (memcmp(pAem->acquiredControllerId, pCommand->commonPdu.controller_entity_id, sizeof(pAem->acquiredControllerId))) {
					// AVDECC_TODO - Per 7.4.1 ACQUIRE_ENTITY Command. Need to add the CONTROLLER_AVAILABLE command and response to ensure the controller is still alive.
					//  For now just respond acquired by a different controller.
					pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_ENTITY_ACQUIRED;
					memcpy(pCommand->entityModelPdu.command_data.acquireEntityRsp.owner_id, pAem->acquiredControllerId, sizeof(pCommand->commonPdu.controller_entity_id));
				}
				else {
					// Entity was already acquired by this controller, so indicate a successful acquisition.
					pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
				}
			}
			else {
				pAem->entityAcquired = TRUE;
				memcpy(pAem->acquiredControllerId, pCommand->commonPdu.controller_entity_id, sizeof(pAem->acquiredControllerId));
				memcpy(pCommand->entityModelPdu.command_data.acquireEntityRsp.owner_id, pCommand->commonPdu.controller_entity_id, sizeof(pCommand->commonPdu.controller_entity_id));
				AVB_LOGF_DEBUG("Entity Acquired by " ENTITYID_FORMAT, pCommand->entityModelPdu.command_data.acquireEntityRsp.owner_id);
			}
		}
		AEM_UNLOCK();
	}

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

void lockEntity()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);

	openavb_aecp_AEMCommandResponse_t *pCommand = openavbAecpSMEntityModelEntityVars.rcvdCommand;
	if (!pCommand) {
		AVB_LOG_ERROR("lockEntity called without command");
		AVB_TRACE_EXIT(AVB_TRACE_AECP);
		return;
	}

	pCommand->headers.message_type = OPENAVB_AECP_MESSAGE_TYPE_AEM_RESPONSE;
	pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_IMPLEMENTED;

	// AVDECC_TODO - needs implementation

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

// Returns TRUE is current rcvdCommand controlID matches the AEM acquire or lock controller id
bool processCommandCheckRestriction_CorrectController()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	bool bResult = TRUE;

	openavb_aecp_AEMCommandResponse_t *pCommand = openavbAecpSMEntityModelEntityVars.rcvdCommand;
	if (!pCommand) {
		AVB_LOG_ERROR("processCommandCheckRestriction_CorrectController called without command");
		AVB_TRACE_EXIT(AVB_TRACE_AECP);
		return bResult;
	}

	openavb_avdecc_entity_model_t *pAem = openavbAemGetModel();
	if (pAem) {
		AEM_LOCK();
		if (pAem->entityAcquired) {
			if (memcmp(pAem->acquiredControllerId, pCommand->commonPdu.controller_entity_id, sizeof(pAem->acquiredControllerId)) == 0) {
				bResult = TRUE;
			}
			else {
				AVB_LOGF_DEBUG("Access denied to " ENTITYID_FORMAT " as " ENTITYID_FORMAT " already acquired it", pCommand->commonPdu.controller_entity_id, pAem->acquiredControllerId);
				bResult = FALSE;
			}
		}
		else if (pAem->entityLocked) {
			if (memcmp(pAem->lockedControllerId, pCommand->commonPdu.controller_entity_id, sizeof(pAem->lockedControllerId)) == 0) {
				bResult = TRUE;
			}
			else {
				AVB_LOGF_DEBUG("Access denied to " ENTITYID_FORMAT " as " ENTITYID_FORMAT " already locked it", pCommand->commonPdu.controller_entity_id, pAem->lockedControllerId);
				bResult = FALSE;
			}
		}
		AEM_UNLOCK();
	}

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
	return bResult;
}

// Returns TRUE the stream_input or stream_output descriptor is currently not running.
// NOTE:  This function is using the last reported state from the client, not the state AVDECC last told the client to be in.
bool processCommandCheckRestriction_StreamNotRunning(U16 descriptor_type, U16 descriptor_index)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	bool bResult = TRUE;

	if (descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT ||
			descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT) {
		U16 configIdx = openavbAemGetConfigIdx();
		openavb_aem_descriptor_stream_io_t *pDescriptorStreamIO = openavbAemGetDescriptor(configIdx, descriptor_type, descriptor_index);
		if (pDescriptorStreamIO) {
			openavbAvdeccMsgStateType_t state = openavbAVDECCGetStreamingState(pDescriptorStreamIO, configIdx);
			if (state >= OPENAVB_AVDECC_MSG_RUNNING) {
				bResult = FALSE;
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
	return bResult;
}

// Process an incoming command and make it the response data on return.
void processCommand()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);

	openavb_aecp_AEMCommandResponse_t *pCommand = openavbAecpSMEntityModelEntityVars.rcvdCommand;
	if (!pCommand) {
		AVB_LOG_ERROR("processCommand called without command");
		AVB_TRACE_EXIT(AVB_TRACE_AECP);
		return;
	}

	// Set message type as response
	pCommand->headers.message_type = OPENAVB_AECP_MESSAGE_TYPE_AEM_RESPONSE;

	// Set to Not Implemented. Will be overridden by commands that are implemented.
	pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_IMPLEMENTED;

	switch (pCommand->entityModelPdu.command_type) {
		case OPENAVB_AEM_COMMAND_CODE_ACQUIRE_ENTITY:
			// Implemented in the acquireEntity() function. Should never get here.
			break;
		case OPENAVB_AEM_COMMAND_CODE_LOCK_ENTITY:
			// Implemented in the lockEntity() function. Should never get here.
			break;
		case OPENAVB_AEM_COMMAND_CODE_ENTITY_AVAILABLE:
			pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
			break;
		case OPENAVB_AEM_COMMAND_CODE_CONTROLLER_AVAILABLE:
			break;
		case OPENAVB_AEM_COMMAND_CODE_READ_DESCRIPTOR:
			{
				openavb_aecp_command_data_read_descriptor_t *pCmd = &pCommand->entityModelPdu.command_data.readDescriptorCmd;
				openavb_aecp_response_data_read_descriptor_t *pRsp = &pCommand->entityModelPdu.command_data.readDescriptorRsp;
				U16 configuration_index = pCmd->configuration_index;
				U16 descriptor_type = pCmd->descriptor_type;
				U16 descriptor_index = pCmd->descriptor_index;

				openavbRC rc = openavbAemSerializeDescriptor(configuration_index, descriptor_type, descriptor_index,
					sizeof(pRsp->descriptor_data), pRsp->descriptor_data, &pRsp->descriptor_length);
				if (IS_OPENAVB_FAILURE(rc)) {
					U8 *pDst = pRsp->descriptor_data;

					// Send a basic response back to the controller.
					pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;
					pRsp->configuration_index = configuration_index;
					pRsp->reserved = 0;
					OCT_D2BHTONS(pDst, descriptor_type);
					OCT_D2BHTONS(pDst, descriptor_index);
					pRsp->descriptor_length = pDst - pRsp->descriptor_data;
				}
				else {
					pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
				}
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_WRITE_DESCRIPTOR:
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_CONFIGURATION:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_CONFIGURATION:
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_STREAM_FORMAT:
			{
				openavb_aecp_commandresponse_data_set_stream_format_t *pCmd = &pCommand->entityModelPdu.command_data.setStreamFormatCmd;
				openavb_aecp_commandresponse_data_set_stream_format_t *pRsp = &pCommand->entityModelPdu.command_data.setStreamFormatRsp;

				if (processCommandCheckRestriction_CorrectController()) {
					if (processCommandCheckRestriction_StreamNotRunning(pCmd->descriptor_type, pCmd->descriptor_index)) {
						if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT) {
							openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
							if (pDescriptorStreamInput) {
								if (memcmp(&pDescriptorStreamInput->current_format, pCmd->stream_format, sizeof(pDescriptorStreamInput->current_format)) == 0) {
									// No change needed.
									pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
								}
								else {
									// AVDECC_TODO:  Verify that the stream format is supported, and notify the Listener of the change.
									//memcpy(&pDescriptorStreamInput->current_format, pCmd->stream_format, sizeof(pDescriptorStreamInput->current_format));
									pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_SUPPORTED;
								}
							}
							else {
								pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;
							}
						}
						else if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT) {
							openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
							if (pDescriptorStreamOutput) {
								if (memcmp(&pDescriptorStreamOutput->current_format, pCmd->stream_format, sizeof(pDescriptorStreamOutput->current_format)) == 0) {
									// No change needed.
									pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
								}
								else {
									// AVDECC_TODO:  Verify that the stream format is supported, and notify the Talker of the change.
									//memcpy(&pDescriptorStreamOutput->current_format, pCmd->stream_format, sizeof(pDescriptorStreamOutput->current_format));
									pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_SUPPORTED;
								}
							}
							else {
								pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;
							}
						}
					}
					else {
						pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_STREAM_IS_RUNNING;
					}
				}
				else {
					pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_ENTITY_ACQUIRED;
				}

				if (pRsp->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT) {
					openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
					if (pDescriptorStreamInput) {
						memcpy(pRsp->stream_format, &pDescriptorStreamInput->current_format, sizeof(pRsp->stream_format));
					}
				}
				else if (pRsp->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT) {
					openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
					if (pDescriptorStreamOutput) {
						memcpy(pRsp->stream_format, &pDescriptorStreamOutput->current_format, sizeof(pRsp->stream_format));
					}
				}
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_STREAM_FORMAT:
			{
				openavb_aecp_command_data_get_stream_format_t *pCmd = &pCommand->entityModelPdu.command_data.getStreamFormatCmd;
				openavb_aecp_response_data_get_stream_format_t *pRsp = &pCommand->entityModelPdu.command_data.getStreamFormatRsp;

				pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;

				if (pRsp->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT) {
					openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
					if (pDescriptorStreamInput) {
						openavbAemStreamFormatToBuf(&pDescriptorStreamInput->current_format, pRsp->stream_format);
						pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
					}
				}
				else if (pRsp->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT) {
					openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
					if (pDescriptorStreamOutput) {
						memcpy(pRsp->stream_format, &pDescriptorStreamOutput->current_format, sizeof(pRsp->stream_format));
						pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
					}
				}
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_VIDEO_FORMAT:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_VIDEO_FORMAT:
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_SENSOR_FORMAT:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_SENSOR_FORMAT:
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_STREAM_INFO:
			{
				openavb_aecp_commandresponse_data_set_stream_info_t *pCmd = &pCommand->entityModelPdu.command_data.setStreamInfoCmd;
				openavb_aecp_commandresponse_data_set_stream_info_t *pRsp = &pCommand->entityModelPdu.command_data.setStreamInfoRsp;

				if (processCommandCheckRestriction_CorrectController()) {
					if (processCommandCheckRestriction_StreamNotRunning(pCmd->descriptor_type, pCmd->descriptor_index)) {
						if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT ||
								pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT) {
							openavb_aem_descriptor_stream_io_t *pDescriptorStreamIO = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
							if (pDescriptorStreamIO) {
								pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;

								if ((pCmd->flags & OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_FORMAT_VALID) != 0) {
									if (memcmp(&pDescriptorStreamIO->current_format, pCmd->stream_format, sizeof(pDescriptorStreamIO->current_format)) != 0) {
										// AVDECC_TODO:  Verify that the stream format is supported, and notify the Listener of the change.
										//memcpy(&pDescriptorStreamInput->current_format, pCmd->stream_format, sizeof(pDescriptorStreamInput->current_format));
										pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_SUPPORTED;
									}
								}

								// AVDECC_TODO:  Add support for ENCRYPTED_PDU.
								if ((pCmd->flags & OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_ENCRYPTED_PDU) != 0) {
									pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_SUPPORTED;
								}

								// AVDECC_TODO:  Add support for accumulated latency.
								// For now, just clear the flags and values.
								pRsp->flags &= ~OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_MSRP_ACC_LAT_VALID;
								pRsp->msrp_accumulated_latency = 0;
								pRsp->flags &= ~OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_MSRP_FAILURE_VALID;
								pRsp->msrp_failure_code = 0;
								memset(pRsp->msrp_failure_bridge_id, 0, sizeof(pRsp->msrp_failure_bridge_id));

								if (pCommand->headers.status != OPENAVB_AEM_COMMAND_STATUS_SUCCESS) {
									// As there are already issues, don't bother trying anything following this.
								}
								else if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT) {
									// If the controller is trying to set the streaming information for the Listener, this is a problem.
									// The Listener gets this information from the Talker when the connection starts, so setting it here makes no sense.
									// We also ignore the CLASS_A flag (or absence of it), as the Talker will indicate that value as well.
									if ((pCmd->flags &
											(OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_ID_VALID |
											 OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_DEST_MAC_VALID |
											 OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_VLAN_ID_VALID)) != 0) {
										AVB_LOG_ERROR("SET_STREAM_INFO cannot set stream parameters for Listener");
										pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_BAD_ARGUMENTS;
									}
								}
								else {
									// Get the streaming values to send to the Talker.
									// Invalid values used to indicate those that should not be changed.
									U8 sr_class = ((pCmd->flags & OPENAVB_ACMP_FLAG_CLASS_B) != 0 ? SR_CLASS_B : SR_CLASS_A);
									U8 stream_id_valid = FALSE;
									U8 stream_src_mac[6] = {0, 0, 0, 0, 0, 0};
									U16 stream_uid = 0;
									U8 stream_dest_valid = FALSE;
									U8 stream_dest_mac[6] = {0, 0, 0, 0, 0, 0};
									U8 stream_vlan_id_valid = FALSE;
									U16 stream_vlan_id = 0;
									if ((pCmd->flags & OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_ID_VALID) != 0) {
										stream_id_valid = TRUE;
										memcpy(stream_src_mac, pCmd->stream_format, 6);
										stream_uid = ntohs(*(U16*) (pCmd->stream_format + 6));
										AVB_LOGF_INFO("AVDECC-specified Stream ID " ETH_FORMAT "/%u for Talker", ETH_OCTETS(stream_src_mac), stream_uid);
									}
									if ((pCmd->flags & OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_DEST_MAC_VALID) != 0) {
										stream_dest_valid = TRUE;
										memcpy(stream_dest_mac, pCmd->stream_dest_mac, 6);
										AVB_LOGF_INFO("AVDECC-specified Stream Dest Addr " ETH_FORMAT " for Talker", ETH_OCTETS(stream_dest_mac));
									}
									if ((pCmd->flags & OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_VLAN_ID_VALID) != 0) {
										stream_vlan_id_valid = TRUE;
										stream_vlan_id = pCmd->stream_vlan_id;
										AVB_LOGF_INFO("AVDECC-specified Stream VLAN ID %u for Talker", stream_vlan_id);
									}

									// Pass the values to the Talker.
									if (!openavbAVDECCSetTalkerStreamInfo(
											pDescriptorStreamIO, sr_class,
											stream_id_valid, stream_src_mac, stream_uid,
											stream_dest_valid, stream_dest_mac,
											stream_vlan_id_valid, stream_vlan_id)) {
										AVB_LOG_ERROR("SET_STREAM_INFO error setting stream parameters for Talker");
										pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_SUPPORTED;
									}
								}
							}
							else {
								pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;
							}
						}
					}
					else {
						pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_STREAM_IS_RUNNING;
					}
				}
				else {
					pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_ENTITY_ACQUIRED;
				}
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_STREAM_INFO:
			{
				openavb_aecp_command_data_get_stream_info_t *pCmd = &pCommand->entityModelPdu.command_data.getStreamInfoCmd;
				openavb_aecp_response_data_get_stream_info_t *pRsp = &pCommand->entityModelPdu.command_data.getStreamInfoRsp;

				pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;

				if (pRsp->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT ||
						pRsp->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT) {
					U16 configIdx = openavbAemGetConfigIdx();
					openavb_aem_descriptor_stream_io_t *pDescriptorStreamIO = openavbAemGetDescriptor(configIdx, pCmd->descriptor_type, pCmd->descriptor_index);
					if (pDescriptorStreamIO && pDescriptorStreamIO->stream) {
						openavbAvdeccMsgStateType_t clientState = openavbAVDECCGetStreamingState(pDescriptorStreamIO, configIdx);

						// Get the flags for the current status.
						pRsp->flags = 0;
						if (pDescriptorStreamIO->fast_connect_status >= OPENAVB_FAST_CONNECT_STATUS_IN_PROGRESS) {
							pRsp->flags |= OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_FAST_CONNECT;
						}
						if (openavbAvdeccGetSaveStateInfo(pDescriptorStreamIO->stream, NULL, NULL, NULL, NULL)) {
							pRsp->flags |= OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_SAVED_STATE;
						}
						if (clientState >= OPENAVB_AVDECC_MSG_RUNNING) {
							pRsp->flags |= (pDescriptorStreamIO->acmp_flags &
									(OPENAVB_ACMP_FLAG_CLASS_B |
									 OPENAVB_ACMP_FLAG_FAST_CONNECT |
									 OPENAVB_ACMP_FLAG_SUPPORTS_ENCRYPTED |
									 OPENAVB_ACMP_FLAG_ENCRYPTED_PDU));
							pRsp->flags |= OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_CONNECTED;
							if (clientState == OPENAVB_AVDECC_MSG_PAUSED) {
								pRsp->flags |= OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAMING_WAIT;
							}

							// For the Listener, use the streaming values we received from the current Talker.
							if (pRsp->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT) {
								// Get the Stream ID.
								memcpy(pRsp->stream_id, pDescriptorStreamIO->acmp_stream_id, 8);
								if (memcmp(pRsp->stream_id, "\x00\x00\x00\x00\x00\x00\x00\x00", 8) != 0) {
									pRsp->flags |= OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_ID_VALID;
								}

								// Get the Destination MAC Address.
								memcpy(pRsp->stream_dest_mac, pDescriptorStreamIO->acmp_dest_addr, 6);
								if (memcmp(pRsp->stream_id, "\x00\x00\x00\x00\x00\x00", 6) != 0) {
									pRsp->flags |= OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_DEST_MAC_VALID;
								}

								// Get the Stream VLAN ID if the other stream identifiers are valid.
								if ((pRsp->flags & (OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_ID_VALID | OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_DEST_MAC_VALID)) ==
										(OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_ID_VALID | OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_DEST_MAC_VALID)) {
									pRsp->stream_vlan_id = pDescriptorStreamIO->acmp_stream_vlan_id;
									pRsp->flags |= OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_VLAN_ID_VALID;
								}
							}
						}

						// For the Talker, use the values we are or will use for a connection.
						if (pRsp->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT) {
							// Get the Stream ID.
							if (pDescriptorStreamIO->stream->stream_addr.mac != NULL) {
								memcpy(pRsp->stream_id, pDescriptorStreamIO->stream->stream_addr.buffer.ether_addr_octet, 6);
								*(U16*)(pRsp->stream_id) = htons(pDescriptorStreamIO->stream->stream_uid);
								if (memcmp(pRsp->stream_id, "\x00\x00\x00\x00\x00\x00\x00\x00", 8) != 0) {
									pRsp->flags |= OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_ID_VALID;
								}
							}

							// Get the Destination MAC Address.
							if (pDescriptorStreamIO->stream->dest_addr.mac != NULL) {
								memcpy(pRsp->stream_dest_mac, pDescriptorStreamIO->stream->dest_addr.buffer.ether_addr_octet, 6);
								if (memcmp(pRsp->stream_id, "\x00\x00\x00\x00\x00\x00", 6) != 0) {
									pRsp->flags |= OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_DEST_MAC_VALID;
								}
							}

							// Get the Stream VLAN ID.
							if (pDescriptorStreamIO->stream->vlan_id != 0) {
								pRsp->stream_vlan_id = pDescriptorStreamIO->stream->vlan_id;
								pRsp->flags |= OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_VLAN_ID_VALID;
							}
						}

						// AVDECC_TODO:  Add TALKER_FAILED flag support.

						// Get the stream format.
						openavbAemStreamFormatToBuf(&pDescriptorStreamIO->current_format, pRsp->stream_format);
						pRsp->flags |= OPENAVB_AEM_SET_STREAM_INFO_COMMAND_FLAG_STREAM_FORMAT_VALID;

						// AVDECC_TODO:  Add support for msrp_accumulated_latency
						// AVDECC_TODO:  Add support for msrp_failure_bridge_id, and msrp_failure_code

						pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
					}
				}
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_NAME:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_NAME:
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_ASSOCIATION_ID:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_ASSOCIATION_ID:
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_SAMPLING_RATE:
			{
				openavb_aecp_commandresponse_data_set_sampling_rate_t *pCmd = &pCommand->entityModelPdu.command_data.setSamplingRateCmd;
				openavb_aecp_commandresponse_data_set_sampling_rate_t *pRsp = &pCommand->entityModelPdu.command_data.setSamplingRateRsp;

				if (processCommandCheckRestriction_CorrectController()) {
					if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_AUDIO_UNIT) {
						openavb_aem_descriptor_audio_unit_t *pDescriptorAudioUnit = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
						if (pDescriptorAudioUnit) {
							if (memcmp(&pDescriptorAudioUnit->current_sampling_rate, pCmd->sampling_rate, sizeof(pDescriptorAudioUnit->current_sampling_rate)) == 0) {
								// No change needed.
								pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
							}
							else {
								// AVDECC_TODO:  Verify that the sample rate is supported, and notify the Talker/Listener of the change.
								//memcpy(&pDescriptorAudioUnit->current_sampling_rate, pCmd->sampling_rate, sizeof(pDescriptorAudioUnit->current_sampling_rate));
								pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_SUPPORTED;
							}
						}
						else {
							pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;
						}
					}
					else {
						pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_IMPLEMENTED;
					}
				}
				else {
					pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_ENTITY_ACQUIRED;
				}

				if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_AUDIO_UNIT) {
					openavb_aem_descriptor_audio_unit_t *pDescriptorAudioUnit = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
					if (pDescriptorAudioUnit) {
						memcpy(pRsp->sampling_rate, &pDescriptorAudioUnit->current_sampling_rate, sizeof(pRsp->sampling_rate));
					}
				}
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_SAMPLING_RATE:
			{
				openavb_aecp_command_data_get_sampling_rate_t *pCmd = &pCommand->entityModelPdu.command_data.getSamplingRateCmd;
				openavb_aecp_response_data_get_sampling_rate_t *pRsp = &pCommand->entityModelPdu.command_data.getSamplingRateRsp;

				pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;

				if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_AUDIO_UNIT) {
					openavb_aem_descriptor_audio_unit_t *pDescriptorAudioUnit = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
					if (pDescriptorAudioUnit) {
						U8 *pSrDst = (U8*)(pRsp->sampling_rate);
						memset(pRsp->sampling_rate, 0, sizeof(pRsp->sampling_rate));
						BIT_D2BHTONL(pSrDst, pDescriptorAudioUnit->current_sampling_rate.pull, 29, 0);
						BIT_D2BHTONL(pSrDst, pDescriptorAudioUnit->current_sampling_rate.base, 0, 0);
						pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
					}
				}
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_CLOCK_SOURCE:
			{
				openavb_aecp_commandresponse_data_set_clock_source_t *pCmd = &pCommand->entityModelPdu.command_data.setClockSourceCmd;
				openavb_aecp_commandresponse_data_set_clock_source_t *pRsp = &pCommand->entityModelPdu.command_data.setClockSourceRsp;

				if (processCommandCheckRestriction_CorrectController()) {
					if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_CLOCK_DOMAIN) {
						openavb_aem_descriptor_clock_domain_t *pDescriptorClockDomain = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
						if (pDescriptorClockDomain) {
							if (pDescriptorClockDomain->clock_source_index == pCmd->clock_source_index) {
								// No change needed.
								pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
							}
							else {
								// AVDECC_TODO:  Verify that the clock source is supported, and notify the Talker/Listener of the change.
								//pDescriptorClockDomain->clock_source_index = pCmd->clock_source_index;
								pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_SUPPORTED;
							}
						}
						else {
							pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;
						}
					}
					else {
						pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_IMPLEMENTED;
					}
				}
				else {
					pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_ENTITY_ACQUIRED;
				}

				if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_CLOCK_DOMAIN) {
					openavb_aem_descriptor_clock_domain_t *pDescriptorClockDomain = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
					if (pDescriptorClockDomain) {
						pRsp->clock_source_index = pDescriptorClockDomain->clock_source_index;
					}
				}
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_CLOCK_SOURCE:
			{
				openavb_aecp_command_data_get_clock_source_t *pCmd = &pCommand->entityModelPdu.command_data.getClockSourceCmd;
				openavb_aecp_response_data_get_clock_source_t *pRsp = &pCommand->entityModelPdu.command_data.getClockSourceRsp;

				pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;

				if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_CLOCK_DOMAIN) {
					openavb_aem_descriptor_clock_domain_t *pDescriptorClockDomain = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
					if (pDescriptorClockDomain) {
						pRsp->clock_source_index = pDescriptorClockDomain->clock_source_index;
						pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
					}
				}
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_CONTROL:
			{
				openavb_aecp_commandresponse_data_set_control_t *pCmd = &pCommand->entityModelPdu.command_data.setControlCmd;
				openavb_aecp_commandresponse_data_set_control_t *pRsp = &pCommand->entityModelPdu.command_data.setControlRsp;

				if (processCommandCheckRestriction_CorrectController()) {
					if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_CONTROL) {
						openavb_aem_descriptor_control_t *pDescriptorControl = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
						if (pDescriptorControl) {
							pDescriptorControl->number_of_values = pCmd->valuesCount;
							int i1;
							for (i1 = 0; i1 < pCmd->valuesCount; i1++) {
								switch (pDescriptorControl->control_value_type) {
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT8:
										pDescriptorControl->value_details.linear_int8[i1].current = pCmd->values.linear_int8[i1];
										break;
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT8:
										pDescriptorControl->value_details.linear_uint8[i1].current = pCmd->values.linear_uint8[i1];
										break;
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT16:
										pDescriptorControl->value_details.linear_int16[i1].current = pCmd->values.linear_int16[i1];
										break;
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT16:
										pDescriptorControl->value_details.linear_uint16[i1].current = pCmd->values.linear_uint16[i1];
										break;
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT32:
										pDescriptorControl->value_details.linear_int32[i1].current = pCmd->values.linear_int32[i1];
										break;
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT32:
										pDescriptorControl->value_details.linear_uint32[i1].current = pCmd->values.linear_uint32[i1];
										break;
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT64:
										pDescriptorControl->value_details.linear_int64[i1].current = pCmd->values.linear_int64[i1];
										break;
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT64:
										pDescriptorControl->value_details.linear_uint64[i1].current = pCmd->values.linear_uint64[i1];
										break;
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_FLOAT:
										pDescriptorControl->value_details.linear_float[i1].current = pCmd->values.linear_float[i1];
										break;
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_DOUBLE:
										pDescriptorControl->value_details.linear_double[i1].current = pCmd->values.linear_double[i1];
										break;
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT8:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT8:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT16:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT16:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT32:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT32:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT64:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT64:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_FLOAT:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_DOUBLE:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_STRING:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT8:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT8:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT16:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT16:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT32:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT32:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT64:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT64:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_FLOAT:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_DOUBLE:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_UTF8:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_BODE_PLOT:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SMPTE_TIME:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SAMPLE_RATE:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_GPTP_TIME:
									case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_VENDOR:
										break;
								}
							}
							pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
						}
						else {
							pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;
						}
					}
					else {
						pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_IMPLEMENTED;
					}
				}
				else {
					pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_ENTITY_ACQUIRED;
				}

				// Populate response
				if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_CONTROL) {
					openavb_aem_descriptor_control_t *pDescriptorControl = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
					if (pDescriptorControl) {
						int i1;
						for (i1 = 0; i1 < pDescriptorControl->number_of_values; i1++) {
							switch (pDescriptorControl->control_value_type) {
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT8:
									pRsp->values.linear_int8[i1] = pDescriptorControl->value_details.linear_int8[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT8:
									pRsp->values.linear_uint8[i1] = pDescriptorControl->value_details.linear_uint8[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT16:
									pRsp->values.linear_int16[i1] = pDescriptorControl->value_details.linear_int16[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT16:
									pRsp->values.linear_uint16[i1] = pDescriptorControl->value_details.linear_uint16[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT32:
									pRsp->values.linear_int32[i1] = pDescriptorControl->value_details.linear_int32[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT32:
									pRsp->values.linear_uint32[i1] = pDescriptorControl->value_details.linear_uint32[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT64:
									pRsp->values.linear_int64[i1] = pDescriptorControl->value_details.linear_int64[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT64:
									pRsp->values.linear_uint64[i1] = pDescriptorControl->value_details.linear_uint64[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_FLOAT:
									pRsp->values.linear_float[i1] = pDescriptorControl->value_details.linear_float[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_DOUBLE:
									pRsp->values.linear_double[i1] = pDescriptorControl->value_details.linear_double[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT8:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT8:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT16:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT16:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT32:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT32:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT64:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT64:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_FLOAT:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_DOUBLE:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_STRING:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT8:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT8:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT16:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT16:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT32:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT32:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT64:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT64:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_FLOAT:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_DOUBLE:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_UTF8:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_BODE_PLOT:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SMPTE_TIME:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SAMPLE_RATE:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_GPTP_TIME:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_VENDOR:
									break;
							}
						}
					}
				}
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_CONTROL:
			{
				openavb_aecp_commandresponse_data_set_control_t *pCmd = &pCommand->entityModelPdu.command_data.setControlCmd;
				openavb_aecp_commandresponse_data_set_control_t *pRsp = &pCommand->entityModelPdu.command_data.setControlRsp;

				pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;

				if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_CONTROL) {
					openavb_aem_descriptor_control_t *pDescriptorControl = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
					if (pDescriptorControl) {
						int i1;
						for (i1 = 0; i1 < pDescriptorControl->number_of_values; i1++) {
							switch (pDescriptorControl->control_value_type) {
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT8:
									pRsp->values.linear_int8[i1] = pDescriptorControl->value_details.linear_int8[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT8:
									pRsp->values.linear_uint8[i1] = pDescriptorControl->value_details.linear_uint8[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT16:
									pRsp->values.linear_int16[i1] = pDescriptorControl->value_details.linear_int16[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT16:
									pRsp->values.linear_uint16[i1] = pDescriptorControl->value_details.linear_uint16[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT32:
									pRsp->values.linear_int32[i1] = pDescriptorControl->value_details.linear_int32[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT32:
									pRsp->values.linear_uint32[i1] = pDescriptorControl->value_details.linear_uint32[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT64:
									pRsp->values.linear_int64[i1] = pDescriptorControl->value_details.linear_int64[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT64:
									pRsp->values.linear_uint64[i1] = pDescriptorControl->value_details.linear_uint64[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_FLOAT:
									pRsp->values.linear_float[i1] = pDescriptorControl->value_details.linear_float[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_DOUBLE:
									pRsp->values.linear_double[i1] = pDescriptorControl->value_details.linear_double[i1].current;
									break;
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT8:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT8:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT16:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT16:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT32:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT32:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT64:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT64:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_FLOAT:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_DOUBLE:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_STRING:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT8:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT8:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT16:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT16:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT32:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT32:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT64:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT64:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_FLOAT:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_DOUBLE:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_UTF8:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_BODE_PLOT:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SMPTE_TIME:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SAMPLE_RATE:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_GPTP_TIME:
								case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_VENDOR:
									break;
							}
						}
					}
				}
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_INCREMENT_CONTROL:
			break;
		case OPENAVB_AEM_COMMAND_CODE_DECREMENT_CONTROL:
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_SIGNAL_SELECTOR:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_SIGNAL_SELECTOR:
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_MIXER:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_MIXER:
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_MATRIX:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_MATRIX:
			break;
		case OPENAVB_AEM_COMMAND_CODE_START_STREAMING:
			{
				openavb_aecp_commandresponse_data_start_streaming_t *pCmd = &pCommand->entityModelPdu.command_data.startStreamingCmd;
				//openavb_aecp_commandresponse_data_start_streaming_t *pRsp = &pCommand->entityModelPdu.command_data.startStreamingRsp;

				if (processCommandCheckRestriction_CorrectController()) {
					if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT) {
						openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
						if (pDescriptorStreamInput) {
							openavbAVDECCPauseStream(pDescriptorStreamInput, FALSE);
							pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
						}
						else {
							pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;
						}
					}
					else if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT) {
						openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
						if (pDescriptorStreamOutput) {
							openavbAVDECCPauseStream(pDescriptorStreamOutput, FALSE);
							pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
						}
						else {
							pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;
						}
					}
					else {
						pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_IMPLEMENTED;
					}
				}
				else {
					pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_ENTITY_ACQUIRED;
				}
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_STOP_STREAMING:
			{
				openavb_aecp_commandresponse_data_start_streaming_t *pCmd = &pCommand->entityModelPdu.command_data.startStreamingCmd;
				//openavb_aecp_commandresponse_data_start_streaming_t *pRsp = &pCommand->entityModelPdu.command_data.startStreamingRsp;

				if (processCommandCheckRestriction_CorrectController()) {
					if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT) {
						openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
						if (pDescriptorStreamInput) {
							openavbAVDECCPauseStream(pDescriptorStreamInput, TRUE);
							pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
						}
						else {
							pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;
						}
					}
					else if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT) {
						openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
						if (pDescriptorStreamOutput) {
							openavbAVDECCPauseStream(pDescriptorStreamOutput, TRUE);
							pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
						}
						else {
							pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;
						}
					}
					else {
						pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_IMPLEMENTED;
					}
				}
				else {
					pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_ENTITY_ACQUIRED;
				}
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_REGISTER_UNSOLICITED_NOTIFICATION:
			break;
		case OPENAVB_AEM_COMMAND_CODE_DEREGISTER_UNSOLICITED_NOTIFICATION:
			break;
		case OPENAVB_AEM_COMMAND_CODE_IDENTIFY_NOTIFICATION:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_AVB_INFO:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_AS_PATH:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_COUNTERS:
			{
				openavb_aecp_command_data_get_counters_t *pCmd = &pCommand->entityModelPdu.command_data.getCountersCmd;
				openavb_aecp_response_data_get_counters_t *pRsp = &pCommand->entityModelPdu.command_data.getCountersRsp;
				pCommand->headers.status = openavbAecpCommandGetCountersHandler(pCmd, pRsp);
			}
			break;
		case OPENAVB_AEM_COMMAND_CODE_REBOOT:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_AUDIO_MAP:
			break;
		case OPENAVB_AEM_COMMAND_CODE_ADD_AUDIO_MAPPINGS:
			break;
		case OPENAVB_AEM_COMMAND_CODE_REMOVE_AUDIO_MAPPINGS:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_VIDEO_MAP:
			break;
		case OPENAVB_AEM_COMMAND_CODE_ADD_VIDEO_MAPPINGS:
			break;
		case OPENAVB_AEM_COMMAND_CODE_REMOVE_VIDEO_MAPPINGS:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_SENSOR_MAP:
			break;
		case OPENAVB_AEM_COMMAND_CODE_ADD_SENSOR_MAPPINGS:
			break;
		case OPENAVB_AEM_COMMAND_CODE_REMOVE_SENSOR_MAPPINGS:
			break;
		case OPENAVB_AEM_COMMAND_CODE_START_OPERATION:
			break;
		case OPENAVB_AEM_COMMAND_CODE_ABORT_OPERATION:
			break;
		case OPENAVB_AEM_COMMAND_CODE_OPERATION_STATUS:
			break;
		case OPENAVB_AEM_COMMAND_CODE_AUTH_ADD_KEY:
			break;
		case OPENAVB_AEM_COMMAND_CODE_AUTH_DELETE_KEY:
			break;
		case OPENAVB_AEM_COMMAND_CODE_AUTH_GET_KEY_LIST:
			break;
		case OPENAVB_AEM_COMMAND_CODE_AUTH_GET_KEY:
			break;
		case OPENAVB_AEM_COMMAND_CODE_AUTH_ADD_KEY_TO_CHAIN:
			break;
		case OPENAVB_AEM_COMMAND_CODE_AUTH_DELETE_KEY_FROM_CHAIN:
			break;
		case OPENAVB_AEM_COMMAND_CODE_AUTH_GET_KEYCHAIN_LIST:
			break;
		case OPENAVB_AEM_COMMAND_CODE_AUTH_GET_IDENTITY:
			break;
		case OPENAVB_AEM_COMMAND_CODE_AUTH_ADD_TOKEN:
			break;
		case OPENAVB_AEM_COMMAND_CODE_AUTH_DELETE_TOKEN:
			break;
		case OPENAVB_AEM_COMMAND_CODE_AUTHENTICATE:
			break;
		case OPENAVB_AEM_COMMAND_CODE_DEAUTHENTICATE:
			break;
		case OPENAVB_AEM_COMMAND_CODE_ENABLE_TRANSPORT_SECURITY:
			break;
		case OPENAVB_AEM_COMMAND_CODE_DISABLE_TRANSPORT_SECURITY:
			break;
		case OPENAVB_AEM_COMMAND_CODE_ENABLE_STREAM_ENCRYPTION:
			break;
		case OPENAVB_AEM_COMMAND_CODE_DISABLE_STREAM_ENCRYPTION:
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_MEMORY_OBJECT_LENGTH:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_MEMORY_OBJECT_LENGTH:
			break;
		case OPENAVB_AEM_COMMAND_CODE_SET_STREAM_BACKUP:
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_STREAM_BACKUP:
			break;
		case OPENAVB_AEM_COMMAND_CODE_EXPANSION:
			break;
		default:
			break;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

void openavbAecpSMEntityModelEntityStateMachine()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	bool bRunning = TRUE;

	openavb_aecp_sm_entity_model_entity_state_t state = OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_WAITING;

	// Lock such that the mutex is held unless waiting for a semaphore. Synchronous processing of command responses.
	AECP_SM_LOCK();
	while (bRunning) {
		switch (state) {
			case OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_WAITING:
				AVB_LOG_DEBUG("State:  OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_WAITING");
				openavbAecpSMEntityModelEntityVars.rcvdAEMCommand = FALSE;
				openavbAecpSMEntityModelEntityVars.doUnsolicited = FALSE;

				// Wait for a change in state
				while (state == OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_WAITING && bRunning) {
					AECP_SM_UNLOCK();
					SEM_ERR_T(err);
					SEM_WAIT(openavbAecpSMEntityModelEntityWaitingSemaphore, err);
					AECP_SM_LOCK();

					if (SEM_IS_ERR_NONE(err)) {
						if (openavbAecpSMEntityModelEntityVars.doTerminate) {
							bRunning = FALSE;
						}
						else if (openavbAecpSMEntityModelEntityVars.doUnsolicited) {
							state = OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_UNSOLICITED_RESPONSE;
						}
						else if (openavbAecpSMEntityModelEntityVars.rcvdAEMCommand) {
							state = OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_RECEIVED_COMMAND;
						}
					}
				}
				break;

			case OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_UNSOLICITED_RESPONSE:
				AVB_LOG_DEBUG("State:  OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_UNSOLICITED_RESPONSE");

				state = OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_WAITING;
				break;

			case OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_RECEIVED_COMMAND:
				AVB_LOG_DEBUG("State:  OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_RECEIVED_COMMAND");

				while (TRUE) {
					openavbAecpSMEntityModelEntityVars.rcvdCommand = getNextCommandFromQueue();
					if (openavbAecpSMEntityModelEntityVars.rcvdCommand == NULL) {
						break;
					}

					if (memcmp(openavbAecpSMEntityModelEntityVars.rcvdCommand->headers.target_entity_id,
							openavbAecpSMGlobalVars.myEntityID,
							sizeof(openavbAecpSMGlobalVars.myEntityID)) != 0) {
						// Not intended for us.
						free(openavbAecpSMEntityModelEntityVars.rcvdCommand);
						continue;
					}

					if (openavbAecpSMEntityModelEntityVars.rcvdCommand->entityModelPdu.command_type == OPENAVB_AEM_COMMAND_CODE_ACQUIRE_ENTITY) {
						acquireEntity();
					}
					else if (openavbAecpSMEntityModelEntityVars.rcvdCommand->entityModelPdu.command_type == OPENAVB_AEM_COMMAND_CODE_LOCK_ENTITY) {
						lockEntity();
					}
					else if (openavbAecpSMEntityModelEntityVars.rcvdCommand->entityModelPdu.command_type == OPENAVB_AEM_COMMAND_CODE_ENTITY_AVAILABLE) {
						// State machine defines just returning the request command. Doing that in the processCommand function for consistency.
						processCommand();
					}
					else {
						processCommand();
					}

					openavbAecpMessageSend(openavbAecpSMEntityModelEntityVars.rcvdCommand);
					free(openavbAecpSMEntityModelEntityVars.rcvdCommand);
				}

				state = OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_WAITING;
				break;

			default:
				AVB_LOG_ERROR("State:  Unknown");
				bRunning = FALSE;	// Unexpected
				break;

		}
	}
	AECP_SM_UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

void* openavbAecpSMEntityModelEntityThreadFn(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	openavbAecpSMEntityModelEntityStateMachine();
	AVB_TRACE_EXIT(AVB_TRACE_AECP);
	return NULL;
}

void openavbAecpSMEntityModelEntityStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);

	MUTEX_ATTR_HANDLE(mtq);
	MUTEX_ATTR_INIT(mtq);
	MUTEX_ATTR_SET_TYPE(mtq, MUTEX_ATTR_TYPE_DEFAULT);
	MUTEX_ATTR_SET_NAME(mtq, "openavbAecpQueueMutex");
	MUTEX_CREATE_ERR();
	MUTEX_CREATE(openavbAecpQueueMutex, mtq);
	MUTEX_LOG_ERR("Could not create/initialize 'openavbAecpQueueMutex' mutex");

	MUTEX_ATTR_HANDLE(mta);
	MUTEX_ATTR_INIT(mta);
	MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
	MUTEX_ATTR_SET_NAME(mta, "openavbAecpSMMutex");
	MUTEX_CREATE(openavbAecpSMMutex, mta);
	MUTEX_LOG_ERR("Could not create/initialize 'openavbAecpSMMutex' mutex");

	SEM_ERR_T(err);
	SEM_INIT(openavbAecpSMEntityModelEntityWaitingSemaphore, 1, err);
	SEM_LOG_ERR(err);

	// Initialize the linked list (queue).
	s_commandQueue = openavbListNewList();

	// Start the Advertise Entity State Machine
	bool errResult;
	THREAD_CREATE(openavbAecpSMEntityModelEntityThread, openavbAecpSMEntityModelEntityThread, NULL, openavbAecpSMEntityModelEntityThreadFn, NULL);
	THREAD_CHECK_ERROR(openavbAecpSMEntityModelEntityThread, "Thread / task creation failed", errResult);
	if (errResult);		// Already reported

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

void openavbAecpSMEntityModelEntityStop()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);

	openavbAecpSMEntityModelEntitySet_doTerminate(TRUE);

	THREAD_JOIN(openavbAecpSMEntityModelEntityThread, NULL);

	// Delete the linked list (queue).
	openavb_aecp_AEMCommandResponse_t *item;
	while ((item = getNextCommandFromQueue()) != NULL) {
		 free(item);
	}
	openavbListDeleteList(s_commandQueue);

	SEM_ERR_T(err);
	SEM_DESTROY(openavbAecpSMEntityModelEntityWaitingSemaphore, err);
	SEM_LOG_ERR(err);

	MUTEX_CREATE_ERR();
	MUTEX_DESTROY(openavbAecpQueueMutex);
	MUTEX_LOG_ERR("Could not destroy 'openavbAecpQueueMutex' mutex");
	MUTEX_DESTROY(openavbAecpSMMutex);
	MUTEX_LOG_ERR("Could not destroy 'openavbAecpSMMutex' mutex");

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

void openavbAecpSMEntityModelEntitySet_rcvdCommand(openavb_aecp_AEMCommandResponse_t *rcvdCommand)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);

	if (memcmp(rcvdCommand->headers.target_entity_id,
			openavbAecpSMGlobalVars.myEntityID,
			sizeof(openavbAecpSMGlobalVars.myEntityID)) != 0) {
		// Not intended for us.
		free(rcvdCommand);
		return;
	}

	int result = addCommandToQueue(rcvdCommand);
	if (result < 0) {
		AVB_LOG_DEBUG("addCommandToQueue failed");
		free(rcvdCommand);
	}
	else if (result == 0) {
		// We just added an item to an empty queue.
		// Notify the machine state thread that something is waiting.
		AECP_SM_LOCK();
		openavbAecpSMEntityModelEntityVars.rcvdAEMCommand = TRUE;

		SEM_ERR_T(err);
		SEM_POST(openavbAecpSMEntityModelEntityWaitingSemaphore, err);
		SEM_LOG_ERR(err);
		AECP_SM_UNLOCK();
	}
	else {
		// We added an item to a non-empty queue.
		// Assume it will be handled when the other items in the queue are handled.
	}

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

void openavbAecpSMEntityModelEntitySet_unsolicited(openavb_aecp_AEMCommandResponse_t *unsolicited)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	AECP_SM_LOCK();
	memcpy(&openavbAecpSMEntityModelEntityVars.unsolicited, unsolicited, sizeof(*unsolicited));
	openavbAecpSMEntityModelEntityVars.doUnsolicited = TRUE;

	SEM_ERR_T(err);
	SEM_POST(openavbAecpSMEntityModelEntityWaitingSemaphore, err);
	SEM_LOG_ERR(err);

	AECP_SM_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

void openavbAecpSMEntityModelEntitySet_doTerminate(bool value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	AECP_SM_LOCK();
	openavbAecpSMEntityModelEntityVars.doTerminate = value;

	SEM_ERR_T(err);
	SEM_POST(openavbAecpSMEntityModelEntityWaitingSemaphore, err);
	SEM_LOG_ERR(err);

	AECP_SM_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}
