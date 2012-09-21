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
 * Windwos specific - queue messages in a thread safe way between threads.
 */

#ifndef _QUE_H_
#define _QUE_H_

#include <stdint.h>		// for uint8_t etc
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

	struct que_def {
		HANDLE space_avail;	// at least one slot empty
		HANDLE data_avail;	// at least one slot full
		CRITICAL_SECTION mutex;	// protect buffer, in_pos, out_pos

		uint8_t *buffer;
		int in_pos;
		int out_pos;
		int entry_count;
		int entry_size;
	};

	struct que_def *que_new(int count, int entry_size);
	void que_delete(struct que_def *q);
	void que_push(struct que_def *q, void *d);
	void que_pop_nowait(struct que_def *q, void *d);
	void que_pop_wait(struct que_def *q, void *d);
	HANDLE que_data_available_object(struct que_def *q);

#ifdef __cplusplus
}
#endif
#endif
