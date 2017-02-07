/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : ACMP - AVDECC Connection Management Protocol
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       10-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Implements the 1722.1 (AVDECC) connection management protocol
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
#include "openavb_acmp.h"
#include "openavb_acmp_sm_listener.h"
#include "openavb_acmp_sm_talker.h"
#include "openavb_acmp_message.h"

extern openavb_avdecc_cfg_t gAvdeccCfg;

openavb_acmp_sm_global_vars_t openavbAcmpSMGlobalVars;

MUTEX_HANDLE(openavbAcmpMutex);
#define ACMP_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAcmpMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ACMP_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAcmpMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

static bool bListenerStarted = FALSE;
static bool bTalkerStarted = FALSE;

openavbRC openavbAcmpStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavb_avdecc_entity_model_t *pAem = openavbAemGetModel();
	if (!pAem) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING), AVB_TRACE_ACMP);
	}

	{
		MUTEX_ATTR_HANDLE(mta);
		MUTEX_ATTR_INIT(mta);
		MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
		MUTEX_ATTR_SET_NAME(mta, "openavbAcmpMutex");
		MUTEX_CREATE_ERR();
		MUTEX_CREATE(openavbAcmpMutex, mta);
		MUTEX_LOG_ERR("Could not create/initialize 'openavbAcmpMutex' mutex");
	}

	ACMP_LOCK();
	{
		// Populate global ACMP vars
		openavb_acmp_control_header_t *pHeader = &openavbAcmpSMGlobalVars.controlHeader;

		pHeader->cd = 1;
		pHeader->subtype = OPENAVB_ACMP_AVTP_SUBTYPE;
		pHeader->sv = 0;
		pHeader->version = 0;
		//pHeader->message_type = 0;				// Set later depending on message type
		//pHeader->status = 0;						// Set later depending on message
		pHeader->control_data_length = 44;
		//pHeader->stream_id;						// Set later depending on stream
	}

	{
		memcpy(openavbAcmpSMGlobalVars.my_id, pAem->pDescriptorEntity->entity_id, sizeof(openavbAcmpSMGlobalVars.my_id));
	}
	ACMP_UNLOCK();

	openavbRC rc = openavbAcmpMessageHandlerStart();
	if (IS_OPENAVB_FAILURE(rc)) {
		openavbAcmpStop();
		AVB_RC_TRACE_RET(rc, AVB_TRACE_ACMP);
	}

	if (gAvdeccCfg.bListener) {
		openavbAcmpSMListenerStart();
		bListenerStarted = TRUE;
	}

	if (gAvdeccCfg.bTalker) {
		openavbAcmpSMTalkerStart();
		bTalkerStarted = TRUE;
	}

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_ACMP);
}

void openavbAcmpStop()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	if (bTalkerStarted) {
		openavbAcmpSMTalkerStop();
	}

	if (bListenerStarted) {
		openavbAcmpSMListenerStop();
	}

	openavbAcmpMessageHandlerStop();

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

