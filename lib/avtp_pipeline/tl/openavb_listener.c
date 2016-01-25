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
* MODULE SUMMARY : Listener implementation
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "openavb_platform.h"
#include "openavb_trace.h"
#include "openavb_tl.h"
#include "openavb_avtp.h"
#include "openavb_listener.h"

// DEBUG Uncomment to turn on logging for just this module.
//#define AVB_LOG_ON	1

#define	AVB_LOG_COMPONENT	"Listener"
#include "openavb_log.h"

#include "openavb_debug.h"

bool listenerStartStream(tl_state_t *pTLState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	assert(!pTLState->bStreaming);

	openavb_tl_cfg_t *pCfg = &pTLState->cfg;
	listener_data_t *pListenerData = pTLState->pPvtListenerData;

	openavbRC rc = openavbAvtpRxInit(pTLState->pMediaQ,
		&pCfg->map_cb,
		&pCfg->intf_cb,
		pListenerData->ifname,
		&pListenerData->streamID,
		pListenerData->destAddr,
		pCfg->raw_rx_buffers,
		pCfg->rx_signal_mode,
		&pListenerData->avtpHandle);
	if (IS_OPENAVB_FAILURE(rc)) {
		AVB_LOG_ERROR("Failed to create AVTP stream");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	// Setup timers
	U64 nowNS;
	CLOCK_GETTIME64(OPENAVB_TIMER_CLOCK, &nowNS);
	pListenerData->nextReportNS = nowNS + (pCfg->report_seconds * NANOSECONDS_PER_SECOND);
	pListenerData->nextSecondNS = nowNS + NANOSECONDS_PER_SECOND;

	// Clear counters
	pListenerData->nReportCalls = 0;
	pListenerData->nReportFrames = 0;

	// Clear stats
	openavbListenerClearStats(pTLState);

	// we're good to go!
	pTLState->bStreaming = TRUE;

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return TRUE;
}

void listenerStopStream(tl_state_t *pTLState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	listener_data_t *pListenerData = pTLState->pPvtListenerData;
	if (!pListenerData) {
		AVB_LOG_ERROR("Invalid listener data");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	openavbListenerAddStat(pTLState, TL_STAT_RX_CALLS, pListenerData->nReportCalls);
	openavbListenerAddStat(pTLState, TL_STAT_RX_FRAMES, pListenerData->nReportFrames);
	openavbListenerAddStat(pTLState, TL_STAT_RX_LOST, openavbAvtpLost(pListenerData->avtpHandle));
	openavbListenerAddStat(pTLState, TL_STAT_RX_BYTES, openavbAvtpBytes(pListenerData->avtpHandle));

	AVB_LOGF_INFO("RX "STREAMID_FORMAT", Totals: calls=%" PRIu64 "frames=%" PRIu64 "lost=%" PRIu64 "bytes=%" PRIu64,
		STREAMID_ARGS(&pListenerData->streamID),
		openavbListenerGetStat(pTLState, TL_STAT_RX_CALLS),
		openavbListenerGetStat(pTLState, TL_STAT_RX_FRAMES),
		openavbListenerGetStat(pTLState, TL_STAT_RX_LOST),
		openavbListenerGetStat(pTLState, TL_STAT_RX_BYTES));

	if (pTLState->bStreaming) {
		openavbAvtpShutdown(pListenerData->avtpHandle);
		pTLState->bStreaming = FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
}

static inline bool listenerDoStream(tl_state_t *pTLState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	openavb_tl_cfg_t *pCfg = &pTLState->cfg;
	listener_data_t *pListenerData = pTLState->pPvtListenerData;
	bool bRet = FALSE;

	if (pTLState->bStreaming) {
		U64 nowNS;

		pListenerData->nReportCalls++;

		// Try to receive a frame
		if (IS_OPENAVB_SUCCESS(openavbAvtpRx(pListenerData->avtpHandle))) {
			pListenerData->nReportFrames++;
		}

		CLOCK_GETTIME64(OPENAVB_TIMER_CLOCK, &nowNS);

		if (pCfg->report_seconds > 0) {
			if (nowNS > pListenerData->nextReportNS) {
			  
				U64 lost = openavbAvtpLost(pListenerData->avtpHandle);
				U64 bytes = openavbAvtpBytes(pListenerData->avtpHandle);
				U32 rxbuf = openavbAvtpRxBufferLevel(pListenerData->avtpHandle);
				U32 mqbuf = openavbMediaQCountItems(pTLState->pMediaQ, TRUE);
				U32 mqrdy = openavbMediaQCountItems(pTLState->pMediaQ, FALSE);
			
				AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, FALSE, "RX UID:%d, ", LOG_RT_DATATYPE_U16, &pListenerData->streamID.uniqueID);
				AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "calls=%ld, ", LOG_RT_DATATYPE_U32, &pListenerData->nReportCalls);
				AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "frames=%ld, ", LOG_RT_DATATYPE_U32, &pListenerData->nReportFrames);
				AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "lost=%lld, ", LOG_RT_DATATYPE_U64, &lost);
				AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "bytes=%lld, ", LOG_RT_DATATYPE_U64, &bytes);
				AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "rxbuf=%d, ", LOG_RT_DATATYPE_U32, &rxbuf);
				AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "mqbuf=%d, ", LOG_RT_DATATYPE_U32, &mqbuf);
				AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, LOG_RT_END, "mqrdy=%d", LOG_RT_DATATYPE_U32, &mqrdy);

				openavbListenerAddStat(pTLState, TL_STAT_RX_CALLS, pListenerData->nReportCalls);
				openavbListenerAddStat(pTLState, TL_STAT_RX_FRAMES, pListenerData->nReportFrames);
				openavbListenerAddStat(pTLState, TL_STAT_RX_LOST, lost);
				openavbListenerAddStat(pTLState, TL_STAT_RX_BYTES, bytes);

				pListenerData->nReportCalls = 0;
				pListenerData->nReportFrames = 0;
				pListenerData->nextReportNS += (pCfg->report_seconds * NANOSECONDS_PER_SECOND);  
			}
		}

		if (nowNS > pListenerData->nextSecondNS) {
			pListenerData->nextSecondNS += NANOSECONDS_PER_SECOND;
			bRet = TRUE;
		}
	}
	else {
            SLEEP(1);
            bRet = TRUE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return bRet;
}

// Called from openavbTLThreadFn() which is started from openavbTLRun() 
void openavbTLRunListener(tl_state_t *pTLState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	openavb_tl_cfg_t *pCfg = &pTLState->cfg;

	pTLState->pPvtListenerData = calloc(1, sizeof(listener_data_t));
	if (!pTLState->pPvtListenerData) {
		AVB_LOG_WARNING("Failed to allocate listener data.");
		return;
	}

	AVBStreamID_t streamID;
	memcpy(streamID.addr, pCfg->stream_addr.mac, ETH_ALEN);
	streamID.uniqueID = pCfg->stream_uid;

	AVB_LOGF_INFO("Attach "STREAMID_FORMAT, STREAMID_ARGS(&streamID));

	// Create Stats Mutex
	{
		MUTEX_ATTR_HANDLE(mta);
		MUTEX_ATTR_INIT(mta);
		MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
		MUTEX_ATTR_SET_NAME(mta, "TLStatsMutex");
		MUTEX_CREATE_ERR();
		MUTEX_CREATE(pTLState->statsMutex, mta);
		MUTEX_LOG_ERR("Could not create/initialize 'TLStatsMutex' mutex");
	}

	// Tell endpoint to listen for our stream.
	// If there is a talker, we'll get callback (above.)
	pTLState->bConnected = openavbTLRunListenerInit(pTLState->endpointHandle, &streamID);
	
	if (pTLState->bConnected) {
		bool bServiceIPC;

		// Do until we are stopped or loose connection to endpoint
		while (pTLState->bRunning && pTLState->bConnected) {

			// Listen for an RX frame (or just sleep if not streaming)
			bServiceIPC = listenerDoStream(pTLState);

			if (bServiceIPC) {
				// Look for messages from endpoint.  Don't block (timeout=0)
				if (!openavbEptClntService(pTLState->endpointHandle, 0)) {
					AVB_LOGF_WARNING("Lost connection to endpoint "STREAMID_FORMAT, STREAMID_ARGS(&streamID));
					pTLState->bConnected = FALSE;
					pTLState->endpointHandle = 0;
				}
			}
		}

		// Stop streaming
		listenerStopStream(pTLState);

		{
			MUTEX_CREATE_ERR();
			MUTEX_DESTROY(pTLState->statsMutex); // Destroy Stats Mutex
			MUTEX_LOG_ERR("Error destroying mutex");
		}

		// withdraw our listener attach
		if (pTLState->bConnected)
			openavbEptClntStopStream(pTLState->endpointHandle, &streamID);
	}
	else {
		AVB_LOGF_WARNING("Failed to connect to endpoint "STREAMID_FORMAT, STREAMID_ARGS(&streamID));
	}

	if (pTLState->pPvtListenerData) {
		free(pTLState->pPvtListenerData);
		pTLState->pPvtListenerData = NULL;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
}

void openavbTLPauseListener(tl_state_t *pTLState, bool bPause)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	listener_data_t *pListenerData = pTLState->pPvtListenerData;
	if (!pListenerData) {
		AVB_LOG_ERROR("Invalid private listener data");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	openavbAvtpPause(pListenerData->avtpHandle, bPause);

	AVB_TRACE_EXIT(AVB_TRACE_TL);
}

void openavbListenerClearStats(tl_state_t *pTLState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	listener_data_t *pListenerData = pTLState->pPvtListenerData;
	if (!pListenerData) {
		AVB_LOG_ERROR("Invalid private listener data");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	LOCK_STATS();
	memset(&pListenerData->stats, 0, sizeof(pListenerData->stats));
	UNLOCK_STATS();

	AVB_TRACE_EXIT(AVB_TRACE_TL);
}

void openavbListenerAddStat(tl_state_t *pTLState, tl_stat_t stat, U64 val)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	listener_data_t *pListenerData = pTLState->pPvtListenerData;
	if (!pListenerData) {
		AVB_LOG_ERROR("Invalid private listener data");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	LOCK_STATS();
	switch (stat) {
		case TL_STAT_TX_CALLS:
		case TL_STAT_TX_FRAMES:
		case TL_STAT_TX_LATE:
		case TL_STAT_TX_BYTES:
			break;
		case TL_STAT_RX_CALLS:
			pListenerData->stats.totalCalls += val;
			break;
		case TL_STAT_RX_FRAMES:
			pListenerData->stats.totalFrames += val;
			break;
		case TL_STAT_RX_LOST:
			pListenerData->stats.totalLost += val;
			break;
		case TL_STAT_RX_BYTES:
			pListenerData->stats.totalBytes += val;
			break;
	}
	UNLOCK_STATS();

	AVB_TRACE_EXIT(AVB_TRACE_TL);
}

U64 openavbListenerGetStat(tl_state_t *pTLState, tl_stat_t stat)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);
	U64 val = 0;

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return 0;
	}

	listener_data_t *pListenerData = pTLState->pPvtListenerData;
	if (!pListenerData) {
		AVB_LOG_ERROR("Invalid private listener data");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return 0;
	}

	LOCK_STATS();
	switch (stat) {
		case TL_STAT_TX_CALLS:
		case TL_STAT_TX_FRAMES:
		case TL_STAT_TX_LATE:
		case TL_STAT_TX_BYTES:
			break;
		case TL_STAT_RX_CALLS:
			val = pListenerData->stats.totalCalls;
			break;
		case TL_STAT_RX_FRAMES:
			val = pListenerData->stats.totalFrames;
			break;
		case TL_STAT_RX_LOST:
			val = pListenerData->stats.totalLost;
			break;
		case TL_STAT_RX_BYTES:
			val = pListenerData->stats.totalBytes;
			break;
	}
	UNLOCK_STATS();

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return val;
}


