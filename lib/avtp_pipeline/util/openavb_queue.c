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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "openavb_debug.h"
#include "openavb_queue.h"

OPENAVB_CODE_MODULE_PRI

struct openavb_queue_elem {
	bool setFlg;
	void *data;
};

struct openavb_queue {
	// Size of each element
	U32 elemSize;
	
	// Number of queue element slots
	U32 queueSize;
	
	// Next element to be filled
	int head;

	// Next element to be pulled
	int tail;
	
	openavb_queue_elem_t elemArray;
};

openavb_queue_t openavbQueueNewQueue(U32 elemSize, U32 queueSize)
{
	if (elemSize < 1 || queueSize < 1)
		return NULL;
	
	openavb_queue_t retQueue = calloc(1, sizeof(struct openavb_queue));
	if (retQueue) {
		retQueue->elemArray = calloc(queueSize, sizeof(struct openavb_queue_elem));
		if (retQueue->elemArray) {
			U32 i1;
			for (i1 = 0; i1 < queueSize; i1++) {
				retQueue->elemArray[i1].data = calloc(1, elemSize);
				if (!retQueue->elemArray[i1].data) {
					openavbQueueDeleteQueue(retQueue);
					return NULL;
				}
			}
		}
		else {
			openavbQueueDeleteQueue(retQueue);
			return NULL;
		}

		retQueue->elemSize = elemSize;
		retQueue->queueSize = queueSize;
		retQueue->head = 0;
		retQueue->tail = 0;
	}

	return retQueue;
}

void openavbQueueDeleteQueue(openavb_queue_t queue)
{
	if (queue) {
		U32 i1;
		for (i1 = 0; i1 < queue->queueSize; i1++) {
			free(queue->elemArray[i1].data);
			queue->elemArray[i1].data = NULL;
		}
		free(queue->elemArray);
		queue->elemArray = NULL;
		free(queue);
	}
}

U32 openavbQueueGetQueueSize(openavb_queue_t queue)
{
	if (queue) {
		return queue->queueSize;
	}
	return 0;
}

U32 openavbQueueGetElemCount(openavb_queue_t queue)
{
	U32 cnt = 0;
	if (queue) {
		if (queue->head > queue->tail) {
			cnt += queue->head - queue->tail - 1;
		}	
		else if (queue->head < queue->tail) {
			cnt += queue->head + ((queue->queueSize - 1) - queue->tail);
		}

		if (queue->elemArray[queue->tail].setFlg) {
			cnt++;
		}
	}
	return cnt;
}

U32 openavbQueueGetElemSize(openavb_queue_t queue)
{
	if (queue) {
		return queue->elemSize;
	}
	return 0;
}

void *openavbQueueData(openavb_queue_elem_t elem)
{
	if (elem) {
		return elem->data;
	}
	return NULL;
}

openavb_queue_elem_t openavbQueueHeadLock(openavb_queue_t queue)
{
	if (queue) {
		if (!queue->elemArray[queue->head].setFlg) {
			return &queue->elemArray[queue->head];
		}
	}
	return NULL;
}

void openavbQueueHeadUnlock(openavb_queue_t queue)
{
}

void openavbQueueHeadPush(openavb_queue_t queue)
{
	if (queue) {
		queue->elemArray[queue->head++].setFlg = TRUE;		
		if (queue->head >= queue->queueSize) {
			queue->head = 0;
		}
	}
}

openavb_queue_elem_t openavbQueueTailLock(openavb_queue_t queue)
{
	if (queue) {
		if (queue->elemArray[queue->tail].setFlg) {
			return &queue->elemArray[queue->tail];
		}
	}
	return NULL;
}

void openavbQueueTailUnlock(openavb_queue_t queue)
{
}

void openavbQueueTailPull(openavb_queue_t queue)
{
	if (queue) {
		queue->elemArray[queue->tail++].setFlg = FALSE;
		if (queue->tail >= queue->queueSize) {
			queue->tail = 0;
		}
	}
}
