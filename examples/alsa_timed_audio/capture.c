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
#include <pthread.h>
#include <thread_signal.h>
#include <util.h>
#include <stream.h>
#include <stdlib.h>
#include <limits.h>
#include <init.h>
#include <audio_input.h>
#include <alsa.h>
#include <stdio.h>

#define PER_STREAM_BUFFER_COUNT 3

// We'll get a "pretty good" estimation of the start time at 50 ms
#define CROSSTSTAMP_THRESHOLD	( 50000000 ) /*ns*/

struct isaudk_capture_handle {
        struct isaudk_input *input;
        struct isaudk_format format;
        isaudk_sample_rate_t rate;

	isaudk_stream_handle_t stream;

        bool start_req;
        bool exit_req;
	bool capturing;
        bool running;

        pthread_t thread;

        pthread_mutex_t capture_state_lock;
        pthread_mutex_t capture_control_lock;
        isaudk_signal_t wake_signal;

        bool fatal;
        isaudk_error_t thread_exit_code;
        bool fatal_loop; // Failsafe indicator

        struct isaudk_system_time       start_time;
        struct isaudk_cross_time        curr_cross_time;
        struct isaudk_cross_time        initial_cross_time;
};

struct per_stream_sample_buffer
{
	isaudk_sample_block_t buffer[PER_STREAM_BUFFER_COUNT];
	uint16_t count[PER_STREAM_BUFFER_COUNT];

	// incremented by the mixer loop each time buffer is read
	uint8_t read_seq[PER_STREAM_BUFFER_COUNT];
	// incremented each time buffer is written
	uint8_t write_seq[PER_STREAM_BUFFER_COUNT];

	uint8_t idx;
	isaudk_signal_t signal;
};

static void capture_init( void ) _isaudk_capture_init;

static struct isaudk_capture_handle _default_capture;
static struct isaudk_capture_handle *default_capture;

isaudk_capture_handle_t isaudk_get_default_capture()
{
	return default_capture;
}

static unsigned
get_buffer_sample_count( isaudk_capture_handle_t capture )
{
	return PS16_SAMPLE_COUNT / capture->format.channels;
}

isaudk_error_t
isaudk_capture_register_stream( isaudk_capture_handle_t capture,
				isaudk_stream_handle_t stream,
				struct isaudk_format *format,
				isaudk_sample_rate_t rate )
{
	isaudk_error_t ret = ISAUDK_SUCCESS;
	struct per_stream_sample_buffer *sample_buffer;
	int i;

	// Check that the format matches
	if( format != NULL &&
	    !isaudk_is_format_equal( format, &capture->format ))
		return ISAUDK_BADFORMAT;

	// Check that the format matches
	if( rate != DEFAULT_SAMPLE_RATE && capture->rate != rate )
		return ISAUDK_BADFORMAT;

	// Allocate a buffer for the stream and add this to the provate
	sample_buffer = (__typeof__(sample_buffer))
		malloc((size_t) sizeof( struct per_stream_sample_buffer ));
	if( sample_buffer == NULL )
		return ISAUDK_NOMEMORY;
	sample_buffer->idx = 0;
	if( isaudk_create_signal( &sample_buffer->signal )
	    != ISAUDK_SIGNAL_OK )
	{
		return ISAUDK_NOMEMORY;
	}
	for( i = 0; i < PER_STREAM_BUFFER_COUNT; ++i )
	{
		sample_buffer->read_seq[i] = 0;
		sample_buffer->write_seq[i] = 0;
	}

	isaudk_stream_set_capture_private( stream, sample_buffer );

	capture->stream = stream;

	return ret;
}

// Inside loop reports fatal error
#define CHECKED_PTHREAD_CALL_FATAL(pthread_call,capture)	\
	{							\
		int err;					\
		err = pthread_call;				\
		if( err != 0 ) {				\
			(capture)->fatal_loop = true;		\
			return NULL;				\
		}						\
	}

#define CAPTURE_LOOP_LOCK_CAPTURE				\
	CHECKED_PTHREAD_CALL_FATAL				\
	( pthread_mutex_lock( &capture->capture_state_lock ), capture )

#define CAPTURE_LOOP_UNLOCK_CAPTURE				\
	CHECKED_PTHREAD_CALL_FATAL				\
	( pthread_mutex_unlock( &capture->capture_state_lock ), capture )

struct capture_loop_arg
{
	struct isaudk_capture_handle *capture;
	isaudk_signal_t signal;
};

void *capture_loop( void *_arg )
{
	struct capture_loop_arg *arg = (struct capture_loop_arg *) _arg;
	struct isaudk_capture_handle *capture = arg->capture;
	isaudk_signal_error_t sigerr;
	struct per_stream_sample_buffer *stream_buffer;
	bool cross_tstamp_result;

	void *buffer;

	capture->running = true;
	capture->initial_cross_time = INVALID_CROSS_TIME;
	capture->curr_cross_time = INVALID_CROSS_TIME;
	capture->thread_exit_code = ISAUDK_SUCCESS;
	capture->start_time.time = ULLONG_MAX;

	// Signal the calling thread
	sigerr = isaudk_signal_send( arg->signal );
	if( sigerr != ISAUDK_SIGNAL_OK ) {
		CAPTURE_LOOP_LOCK_CAPTURE;
		capture->running = false;
		capture->thread_exit_code = ISAUDK_PTHREAD;
		CAPTURE_LOOP_UNLOCK_CAPTURE;

		return NULL;
	}

	capture->input->fn->start( capture->input->ctx );
	CAPTURE_LOOP_LOCK_CAPTURE;
	cross_tstamp_result = capture->input->fn->get_cross_tstamp
		( capture->input->ctx, &capture->initial_cross_time );
	CAPTURE_LOOP_UNLOCK_CAPTURE;
	capture->capturing = true;

	while( !capture->exit_req )
	{
		unsigned samples_to_read;
		bool read_result;
		struct isaudk_cross_time curr_cross_time;

		stream_buffer = isaudk_stream_get_capture_private
			( capture->stream );
		buffer = get_buffer_pointer
			( &capture->format,
			  stream_buffer->buffer + stream_buffer->idx );

		// Capture buffer
		while( stream_buffer->read_seq[stream_buffer->idx] !=
		       stream_buffer->write_seq[stream_buffer->idx] )
		{
			isaudk_signal_wait( capture->wake_signal, 0 );
		}

		samples_to_read = get_buffer_sample_count( capture );
		read_result = capture->input->fn->read_input_buffer
			( capture->input->ctx, buffer, &samples_to_read );
		if( !read_result )
		{
			CAPTURE_LOOP_LOCK_CAPTURE;
			capture->thread_exit_code = ISAUDK_FATAL;
			CAPTURE_LOOP_UNLOCK_CAPTURE;
			break;
		}
		++stream_buffer->write_seq[stream_buffer->idx];

		cross_tstamp_result = capture->input->fn->get_cross_tstamp
			( capture->input->ctx, &curr_cross_time );

		if( !isaudk_crosstime_equal
		    ( &capture->curr_cross_time, &INVALID_CROSS_TIME ) &&
		    ((capture->curr_cross_time.dev.time <=
		      CROSSTSTAMP_THRESHOLD &&
		      curr_cross_time.dev.time > CROSSTSTAMP_THRESHOLD) ))
		{
			double ratio;
			unsigned delta;

			ratio  = curr_cross_time.sys.time;
			ratio -= capture->initial_cross_time.sys.time;
			ratio /= curr_cross_time.dev.time -
				capture->initial_cross_time.dev.time;

			delta = capture->initial_cross_time.dev.time * ratio;

			capture->start_time = capture->initial_cross_time.sys;
			capture->start_time.time -= delta;
		}

		stream_buffer->idx = ( stream_buffer->idx + 1 ) %
			PER_STREAM_BUFFER_COUNT;
		CAPTURE_LOOP_LOCK_CAPTURE;
		capture->curr_cross_time = curr_cross_time;
		CAPTURE_LOOP_UNLOCK_CAPTURE;
		// Signal for more data
		isaudk_signal_send( stream_buffer->signal );
	}

	capture->input->fn->stop( capture->input->ctx );
	// Exit normally
	capture->running = false;

	// Send a signal at the end, if we exit abnormally the client may
	// be "hung" waiting for a signal
	isaudk_signal_send( stream_buffer->signal );

	return NULL;
}

// Inside loop reports fatal error
#define CHECKED_PTHREAD_CALL_FATAL(pthread_call,capture)	\
	{							\
		int err;					\
		err = pthread_call;				\
		if( err != 0 ) {				\
			(capture)->fatal_loop = true;		\
			return NULL;				\
		}						\
	}

#define CAPTURE_LOOP_LOCK_CAPTURE					\
	CHECKED_PTHREAD_CALL_FATAL					\
	( pthread_mutex_lock( &capture->capture_state_lock ), capture )

#define CAPTURE_LOOP_UNLOCK_CAPTURE					\
	CHECKED_PTHREAD_CALL_FATAL					\
	( pthread_mutex_unlock( &capture->capture_state_lock ), capture )

#define COND_RETURN_UNLOCK(cond,stmt,retval)	\
	if(( cond ))				\
	{					\
		stmt;				\
		ret = (retval);			\
		goto unlock;			\
	}


isaudk_error_t
isaudk_start_capture( isaudk_capture_handle_t capture )
{
	struct capture_loop_arg arg;
	isaudk_error_t ret = ISAUDK_SUCCESS;

	if( pthread_mutex_lock( &capture->capture_control_lock ) != 0 )
		return ISAUDK_PTHREAD;

	if( capture->start_req )
	{
		COND_RETURN_UNLOCK( capture->running,, ISAUDK_SUCCESS )
		else
			COND_RETURN_UNLOCK( capture->fatal ||
					    capture->fatal_loop,,
					    ISAUDK_FATAL )
			else
				COND_RETURN_UNLOCK
					( true,, capture->thread_exit_code );
	}

	COND_RETURN_UNLOCK
		( isaudk_create_signal( &arg.signal ) != ISAUDK_SIGNAL_OK,
		  capture->fatal = true, ISAUDK_PTHREAD );

	arg.capture = capture;
	capture->exit_req = false;

	COND_RETURN_UNLOCK
		( pthread_create( &capture->thread, NULL, capture_loop, &arg )
		  != 0, capture->fatal = true, ISAUDK_PTHREAD );

	COND_RETURN_UNLOCK
		( isaudk_signal_wait( arg.signal, 0 ) != ISAUDK_SIGNAL_OK,
		  {
			  pthread_cancel( capture->thread );
			  capture->fatal = true; }, ISAUDK_PTHREAD )

	capture->start_req = true;

unlock:
	pthread_mutex_unlock( &capture->capture_control_lock );

	return ret;
}

void isaudk_capture_get_stream_buffer( isaudk_stream_handle_t stream,
				     isaudk_sample_block_t **sample_buffer,
				     unsigned *count,
				     isaudk_signal_t *signal )
{
	struct per_stream_sample_buffer *buffer;

	buffer = isaudk_stream_get_capture_private( stream );
	*sample_buffer = buffer->buffer;
	*count = PER_STREAM_BUFFER_COUNT;
	*signal = buffer->signal;
}


uint8_t
isaudk_capture_get_current_index( isaudk_stream_handle_t stream, bool *roll )
{
	struct per_stream_sample_buffer *buffer;
	uint8_t retval;

	buffer = isaudk_stream_get_capture_private( stream );
	retval = buffer->idx;

	if( buffer->write_seq[retval] != buffer->read_seq[retval] )
		*roll = true;
	else
		*roll = false;

	return buffer->idx;
}

// Mark buffer as read and usable by capture loop
isaudk_error_t
isaudk_capture_mark_buffer_done( isaudk_stream_handle_t stream,
				 struct isaudk_capture_handle *capture,
				 size_t idx )
{
	struct per_stream_sample_buffer *buffer;
	isaudk_error_t thread_exit_code;

	buffer = (__typeof__(buffer))
		isaudk_stream_get_capture_private( stream );

	if( capture->fatal )
		return ISAUDK_FATAL;

	// Check the arguments
	if( idx > PER_STREAM_BUFFER_COUNT - 1 )
		return ISAUDK_INVALIDARG;

	// Don't read old data in the buffer
	if( UINT8_ADD( buffer->read_seq[idx], 1 ) != buffer->write_seq[idx] )
		return ISAUDK_AGAIN;

	pthread_mutex_lock( &capture->capture_state_lock );
	thread_exit_code = capture->thread_exit_code;
	pthread_mutex_unlock( &capture->capture_state_lock );
	if( thread_exit_code != ISAUDK_SUCCESS )
		return thread_exit_code;

	++buffer->read_seq[idx];
	isaudk_signal_send( capture->wake_signal );

	return ISAUDK_SUCCESS;
}

isaudk_error_t
isaudk_capture_get_start_time( isaudk_capture_handle_t capture,
			       struct isaudk_system_time *start_time )
{
	struct isaudk_system_time start_time_tmp;
	isaudk_error_t error;

	pthread_mutex_lock( &capture->capture_state_lock );
	start_time_tmp = capture->start_time;
	error = capture->thread_exit_code;
	pthread_mutex_unlock( &capture->capture_state_lock );

	if( error != ISAUDK_SUCCESS )
		return error;

	if( start_time_tmp.time != ULLONG_MAX )
	{
		*start_time = start_time_tmp;
		return ISAUDK_SUCCESS;
	}

	return ISAUDK_AGAIN;
}

bool _capture_init( struct isaudk_capture_handle *capture )
{
	if( pthread_mutex_init( &capture->capture_control_lock, NULL ) != 0 )
		return false;

	if( pthread_mutex_init( &capture->capture_state_lock, NULL ) != 0 )
		return false;

	capture->fatal = false;
	capture->running = false;
	capture->start_req = false;

	if( isaudk_create_signal( &capture->wake_signal ) != ISAUDK_SIGNAL_OK )
	{
		return ISAUDK_NOMEMORY;
	}

	return true;
}

static void capture_init( void )
{
	// Default ALSA settings must match these defaults
	_default_capture.input = isaudk_get_default_alsa_input();

	if( _default_capture.input == NULL )
		return;

	// Setup the default capture here
	if( !_capture_init( &_default_capture ))
		return;

	_default_capture.format.encoding = ISAUDK_ENC_PS16;
	_default_capture.format.channels = 2;
	_default_capture.rate = _48K_SAMPLE_RATE;
	if( !_default_capture.input->fn->set_audio_param
	    ( _default_capture.input->ctx, &_default_capture.format,
	      _48K_SAMPLE_RATE ))
		return;

	default_capture = &_default_capture;
}

isaudk_error_t
isaudk_capture_get_stream_info( struct isaudk_capture_handle *handle,
				struct isaudk_format *format,
				isaudk_sample_rate_t *rate )
{
	if( format != NULL )
		*format = handle->format;
	if( rate != NULL )
		*rate = handle->rate;

	return ISAUDK_SUCCESS;
}
