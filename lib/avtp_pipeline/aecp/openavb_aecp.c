/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AVDECC Enumeration and control protocol (AECP)
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       13-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Implements the AVDECC Enumeration and control protocol (AECP)
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */
#include "openavb_platform.h"

#include <signal.h>

#define	AVB_LOG_COMPONENT	"AECP"
#include "openavb_log.h"

#include "openavb_aem.h"
#include "openavb_aecp.h"
#include "openavb_aecp_sm_entity_model_entity.h"
#include "openavb_aecp_message.h"

openavb_aecp_sm_global_vars_t openavbAecpSMGlobalVars;

MUTEX_HANDLE(openavbAecpMutex);
#define ADP_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAecpMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ADP_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAecpMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

openavbRC openavbAecpStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);

	openavb_avdecc_entity_model_t *pAem = openavbAemGetModel();
	if (!pAem) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING), AVB_TRACE_AECP);
	}

	MUTEX_ATTR_HANDLE(mta);
	MUTEX_ATTR_INIT(mta);
	MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
	MUTEX_ATTR_SET_NAME(mta, "openavbAecpMutex");
	MUTEX_CREATE_ERR();
	MUTEX_CREATE(openavbAecpMutex, mta);
	MUTEX_LOG_ERR("Could not create/initialize 'openavbAecpMutex' mutex");

	// Set AECP global vars
	memcpy(openavbAecpSMGlobalVars.myEntityID, pAem->pDescriptorEntity->entity_id, sizeof(openavbAecpSMGlobalVars.myEntityID));

	openavbRC rc = openavbAecpMessageHandlerStart();
	if (IS_OPENAVB_FAILURE(rc)) {
		openavbAecpStop();
		AVB_RC_TRACE_RET(rc, AVB_TRACE_AECP);
	}

	openavbAecpSMEntityModelEntityStart();

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AECP);
}

void openavbAecpStop()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AECP);

	openavbAecpSMEntityModelEntityStop();
	openavbAecpMessageHandlerStop();

	AVB_TRACE_EXIT(AVB_TRACE_AECP);
}


