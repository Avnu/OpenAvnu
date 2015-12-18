/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
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
* MODULE SUMMARY : Common implementation for the talker and listener
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "openavb_tl.h"
#include "openavb_trace.h"
#include "openavb_mediaq.h"
#include "openavb_talker.h"
#include "openavb_listener.h"
// #include "openavb_avtp.h"
#include "openavb_platform.h"

#define	AVB_LOG_COMPONENT	"Talker / Listener"
#include "openavb_pub.h"
#include "openavb_log.h"

U32 gMaxTL;
tl_handle_t *gTLHandleList;

// We are accessed from multiple threads, so need a mutex
MUTEX_HANDLE(gTLStateMutex);

#define MATCH(A, B)(strcasecmp((A), (B)) == 0)
//#define MATCH_LEFT(A, B, C)(strncasecmp((A), (B), (C)) == 0)
#define MATCH_LEFT(A, B, C) (memcmp((A), (B), (C)) == 0)

THREAD_TYPE(listenerThread);
THREAD_TYPE(talkerThread);

void* openavbTLThreadFn(void *pv);
#define THREAD_CREATE_TALKER() THREAD_CREATE(talkerThread, pTLState->TLThread, NULL, openavbTLThreadFn, pTLState)
#define THREAD_CREATE_LISTENER() THREAD_CREATE(listenerThread, pTLState->TLThread, NULL, openavbTLThreadFn, pTLState)

void timespec_add_usec(struct timespec *t, unsigned long us)
{
	t->tv_nsec += us * NANOSECONDS_PER_USEC;
	t->tv_sec += t->tv_nsec / NANOSECONDS_PER_SECOND;
	t->tv_nsec = t->tv_nsec % NANOSECONDS_PER_SECOND;
}

void timespec_sub_usec(struct timespec *t, unsigned long us)
{
	t->tv_nsec -= us * NANOSECONDS_PER_USEC;
	t->tv_sec += t->tv_nsec / NANOSECONDS_PER_SECOND;
	t->tv_nsec = t->tv_nsec % NANOSECONDS_PER_SECOND;
	if (t->tv_nsec < 0) {
		t->tv_sec--;
		t->tv_nsec = NANOSECONDS_PER_SECOND + t->tv_nsec;
	}
}

unsigned long timespec_usec_diff(struct timespec *t1, struct timespec *t2)
{
	return (t1->tv_sec - t2->tv_sec) * MICROSECONDS_PER_SECOND
		   + (t1->tv_nsec - t2->tv_nsec) / NANOSECONDS_PER_USEC;
}

int timespec_cmp(struct timespec *a, struct timespec *b)
{
	if (a->tv_sec > b->tv_sec)
		return 1;
	else if (a->tv_sec < b->tv_sec)
		return -1;
	else {
		if (a->tv_nsec > b->tv_nsec)
			return 1;
		else if (a->tv_nsec < b->tv_nsec)
			return -1;
	}
	return 0;
}

static bool TLHandleListAdd(tl_handle_t handle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!handle || !gTLHandleList) {
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	TL_LOCK();
	int i1;
	for (i1 = 0; i1 < gMaxTL; i1++) {
		if (!gTLHandleList[i1]) {
			gTLHandleList[i1] = handle;
			TL_UNLOCK();
			AVB_TRACE_EXIT(AVB_TRACE_TL);
			return TRUE;
		}
	}
	TL_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return FALSE;
}

bool TLHandleListRemove(tl_handle_t handle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!handle || !gTLHandleList) {
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	TL_LOCK();
	int i1;
	for (i1 = 0; i1 < gMaxTL; i1++) {
		if (gTLHandleList[i1] == handle) {
			gTLHandleList[i1] = NULL;
			TL_UNLOCK();
			AVB_TRACE_EXIT(AVB_TRACE_TL);
			return TRUE;
		}
	}
	TL_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return FALSE;
}

static bool checkIntfCallbacks(openavb_tl_cfg_t *pCfg)
{
	bool validCfg = TRUE;

	if (!pCfg->intf_cb.intf_cfg_cb) {
		AVB_LOG_WARNING("INI file doesn't specify inferface callback for '_cfg'.");
		// validCfg = FALSE;
	}
	if ((pCfg->role == AVB_ROLE_TALKER) && !pCfg->intf_cb.intf_tx_init_cb) {
		AVB_LOG_WARNING("INI file doesn't specify inferface callback for '_tx_init'.");
		// validCfg = FALSE;
	}
	if ((pCfg->role == AVB_ROLE_TALKER) && !pCfg->intf_cb.intf_tx_cb) {
		AVB_LOG_ERROR("INI file doesn't specify inferface callback for '_tx'.");
		validCfg = FALSE;
	}
	if ((pCfg->role == AVB_ROLE_LISTENER) && !pCfg->intf_cb.intf_rx_init_cb) {
		AVB_LOG_WARNING("INI file doesn't specify inferface callback for '_rx_init'.");
		// validCfg = FALSE;
	}
	if ((pCfg->role == AVB_ROLE_LISTENER) && !pCfg->intf_cb.intf_rx_cb) {
		AVB_LOG_ERROR("INI file doesn't specify inferface callback for '_rx'.");
		validCfg = FALSE;
	}
	if (!pCfg->intf_cb.intf_end_cb) {
		AVB_LOG_WARNING("INI file doesn't specify inferface callback for '_end'.");
		// validCfg = FALSE;
	}
	if (!pCfg->intf_cb.intf_gen_init_cb) {
		AVB_LOG_WARNING("INI file doesn't specify inferface callback for '_gen_init'.");
		// validCfg = FALSE;
	}
	if (!pCfg->intf_cb.intf_gen_end_cb) {
		AVB_LOG_WARNING("INI file doesn't specify inferface callback for '_gen_end'.");
		// validCfg = FALSE;
	}
	if (!pCfg->intf_cb.intf_avdecc_init_cb) {
		// Optional callback
		// CORE_TODO: AVDECC not formally supported yet.
		// AVB_LOG_WARNING("INI file doesn't specify inferface callback for '_avdecc_init'.");
		// validCfg = FALSE;
	}

	return validCfg;
}

static bool checkMapCallbacks(openavb_tl_cfg_t *pCfg)
{
	bool validCfg = TRUE;

	if (!pCfg->map_cb.map_cfg_cb) {
		AVB_LOG_WARNING("INI doesn't specify mapping callback for '_cfg'.");
		// validCfg = FALSE;
	}
	if (!pCfg->map_cb.map_subtype_cb) {
		AVB_LOG_WARNING("INI doesn't specify mapping callback for '_subtype'.");
		// validCfg = FALSE;
	}
	if (!pCfg->map_cb.map_avtp_version_cb) {
		AVB_LOG_WARNING("INI doesn't specify mapping callback for '_avtp_version'.");
		// validCfg = FALSE;
	}
	if ((pCfg->role == AVB_ROLE_TALKER) && !pCfg->map_cb.map_tx_init_cb) {
		AVB_LOG_WARNING("INI doesn't specify mapping callback for '_tx_init'.");
		// validCfg = FALSE;
	}
	if ((pCfg->role == AVB_ROLE_TALKER) && !pCfg->map_cb.map_tx_cb) {
		AVB_LOG_ERROR("INI doesn't specify mapping callback for '_tx'.");
		validCfg = FALSE;
	}
	if ((pCfg->role == AVB_ROLE_LISTENER) && !pCfg->map_cb.map_rx_init_cb) {
		AVB_LOG_WARNING("INI doesn't specify mapping callback for '_rx_init'.");
		// validCfg = FALSE;
	}
	if ((pCfg->role == AVB_ROLE_LISTENER) && !pCfg->map_cb.map_rx_cb) {
		AVB_LOG_ERROR("INI doesn't specify mapping callback for '_rx'.");
		validCfg = FALSE;
	}
	if (!pCfg->map_cb.map_end_cb) {
		AVB_LOG_WARNING("INI doesn't specify mapping callback for '_end'.");
		// validCfg = FALSE;
	}
	if (!pCfg->map_cb.map_gen_init_cb) {
		AVB_LOG_WARNING("INI doesn't specify mapping callback for '_gen_init'.");
		// validCfg = FALSE;
	}
	if (!pCfg->map_cb.map_gen_end_cb) {
		AVB_LOG_WARNING("INI doesn't specify mapping callback for '_gen_end'.");
		// validCfg = FALSE;
	}
	if (!pCfg->map_cb.map_avdecc_init_cb) {
		// Optional callback
		// CORE_TODO: AVDECC not formally supported yet.
		// AVB_LOG_WARNING("INI doesn't specify mapping callback for '_avdecc_init'.");
		// validCfg = FALSE;
	}

	return validCfg;
}

void openavbTLUnconfigure(tl_state_t *pTLState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	// CORE_TODO: Disable this functionality until updated to properly handle distinction between values point to static const char
	// and dynamically allocated strings.
#if 0
	openavb_tl_cfg_t *pCfg = &pTLState->cfg;

	int i1;
	for (i1 = 0; i1 < pCfg->nLibCfgItems; i1++) {
		if (pCfg->libCfgNames[i1])
			free(pCfg->libCfgNames[i1]);
		if (pNVCfg->libCfgValues[i1])
			free(pNVCfg->libCfgValues[i1]);
		pCfg->libCfgNames[i1] = pNVCfg->libCfgValues[i1] = NULL;
	}
	pCfg->nLibCfgItems = 0;
#endif

	AVB_TRACE_EXIT(AVB_TRACE_TL);
}


/* Public APIs
 */
// General initizlization of the talker and listener library. Must be called prior to using any other TL APIs.
EXTERN_DLL_EXPORT bool openavbTLInitialize(U32 maxTL)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);
	LOG_EAVB_CORE_VERSION();

	// CORE_TODO : This can be used to AVB stack init functionality once such a thing exists in the reference implementation
	AVB_TIME_INIT();

	gMaxTL = maxTL;

	{
		MUTEX_ATTR_HANDLE(mta);
		MUTEX_ATTR_INIT(mta);
		MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
		MUTEX_ATTR_SET_NAME(mta, "gTLStateMutex");
		MUTEX_CREATE_ERR();
		MUTEX_CREATE(gTLStateMutex, mta);
		MUTEX_LOG_ERR("Error creating mutex");
	}

	gTLHandleList = calloc(1, sizeof(tl_handle_t) * gMaxTL);
	if (gTLHandleList) {
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return TRUE;
	}
	else {
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}
}

// General cleanup of the talker and listener library. Should be called after all Talker and Listeners are closed.
EXTERN_DLL_EXPORT bool openavbTLCleanup()
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);
	if (gTLHandleList) {
		free(gTLHandleList);
		gTLHandleList = NULL;
	}
	else {
		return FALSE;
	}

	{
		MUTEX_CREATE_ERR();
		MUTEX_DESTROY(gTLStateMutex);
		MUTEX_LOG_ERR("Error destroying mutex");
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return TRUE;
}

EXTERN_DLL_EXPORT bool openavbGetVersion(U8 *major, U8 *minor, U8 *revision)
{
	if (!major || !minor || !revision) {
		return FALSE;
	}

	*major = AVB_CORE_VER_MAJOR;
	*minor = AVB_CORE_VER_MINOR;
	*revision = AVB_CORE_VER_REVISION;
	return TRUE;
}

EXTERN_DLL_EXPORT tl_handle_t openavbTLOpen(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = calloc(1, sizeof(tl_state_t));

	if (!pTLState) {
		AVB_LOG_ERROR("Unable to allocate talker listener state data.");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return NULL;
	}

	if (!TLHandleListAdd(pTLState)) {
		AVB_LOG_ERROR("To many talker listeners open.");
		free(pTLState);
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return NULL;
	}

	return pTLState;
}

EXTERN_DLL_EXPORT void openavbTLInitCfg(openavb_tl_cfg_t *pCfg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	memset(pCfg, 0, sizeof(openavb_tl_cfg_t));

	// Set default values.
	pCfg->role = AVB_ROLE_UNDEFINED;
	//pCfg->map_cb;
	//pCfg->intf_cb;
	//pCfg->dest_addr;
	//pCfg->stream_addr;
	pCfg->stream_uid = -1;
	pCfg->max_interval_frames = 1;
	pCfg->max_frame_size = 1500;
	pCfg->max_transit_usec = 50000;
	pCfg->max_transmit_deficit_usec = 50000;
	pCfg->internal_latency = 0;
	pCfg->max_stale = MICROSECONDS_PER_SECOND;
	pCfg->batch_factor = 1;
	pCfg->report_seconds = 0;
	pCfg->start_paused = FALSE;
	pCfg->sr_class = SR_CLASS_B;
	pCfg->sr_rank = SR_RANK_REGULAR;
	pCfg->raw_tx_buffers = 8;
	pCfg->raw_rx_buffers = 100;
	pCfg->tx_blocking_in_intf =  0;
	pCfg->rx_signal_mode = 1;
	pCfg->pMapInitFn = NULL;
	pCfg->pIntfInitFn = NULL;
	pCfg->vlan_id = VLAN_NULL;

	AVB_TRACE_EXIT(AVB_TRACE_TL);
}

EXTERN_DLL_EXPORT bool openavbTLConfigure(tl_handle_t handle, openavb_tl_cfg_t *pCfgIn, openavb_tl_cfg_name_value_t *pNVCfg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = (tl_state_t *)handle;

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid handle.");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	// Create the mediaQ
	pTLState->pMediaQ = openavbMediaQCreate();
	if (!pTLState->pMediaQ) {
		AVB_LOG_ERROR("Unable to create media queue");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	// CORE_TODO: It's not safe to simply copy the openavb_tl_cfg_t since there are embedded pointers in the cfg_mac_t member.
	// Those pointers need to be updated after a copy. Longer term the cfg_mac_t should be changed to not contain the mac
	// member to remedy this issue and avoid further bugs.
	memcpy(&pTLState->cfg, pCfgIn, sizeof(openavb_tl_cfg_t));
	pTLState->cfg.dest_addr.mac = &pTLState->cfg.dest_addr.buffer;
	pTLState->cfg.stream_addr.mac = &pTLState->cfg.stream_addr.buffer;

	openavb_tl_cfg_t *pCfg = &pTLState->cfg;

	if (!((pCfg->role == AVB_ROLE_TALKER) || (pCfg->role == AVB_ROLE_LISTENER))) {
		AVB_LOG_ERROR("Talker - Listener Config Error: invalid role");
		return FALSE;
	}

	if ((pCfg->role == AVB_ROLE_TALKER) && (pCfg->max_interval_frames == 0)) {
		AVB_LOG_ERROR("Talker - Listener Config Error: talker role requires 'max_interval_frames'");
		return FALSE;
	}

	openavbMediaQSetMaxStaleTail(pTLState->pMediaQ, pCfg->max_stale);

	if (!openavbTLOpenLinkLibsOsal(pTLState)) {
		AVB_LOG_ERROR("Failed to open mapping / interface library");
		return FALSE;
	}

	if (pCfg->pMapInitFn && pCfg->pMapInitFn(pTLState->pMediaQ, &pCfg->map_cb, pCfg->max_transit_usec)) {
		checkMapCallbacks(&pTLState->cfg);
	}
	else {
		AVB_LOG_ERROR("Mapping initialize function error.");
		return FALSE;
	}

	if (pCfg->pIntfInitFn && pCfg->pIntfInitFn(pTLState->pMediaQ, &pCfg->intf_cb)) {
		checkIntfCallbacks(&pTLState->cfg);
	}
	else {
		AVB_LOG_ERROR("Interface initialize function error.");
		return FALSE;
	}

	// Submit configuration values to mapping and interface modules
	int i;
	for (i = 0; i < pNVCfg->nLibCfgItems; i++) {
		if (MATCH_LEFT(pNVCfg->libCfgNames[i], "intf_nv_", 8)) {
			if (pCfg->intf_cb.intf_cfg_cb) {
				pCfg->intf_cb.intf_cfg_cb(pTLState->pMediaQ, pNVCfg->libCfgNames[i], pNVCfg->libCfgValues[i]);
			}
			else {
				AVB_LOGF_ERROR("No interface module cfg function; ignoring %s", pNVCfg->libCfgNames[i]);
			}
		}
		else if (MATCH_LEFT(pNVCfg->libCfgNames[i], "map_nv_", 7)) {
			if (pCfg->map_cb.map_cfg_cb) {
				pCfg->map_cb.map_cfg_cb(pTLState->pMediaQ, pNVCfg->libCfgNames[i], pNVCfg->libCfgValues[i]);
			}
			else {
				AVB_LOGF_ERROR("No mapping module cfg function; ignoring %s", pNVCfg->libCfgNames[i]);
			}
		}
		else {
			assert(0);
		}
	} // for loop ends

	pTLState->cfg.map_cb.map_gen_init_cb(pTLState->pMediaQ);
	pTLState->cfg.intf_cb.intf_gen_init_cb(pTLState->pMediaQ);

	return TRUE;
}

EXTERN_DLL_EXPORT bool openavbTLRun(tl_handle_t handle)
{
	bool retVal = FALSE;

	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = (tl_state_t *)handle;

	do {
		if (!pTLState) {
			AVB_LOG_ERROR("Invalid handle.");
			break;
		}

		pTLState->bRunning = TRUE;
		if (pTLState->cfg.role == AVB_ROLE_TALKER) {
			THREAD_CREATE_TALKER();
		}
		else if (pTLState->cfg.role == AVB_ROLE_LISTENER) {
			THREAD_CREATE_LISTENER();
		}

		retVal = TRUE;

	} while (0);

	

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return retVal;
}

extern DLL_EXPORT bool openavbTLStop(tl_handle_t handle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = (tl_state_t *)handle;

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid handle.");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	if (pTLState->bRunning) {
		// don't set bStreaming to false here, that's needed to track
		// that the streaming thread is running, so we can shut it down.
		//pTLState->bStreaming = FALSE;
		pTLState->bRunning = FALSE;

		THREAD_JOIN(pTLState->TLThread, NULL);
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return TRUE;
}

EXTERN_DLL_EXPORT bool openavbTLClose(tl_handle_t handle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = (tl_state_t *)handle;

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid handle.");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	if (pTLState->bRunning == TRUE) {
		// In case openavbTLStop wasn't called stop is now.
		openavbTLStop(handle);
	}

	pTLState->cfg.intf_cb.intf_gen_end_cb(pTLState->pMediaQ);
	pTLState->cfg.map_cb.map_gen_end_cb(pTLState->pMediaQ);

	TLHandleListRemove(handle);

	openavbTLUnconfigure(pTLState);
	openavbTLCloseLinkLibsOsal(pTLState);

	if (pTLState->pMediaQ) {
		openavbMediaQDelete(pTLState->pMediaQ);
		pTLState->pMediaQ = NULL;
	}

	// Free TLState
	free(pTLState);
	pTLState = NULL;

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return TRUE;
}

EXTERN_DLL_EXPORT void* openavbTLGetIntfHostCBList(tl_handle_t handle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = (tl_state_t *)handle;

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid handle.");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return pTLState->cfg.intf_cb.intf_host_cb_list;
}

EXTERN_DLL_EXPORT void* openavbTLGetIntfHandle(tl_handle_t handle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = (tl_state_t *)handle;

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid handle.");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return pTLState->pMediaQ;
}

EXTERN_DLL_EXPORT bool openavbTLIsRunning(tl_handle_t handle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = (tl_state_t *)handle;

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid handle.");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return pTLState->bRunning;
}

EXTERN_DLL_EXPORT bool openavbTLIsConnected(tl_handle_t handle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = (tl_state_t *)handle;

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid handle.");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return pTLState->bConnected;
}

EXTERN_DLL_EXPORT bool openavbTLIsStreaming(tl_handle_t handle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = (tl_state_t *)handle;

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid handle.");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return pTLState->bStreaming;
}

EXTERN_DLL_EXPORT avb_role_t openavbTLGetRole(tl_handle_t handle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = (tl_state_t *)handle;

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid handle");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return AVB_ROLE_UNDEFINED;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return pTLState->cfg.role;
}


EXTERN_DLL_EXPORT U64 openavbTLStat(tl_handle_t handle, tl_stat_t stat)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);
	U64 val = 0;

	tl_state_t *pTLState = (tl_state_t *)handle;

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid handle");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return 0;
	}

	if (pTLState->cfg.role == AVB_ROLE_TALKER) {
		val = openavbTalkerGetStat(pTLState, stat);
	}
	else if (pTLState->cfg.role == AVB_ROLE_LISTENER) {
		val = openavbListenerGetStat(pTLState, stat);
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return val;
}

EXTERN_DLL_EXPORT void openavbTLPauseStream(tl_handle_t handle, bool bPause)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = (tl_state_t *)handle;

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid handle.");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	if (pTLState->cfg.role == AVB_ROLE_TALKER) {
		openavbTLPauseTalker(pTLState, bPause);
	}
	else if (pTLState->cfg.role == AVB_ROLE_LISTENER) {
		openavbTLPauseListener(pTLState, bPause);
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
}



