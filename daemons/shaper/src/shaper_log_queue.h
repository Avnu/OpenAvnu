/*************************************************************************************************************
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

#ifndef SHAPER_LOG_QUEUE_H
#define SHAPER_LOG_QUEUE_H 1

typedef struct shaper_log_queue_elem * shaper_log_queue_elem_t;
typedef struct shaper_log_queue * shaper_log_queue_t;

// Create an queue. Returns NULL on failure.
shaper_log_queue_t shaperLogQueueNewQueue(uint32_t elemSize, uint32_t queueSize);

// Delete an array.
void shaperLogQueueDeleteQueue(shaper_log_queue_t queue);

// Get number of queue slots
uint32_t shaperLogQueueGetQueueSize(shaper_log_queue_t queue);

// Get number of element
uint32_t shaperLogQueueGetElemCount(shaper_log_queue_t queue);

// Get element size
uint32_t shaperLogQueueGetElemSize(shaper_log_queue_t queue);

// Get data of the element. Returns NULL on failure.
void *shaperLogQueueData(shaper_log_queue_elem_t elem);

// Lock the head element.
shaper_log_queue_elem_t shaperLogQueueHeadLock(shaper_log_queue_t queue);

// Unlock the head element.
void shaperLogQueueHeadUnlock(shaper_log_queue_t queue);

// Push the head element making it available for tail access.
void shaperLogQueueHeadPush(shaper_log_queue_t queue);

// Lock the tail element.
shaper_log_queue_elem_t shaperLogQueueTailLock(shaper_log_queue_t queue);

// Unlock the tail element.
void shaperLogQueueTailUnlock(shaper_log_queue_t queue);

// Pull (remove) the tail element
void shaperLogQueueTailPull(shaper_log_queue_t queue);

#endif // SHAPER_LOG_QUEUE_H
