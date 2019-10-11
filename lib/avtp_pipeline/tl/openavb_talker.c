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
* MODULE SUMMARY : Talker implementation
*/

#include <stdlib.h>
#include "openavb_platform.h"
#include "openavb_trace.h"
#include "openavb_tl.h"
#include "openavb_avtp.h"
#include "openavb_talker.h"
#include "openavb_avdecc_msg_client.h"

// DEBUG Uncomment to turn on logging for just this module.
//#define AVB_LOG_ON	1

#define	AVB_LOG_COMPONENT	"Talker"
#include "openavb_log.h"

#include "openavb_debug.h"



bool talkerStartStream(tl_state_t *pTLState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	openavb_tl_cfg_t *pCfg = &pTLState->cfg;
	talker_data_t *pTalkerData = pTLState->pPvtTalkerData;

	assert(!pTLState->bStreaming);

	pTalkerData->wakeFrames = pCfg->max_interval_frames * pCfg->batch_factor;

	// Set a max_transmit_deficit_usec default
	if (pCfg->max_transmit_deficit_usec == 0)
		pCfg->max_transmit_deficit_usec = 50000;

	U32 transmitInterval = pTalkerData->classRate;
	if (pCfg->map_cb.map_transmit_interval_cb(pTLState->pMediaQ)) {
		// Override the class observation interval with the one provided by the mapping module.
		transmitInterval = pCfg->map_cb.map_transmit_interval_cb(pTLState->pMediaQ);
	}

	if (pCfg->intf_cb.intf_enable_fixed_timestamp) {
		pCfg->intf_cb.intf_enable_fixed_timestamp(pTLState->pMediaQ, pCfg->fixed_timestamp, transmitInterval, pCfg->batch_factor);
	} else if (pCfg->fixed_timestamp) {
		AVB_LOG_ERROR("Fixed timestamp enabled but interface doesn't support it");
	}

	openavbRC rc = openavbAvtpTxInit(pTLState->pMediaQ,
		&pCfg->map_cb,
		&pCfg->intf_cb,
		pTalkerData->ifname,
		&pTalkerData->streamID,
		pTalkerData->destAddr,
		pCfg->max_transit_usec,
		pTalkerData->fwmark,
		pTalkerData->vlanID,
		pTalkerData->vlanPCP,
		pTalkerData->wakeFrames * pCfg->raw_tx_buffers,
		&pTalkerData->avtpHandle);
	if (IS_OPENAVB_FAILURE(rc)) {
		AVB_LOG_ERROR("Failed to create AVTP stream");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	avtp_stream_t *pStream = (avtp_stream_t *)(pTalkerData->avtpHandle);

	pTalkerData->wakeRate = transmitInterval / pCfg->batch_factor;

	pTalkerData->sleepUsec = MICROSECONDS_PER_SECOND / pTalkerData->wakeRate;
	pTalkerData->intervalNS = NANOSECONDS_PER_SECOND / pTalkerData->wakeRate;

	U32 SRKbps = ((unsigned long)pTalkerData->classRate * (unsigned long)pCfg->max_interval_frames * (unsigned long)pStream->frameLen * 8L) / 1000;
	U32 DataKbps = ((unsigned long)pTalkerData->wakeRate * (unsigned long)pCfg->max_interval_frames * (unsigned long)pStream->frameLen * 8L) / 1000;

	AVB_LOGF_INFO(STREAMID_FORMAT", sr-rate=%" PRIu32 ", data-rate=%lu, frames=%" PRIu16 ", size=%" PRIu16 ", batch=%" PRIu32 ", sleep=%" PRIu64 "us, sr-Kbps=%d, data-Kbps=%d",
		STREAMID_ARGS(&pTalkerData->streamID), pTalkerData->classRate, pTalkerData->wakeRate,
		pTalkerData->tSpec.maxIntervalFrames, pTalkerData->tSpec.maxFrameSize,
		pCfg->batch_factor, pTalkerData->intervalNS / 1000, SRKbps, DataKbps);


	// number of intervals per report
	pTalkerData->wakesPerReport = pCfg->report_seconds * NANOSECONDS_PER_SECOND / pTalkerData->intervalNS;
	// counts of intervals and frames between reports
	pTalkerData->cntFrames = 0;
	pTalkerData->cntWakes = 0;

	// setup the initial times
	U64 nowNS;

	if (!pCfg->spin_wait) {
		CLOCK_GETTIME64(OPENAVB_TIMER_CLOCK, &nowNS);
	} else {
		CLOCK_GETTIME64(OPENAVB_CLOCK_WALLTIME, &nowNS);
	}

	// Align clock : allows for some performance gain
	nowNS = ((nowNS + (pTalkerData->intervalNS)) / pTalkerData->intervalNS) * pTalkerData->intervalNS;

	pTalkerData->nextReportNS = nowNS + (pCfg->report_seconds * NANOSECONDS_PER_SECOND);
	pTalkerData->lastReportFrames = 0;
	pTalkerData->nextSecondNS = nowNS + NANOSECONDS_PER_SECOND;
	pTalkerData->nextCycleNS = nowNS + pTalkerData->intervalNS;

	// Clear stats
	openavbTalkerClearStats(pTLState);

	// we're good to go!
	pTLState->bStreaming = TRUE;

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return TRUE;
}

void talkerStopStream(tl_state_t *pTLState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	talker_data_t *pTalkerData = pTLState->pPvtTalkerData;
	if (!pTalkerData) {
		AVB_LOG_ERROR("Invalid listener data");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	void *rawsock = NULL;
	if (pTalkerData->avtpHandle) {
		rawsock = ((avtp_stream_t*)pTalkerData->avtpHandle)->rawsock;
	}

	openavbTalkerAddStat(pTLState, TL_STAT_TX_CALLS, pTalkerData->cntWakes);
	openavbTalkerAddStat(pTLState, TL_STAT_TX_FRAMES, pTalkerData->cntFrames);
//	openavbTalkerAddStat(pTLState, TL_STAT_TX_LATE, 0);		// Can't calculate at this time
	openavbTalkerAddStat(pTLState, TL_STAT_TX_BYTES, openavbAvtpBytes(pTalkerData->avtpHandle));

	AVB_LOGF_INFO("TX "STREAMID_FORMAT", Totals: calls=%" PRIu64 ", frames=%" PRIu64 ", late=%" PRIu64 ", bytes=%" PRIu64 ", TXOutOfBuffs=%ld",
		STREAMID_ARGS(&pTalkerData->streamID),
		openavbTalkerGetStat(pTLState, TL_STAT_TX_CALLS),
		openavbTalkerGetStat(pTLState, TL_STAT_TX_FRAMES),
		openavbTalkerGetStat(pTLState, TL_STAT_TX_LATE),
		openavbTalkerGetStat(pTLState, TL_STAT_TX_BYTES),
		rawsock ? openavbRawsockGetTXOutOfBuffers(rawsock) : 0
		);

	if (pTLState->bStreaming) {
		openavbAvtpShutdownTalker(pTalkerData->avtpHandle);
		pTLState->bStreaming = FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
}

static inline void talkerShowStats(talker_data_t *pTalkerData, tl_state_t *pTLState)
{
	S32 late = pTalkerData->wakesPerReport - pTalkerData->cntWakes;
	U64 bytes = openavbAvtpBytes(pTalkerData->avtpHandle);
	if (late < 0) late = 0;
	U32 txbuf = openavbAvtpTxBufferLevel(pTalkerData->avtpHandle);
	U32 mqbuf = openavbMediaQCountItems(pTLState->pMediaQ, TRUE);

	AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, FALSE, "TX UID:%d, ", LOG_RT_DATATYPE_U16, &pTalkerData->streamID.uniqueID);
	AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "calls=%ld, ", LOG_RT_DATATYPE_U32, &pTalkerData->cntWakes);
	AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "frames=%ld, ", LOG_RT_DATATYPE_U32, &pTalkerData->cntFrames);
	AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "late=%d, ", LOG_RT_DATATYPE_U32, &late);
	AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "bytes=%lld, ", LOG_RT_DATATYPE_U64, &bytes);
	AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "txbuf=%d, ", LOG_RT_DATATYPE_U32, &txbuf);
	AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, LOG_RT_END, "mqbuf=%d, ", LOG_RT_DATATYPE_U32, &mqbuf);

	openavbTalkerAddStat(pTLState, TL_STAT_TX_LATE, late);
	openavbTalkerAddStat(pTLState, TL_STAT_TX_BYTES, bytes);
}

static inline bool talkerDoStream(tl_state_t *pTLState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return FALSE;
	}

	openavb_tl_cfg_t *pCfg = &pTLState->cfg;
	talker_data_t *pTalkerData = pTLState->pPvtTalkerData;
	bool bRet = FALSE;

	if (pTLState->bStreaming) {
		U64 nowNS;

		if (!pCfg->tx_blocking_in_intf) {

			if (!pCfg->spin_wait) {
				// sleep until the next interval
				SLEEP_UNTIL_NSEC(pTalkerData->nextCycleNS);
			} else {
#if !IGB_LAUNCHTIME_ENABLED && !ATL_LAUNCHTIME_ENABLED
				SPIN_UNTIL_NSEC(pTalkerData->nextCycleNS);
#endif
			}

			//AVB_DBG_INTERVAL(8000, TRUE);

			// send the frames for this interval
			int i;
			for (i = pTalkerData->wakeFrames; i > 0; i--) {
				if (IS_OPENAVB_SUCCESS(openavbAvtpTx(pTalkerData->avtpHandle, i == 1, pCfg->tx_blocking_in_intf)))
					pTalkerData->cntFrames++;
				else
					break;
			}
		}
		else {
			// Interface module block option
			if (IS_OPENAVB_SUCCESS(openavbAvtpTx(pTalkerData->avtpHandle, TRUE, pCfg->tx_blocking_in_intf)))
				pTalkerData->cntFrames++;
		}

		if (!pCfg->spin_wait) {
			CLOCK_GETTIME64(OPENAVB_TIMER_CLOCK, &nowNS);
		} else {
			CLOCK_GETTIME64(OPENAVB_CLOCK_WALLTIME, &nowNS);
		}

		if (pTalkerData->cntWakes++ % pTalkerData->wakeRate == 0) {
			// time to service the endpoint IPC
			bRet = TRUE;

			// Don't need to check again for another second.
			pTalkerData->nextSecondNS = nowNS + NANOSECONDS_PER_SECOND;
		}

		if (pCfg->report_seconds > 0) {
			if (nowNS > pTalkerData->nextReportNS) {
				talkerShowStats(pTalkerData, pTLState);
			  
				openavbTalkerAddStat(pTLState, TL_STAT_TX_CALLS, pTalkerData->cntWakes);
				openavbTalkerAddStat(pTLState, TL_STAT_TX_FRAMES, pTalkerData->cntFrames);

				pTalkerData->cntFrames = 0;
				pTalkerData->cntWakes = 0;
				pTalkerData->nextReportNS = nowNS + (pCfg->report_seconds * NANOSECONDS_PER_SECOND);
			}
		} else if (pCfg->report_frames > 0 && pTalkerData->cntFrames != pTalkerData->lastReportFrames) {
			if (pTalkerData->cntFrames % pCfg->report_frames == 1) {
				talkerShowStats(pTalkerData, pTLState);
				pTalkerData->lastReportFrames = pTalkerData->cntFrames;
			}
		}

		if (nowNS > pTalkerData->nextSecondNS) {
			pTalkerData->nextSecondNS = nowNS + NANOSECONDS_PER_SECOND;
			bRet = TRUE;
		}

		if (!pCfg->tx_blocking_in_intf) {
			pTalkerData->nextCycleNS += pTalkerData->intervalNS;

			if ((pTalkerData->nextCycleNS + (pCfg->max_transmit_deficit_usec * 1000)) < nowNS) {
				// Hit max deficit time. Something must be wrong. Reset the cycle timer.	
				// Align clock : allows for some performance gain
				nowNS = ((nowNS + (pTalkerData->intervalNS)) / pTalkerData->intervalNS) * pTalkerData->intervalNS;
				pTalkerData->nextCycleNS = nowNS + pTalkerData->intervalNS;
			}				
		}
	}
	else {
		SLEEP_MSEC(10);

		// time to service the endpoint IPC
		bRet = TRUE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return bRet;
}


// Called from openavbTLThreadFn() which is started from openavbTLRun() 
void openavbTLRunTalker(tl_state_t *pTLState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	pTLState->pPvtTalkerData = calloc(1, sizeof(talker_data_t));
	if (!pTLState->pPvtTalkerData) {
		AVB_LOG_WARNING("Failed to allocate talker data.");
		return;
	}

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

	/* If using endpoint register talker,
	   else register with tpsec */
	pTLState->bConnected = openavbTLRunTalkerInit(pTLState); 

	if (pTLState->bConnected) {
		bool bServiceIPC;

		// Notify AVDECC Msg of the state change.
		openavbAvdeccMsgClntNotifyCurrentState(pTLState);

		// Do until we are stopped or lose connection to endpoint
		while (pTLState->bRunning && pTLState->bConnected) {

			// Talk (or just sleep if not streaming.)
			bServiceIPC = talkerDoStream(pTLState);

			// TalkerDoStream() returns TRUE occasionally,
			// so that we can service our IPC at that low rate.
			if (bServiceIPC) {
				// Look for messages from endpoint.  Don't block (timeout=0)
				if (!openavbEptClntService(pTLState->endpointHandle, 0)) {
					AVB_LOGF_WARNING("Lost connection to endpoint, will retry "STREAMID_FORMAT, STREAMID_ARGS(&(((talker_data_t *)pTLState->pPvtTalkerData)->streamID)));
					pTLState->bConnected = FALSE;
					pTLState->endpointHandle = 0;
				}
			}
		}

		// Stop streaming
		talkerStopStream(pTLState);

		{
			MUTEX_CREATE_ERR();
			MUTEX_DESTROY(pTLState->statsMutex); // Destroy Stats Mutex
			MUTEX_LOG_ERR("Error destroying mutex");
		}

		// withdraw our talker registration
		if (pTLState->bConnected)
			openavbEptClntStopStream(pTLState->endpointHandle, &(((talker_data_t *)pTLState->pPvtTalkerData)->streamID));

		openavbTLRunTalkerFinish(pTLState);

		// Notify AVDECC Msg of the state change.
		openavbAvdeccMsgClntNotifyCurrentState(pTLState);
	}
	else {
		AVB_LOGF_WARNING("Failed to connect to endpoint"STREAMID_FORMAT, STREAMID_ARGS(&(((talker_data_t *)pTLState->pPvtTalkerData)->streamID)));
	}

	if (pTLState->pPvtTalkerData) {
		free(pTLState->pPvtTalkerData);
		pTLState->pPvtTalkerData = NULL;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
}

void openavbTLPauseTalker(tl_state_t *pTLState, bool bPause)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	talker_data_t *pTalkerData = pTLState->pPvtTalkerData;
	if (!pTalkerData) {
		AVB_LOG_ERROR("Invalid private talker data");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	pTLState->bPaused = bPause;
	openavbAvtpPause(pTalkerData->avtpHandle, bPause);

	// Notify AVDECC Msg of the state change.
	openavbAvdeccMsgClntNotifyCurrentState(pTLState);

	AVB_TRACE_EXIT(AVB_TRACE_TL);
}

void openavbTalkerClearStats(tl_state_t *pTLState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	talker_data_t *pTalkerData = pTLState->pPvtTalkerData;
	if (!pTalkerData) {
		AVB_LOG_ERROR("Invalid private talker data");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	LOCK_STATS();
	memset(&pTalkerData->stats, 0, sizeof(pTalkerData->stats));
	UNLOCK_STATS();

	AVB_TRACE_EXIT(AVB_TRACE_TL);
}

void openavbTalkerAddStat(tl_state_t *pTLState, tl_stat_t stat, U64 val)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	talker_data_t *pTalkerData = pTLState->pPvtTalkerData;
	if (!pTalkerData) {
		AVB_LOG_ERROR("Invalid private talker data");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return;
	}

	LOCK_STATS();
	switch (stat) {
		case TL_STAT_TX_CALLS:
			pTalkerData->stats.totalCalls += val;
			break;
		case TL_STAT_TX_FRAMES:
			pTalkerData->stats.totalFrames += val;
			break;
		case TL_STAT_TX_LATE:
			pTalkerData->stats.totalLate += val;
			break;
		case TL_STAT_TX_BYTES:
			pTalkerData->stats.totalBytes += val;
			break;
		case TL_STAT_RX_CALLS:
		case TL_STAT_RX_FRAMES:
		case TL_STAT_RX_LOST:
		case TL_STAT_RX_BYTES:
			break;
	}
	UNLOCK_STATS();

	AVB_TRACE_EXIT(AVB_TRACE_TL);
}

U64 openavbTalkerGetStat(tl_state_t *pTLState, tl_stat_t stat)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);
	U64 val = 0;

	if (!pTLState) {
		AVB_LOG_ERROR("Invalid TLState");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return 0;
	}

	talker_data_t *pTalkerData = pTLState->pPvtTalkerData;
	if (!pTalkerData) {
		AVB_LOG_ERROR("Invalid private talker data");
		AVB_TRACE_EXIT(AVB_TRACE_TL);
		return 0;
	}

	LOCK_STATS();
	switch (stat) {
		case TL_STAT_TX_CALLS:
			val = pTalkerData->stats.totalCalls;
			break;
		case TL_STAT_TX_FRAMES:
			val = pTalkerData->stats.totalFrames;
			break;
		case TL_STAT_TX_LATE:
			val = pTalkerData->stats.totalLate;
			break;
		case TL_STAT_TX_BYTES:
			val = pTalkerData->stats.totalBytes;
			break;
		case TL_STAT_RX_CALLS:
		case TL_STAT_RX_FRAMES:
		case TL_STAT_RX_LOST:
		case TL_STAT_RX_BYTES:
			break;
	}
	UNLOCK_STATS();

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return val;
}

