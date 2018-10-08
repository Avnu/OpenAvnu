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

#ifndef MIXER_H
#define MIXER_H

#include <sdk.h>

isaudk_mixer_handle_t isaudk_get_default_mixer();

isaudk_error_t
isaudk_start_mixer( isaudk_mixer_handle_t mixer, struct isaudk_system_time start_time );

void isaudk_mixer_get_stream_buffer( isaudk_stream_handle_t stream,
				     isaudk_sample_block_t **sample_buffer,
				     unsigned *count,
				     isaudk_signal_t *signal );

uint8_t
isaudk_mixer_get_current_index( isaudk_stream_handle_t stream, bool *roll );

bool
isaudk_mixer_get_flag( isaudk_stream_handle_t stream );

void
isaudk_mixer_clear_flag( isaudk_stream_handle_t stream );

int
isaudk_mixer_get_remainder( isaudk_stream_handle_t stream );

isaudk_error_t
isaudk_mixer_set_buffer_sample_count( isaudk_stream_handle_t stream,
				      isaudk_mixer_handle_t mixer,
				      size_t idx, size_t count, bool eos );

isaudk_error_t
isaudk_mixer_get_start_time( isaudk_mixer_handle_t mixer,
			     struct isaudk_system_time *start_time );
isaudk_error_t
isaudk_mixer_get_audio_start_time( isaudk_mixer_handle_t mixer,
			     struct isaudk_system_time *start_time );

isaudk_error_t
isaudk_mixer_register_stream( struct isaudk_mixer_handle *mixer,
			      isaudk_stream_handle_t stream,
			      struct isaudk_format *format,
			      isaudk_sample_rate_t rate );

isaudk_error_t
isaudk_mixer_get_stream_info( isaudk_mixer_handle_t handle,
			      struct isaudk_format *format,
			      isaudk_sample_rate_t *rate );

#endif/*MIXER_H*/
