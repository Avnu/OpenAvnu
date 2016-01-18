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
* MODULE SUMMARY : Media Queue for data transfer betting mapping modules and interface modules.
*/

#include "openavb_platform.h"

#include <stdlib.h>
#include "openavb_types_pub.h"
#include "openavb_trace.h"
#include "openavb_mediaq.h"
#include "openavb_avtp_time_pub.h"

#include "openavb_printbuf.h"

#define	AVB_LOG_COMPONENT	"Media Queue"
#include "openavb_log.h"

static MUTEX_HANDLE(gMediaQMutex);
#define MEDIAQ_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(gMediaQMutex); MUTEX_LOG_ERR("Mutex Lock failure"); }
#define MEDIAQ_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(gMediaQMutex); MUTEX_LOG_ERR("Mutex Unlock failure"); }

// CORE_TODO : Currently this first future logic for purge the media queue and incoming items
// may be too aggressive and can result in never catching up. Therefore it is disabled for now
//#define ENABLE_FIRST_FUTURE 1

//#define DUMP_HEAD_PUSH 		1
//#define DUMP_TAIL_PULL 		1

#if  DUMP_HEAD_PUSH
FILE *pFileHeadPush = 0;
#endif
#if  DUMP_TAIL_PULL
FILE *pFileTailPull = 0;
#endif

typedef struct {
	// Maximum number of items the queue can hold.
	int itemCount;

	// The size in bytes of each item
	int itemSize;

	// Pointer to the array of items.
	media_q_item_t *pItems;

	// Next item to be filled
	int head;	

	// True if the head item is locked.
	bool headLocked; 
				
	// Next item to be pulled
	int tail;	
				
	// True is next item to be pulled is locked
	bool tailLocked;

	// set if timestamp is ever in the future
	bool firstFuture;

	// Maximum latency
	U32 maxLatencyUsec;
		
	// Determines if mutuxes will be used for Head and Tail access.
	bool threadSafeOn;
		
	// Maximum stale tail
	U32 maxStaleTailUsec;

} media_q_info_t;

static void x_openavbMediaQIncrementHead(media_q_info_t *pMediaQInfo)	
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);
	
	// Module internal function therefore not validating pMediaQInfo
	
	int startingHead = pMediaQInfo->head;
	while (++pMediaQInfo->head != startingHead)
	{
		if (pMediaQInfo->head >= pMediaQInfo->itemCount) {
			pMediaQInfo->head = 0;
		}

		// If head catches up with tail deactivate the head.
		if (pMediaQInfo->head == pMediaQInfo->tail) {
			break; // Set head to pMediaQInfo->head = -1;
		}

		if (!pMediaQInfo->pItems[pMediaQInfo->head].taken) {
			// Found item
			AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
			return;
		}
	}
	
	// Deactivate the head
	pMediaQInfo->head = -1;
	
	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
}

static void x_openavbMediaQIncrementTail(media_q_info_t *pMediaQInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);
	
	// Module internal function therefore not validating pMediaQInfo
	
	int startingTail = pMediaQInfo->tail;
	while (++pMediaQInfo->tail != startingTail)
	{
		if (pMediaQInfo->tail >= pMediaQInfo->itemCount) {
			pMediaQInfo->tail = 0;
		}

		// If tail catches up with head deactivate the tail.
		if (pMediaQInfo->tail == pMediaQInfo->head) {
			break; // Set head to pMediaQInfo->tail = -1;
		}

		if (!pMediaQInfo->pItems[pMediaQInfo->tail].taken) {
			// Found item
			AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
			return;
		}
	}
	
	// Deactivate the tail
	pMediaQInfo->tail = -1;
	
	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
}

	
// CORE_TODO: May need to add mutex protection when merging with OSAL/HAL branch.
void x_openavbMediaQPurgeStaleTail(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);

#if 0 // debug
	// Debug code to report delta from TS
	static bool init = TRUE;
	static U32 cnt = 0;
	static openavb_printbuf_t printBuf;
	if (init) {
		printBuf = openavbPrintbufNew(2000, 20);
		init = FALSE;
	}
#endif


	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);

			if (pMediaQInfo->maxStaleTailUsec > 0) {
				bool bFirst = TRUE;
				bool bMore = TRUE;
				while (bMore) {
					bMore = FALSE;
					if (pMediaQInfo->itemCount > 0) {
						if (pMediaQInfo->tail > -1) {
							media_q_item_t *pTail = &pMediaQInfo->pItems[pMediaQInfo->tail];
	
							if (pTail) {
								pMediaQInfo->tailLocked = TRUE;
								bool bPurge = FALSE;

#ifdef ENABLE_FIRST_FUTURE
								if (bFirst) {
									S32 delta = openavbAvtpTimeUsecDelta(pTail->pAvtpTime);
									S32 maxStale = (S32)(0 - pMediaQInfo->maxStaleTailUsec);

									if (delta < maxStale) {
										IF_LOG_INTERVAL(100) AVB_LOGF_INFO("Purging stale MediaQ items: delta:%dus maxStale%dus", delta, maxStale);
										
										bPurge = TRUE;
									}
									bFirst = FALSE;
								}
								else {
									// Once we have triggered a stale tail purge everything past presentation time.
									if (openavbAvtpTimeIsPast(pTail->pAvtpTime)) {
										bPurge = TRUE;
									}
								}

								if (bPurge) {
									openavbMediaQTailPull(pMediaQ);
									pTail = NULL;
									bMore = TRUE;
								}
								else {
									pMediaQInfo->tailLocked = FALSE;
									pTail = NULL;
								}
#else // ENABLE_FIRST_FUTURE
								if (bFirst) {
									S32 delta = openavbAvtpTimeUsecDelta(pTail->pAvtpTime);
									S32 maxStale = (S32)(0 - pMediaQInfo->maxStaleTailUsec);
									if(delta >= 0) {
										pMediaQInfo->firstFuture = TRUE;
									}
									else if (delta < maxStale) {
										bPurge = TRUE;
										pMediaQInfo->firstFuture = FALSE;
									}
	
									bFirst = FALSE;
								}
								else {
									// Once we have triggered a stale tail purge everything past presentation time.
									if (openavbAvtpTimeIsPast(pTail->pAvtpTime)) {
#if 0 // debug
										if (!(++cnt % 1)) {
											openavbPrintbufPrintf(printBuf, "Purge More\n");
										}
#endif
										bPurge = TRUE;
									}
									else {
										pMediaQInfo->firstFuture = TRUE;
									}
								}

								if (bPurge || (pMediaQInfo->firstFuture == FALSE)) {
									openavbMediaQTailPull(pMediaQ);
									pTail = NULL;
									bMore = TRUE;
								}
								else {
									pMediaQInfo->tailLocked = FALSE;
									pTail = NULL;
								}
#endif // ENABLE_FIRST_FUTURE								
								
							}
						}
					}
				}
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
}

	
media_q_t* openavbMediaQCreate()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ);

	media_q_t *pMediaQ = calloc(1, sizeof(media_q_t));

	if (pMediaQ) {
		pMediaQ->pPvtMediaQInfo = calloc(1, sizeof(media_q_info_t));
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			pMediaQInfo->itemCount = 0;
			pMediaQInfo->itemSize = 0;
			pMediaQInfo->head = 0;
			pMediaQInfo->headLocked = FALSE;
			pMediaQInfo->tail = -1;
			pMediaQInfo->tailLocked = FALSE;
			pMediaQInfo->firstFuture = TRUE;
			pMediaQInfo->maxLatencyUsec = 0;
			pMediaQInfo->threadSafeOn = FALSE;
			pMediaQInfo->maxStaleTailUsec = MICROSECONDS_PER_SECOND;
		}
		else {
			openavbMediaQDelete(pMediaQ);
			pMediaQ = NULL;
		}
	}
		
	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
	return pMediaQ;
}

void openavbMediaQThreadSafeOn(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ);

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			pMediaQInfo->threadSafeOn = TRUE;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
}



bool openavbMediaQSetSize(media_q_t *pMediaQ, int itemCount, int itemSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ);

	if (pMediaQ) {
		if (itemCount < 1 || itemSize < 1) {
			AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
			return FALSE;
		}

		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);

			// Don't want to re-allocate new memory each time
			if (!pMediaQInfo->pItems)
			{
				pMediaQInfo->pItems = calloc(itemCount, sizeof(media_q_item_t));
				if (pMediaQInfo->pItems) {
					pMediaQInfo->itemCount = itemCount;
					pMediaQInfo->itemSize = itemSize;

					int i1;
					for (i1 = 0; i1 < itemCount; i1++) {
						pMediaQInfo->pItems[i1].pAvtpTime = openavbAvtpTimeCreate(pMediaQInfo->maxLatencyUsec);
						pMediaQInfo->pItems[i1].pPubData = calloc(1, itemSize);
						pMediaQInfo->pItems[i1].dataLen = 0;
						pMediaQInfo->pItems[i1].itemSize = itemSize;
						if (!pMediaQInfo->pItems[i1].pPubData) {
							AVB_LOG_ERROR("Out of memory creating MediaQ item");
							AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
							return FALSE;
						}
					}
				}
				else {
					AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
					return FALSE;
				}
			} else if (itemCount != pMediaQInfo->itemCount || itemSize != pMediaQInfo->itemSize){
				AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
				return FALSE;
			}
		}
		else {
			AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
			return FALSE;
		}
	}
	else {
		AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
	return TRUE;
}

bool openavbMediaQAllocItemMapData(media_q_t *pMediaQ, int itemPubMapSize, int itemPvtMapSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ);

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->pItems) {
				int i1;
				for (i1 = 0; i1 < pMediaQInfo->itemCount; i1++) {
					if (itemPubMapSize) {
						if (!pMediaQInfo->pItems[i1].pPubMapData) {
							pMediaQInfo->pItems[i1].pPubMapData = calloc(1, itemPubMapSize);
							if (!pMediaQInfo->pItems[i1].pPubMapData) {
								AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
								return FALSE;
							}
						}
						else {
							AVB_LOG_ERROR("Attemping to reallocate public map data");
							AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
							return FALSE;
						}
					}
					if (itemPvtMapSize) {
						if (!pMediaQInfo->pItems[i1].pPvtMapData) {
							pMediaQInfo->pItems[i1].pPvtMapData = calloc(1, itemPvtMapSize);
							if (!pMediaQInfo->pItems[i1].pPvtMapData) {
								AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
								return FALSE;
							}
						}
						else {
							AVB_LOG_ERROR("Attemping to reallocate private map data");
							AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
							return FALSE;
						}
					}
				}

				AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
				return TRUE;
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
	return FALSE;
}


bool openavbMediaQAllocItemIntfData(media_q_t *pMediaQ, int itemIntfSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ);

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->pItems) {
				int i1;
				for (i1 = 0; i1 < pMediaQInfo->itemCount; i1++) {
					if (!pMediaQInfo->pItems[i1].pPvtIntfData) {
						pMediaQInfo->pItems[i1].pPvtIntfData = calloc(1, itemIntfSize);
						if (!pMediaQInfo->pItems[i1].pPvtIntfData) {
							AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
							return FALSE;
						}
					}
					else {
						AVB_LOG_ERROR("Attemping to reallocate private interface data");
						AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
						return FALSE;
					}
				}
				AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
				return TRUE;
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
	return FALSE;
}

bool openavbMediaQDelete(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ);

	if (pMediaQ) {

#if DUMP_HEAD_PUSH
		if (pFileHeadPush) {
			fflush(pFileHeadPush);
			fclose(pFileHeadPush);
			pFileHeadPush = NULL;
		}
#endif	

#if DUMP_TAIL_PULL
		if (pFileTailPull) {
			fflush(pFileTailPull);
			fclose(pFileTailPull);
			pFileTailPull = NULL;
		}
#endif	
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->pItems) {
				int i1;
				for (i1 = 0; i1 < pMediaQInfo->itemCount; i1++) {
					
					if (pMediaQInfo->pItems[i1].taken) {
						AVB_LOG_ERROR("Deleting MediaQ with an item TAKEN. The item will be orphaned.");
					}
					else {
						openavbAvtpTimeDelete(pMediaQInfo->pItems[i1].pAvtpTime);
						if (pMediaQInfo->pItems[i1].pPubData) {
							free(pMediaQInfo->pItems[i1].pPubData);
							pMediaQInfo->pItems[i1].pPubData = NULL;
						}
						if (pMediaQInfo->pItems[i1].pPubMapData) {
							free(pMediaQInfo->pItems[i1].pPubMapData);
							pMediaQInfo->pItems[i1].pPubMapData = NULL;
						}
						if (pMediaQInfo->pItems[i1].pPvtMapData) {
							free(pMediaQInfo->pItems[i1].pPvtMapData);
							pMediaQInfo->pItems[i1].pPvtMapData = NULL;
						}
						if (pMediaQInfo->pItems[i1].pPvtIntfData) {
							free(pMediaQInfo->pItems[i1].pPvtIntfData);
							pMediaQInfo->pItems[i1].pPvtIntfData = NULL;
						}
					}
				}
				free(pMediaQInfo->pItems);
				pMediaQInfo->pItems = NULL;
			}
			free(pMediaQ->pPvtMediaQInfo);
			pMediaQ->pPvtMediaQInfo = NULL;

			if (pMediaQ->pPubMapInfo) {
				free(pMediaQ->pPubMapInfo);
				pMediaQ->pPubMapInfo = NULL;
			}

			if (pMediaQ->pPvtMapInfo) {
				free(pMediaQ->pPvtMapInfo);
				pMediaQ->pPvtMapInfo = NULL;
			}

			if (pMediaQ->pPvtIntfInfo) {
				free(pMediaQ->pPvtIntfInfo);
				pMediaQ->pPvtIntfInfo = NULL;
			}
		}

		if (pMediaQ->pMediaQDataFormat) {
			free(pMediaQ->pMediaQDataFormat);
			pMediaQ->pMediaQDataFormat = NULL;
		}

		free(pMediaQ);
		pMediaQ = NULL;
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
	return FALSE;
}

void openavbMediaQSetMaxLatency(media_q_t *pMediaQ, U32 maxLatencyUsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ);

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			pMediaQInfo->maxLatencyUsec = maxLatencyUsec;
		}
	}
		
	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
}

void openavbMediaQSetMaxStaleTail(media_q_t *pMediaQ, U32 maxStaleTailUsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ);

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			pMediaQInfo->maxStaleTailUsec = maxStaleTailUsec;
		}
	}
		
	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ);
}

media_q_item_t *openavbMediaQHeadLock(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->threadSafeOn) {
				MEDIAQ_LOCK();
			}
			if (pMediaQInfo->itemCount > 0) {
				if (pMediaQInfo->head > -1) {
					pMediaQInfo->headLocked = TRUE;
					AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
					// Mutex (LOCK()) if acquired stays locked
					return &pMediaQInfo->pItems[pMediaQInfo->head];
				}
			}
			if (pMediaQInfo->threadSafeOn) {
				MEDIAQ_UNLOCK();
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
	return NULL;
}

void openavbMediaQHeadUnlock(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->itemCount > 0) {
				if (pMediaQInfo->head > -1) {
					pMediaQInfo->headLocked = FALSE;
					if (pMediaQInfo->threadSafeOn) {
						MEDIAQ_UNLOCK();
					}
				}
			}
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
}

bool openavbMediaQHeadPush(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->itemCount > 0) {
				if (pMediaQInfo->head > -1) {
					media_q_item_t *pHead = &pMediaQInfo->pItems[pMediaQInfo->head];

#if DUMP_HEAD_PUSH
					media_q_item_t *pMediaQItem = &pMediaQInfo->pItems[pMediaQInfo->head];
					if (!pFileHeadPush) {
						char filename[128];
						sprintf(filename, "headpush_%5.5d.dat", GET_PID());
						pFileHeadPush = fopen(filename, "wb");
					}
					if (pFileHeadPush) {
						size_t result = fwrite(pMediaQItem->pPubData, 1, pMediaQItem->dataLen, pFileHeadPush);
						if (result != pMediaQItem->dataLen)
							printf("ERROR writing head push log");
					}
#endif

					// If tail not set, set it now
					if (pMediaQInfo->tail == -1) {
						pMediaQInfo->tail = pMediaQInfo->head;
					}
					
					pHead->readIdx = 0;		// Reset read index

					x_openavbMediaQIncrementHead(pMediaQInfo);

					pMediaQInfo->headLocked = FALSE;
					if (pMediaQInfo->threadSafeOn) {
						MEDIAQ_UNLOCK();
					}

					AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
					return TRUE;
				}
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
	return FALSE;
}

media_q_item_t* openavbMediaQTailLock(media_q_t *pMediaQ, bool ignoreTimestamp)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);

	if (!ignoreTimestamp) {
		x_openavbMediaQPurgeStaleTail(pMediaQ);
	}

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->threadSafeOn) {
				MEDIAQ_LOCK();
			}
			if (pMediaQInfo->itemCount > 0) {
				if (pMediaQInfo->tail > -1) {
					media_q_item_t *pTail = &pMediaQInfo->pItems[pMediaQInfo->tail];

					// Check if tail item is ready.
					if (!ignoreTimestamp) {
						if (!openavbAvtpTimeIsPast(pTail->pAvtpTime)) {
							if (pMediaQInfo->threadSafeOn) {
								MEDIAQ_UNLOCK();
							}
							return NULL;
						}
					}

					pMediaQInfo->tailLocked = TRUE;
					AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
					// Mutex (LOCK()) if acquired stays locked
					return pTail;
				}
			}
			if (pMediaQInfo->threadSafeOn) {
				MEDIAQ_UNLOCK();
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
	return NULL;
}

void openavbMediaQTailUnlock(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->itemCount > 0) {
				if (pMediaQInfo->tail > -1) {
					pMediaQInfo->tailLocked = FALSE;
					if (pMediaQInfo->threadSafeOn) {
						MEDIAQ_UNLOCK();
					}
				}
			}
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
}

bool openavbMediaQTailPull(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->itemCount > 0) {
				if (pMediaQInfo->tail > -1) {
					media_q_item_t *pTail = &pMediaQInfo->pItems[pMediaQInfo->tail];

#if DUMP_TAIL_PULL
					media_q_item_t *pMediaQItem = &pMediaQInfo->pItems[pMediaQInfo->tail];
					if (!pFileTailPull) {
						char filename[128];
						sprintf(filename, "tailpull_%5.5d.dat", GET_PID());
						pFileTailPull = fopen(filename, "wb");
					}
					if (pFileTailPull) {
						size_t result = fwrite(pMediaQItem->pPubData, 1, pMediaQItem->dataLen, pFileTailPull);
						if (result != pMediaQItem->dataLen)
							printf("ERROR writing tail pull log");
					}
#endif

					// If head not set, set it now
					if (pMediaQInfo->head == -1) {
						pMediaQInfo->head = pMediaQInfo->tail;
					}

					pTail->readIdx = 0;		// Reset read index
					pTail->dataLen = 0;		// Clears out the data

					x_openavbMediaQIncrementTail(pMediaQInfo);

					pMediaQInfo->tailLocked = FALSE;
					if (pMediaQInfo->threadSafeOn) {
						MEDIAQ_UNLOCK();
					}
					
					AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
					return TRUE;
				}
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
	return FALSE;
}

bool openavbMediaQTailItemTake(media_q_t *pMediaQ, media_q_item_t* pItem)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);

	if (pMediaQ && pItem) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->itemCount > 0) {
				if (pMediaQInfo->tail > -1) {

					x_openavbMediaQIncrementTail(pMediaQInfo);

					pItem->taken = TRUE;
					pMediaQInfo->tailLocked = FALSE;
					if (pMediaQInfo->threadSafeOn) {
						MEDIAQ_UNLOCK();
					}
					
					AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
					return TRUE;
				}
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
	return FALSE;
}

bool openavbMediaQTailItemGive(media_q_t *pMediaQ, media_q_item_t* pItem)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);

	if (pItem) {
		pItem->taken = FALSE;
		pItem->readIdx = 0;		// Reset read index
		pItem->dataLen = 0;		// Clears out the data

		if (pMediaQ) {
			if (pMediaQ->pPvtMediaQInfo) {
				media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
				if (pMediaQInfo->threadSafeOn) {
					MEDIAQ_LOCK();
				}
				if (pMediaQInfo->itemCount > 0) {
					if (pMediaQInfo->head == -1) {
						// Transition from full mediaq to an available item slot. Find this item that was just give back
						int i1;
						for (i1 = 0; i1 < pMediaQInfo->itemCount; i1++)
						{
							if (!pMediaQInfo->pItems[i1].taken) {
								pMediaQInfo->head = i1;
								break;
							}
						}
					}
				}
				if (pMediaQInfo->threadSafeOn) {
					MEDIAQ_UNLOCK();
				}
			}
		}
		AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
		return TRUE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
	return FALSE;
}


bool openavbMediaQUsecTillTail(media_q_t *pMediaQ, U32 *pUsecTill)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);

	if (pMediaQ && pUsecTill) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->itemCount > 0) {
				if (pMediaQInfo->tail > -1) {
					media_q_item_t *pTail = &pMediaQInfo->pItems[pMediaQInfo->tail];

					U32 usecTill;
					
					if (openavbAvtpTimeUsecTill(pTail->pAvtpTime, &usecTill)) {
						*pUsecTill = usecTill;
						return TRUE;
					}
				}
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
	return FALSE;
}

bool openavbMediaQIsAvailableBytes(media_q_t *pMediaQ, U32 bytes, bool ignoreTimestamp)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);

	if (!ignoreTimestamp) {
		x_openavbMediaQPurgeStaleTail(pMediaQ);
	}

	int byteCnt = 0;

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->itemCount > 0) {
				if (pMediaQInfo->tail > -1) {
					// Check if tail item is ready.
					int tailIdx = pMediaQInfo->tail;
					int endIdx = pMediaQInfo->head > -1 ? pMediaQInfo->head : pMediaQInfo->tail;
					if (ignoreTimestamp) {
						while (1) {
							media_q_item_t *pTail = &pMediaQInfo->pItems[tailIdx];
							
							if (!pTail->taken) {
								byteCnt += pTail->dataLen - pTail->readIdx;
	
								if (byteCnt >= bytes) {
									// Met the available byte count
									AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
									return TRUE;
								}
							}

							tailIdx++;
							if (tailIdx >= pMediaQInfo->itemCount)
								tailIdx = 0;
							if (tailIdx == endIdx)
								break;
						}
					}
					else {
						U64 nSecTime;
						CLOCK_GETTIME64(OPENAVB_CLOCK_WALLTIME, &nSecTime);
						while (1) {
							media_q_item_t *pTail = &pMediaQInfo->pItems[tailIdx];
							assert(pTail);

							if (!pTail->taken) {
								if (!openavbAvtpTimeIsPastTime(pTail->pAvtpTime, nSecTime))
									break;

								byteCnt += pTail->dataLen - pTail->readIdx;
	
								if (byteCnt >= bytes) {
									// Met the available byte count
									AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
									return TRUE;
								}
							}

							tailIdx++;
							if (tailIdx >= pMediaQInfo->itemCount)
								tailIdx = 0;
							if (tailIdx == endIdx)
								break;
						}
					}
				}
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
	return FALSE;
}

U32 openavbMediaQCountItems(media_q_t *pMediaQ, bool ignoreTimestamp)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);

	if (!ignoreTimestamp) {
		x_openavbMediaQPurgeStaleTail(pMediaQ);
	}

	U32 itemCnt = 0;

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->itemCount > 0) {
				if (pMediaQInfo->tail > -1) {
					// Check if tail item is ready.
					int tailIdx = pMediaQInfo->tail;
					if (ignoreTimestamp) {
						while (1) {
							media_q_item_t *pTail = &pMediaQInfo->pItems[tailIdx];

							if (!pTail->taken) {
								itemCnt++;
							}

							tailIdx++;
							if (tailIdx >= pMediaQInfo->itemCount)
								tailIdx = 0;
							if (tailIdx == pMediaQInfo->head)
								break;
							if (itemCnt >= pMediaQInfo->itemCount)								
								break;
						}
					}
					else {
						U64 nSecTime;
						CLOCK_GETTIME64(OPENAVB_CLOCK_WALLTIME, &nSecTime);
						while (1) {
							media_q_item_t *pTail = &pMediaQInfo->pItems[tailIdx];

							if (!pTail->taken) {
								if (!openavbAvtpTimeIsPastTime(pTail->pAvtpTime, nSecTime))
									break;

								itemCnt++;
							}

							tailIdx++;
							if (tailIdx >= pMediaQInfo->itemCount)
								tailIdx = 0;
							if (tailIdx == pMediaQInfo->head)
								break;
							if (itemCnt >= pMediaQInfo->itemCount)								
								break;
						}
					}
				}
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
	return itemCnt;
}

bool openavbMediaQAnyReadyItems(media_q_t *pMediaQ, bool ignoreTimestamp)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MEDIAQ_DETAIL);

	if (!ignoreTimestamp) {
		x_openavbMediaQPurgeStaleTail(pMediaQ);
	}

	if (pMediaQ) {
		if (pMediaQ->pPvtMediaQInfo) {
			media_q_info_t *pMediaQInfo = (media_q_info_t *)(pMediaQ->pPvtMediaQInfo);
			if (pMediaQInfo->itemCount > 0) {
				if (pMediaQInfo->tail > -1) {
					// Check if tail item is ready.
					int tailIdx = pMediaQInfo->tail;
					if (ignoreTimestamp) {
						media_q_item_t *pTail = &pMediaQInfo->pItems[tailIdx];

						if (!pTail->taken) {
							AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
							return TRUE;
						}
					}
					else {
						U64 nSecTime;
						CLOCK_GETTIME64(OPENAVB_CLOCK_WALLTIME, &nSecTime);
						media_q_item_t *pTail = &pMediaQInfo->pItems[tailIdx];
						assert(pTail);
					
						if (!pTail->taken) {
							if (openavbAvtpTimeIsPastTime(pTail->pAvtpTime, nSecTime)) {
								AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
								return TRUE;
							}
						}
					}
				}
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MEDIAQ_DETAIL);
	return FALSE;
}

