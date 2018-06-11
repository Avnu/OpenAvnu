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

#ifndef STREAM_H
#define STREAM_H

#include <sdk.h>
#include <linked_list.h>

extern const struct isaudk_cross_time INVALID_CROSS_TIME;
extern const isaudk_sample_rate_t DEFAULT_SAMPLE_RATE;

// These functions are for internal use

linked_list_element_t *
isaudk_get_stream_mixer_reference( isaudk_stream_handle_t handle );

isaudk_stream_handle_t isaudk_mixer_reference_to_stream
( linked_list_element_t *mix_ref );

void isaudk_stream_set_mixer_private
( struct isaudk_stream_handle *handle, void *priv );

void *isaudk_stream_get_mixer_private( isaudk_stream_handle_t handle );

void isaudk_stream_set_capture_private
( struct isaudk_stream_handle *handle, void *priv );

void *isaudk_stream_get_capture_private( isaudk_stream_handle_t handle );

#endif/*STREAM_H*/
