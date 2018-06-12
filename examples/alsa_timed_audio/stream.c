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

#include <sdk.h>
#include <util.h>
#include <stack.h>
#include <mixer.h>
#include <capture.h>

#include <stdlib.h>
#include <limits.h>

enum isaudk_stream_state {
	ISAUDK_STREAM_RUNNING,
	ISAUDK_STREAM_STOPPED,
};

struct isaudk_stream_handle {
	isaudk_direction_t direction;
	enum isaudk_stream_state state;
	struct isaudk_system_time req_start_time;
	union {
		isaudk_mixer_handle_t mixer;
		isaudk_capture_handle_t capture;
	};

	union {
		void *mixer_private;
		void *capture_private;
	};

	stack_element_t stream_stack_element;
	linked_list_element_t mixer_list_element;
};

#define HANDLE_ALLOCATION_COUNT 4

const struct isaudk_cross_time
INVALID_CROSS_TIME = { .sys = { .time = ULLONG_MAX },
		       .dev = { .time = 0 }};

const isaudk_sample_rate_t DEFAULT_SAMPLE_RATE = 0;

const isaudk_sample_rate_t _48K_SAMPLE_RATE = 48000/ISAUDK_RATE_MULTIPLIER;
const struct isaudk_system_time ISAUDK_PLAY_IMMED = { .time = 0 };

static stack_t stream_handle_free = STATIC_STACK_INIT;
static stack_t stream_handle_use = STATIC_STACK_INIT;

#define stackelement_to_stream( strm_stack_element ) \
	container_addr( stream_stack_element, strm_stack_element,	\
			struct isaudk_stream_handle );

#define mixer_listelement_to_stream( mixr_list_element ) \
	container_addr( mixer_list_element, mixr_list_element,	\
			struct isaudk_stream_handle );

bool check_stack( stack_t *stack )
{
	if( !stack_is_valid( *stack )) {
		*stack = stack_alloc();
		if( !stack_is_valid( *stack ))
			return false;
	}

	return true;
}


// Cleans up all stream state, called on unload
void stream_cleanup( void )
{
	// Delete all stream state
}

static bool
allocate_stream_handle( void )
{
	int i;

	if( !check_stack( &stream_handle_free ))
		return ISAUDK_NOMEMORY;

	struct isaudk_stream_handle *handle = (__typeof__(handle))
		malloc((size_t) HANDLE_ALLOCATION_COUNT*sizeof(*handle));
	if( handle == NULL )
		return false;

	for( i = 0; i < HANDLE_ALLOCATION_COUNT; ++i )
	{
		stack_init_element( &( handle+i )->stream_stack_element );
		ll_init_element( &( handle+i )->mixer_list_element );
		push( stream_handle_free,
		      &( handle+i )->stream_stack_element );
	}

	return true;
}

static isaudk_error_t
isaudk_allocate_stream( struct isaudk_stream_handle **handle,
			isaudk_direction_t direction )
{
	stack_element_t *stream_stack_element;

	stream_stack_element = pop( stream_handle_free );
	if( stream_stack_element == NULL ) {
		if( allocate_stream_handle() )
			stream_stack_element = pop( stream_handle_free );
		else
			return ISAUDK_NOMEMORY;
	}
	*handle = stackelement_to_stream( stream_stack_element );
	(*handle)->direction = direction;

	if( !check_stack( &stream_handle_use ))
		return ISAUDK_NOMEMORY;

	push( stream_handle_use, stream_stack_element );

	return ISAUDK_SUCCESS;
}

#if 0
static isaudk_error_t
isaudk_deallocate_stream( struct isaudk_stream_handle **handle )
{
}
#endif

isaudk_error_t
isaudk_open_stream_capture( struct isaudk_stream_handle **handle,
			    struct isaudk_format *format,
			    isaudk_sample_rate_t rate,
			    isaudk_capture_handle_t capture )
{
	isaudk_error_t err;

	err = isaudk_allocate_stream( handle, ISAUDK_CAPTURE );
	if( err != ISAUDK_SUCCESS )
		return err;

	if( capture != NULL )
		(*handle)->capture = capture;
	else
		(*handle)->capture = isaudk_get_default_capture();

	return isaudk_capture_register_stream
		( (*handle)->capture, *handle, format, rate );
}

isaudk_error_t
isaudk_open_stream_mixer( struct isaudk_stream_handle **handle,
			  struct isaudk_format *format,
			  isaudk_sample_rate_t rate,
			  isaudk_mixer_handle_t mixer )
{
	isaudk_error_t err;

	err = isaudk_allocate_stream( handle, ISAUDK_RENDER );
	if( err != ISAUDK_SUCCESS )
		return err;

	if( mixer != NULL )
		(*handle)->mixer = mixer;
	else
		(*handle)->mixer = isaudk_get_default_mixer();

	return isaudk_mixer_register_stream
		( (*handle)->mixer, *handle, format, rate );
}

isaudk_error_t
isaudk_open_stream( struct isaudk_stream_handle **handle,
		    struct isaudk_format *format,
		    isaudk_sample_rate_t rate,
		    isaudk_direction_t direction )
{
	if( direction == ISAUDK_RENDER )
		return isaudk_open_stream_mixer( handle, format, rate, NULL );
	else
		return isaudk_open_stream_capture
			( handle, format, rate, NULL );

	return ISAUDK_UNIMPL; // Place holder for capture
}

isaudk_error_t
isaudk_open_default_stream( struct isaudk_stream_handle **handle,
			    isaudk_direction_t direction )
{
	return isaudk_open_stream( handle, NULL, DEFAULT_SAMPLE_RATE,
				   direction );
}


isaudk_error_t
isaudk_get_stream_info( struct isaudk_stream_handle *handle,
			struct isaudk_format *format,
			isaudk_sample_rate_t *rate,
			isaudk_direction_t *direction )
{
	if( direction != NULL )
		*direction = handle->direction;
	if( handle->direction == ISAUDK_RENDER )
	{
		return isaudk_mixer_get_stream_info
			( handle->mixer, format, rate );
	}
	else
	{
		return isaudk_capture_get_stream_info
			( handle->capture, format, rate );
	}

	return ISAUDK_UNIMPL;
}

void
isaudk_get_stream_buffers( struct isaudk_stream_handle *handle,
			   isaudk_sample_block_t **buffer, size_t *count,
			   isaudk_signal_t *signal )
{
	unsigned u_count;

	if( handle->direction == ISAUDK_RENDER )
		isaudk_mixer_get_stream_buffer
			( handle, buffer, &u_count, signal );
	else
		isaudk_capture_get_stream_buffer
			( handle, buffer, &u_count, signal );

	*count = u_count;
}

uint8_t
isaudk_get_stream_buffer_index( struct isaudk_stream_handle *handle,
				bool *roll )
{
	if( handle->direction == ISAUDK_RENDER )
		return isaudk_mixer_get_current_index( handle, roll );
	else
		return isaudk_capture_get_current_index( handle, roll );

	return ISAUDK_UNIMPL;
}

bool
isaudk_get_stream_flag( struct isaudk_stream_handle *handle )
{
	return isaudk_mixer_get_flag( handle );
}

void
isaudk_clear_stream_flag( struct isaudk_stream_handle *handle )
{
	return isaudk_mixer_clear_flag( handle );
}

int
isaudk_get_stream_remainder( struct isaudk_stream_handle *handle )
{
	return isaudk_mixer_get_remainder( handle );
}

isaudk_error_t
isaudk_stream_set_buffer_sample_count( struct isaudk_stream_handle *handle,
				       size_t idx, size_t count, bool eos )
{
	return isaudk_mixer_set_buffer_sample_count( handle, handle->mixer,
						     idx, count, eos );
}

isaudk_error_t
isaudk_start_stream_at( struct isaudk_stream_handle *handle,
			struct isaudk_system_time start_time )
{
//	if( start_time.time != ISAUDK_PLAY_IMMED.time )
//		return ISAUDK_UNIMPL;


	if( handle->direction == ISAUDK_RENDER )
		return isaudk_start_mixer( handle->mixer, start_time );
	else
		return isaudk_start_capture( handle->capture );

	return ISAUDK_UNIMPL;
}

isaudk_error_t
isaudk_get_stream_start_time( isaudk_stream_handle_t handle,
			      struct isaudk_system_time *start_time )
{
	if( handle->direction == ISAUDK_RENDER )
		return isaudk_mixer_get_start_time
			( handle->mixer, start_time );
	else
		return isaudk_capture_get_start_time
			( handle->capture, start_time );

	return ISAUDK_UNIMPL;
}

isaudk_error_t
isaudk_get_stream_audio_start_time( isaudk_stream_handle_t handle,
			      struct isaudk_system_time *start_time )
{
	if (start_time->time == 0xFFFFFF)
		return ISAUDK_INVALIDTIME;
	if( handle->direction == ISAUDK_RENDER )
		return isaudk_mixer_get_audio_start_time
			( handle->mixer, start_time );
	else
		return isaudk_capture_get_start_time
			( handle->capture, start_time );

	return ISAUDK_UNIMPL;
}

void
isaudk_stream_set_mixer_private
( struct isaudk_stream_handle *handle, void *priv )
{
	handle->mixer_private = priv;
}

void *
isaudk_stream_get_mixer_private( struct isaudk_stream_handle *handle )
{
	return handle->mixer_private;
}

void isaudk_stream_set_capture_private
( struct isaudk_stream_handle *handle, void *priv )
{
	handle->capture_private = priv;
}

void *isaudk_stream_get_capture_private( isaudk_stream_handle_t handle )
{
	return handle->capture_private;
}

linked_list_element_t *
isaudk_get_stream_mixer_reference( struct isaudk_stream_handle *handle )
{
	return &handle->mixer_list_element;
}

struct isaudk_stream_handle *isaudk_mixer_reference_to_stream
( linked_list_element_t *mix_ref )
{
	return mixer_listelement_to_stream( mix_ref );
}

isaudk_error_t
isaudk_stream_mark_buffer_done( struct isaudk_stream_handle *stream,
				size_t idx )
{
	if( stream->direction != ISAUDK_CAPTURE )
		return ISAUDK_INVALIDARG;

	return isaudk_capture_mark_buffer_done( stream, stream->capture, idx );
}
