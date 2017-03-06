/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AVDECC Enumeration and control protocol (ACMP) : Entity Model Entity State Machine
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       13-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Implements the AVDECC Enumeration and control protocol (ACMP) : Entity Model Entity State Machine
 * IEEE Std 1722.1-2013 clause 9.2.2.3
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
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

#include "openavb_avdecc_pipeline_interaction_pub.h"
#include "openavb_aecp_cmd_get_counters.h"

typedef enum {
	OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_WAITING,
	OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_UNSOLICITED_RESPONSE,
	OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_RECEIVED_COMMAND,
} openavb_aecp_sm_entity_model_entity_state_t;

extern openavb_aecp_sm_global_vars_t openavbAecpSMGlobalVars;
openavb_aecp_sm_entity_model_entity_vars_t openavbAecpSMEntityModelEntityVars;

extern MUTEX_HANDLE(openavbAecpMutex);
#define AECP_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAecpMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define AECP_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAecpMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

extern MUTEX_HANDLE(openavbAemMutex);
#define AEM_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAemMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define AEM_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAemMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

MUTEX_HANDLE(openavbAecpSMMutex);
#define AECP_SM_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAecpSMMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define AECP_SM_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAecpSMMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

SEM_T(openavbAecpSMEntityModelEntityWaitingSemaphore);
THREAD_TYPE(openavbAecpSMEntityModelEntityThread);
THREAD_DEFINITON(openavbAecpSMEntityModelEntityThread);


void acquireEntity()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);

	openavb_aecp_AEMCommandResponse_t *pCommand = &openavbAecpSMEntityModelEntityVars.rcvdCommand;

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
					pAem->entityAcquired = TRUE;
					memcpy(pAem->acquiredControllerId, pCommand->commonPdu.controller_entity_id, sizeof(pAem->acquiredControllerId));
					memcpy(pCommand->entityModelPdu.command_data.acquireEntityRsp.owner_id, pCommand->commonPdu.controller_entity_id, sizeof(pCommand->commonPdu.controller_entity_id));
				}
			}
		}
		AEM_UNLOCK();
	}

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

void lockEntity()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);

	openavb_aecp_AEMCommandResponse_t *pCommand = &openavbAecpSMEntityModelEntityVars.rcvdCommand;

	pCommand->headers.message_type = OPENAVB_AECP_MESSAGE_TYPE_AEM_RESPONSE;
	pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_IMPLEMENTED;

	// AVDECC_TODO - needs implementation

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

// Returns TRUE is current rcvdCommand controlID matches the AEM acquire or lock controller id
bool processCommandCheckRestriction_CorrectController()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	bool bResult = FALSE;

	openavb_aecp_AEMCommandResponse_t *pCommand = &openavbAecpSMEntityModelEntityVars.rcvdCommand;

	openavb_avdecc_entity_model_t *pAem = openavbAemGetModel();
	if (pAem) {
		AEM_LOCK();
		if (pAem->entityAcquired) {
			if (memcmp(pAem->acquiredControllerId, pCommand->commonPdu.controller_entity_id, sizeof(pAem->acquiredControllerId)) == 0) {
				bResult = TRUE;
			}
		}
		else if (pAem->entityLocked) {
			if (memcmp(pAem->lockedControllerId, pCommand->commonPdu.controller_entity_id, sizeof(pAem->acquiredControllerId)) == 0) {
				bResult = TRUE;
			}
		}
		AEM_UNLOCK();
	}

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
	return bResult;
}

// Returns TRUE the stream_input or stream_output descriptor is currently not running.
bool processCommandCheckRestriction_StreamNotRunning(U16 descriptor_type, U16 descriptor_index)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	bool bResult = FALSE;

	if (descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT) {
		U16 configIdx = openavbAemGetConfigIdx();
		openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput = openavbAemGetDescriptor(configIdx, descriptor_type, descriptor_index);
		if (pDescriptorStreamInput) {
			if (!openavbAVDECCListenerIsStreaming(pDescriptorStreamInput, configIdx)) {
				bResult = TRUE;
			}
		}
	}
	else if (descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT) {
		U16 configIdx = openavbAemGetConfigIdx();
		openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput = openavbAemGetDescriptor(configIdx, descriptor_type, descriptor_index);
		if (pDescriptorStreamOutput) {
			if (!openavbAVDECCTalkerIsStreaming(pDescriptorStreamOutput, configIdx)) {
				bResult = TRUE;
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

	openavb_aecp_AEMCommandResponse_t *pCommand = &openavbAecpSMEntityModelEntityVars.rcvdCommand;

	// Set message type as response
	pCommand->headers.message_type = OPENAVB_AECP_MESSAGE_TYPE_AEM_RESPONSE;

	// Set to Not Implemented. Will be overridden by commands that are implemented.
	pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NOT_IMPLEMENTED;

	switch (openavbAecpSMEntityModelEntityVars.rcvdCommand.entityModelPdu.command_type) {
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
								memcpy(&pDescriptorStreamInput->current_format, pCmd->stream_format, sizeof(pDescriptorStreamInput->current_format));
								pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
							}
							else {
								pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_NO_SUCH_DESCRIPTOR;
							}
						}
						else if (pCmd->descriptor_type == OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT) {
							openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput = openavbAemGetDescriptor(openavbAemGetConfigIdx(), pCmd->descriptor_type, pCmd->descriptor_index);
							if (pDescriptorStreamOutput) {
								memcpy(&pDescriptorStreamOutput->current_format, pCmd->stream_format, sizeof(pDescriptorStreamOutput->current_format));
								pCommand->headers.status = OPENAVB_AEM_COMMAND_STATUS_SUCCESS;
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
						memcpy(pRsp->stream_format, &pDescriptorStreamInput->current_format, sizeof(pRsp->stream_format));
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
			break;
		case OPENAVB_AEM_COMMAND_CODE_GET_STREAM_INFO:
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
							memcpy(&pDescriptorAudioUnit->current_sampling_rate, pCmd->sampling_rate, sizeof(pDescriptorAudioUnit->current_sampling_rate));
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
						memcpy(pRsp->sampling_rate, &pDescriptorAudioUnit->current_sampling_rate, sizeof(pRsp->sampling_rate));
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
							pDescriptorClockDomain->clock_source_index = pCmd->clock_source_index;
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

	while (bRunning) {
		switch (state) {
			case OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_WAITING:
				AVB_LOG_DEBUG("State:  OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_WAITING");
				openavbAecpSMEntityModelEntityVars.rcvdAEMCommand = FALSE;
				openavbAecpSMEntityModelEntityVars.doUnsolicited = FALSE;

				// Wait for a change in state
				while (state == OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_WAITING && bRunning) {
					SEM_ERR_T(err);
					SEM_WAIT(openavbAecpSMEntityModelEntityWaitingSemaphore, err);

					if (SEM_IS_ERR_NONE(err)) {
						if (openavbAecpSMEntityModelEntityVars.doTerminate) {
							bRunning = FALSE;
						}
						else if (openavbAecpSMEntityModelEntityVars.doUnsolicited) {
							state = OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_UNSOLICITED_RESPONSE;
						}
						else if (openavbAecpSMEntityModelEntityVars.rcvdAEMCommand &&
								 !memcmp(openavbAecpSMEntityModelEntityVars.rcvdCommand.headers.target_entity_id, openavbAecpSMGlobalVars.myEntityID, sizeof(openavbAecpSMGlobalVars.myEntityID))) {
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
				{
					AECP_SM_LOCK();
					if (openavbAecpSMEntityModelEntityVars.rcvdCommand.entityModelPdu.command_type == OPENAVB_AEM_COMMAND_CODE_ACQUIRE_ENTITY) {
						acquireEntity();
					}
					else if (openavbAecpSMEntityModelEntityVars.rcvdCommand.entityModelPdu.command_type == OPENAVB_AEM_COMMAND_CODE_LOCK_ENTITY) {
						lockEntity();
					}
					else if (openavbAecpSMEntityModelEntityVars.rcvdCommand.entityModelPdu.command_type == OPENAVB_AEM_COMMAND_CODE_ENTITY_AVAILABLE) {
						// State machine defines just returning the request command. Doing that in the processCommand function for consistency.
						processCommand();
					}
					else {
						processCommand();
					}

					openavbAecpMessageSend(&openavbAecpSMEntityModelEntityVars.rcvdCommand);
					AECP_SM_UNLOCK();
				}
				state = OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_STATE_WAITING;
				break;
	
			default:
				AVB_LOG_DEBUG("State:  Unknown");
				bRunning = FALSE;	// Unexpected
				break;

		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

void* openavbAecpSMEntityModelEntityThreadFn(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	openavbAecpSMEntityModelEntityStateMachine();
	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return NULL;
}

void openavbAecpSMEntityModelEntityStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	
	MUTEX_ATTR_HANDLE(mta);
	MUTEX_ATTR_INIT(mta);
	MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
	MUTEX_ATTR_SET_NAME(mta, "openavbAecpSMMutex");
	MUTEX_CREATE_ERR();
	MUTEX_CREATE(openavbAecpSMMutex, mta);
	MUTEX_LOG_ERR("Could not create/initialize 'openavbAecpSMMutex' mutex");

	SEM_ERR_T(err);
	SEM_INIT(openavbAecpSMEntityModelEntityWaitingSemaphore, 1, err);
	SEM_LOG_ERR(err);

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

	SEM_ERR_T(err);
	SEM_DESTROY(openavbAecpSMEntityModelEntityWaitingSemaphore, err);
	SEM_LOG_ERR(err);

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

void openavbAecpSMEntityModelEntitySet_rcvdCommand(openavb_aecp_AEMCommandResponse_t *rcvdCommand)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	AECP_SM_LOCK();
	memcpy(&openavbAecpSMEntityModelEntityVars.rcvdCommand, rcvdCommand, sizeof(*rcvdCommand));
	AECP_SM_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

void openavbAecpSMEntityModelEntitySet_rcvdAEMCommand(bool value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	AECP_SM_LOCK();
	openavbAecpSMEntityModelEntityVars.rcvdAEMCommand = value;

	SEM_ERR_T(err);
	SEM_POST(openavbAecpSMEntityModelEntityWaitingSemaphore, err);
	SEM_LOG_ERR(err);

	AECP_SM_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

void openavbAecpSMEntityModelEntitySet_unsolicited(openavb_aecp_AEMCommandResponse_t *unsolicited)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	AECP_SM_LOCK();
	memcpy(&openavbAecpSMEntityModelEntityVars.unsolicited, unsolicited, sizeof(*unsolicited));
	AECP_SM_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}

void openavbAecpSMEntityModelEntitySet_doUnsolicited(bool value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);
	AECP_SM_LOCK();
	openavbAecpSMEntityModelEntityVars.doUnsolicited = value;

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





