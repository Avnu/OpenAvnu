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
* MODULE SUMMARY : Interface for a basic dynamic array abstraction.
* 
* - Fixed size queue.
* - Only head and tail access possible.
* - Head and Tail locking.
* - If there is a single task accessing head and a single task accessing tail no synchronization is needed.
* - If synchronization is needed the Pull and Push functions should be protected before calling.
*/

#ifndef OPENAVB_QUEUE_H
#define OPENAVB_QUEUE_H 1

#include "openavb_types.h"

typedef struct openavb_queue_elem * openavb_queue_elem_t;
typedef struct openavb_queue * openavb_queue_t;

// Create an queue. Returns NULL on failure.
openavb_queue_t openavbQueueNewQueue(U32 elemSize, U32 queueSize);

// Delete an array.
void openavbQueueDeleteQueue(openavb_queue_t queue);

// Get number of queue slots
U32 openavbQueueGetQueueSize(openavb_queue_t queue);

// Get number of element
U32 openavbQueueGetElemCount(openavb_queue_t queue);

// Get element size
U32 openavbQueueGetElemSize(openavb_queue_t queue);

// Get data of the element. Returns NULL on failure.
void *openavbQueueData(openavb_queue_elem_t elem);

// Lock the head element.
openavb_queue_elem_t openavbQueueHeadLock(openavb_queue_t queue);

// Unlock the head element.
void openavbQueueHeadUnlock(openavb_queue_t queue);

// Push the head element making it available for tail access.
void openavbQueueHeadPush(openavb_queue_t queue);

// Lock the tail element.
openavb_queue_elem_t openavbQueueTailLock(openavb_queue_t queue);

// Unlock the tail element.
void openavbQueueTailUnlock(openavb_queue_t queue);

// Pull (remove) the tail element
void openavbQueueTailPull(openavb_queue_t queue);

#endif // OPENAVB_QUEUE_H
