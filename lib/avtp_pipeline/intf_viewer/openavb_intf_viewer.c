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
* MODULE SUMMARY : Viewer interface module.
* 
* This interface module is only a listener and is used to simply display
* contents recieved in a number of different formats. Additionally it is
* mapping type aware and can display header values for different mappings
*/

#include <stdlib.h>
#include <string.h>
#include "openavb_platform_pub.h"
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_intf_pub.h"

#define	AVB_LOG_COMPONENT	"Viewer Interface"
#include "openavb_log_pub.h" 

typedef enum {
	VIEWER_MODE_DETAIL = 0,
	VIEWER_MODE_MAPPING_AWARE = 1,
	VIEWER_MODE_AVTP_TIMESTAMP = 2,
	VIEWER_MODE_LATENCY = 3,
	VIEWER_MODE_SELECTIVE_TIMESTAMP = 4,
	VIEWER_MODE_LATE = 5,
	VIEWER_MODE_GAP = 6,
} viewer_mode_t;  

typedef struct {
	/////////////
	// Config data
	/////////////
	viewer_mode_t viewType;

	// Frequency of output
	U32 viewInterval;

	// Offest into the raw frame to output
	U32 rawOffset;

	// Length of the raw frame to output
	U32 rawLength;

	// Ignore timestamp at listener.
	bool ignoreTimestamp;

	/////////////
	// Variable data
	/////////////
	U32 servicedCount;

	S64 accumLateNS;

	S32 maxLateNS;

	U64 accumGapNS;

	U32 maxGapNS;
	
	U64 prevNowTime;

	S64 accumAvtpDeltaNS;

	S32 maxAvtpDeltaNS;
	
	U64 prevAvtpTimestampTime;

	U32 skipCountdown;
	
	float jitter;
	
	S32 avgForJitter;

} pvt_data_t;

// Each configuration name value pair for this mapping will result in this callback being called.
void openavbIntfViewerCfgCB(media_q_t *pMediaQ, const char *name, const char *value) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		char *pEnd;
		long tmp;

		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		// intf_nv_view_type: Type of viewing output. 0 = raw, 1 = mapping aware, 2 = timestamps only, 3 = latency output, 4 = Selective timestamp error reporting
		if (strcmp(name, "intf_nv_view_type") == 0) {
			pPvtData->viewType = (viewer_mode_t)strtol(value, &pEnd, 10);
		}

		else if (strcmp(name, "intf_nv_view_interval") == 0) {
			pPvtData->viewInterval = strtol(value, &pEnd, 10);
			if (pPvtData->viewInterval == 0) {
				pPvtData->viewInterval = 1000;			  
			}
		}

		else if (strcmp(name, "intf_nv_raw_offset") == 0) {
			pPvtData->rawOffset = strtol(value, &pEnd, 10);
		}

		else if (strcmp(name, "intf_nv_raw_length") == 0) {
			pPvtData->rawLength = strtol(value, &pEnd, 10);
			if (pPvtData->rawLength < 1)
				pPvtData->rawLength = 1000;
		}

		else if (strcmp(name, "intf_nv_ignore_timestamp") == 0) {
			tmp = strtol(value, &pEnd, 10);
			if (*pEnd == '\0' && tmp == 1) {
				pPvtData->ignoreTimestamp = (tmp == 1);
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfViewerGenInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// No talker functionality in this interface
void openavbIntfViewerTxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// No talker functionality in this interface
bool openavbIntfViewerTxCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);
	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return FALSE;
}

// A call to this callback indicates that this interface module will be
// a listener. Any listener initialization can be done in this function.
void openavbIntfViewerRxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback is called when acting as a listener.
bool openavbIntfViewerRxCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return FALSE;
		}

		media_q_item_t *pMediaQItem = openavbMediaQTailLock(pMediaQ, pPvtData->ignoreTimestamp);
		if (pMediaQItem) {

		  	// The skip countdown allow the viewer modes to set a number of packets to ignore 
		  	// after logging to reduce or eliminate the logging from affecting the stats.
			if (pPvtData->skipCountdown)
				pPvtData->skipCountdown--;
		  
			if (pMediaQItem->dataLen && !pPvtData->skipCountdown) {
				pPvtData->servicedCount++;

				if (pPvtData->viewType == VIEWER_MODE_DETAIL) {
					U32 avtpTimestamp;
					U64 avtpTimestampTime;
					bool avtpTimestampValid;
					U32 nowTimestamp;
					U64 nowTimestampTime;
					bool nowTimestampValid;
					U64 nowTime;
					S32 lateNS = 0;
					U64 gapNS = 0;
					
					avtpTimestamp = openavbAvtpTimeGetAvtpTimestamp(pMediaQItem->pAvtpTime);
					avtpTimestampTime = openavbAvtpTimeGetAvtpTimeNS(pMediaQItem->pAvtpTime);
					avtpTimestampValid = openavbAvtpTimeTimestampIsValid(pMediaQItem->pAvtpTime);
					
					openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
					nowTimestamp = openavbAvtpTimeGetAvtpTimestamp(pMediaQItem->pAvtpTime);
					nowTimestampTime = openavbAvtpTimeGetAvtpTimeNS(pMediaQItem->pAvtpTime);
					nowTimestampValid = openavbAvtpTimeTimestampIsValid(pMediaQItem->pAvtpTime);
					
					CLOCK_GETTIME64(OPENAVB_CLOCK_REALTIME, &nowTime);

					if (avtpTimestampValid && nowTimestampValid) {
						lateNS = nowTimestampTime - avtpTimestampTime;
						if (lateNS > pPvtData->maxLateNS) {
							pPvtData->maxLateNS = lateNS;
						}
						pPvtData->accumLateNS += lateNS;
						
						if (pPvtData->servicedCount > 1) {
							gapNS = nowTime - pPvtData->prevNowTime;
							if (gapNS > pPvtData->maxGapNS) {
								pPvtData->maxGapNS = gapNS;
							}
							pPvtData->accumGapNS += gapNS;
						}
						pPvtData->prevNowTime = nowTime;
						
						if ((pPvtData->servicedCount % pPvtData->viewInterval) == 0) {
							S32 lateAvg = pPvtData->accumLateNS / pPvtData->servicedCount;
							S32 gapAvg = pPvtData->accumGapNS / (pPvtData->servicedCount - 1);
							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, LOG_RT_END, "****************************", LOG_RT_DATATYPE_CONST_STR, NULL);
							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, LOG_RT_END, "Packets: %u", LOG_RT_DATATYPE_U32, &pPvtData->servicedCount);
							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, LOG_RT_END, "AVTP Timestamp: %u NS", LOG_RT_DATATYPE_U32, &avtpTimestamp);
							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, LOG_RT_END, "Now Timestamp:  %u NS", LOG_RT_DATATYPE_U32, &nowTimestamp);
							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, LOG_RT_END, "Late: %d NS", LOG_RT_DATATYPE_S32, &lateNS);
							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, LOG_RT_END, "Late Avg: %d NS", LOG_RT_DATATYPE_S32, &lateAvg);
							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, LOG_RT_END, "Late Max: %d NS", LOG_RT_DATATYPE_S32, &pPvtData->maxLateNS);
							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, LOG_RT_END, "Gap: %u NS", LOG_RT_DATATYPE_U32, &gapNS);
							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, LOG_RT_END, "Gap Avg: %u NS", LOG_RT_DATATYPE_U32, &gapAvg);
							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, LOG_RT_END, "Gap Max: %u NS", LOG_RT_DATATYPE_U32, &pPvtData->maxGapNS);
							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, LOG_RT_END, "Data length:  %u", LOG_RT_DATATYPE_U32, &pMediaQItem->dataLen);
							
							pPvtData->accumLateNS = 0;
							pPvtData->maxLateNS = 0;
							pPvtData->accumGapNS = 0;
							pPvtData->maxGapNS = 0;
							pPvtData->prevNowTime = 0;							
							pPvtData->servicedCount = 0;
							pPvtData->skipCountdown = 10;
						}
					}
				}
				
				else if (pPvtData->viewType == VIEWER_MODE_MAPPING_AWARE) {
				}

				else if (pPvtData->viewType == VIEWER_MODE_AVTP_TIMESTAMP) {
					U64 avtpTimestampTime;
					bool avtpTimestampValid;
					S32 deltaNS = 0;
					
					avtpTimestampTime = openavbAvtpTimeGetAvtpTimeNS(pMediaQItem->pAvtpTime);
					avtpTimestampValid = openavbAvtpTimeTimestampIsValid(pMediaQItem->pAvtpTime);
					
					if (avtpTimestampValid) {
						if (pPvtData->servicedCount > 1) {
							deltaNS = avtpTimestampTime - pPvtData->prevAvtpTimestampTime;
							if (deltaNS > pPvtData->maxAvtpDeltaNS) {
								pPvtData->maxAvtpDeltaNS = deltaNS;
							}
							pPvtData->accumAvtpDeltaNS += deltaNS;

							if (pPvtData->avgForJitter != 0) {
								S32 deltaJitter = pPvtData->avgForJitter - deltaNS;
								if (deltaJitter < 0) deltaJitter = -deltaJitter;
								pPvtData->jitter += (1.0/16.0) * ((float)deltaJitter - pPvtData->jitter);
							}
						}
						pPvtData->prevAvtpTimestampTime = avtpTimestampTime;

						if ((pPvtData->servicedCount % pPvtData->viewInterval) == 0) {
							S32 deltaAvg = pPvtData->accumAvtpDeltaNS / (pPvtData->servicedCount - 1);
							U32 jitter = (U32)(pPvtData->jitter);
							
							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, FALSE, "AVTP Timestamp Delta: %d NS  ", LOG_RT_DATATYPE_S32, &deltaNS);
							AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "AVTP Timestamp Delta Avg: %d NS  ", LOG_RT_DATATYPE_S32, &deltaAvg);
							AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "AVTP Timestamp Delta Max: %d NS  ", LOG_RT_DATATYPE_S32, &pPvtData->maxAvtpDeltaNS);
							AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, LOG_RT_END, "Jitter: %d", LOG_RT_DATATYPE_S32, &jitter);
							
							pPvtData->accumAvtpDeltaNS = 0;
							pPvtData->maxAvtpDeltaNS = 0;
							pPvtData->servicedCount = 0;
							pPvtData->prevAvtpTimestampTime = 0;
							pPvtData->skipCountdown = 10;
							pPvtData->jitter = 0.0;
							pPvtData->avgForJitter = deltaAvg;
						}
					}
				}

				else if (pPvtData->viewType == VIEWER_MODE_LATENCY) {
					U64 avtpTimestampTime;
					bool avtpTimestampValid;
					U64 nowTimestampTime;
					bool nowTimestampValid;
					S32 lateNS = 0;
					
					avtpTimestampTime = openavbAvtpTimeGetAvtpTimeNS(pMediaQItem->pAvtpTime);
					avtpTimestampValid = openavbAvtpTimeTimestampIsValid(pMediaQItem->pAvtpTime);
					
					openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
					nowTimestampTime = openavbAvtpTimeGetAvtpTimeNS(pMediaQItem->pAvtpTime);
					nowTimestampValid = openavbAvtpTimeTimestampIsValid(pMediaQItem->pAvtpTime);

					if (avtpTimestampValid && nowTimestampValid) {
						lateNS = nowTimestampTime - avtpTimestampTime;
						if (lateNS > pPvtData->maxLateNS) {
							pPvtData->maxLateNS = lateNS;
						}
						pPvtData->accumLateNS += lateNS;
						
						if (pPvtData->avgForJitter != 0) {
							S32 lateJitter = pPvtData->avgForJitter - lateNS;
							if (lateJitter < 0) lateJitter = -lateJitter;
							pPvtData->jitter += (1.0/16.0) * ((float)lateJitter - pPvtData->jitter);
						}
						
						if ((pPvtData->servicedCount % pPvtData->viewInterval) == 0) {
							S32 lateAvg = pPvtData->accumLateNS / pPvtData->servicedCount;
							U32 jitter = (U32)(pPvtData->jitter);
							
							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, FALSE, "Latency: %d NS  ", LOG_RT_DATATYPE_S32, &lateNS);
							AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "Latency Avg: %d NS  ", LOG_RT_DATATYPE_S32, &lateAvg);
							AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "Latency Max: %d NS  ", LOG_RT_DATATYPE_S32, &pPvtData->maxLateNS);
							AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, LOG_RT_END, "Jitter: %d", LOG_RT_DATATYPE_S32, &jitter);
							
							pPvtData->accumLateNS = 0;
							pPvtData->maxLateNS = 0;
							pPvtData->servicedCount = 0;
							pPvtData->skipCountdown = 10;
							pPvtData->jitter = 0.0;
							pPvtData->avgForJitter = lateAvg;
						}
					}
				}
			  
				else if (pPvtData->viewType == VIEWER_MODE_SELECTIVE_TIMESTAMP) {
				}				  

				else if (pPvtData->viewType == VIEWER_MODE_LATE) {
					U64 avtpTimestampTime;
					bool avtpTimestampValid;
					U64 nowTimestampTime;
					bool nowTimestampValid;
					S32 lateNS = 0;
					
					avtpTimestampTime = openavbAvtpTimeGetAvtpTimeNS(pMediaQItem->pAvtpTime);
					avtpTimestampValid = openavbAvtpTimeTimestampIsValid(pMediaQItem->pAvtpTime);
					
					openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
					nowTimestampTime = openavbAvtpTimeGetAvtpTimeNS(pMediaQItem->pAvtpTime);
					nowTimestampValid = openavbAvtpTimeTimestampIsValid(pMediaQItem->pAvtpTime);

					if (avtpTimestampValid && nowTimestampValid) {
						lateNS = nowTimestampTime - avtpTimestampTime;
						if (lateNS > pPvtData->maxLateNS) {
							pPvtData->maxLateNS = lateNS;
						}
						pPvtData->accumLateNS += lateNS;
						
						if (pPvtData->avgForJitter != 0) {
							S32 lateJitter = pPvtData->avgForJitter - lateNS;
							if (lateJitter < 0) lateJitter = -lateJitter;
							pPvtData->jitter += (1.0/16.0) * ((float)lateJitter - pPvtData->jitter);
						}
						
						if ((pPvtData->servicedCount % pPvtData->viewInterval) == 0) {
							S32 lateAvg = pPvtData->accumLateNS / pPvtData->servicedCount;
							U32 jitter = (U32)(pPvtData->jitter);

							AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, FALSE, "Late: %d NS  ", LOG_RT_DATATYPE_S32, &lateNS);
							AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "Late Avg: %d NS  ", LOG_RT_DATATYPE_S32, &lateAvg);
							AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "Late Max: %d NS  ", LOG_RT_DATATYPE_S32, &pPvtData->maxLateNS);
							AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, LOG_RT_END, "Jitter: %d", LOG_RT_DATATYPE_S32, &jitter);
							
							pPvtData->accumLateNS = 0;
							pPvtData->maxLateNS = 0;
							pPvtData->servicedCount = 0;
							pPvtData->skipCountdown = 10;
							pPvtData->jitter = 0.0;
							pPvtData->avgForJitter = lateAvg;
						}
					}
				}
				
				else if (pPvtData->viewType == VIEWER_MODE_GAP) {
					U64 nowTime;
					U64 gapNS = 0;
					
					CLOCK_GETTIME64(OPENAVB_CLOCK_REALTIME, &nowTime);

					if (pPvtData->servicedCount > 1) {
						gapNS = nowTime - pPvtData->prevNowTime;
						if (gapNS > pPvtData->maxGapNS) {
							pPvtData->maxGapNS = gapNS;
						}
						pPvtData->accumGapNS += gapNS;
						
						if (pPvtData->avgForJitter != 0) {
							S32 gapJitter = pPvtData->avgForJitter - gapNS;
							if (gapJitter < 0) gapJitter = -gapJitter;
							pPvtData->jitter += (1.0/16.0) * ((float)gapJitter - pPvtData->jitter);
						}
					}
					pPvtData->prevNowTime = nowTime;
						
					if ((pPvtData->servicedCount % pPvtData->viewInterval) == 0) {
						S32 gapAvg = pPvtData->accumGapNS / (pPvtData->servicedCount - 1);
						U32 jitter = (U32)(pPvtData->jitter);
						
						AVB_LOGRT_INFO(LOG_RT_BEGIN, LOG_RT_ITEM, FALSE, "Gap: %d NS  ", LOG_RT_DATATYPE_S32, &gapNS);
						AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "Gap Avg: %d NS  ", LOG_RT_DATATYPE_S32, &gapAvg);
						AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, FALSE, "Gap Max: %d NS  ", LOG_RT_DATATYPE_S32, &pPvtData->maxGapNS);
						AVB_LOGRT_INFO(FALSE, LOG_RT_ITEM, LOG_RT_END, "Jitter: %d", LOG_RT_DATATYPE_S32, &jitter);
					  
						pPvtData->accumGapNS = 0;
						pPvtData->maxGapNS = 0;
						pPvtData->prevNowTime = 0;							
						pPvtData->servicedCount = 0;
						pPvtData->skipCountdown = 10;
						pPvtData->jitter = 0.0;
						pPvtData->avgForJitter = gapAvg;
					}
				}
				
			}
			openavbMediaQTailPull(pMediaQ);
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return TRUE;
}


// This callback will be called when the interface needs to be closed. All shutdown should 
// occur in this function.
void openavbIntfViewerEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfViewerGenEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Main initialization entry point into the interface module
extern DLL_EXPORT bool openavbIntfViewerInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pMediaQ->pPvtIntfInfo = calloc(1, sizeof(pvt_data_t));		// Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pPvtIntfInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for AVTP interface module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;

		pIntfCB->intf_cfg_cb = openavbIntfViewerCfgCB;
		pIntfCB->intf_gen_init_cb = openavbIntfViewerGenInitCB;
		pIntfCB->intf_tx_init_cb = openavbIntfViewerTxInitCB;
		pIntfCB->intf_tx_cb = openavbIntfViewerTxCB;
		pIntfCB->intf_rx_init_cb = openavbIntfViewerRxInitCB;
		pIntfCB->intf_rx_cb = openavbIntfViewerRxCB;
		pIntfCB->intf_end_cb = openavbIntfViewerEndCB;
		pIntfCB->intf_gen_end_cb = openavbIntfViewerGenEndCB;

		pPvtData->viewType = VIEWER_MODE_DETAIL;
		pPvtData->viewInterval = 1000;
		pPvtData->rawOffset = 0;
		pPvtData->rawLength = 20;
		pPvtData->ignoreTimestamp = FALSE;
		pPvtData->skipCountdown = 0;
		pPvtData->jitter = 0.0;
		pPvtData->avgForJitter = 0;
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return TRUE;
}
