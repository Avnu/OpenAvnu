/******************************************************************************

  Copyright (c) 2018, Intel Corporation
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

#ifndef OUTPUT_H
#define OUTPUT_H

#include <sdk.h>
#include <stack.h>

typedef struct isaudk_output_context*	isaudk_output_context_t;

// Audio output operations
//
struct isaudk_output_fn {
	// Set audio format
	bool ( *set_audio_param )( isaudk_output_context_t ctx,
				   struct isaudk_format *format,
				   unsigned rate );
	// Get system/device timestamp
	bool ( *get_cross_tstamp )( isaudk_output_context_t ctx,
				    struct isaudk_cross_time *time );
	// Queue buffer
	bool ( *queue_output_buffer )( isaudk_output_context_t ctx,
				       void *buffer, unsigned *count );
	// Start
	bool ( *start )( isaudk_output_context_t ctx );

	// Stop
	bool ( *stop )( isaudk_output_context_t ctx );
};

struct isaudk_output {
	struct isaudk_output_fn 	*fn;
	isaudk_output_context_t		 ctx;
};

#endif/*OUTPUT_H*/
