/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : ADP - AVDECC Discovery Protocol
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       18-Nov-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Implements the 1722.1 (AVDECC) discovery protocol
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */
#include "openavb_platform.h"

#include <signal.h>

#define	AVB_LOG_COMPONENT	"ADP"
#include "openavb_log.h"

#include "openavb_aem.h"
#include "openavb_adp.h"
#include "openavb_adp_sm_advertise_entity.h"
#include "openavb_adp_sm_advertise_interface.h"
#include "openavb_adp_message.h"

#ifdef AVB_PTP_AVAILABLE
#include "openavb_ptp_api.h"

// PTP declarations
openavbRC openavbPtpInitializeSharedMemory  ();
void     openavbPtpReleaseSharedMemory     ();
openavbRC openavbPtpUpdateSharedMemoryEntry ();
openavbRC openavbPtpFindLatestSharedMemoryEntry(U32 *index);
extern gmChangeTable_t openavbPtpGMChageTable;
#endif // AVB_PTP_AVAILABLE

openavb_adp_sm_global_vars_t openavbAdpSMGlobalVars;

MUTEX_HANDLE(openavbAdpMutex);
#define ADP_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAdpMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ADP_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAdpMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

extern openavb_avdecc_cfg_t gAvdeccCfg;

static bool s_bPreviousHaveTL = false;

openavbRC openavbAdpStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	openavb_avdecc_entity_model_t *pAem = openavbAemGetModel();
	if (!pAem) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING), AVB_TRACE_ADP);
	}

	{
		MUTEX_ATTR_HANDLE(mta);
		MUTEX_ATTR_INIT(mta);
		MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
		MUTEX_ATTR_SET_NAME(mta, "openavbAdpMutex");
		MUTEX_CREATE_ERR();
		MUTEX_CREATE(openavbAdpMutex, mta);
		MUTEX_LOG_ERR("Could not create/initialize 'openavbAdpMutex' mutex");
	}

	ADP_LOCK();
	{
		// Populate global ADP vars
		openavb_adp_control_header_t *pHeader = &openavbAdpSMGlobalVars.entityInfo.header;

		pHeader->cd = 1;
		pHeader->subtype = OPENAVB_ADP_AVTP_SUBTYPE;
		pHeader->sv = 0;
		pHeader->version = 0;
		pHeader->message_type = 0;					// Set later depending on message type
		pHeader->valid_time = gAvdeccCfg.valid_time;
		pHeader->control_data_length = 56;
		memcpy(pHeader->entity_id, pAem->pDescriptorEntity->entity_id, sizeof(pAem->pDescriptorEntity->entity_id));
	}

	{
		// Populate global ADP PDU vars
		openavb_adp_data_unit_t *pPdu = &openavbAdpSMGlobalVars.entityInfo.pdu;

#ifdef AVB_PTP_AVAILABLE
		openavbRC  retCode = OPENAVB_PTP_FAILURE;
		retCode = openavbPtpInitializeSharedMemory();
		if (IS_OPENAVB_FAILURE(retCode)) {
			AVB_LOG_WARNING("Failed to init PTP shared memory");
		}
#endif // AVB_PTP_AVAILABLE

		memcpy(pPdu->entity_model_id, pAem->pDescriptorEntity->entity_model_id, sizeof(pPdu->entity_model_id));
		pPdu->entity_capabilities = pAem->pDescriptorEntity->entity_capabilities;
		pPdu->talker_stream_sources = pAem->pDescriptorEntity->talker_stream_sources;
		pPdu->talker_capabilities = pAem->pDescriptorEntity->talker_capabilities;
		pPdu->listener_stream_sinks = pAem->pDescriptorEntity->listener_stream_sinks;
		pPdu->listener_capabilities = pAem->pDescriptorEntity->listener_capabilities;
		pPdu->controller_capabilities = pAem->pDescriptorEntity->controller_capabilities;
		pPdu->available_index = pAem->pDescriptorEntity->available_index;
		// The pPdu->gptp_grandmaster_id and pPdu->gptp_domain_number will be filled in when the ADPDU is transmitted.
		// pPdu->reserved0;
		// pPdu->identify_control_index = ???;											// AVDECC_TODO	
		// pPdu->interface_index = ???;													// AVDECC_TODO	
		memcpy(pPdu->association_id, pAem->pDescriptorEntity->association_id, sizeof(pPdu->association_id));
		// pPdu->reserved1;
	}
	ADP_UNLOCK();

	// Wait until we are notified that we have a Talker or Listener before supporting discover.
	s_bPreviousHaveTL = false;

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_ADP);
}

void openavbAdpStop()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

#ifdef AVB_PTP_AVAILABLE
	openavbPtpReleaseSharedMemory();
#endif // AVB_PTP_AVAILABLE

	// Stop Advertising and supporting Discovery.
	openavbAdpHaveTL(false);

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}


openavbRC openavbAdpHaveTL(bool bHaveTL)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	if (bHaveTL && !s_bPreviousHaveTL) {
		// Start Advertising and supporting Discovery.
		openavbRC rc = openavbAdpMessageHandlerStart();
		if (IS_OPENAVB_FAILURE(rc)) {
			AVB_RC_TRACE_RET(rc, AVB_TRACE_ADP);
		}

		openavbAdpSMAdvertiseInterfaceStart();
		openavbAdpSMAdvertiseEntityStart();
		s_bPreviousHaveTL = true;
	}
	else if (!bHaveTL && s_bPreviousHaveTL) {
		// Stop Advertising and supporting Discovery.
		openavbAdpSMAdvertiseInterfaceStop();
		openavbAdpSMAdvertiseEntityStop();
		openavbAdpMessageHandlerStop();
		s_bPreviousHaveTL = false;
	}

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_ADP);
}
