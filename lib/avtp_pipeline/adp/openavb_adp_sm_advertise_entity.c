/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : ADP - AVDECC Discovery Protocol : Advertise Entity State Machine
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       5-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Implements the AVDECC Discovery Protocol : Advertise Entity State Machine
 * IEEE Std 1722.1-2013 clause 6.2.4.
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
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

					ADP_LOCK();
					openavbAdpSMGlobalVars.entityInfo.pdu.available_index = 0;
					ADP_UNLOCK();

					state = OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_ADVERTISE;
				}
				break;
			case OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_ADVERTISE:
				{
					AVB_TRACE_LINE(AVB_TRACE_ADP);

					openavbAdpSMAdvertiseEntity_sendAvailable();
					ADP_LOCK();
					// Slightly deviates from the spec in that reannounceTimerTimeout is a delay time rather than a absolute time.
					openavbAdpSMAdvertiseEntityVars.reannounceTimerTimeout = openavbAdpSMGlobalVars.entityInfo.header.valid_time / 4;
					if (openavbAdpSMAdvertiseEntityVars.reannounceTimerTimeout < 1) {
						openavbAdpSMAdvertiseEntityVars.reannounceTimerTimeout = 1;
					}
					openavbAdpSMAdvertiseEntityVars.needsAdvertise = FALSE;
					ADP_UNLOCK();
					state = OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_WAITING;
				}
				break;
			case OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_WAITING:
				{
					AVB_TRACE_LINE(AVB_TRACE_ADP);

					ADP_LOCK();
					openavbAdpSMAdvertiseInterfaceSet_rcvdDiscover(FALSE);
					openavbAdpSMGlobalVars.entityInfo.pdu.available_index++;
					ADP_UNLOCK();

					// Wait for timeout or semaphore
					struct timespec timeout;
					CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &timeout);
					timeout.tv_sec += openavbAdpSMAdvertiseEntityVars.reannounceTimerTimeout;

					// Wait for change in state
					while (state == OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_WAITING && bRunning) {
						U32 timeoutMSec = 0;
						struct timespec now;
						CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &now);
						timeoutMSec = openavbTimeUntilMSec(&now, &timeout);
						SEM_ERR_T(err);
						SEM_TIMEDWAIT(openavbAdpSMAdvertiseEntitySemaphore, timeoutMSec, err);

						if (!SEM_IS_ERR_NONE(err)) {
							if (SEM_IS_ERR_TIMEOUT(err)) {
								state = OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_ADVERTISE;
							}
						}
						else if (openavbAdpSMAdvertiseEntityVars.doTerminate) {
							bRunning = FALSE;
						}
						else if (openavbAdpSMAdvertiseEntityVars.needsAdvertise) {
							state = OPENAVB_ADP_SM_ADVERTISE_ENTITY_STATE_ADVERTISE;
						}
					}
				}
				break;

			default:
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
	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return NULL;
}

void openavbAdpSMAdvertiseEntityStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	SEM_ERR_T(err);
	SEM_INIT(openavbAdpSMAdvertiseEntitySemaphore, 1, err);
	SEM_LOG_ERR(err);

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





