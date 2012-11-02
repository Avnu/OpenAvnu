/******************************************************************************

  Copyright (c) 2012, AudioScience, Inc
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

/*
 * Queue messages in a thread safe way between threads.
 */

#include "que.h"

struct que_def *que_new(int entry_count, int entry_size)
{
	struct que_def *q;

	q = (struct que_def *)calloc(1, sizeof(struct que_def));
	if (!q)
		return q;
	q->buffer = (uint8_t *) calloc(entry_count, entry_size);
	if (!q->buffer) {
		free(q);
		return NULL;
	}
	q->entry_count = entry_count;
	q->entry_size = entry_size;
	q->space_avail =
	    CreateSemaphore(NULL, q->entry_count, q->entry_count, NULL);
	q->data_avail = CreateSemaphore(NULL, 0, q->entry_count, NULL);
	InitializeCriticalSection(&q->mutex);

	return q;
}

void que_delete(struct que_def *q)
{
	if (q->buffer)
		free(q->buffer);
	free(q);
}

void que_push(struct que_def *q, void *d)
{
	WaitForSingleObject(q->space_avail, INFINITE);
	EnterCriticalSection(&q->mutex);
	memcpy(&q->buffer[q->in_pos * q->entry_size], d, q->entry_size);
	q->in_pos = (q->in_pos + 1) % q->entry_count;
	LeaveCriticalSection(&q->mutex);
	ReleaseSemaphore(q->data_avail, 1, NULL);
}

void que_pop_nowait(struct que_def *q, void *d)
{
	EnterCriticalSection(&q->mutex);
	memcpy(d, &q->buffer[q->out_pos * q->entry_size], q->entry_size);
	q->out_pos = (q->out_pos + 1) % q->entry_count;
	LeaveCriticalSection(&q->mutex);
	ReleaseSemaphore(q->space_avail, 1, NULL);
}

void que_pop_wait(struct que_def *q, void *d)
{
	WaitForSingleObject(q->data_avail, INFINITE);
	que_pop_nowait(q, d);
}

HANDLE que_data_available_object(struct que_def *q)
{
	return q->data_avail;
}
