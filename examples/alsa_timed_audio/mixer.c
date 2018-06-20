
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

#include <linked_list.h>
#include <audio_output.h>
#include <util.h>
#include <stream.h>
#include <init.h>
#include <string.h>
#include <alsa.h>
#include <math.h>
#include <thread_signal.h>
#include <stdlib.h>
#include <stdio.h>

#include <stdint.h>
#include <pthread.h>
#include <limits.h>
//maximum allowed PER_STREAM_BUFFER_COUNT is 100
#define PER_STREAM_BUFFER_COUNT 3

#define NSEC_PER_SEC 			( 1000000000ULL )
#define START_THRESHOLD			( 30000000 ) /*ns*/

// We'll get a "pretty good" estimation of the start time at 50 ms
#define CROSSTSTAMP_THRESHOLD_INIT	( 1000 ) /*ns*/
#define CROSSTSTAMP_THRESHOLD_STOP	( 50000000 ) /*ns*/

struct isaudk_mixer_handle {
	uint8_t master_output_volume;
	struct isaudk_output *output;
	struct isaudk_format format;
	isaudk_sample_rate_t rate;

	bool start_req;
	bool exit_req;
	bool running;
	bool playing;
	isaudk_mode_t mode;

	pthread_t thread;

	pthread_mutex_t mixer_state_lock;
	pthread_mutex_t mixer_control_lock;
	isaudk_signal_t wake_signal;

	bool fatal;
	isaudk_error_t thread_exit_code;
	bool fatal_loop; // Failsafe indicator

	struct isaudk_system_time	start_time;
	struct isaudk_system_time	requested_start_time;
	struct isaudk_system_time	audio_start_time;
	struct isaudk_cross_time	curr_cross_time;
	struct isaudk_cross_time	initial_cross_time;
	struct isaudk_cross_time	audio_initial_cross_time;
	uint64_t samples_written;

	linked_list_t stream_list;
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
	uint8_t eos;
	bool flag;
	int remainder;
	isaudk_signal_t signal;
};

void mixer_init( void ) _isaudk_mixer_init;
void mixer_stop( void ) _isaudk_mixer_stop;

static struct isaudk_mixer_handle _default_mixer;
static struct isaudk_mixer_handle *default_mixer;

bool _mixer_init( struct isaudk_mixer_handle *mixer )
{
	if( pthread_mutex_init( &mixer->mixer_control_lock, NULL ) != 0 )
		return false;

	if( pthread_mutex_init( &mixer->mixer_state_lock, NULL ) != 0 )
		return false;

	mixer->master_output_volume = 0xFF;
	mixer->fatal = false;
	mixer->running = false;
	mixer->start_req = false;
	mixer->stream_list = ll_alloc();
	mixer->mode = ISAUDK_SILENCE;

	if( isaudk_create_signal( &mixer->wake_signal ) != ISAUDK_SIGNAL_OK )
	{
		return ISAUDK_NOMEMORY;
	}

	return true;
}

void mixer_init( void )
{
	// Default ALSA settings must match these defaults
	_default_mixer.output = isaudk_get_default_alsa_output();

	if( _default_mixer.output == NULL )
		return;

	// Setup the default mixer here
	if( !_mixer_init( &_default_mixer ))
		return;

	_default_mixer.format.encoding = ISAUDK_ENC_PS16;
	_default_mixer.format.channels = 2;
	_default_mixer.rate = _48K_SAMPLE_RATE;
	if( !_default_mixer.output->fn->set_audio_param
	    ( _default_mixer.output->ctx, &_default_mixer.format,
	      _48K_SAMPLE_RATE ))
		return;

	default_mixer = &_default_mixer;
}

struct isaudk_mixer_handle *isaudk_get_default_mixer()
{
	return default_mixer;
}

isaudk_error_t
isaudk_mixer_register_stream( struct isaudk_mixer_handle *mixer,
			      isaudk_stream_handle_t stream,
			      struct isaudk_format *format,
			      isaudk_sample_rate_t rate )
{
	isaudk_error_t ret = ISAUDK_SUCCESS;
	struct per_stream_sample_buffer *sample_buffer;
	int i;

	// Check that the format matches
	if( format != NULL &&
	    !isaudk_is_format_equal( format, &mixer->format ))
		return ISAUDK_BADFORMAT;

	// Check that the format matches
	if( rate != DEFAULT_SAMPLE_RATE && mixer->rate != rate )
		return ISAUDK_BADFORMAT;

	// Allocate a buffer for the stream and add this to the provate
	sample_buffer = (__typeof__(sample_buffer))
		malloc((size_t) sizeof( struct per_stream_sample_buffer ));
	if( sample_buffer == NULL )
		return ISAUDK_NOMEMORY;
	sample_buffer->idx = 0;
	sample_buffer->flag = false;
	sample_buffer->remainder = PS16_SAMPLE_COUNT/mixer->format.channels;
	if( isaudk_create_signal( &sample_buffer->signal )
	    != ISAUDK_SIGNAL_OK )
	{
		return ISAUDK_NOMEMORY;
	}
	for( i = 0; i < PER_STREAM_BUFFER_COUNT; ++i )
	{
		sample_buffer->read_seq[i] = 1;
		sample_buffer->write_seq[i] = 0;
	}
	sample_buffer->eos = UCHAR_MAX;

	isaudk_stream_set_mixer_private( stream, sample_buffer );

	if( pthread_mutex_lock( &mixer->mixer_state_lock ) != 0 )
		return ISAUDK_PTHREAD;

	if( !ll_add_head( mixer->stream_list,
			  isaudk_get_stream_mixer_reference( stream )))
	{
		ret = ISAUDK_NOMEMORY;
		goto unlock;
	}


unlock:
	if( pthread_mutex_unlock( &mixer->mixer_state_lock ) != 0 )
		return ISAUDK_PTHREAD;

	return ret;
}

static unsigned get_buffer_sample_count( struct isaudk_mixer_handle *mixer )
{
	switch( mixer->format.encoding )
	{
	default:
		// Unsupported format request
		return 0;
	case ISAUDK_ENC_PS16:
		return PS16_SAMPLE_COUNT;
	}

	return 0;
}

static unsigned get_start_threshold( struct isaudk_mixer_handle *mixer )
{
	return START_THRESHOLD /
		( NSEC_PER_SEC / ( mixer->rate * ISAUDK_RATE_MULTIPLIER ));
}

isaudk_error_t
isaudk_mixer_set_buffer_sample_count( isaudk_stream_handle_t stream,
				      struct isaudk_mixer_handle *mixer,
				      size_t idx, size_t count, bool eos )
{
	struct per_stream_sample_buffer *buffer;
	isaudk_error_t thread_exit_code;

	buffer = (__typeof__(buffer))
		isaudk_stream_get_mixer_private( stream );

	if( mixer->fatal )
		return ISAUDK_FATAL;

	// Check the arguments
	if( idx > PER_STREAM_BUFFER_COUNT - 1 )
		return ISAUDK_INVALIDARG;

	if( count > get_buffer_sample_count( mixer ))
		return ISAUDK_INVALIDARG;

	// This would cause us to over-write a buffer that hasn't rendered
	if( UINT8_ADD( buffer->write_seq[idx], 1 ) != buffer->read_seq[idx] )
	{
		return ISAUDK_AGAIN;
	}

	pthread_mutex_lock( &mixer->mixer_state_lock );
	thread_exit_code = mixer->thread_exit_code;
	pthread_mutex_unlock( &mixer->mixer_state_lock );
	if( thread_exit_code != ISAUDK_SUCCESS )
		return thread_exit_code;

	buffer->count[idx] = count;
	if( eos )
		buffer->eos = idx;

	++buffer->write_seq[idx];
	isaudk_signal_send( mixer->wake_signal );

	return ISAUDK_SUCCESS;
}

struct mixer_loop_arg
{
	struct isaudk_mixer_handle *mixer;
	isaudk_signal_t signal;
};

// Inside loop reports fatal error
#define CHECKED_PTHREAD_CALL_FATAL(pthread_call,mixer)	\
	{						\
		int err;				\
		err = pthread_call;			\
		if( err != 0 ) {			\
			(mixer)->fatal_loop = true;	\
			return NULL;			\
		}					\
	}

#define MIXER_LOOP_LOCK_MIXER				\
	CHECKED_PTHREAD_CALL_FATAL			\
	( pthread_mutex_lock( &mixer->mixer_state_lock ), mixer )

#define MIXER_LOOP_UNLOCK_MIXER				\
	CHECKED_PTHREAD_CALL_FATAL			\
	( pthread_mutex_unlock( &mixer->mixer_state_lock ), mixer )

void *mixer_loop( void *_arg )
{
	struct mixer_loop_arg *arg = (struct mixer_loop_arg *) _arg;
	struct isaudk_mixer_handle *mixer = arg->mixer;
	isaudk_signal_error_t sigerr;
	struct per_stream_sample_buffer *stream_buffer;
	unsigned cross_timestamp_thresh = CROSSTSTAMP_THRESHOLD_INIT;
	int64_t buffer_cycles_done = 0;
	int64_t remainder_sample_count, wait_buffer_count = -10, wait_time;
	int64_t wait_samples, samples_played = 0;
	double small_remainder;

	void *buffer;
	void *buffer_from;
	void *next_buffer;
	void *audio_buffer;
	isaudk_stream_handle_t stream;

	mixer->running = true;
	mixer->initial_cross_time = INVALID_CROSS_TIME;
	mixer->curr_cross_time = INVALID_CROSS_TIME;
	mixer->samples_written = 0;
	mixer->playing = false;
	mixer->thread_exit_code = ISAUDK_SUCCESS;
	mixer->start_time.time = ULLONG_MAX;
	mixer->mode = ISAUDK_SILENCE;


	mixer->audio_initial_cross_time.sys.time = 0;
	mixer->audio_initial_cross_time.dev.time = 0;

	struct isaudk_cross_time init_cross_time;
	mixer->output->fn->get_cross_tstamp
		( mixer->output->ctx, &init_cross_time );

	// Signal the calling thread
	sigerr = isaudk_signal_send( arg->signal );
	if( sigerr != ISAUDK_SIGNAL_OK ) {
		MIXER_LOOP_LOCK_MIXER;
		mixer->running = false;
		mixer->thread_exit_code = ISAUDK_PTHREAD;
		MIXER_LOOP_UNLOCK_MIXER;

		return NULL;
	}

	while( !mixer->exit_req )
	{
		unsigned samples_to_write;
		bool write_result;
		struct isaudk_cross_time curr_cross_time;
		int16_t SILENCE[PS16_SAMPLE_COUNT] = {0};
		int16_t offset;
		int buffer_idx;

		MIXER_LOOP_LOCK_MIXER;
		stream = isaudk_mixer_reference_to_stream
			( ll_get_addr( ll_get_head( mixer->stream_list )) );
		MIXER_LOOP_UNLOCK_MIXER;

		stream_buffer = isaudk_stream_get_mixer_private( stream );
		stream_buffer->remainder = 0;
		buffer = get_buffer_pointer
			( &mixer->format,
			  stream_buffer->buffer + stream_buffer->idx );

		// Play buffer
		while( stream_buffer->read_seq[stream_buffer->idx] !=
		       stream_buffer->write_seq[stream_buffer->idx] )
		{
			isaudk_signal_wait( mixer->wake_signal, 0 );
		}

		samples_to_write = stream_buffer->count[stream_buffer->idx];
		if  (mixer->mode == ISAUDK_AUDIO) {
			buffer_idx = stream_buffer->idx;
			audio_buffer = get_buffer_pointer( &mixer->format, stream_buffer->buffer + buffer_idx );
			if ( buffer_idx == (PER_STREAM_BUFFER_COUNT - 1) ) {
				next_buffer = get_buffer_pointer( &mixer->format, stream_buffer->buffer );
			}
			else {
				next_buffer = get_buffer_pointer( &mixer->format, stream_buffer->buffer + buffer_idx + 1 );
			}
			memcpy( (void *)( SILENCE ), (const void *)( &((int16_t *)(audio_buffer))[PS16_SAMPLE_COUNT - offset] ), offset*sizeof(int16_t) );
			memcpy( (void *)( &(SILENCE[offset]) ), (const void *)(next_buffer), (PS16_SAMPLE_COUNT - offset)*sizeof(int16_t) );
			write_result = mixer->output->fn->queue_output_buffer( mixer->output->ctx, SILENCE, &samples_to_write );
		}
		else {
			write_result = mixer->output->fn->queue_output_buffer( mixer->output->ctx, SILENCE, &samples_to_write );
		}

		buffer_cycles_done++;
		samples_played += samples_to_write;

		if( !write_result )
		{
			MIXER_LOOP_LOCK_MIXER;
			mixer->thread_exit_code = ISAUDK_FATAL;
			MIXER_LOOP_UNLOCK_MIXER;
			break;
		}

		mixer->samples_written += samples_to_write;
		if  (mixer->mode == ISAUDK_AUDIO ) {
			++stream_buffer->read_seq[stream_buffer->idx];
		}

		if( !mixer->playing )
		{
			if( mixer->samples_written >
			    get_start_threshold( mixer ))
			{
				mixer->output->fn->start( mixer->output->ctx );

				MIXER_LOOP_LOCK_MIXER;
				mixer->output->fn->get_cross_tstamp
					( mixer->output->ctx,
					  &mixer->initial_cross_time );
				MIXER_LOOP_UNLOCK_MIXER;

				mixer->playing = true;
			}
		}

		if( mixer->playing )
		{
			mixer->output->fn->get_cross_tstamp
				( mixer->output->ctx, &curr_cross_time );
		}

		if( mixer->playing &&
		    ((mixer->curr_cross_time.dev.time <= cross_timestamp_thresh
		      && curr_cross_time.dev.time > cross_timestamp_thresh) ||
		     stream_buffer->eos == stream_buffer->idx ) )
		{
			double ratio;
			unsigned delta;

			ratio  = curr_cross_time.sys.time;
			ratio -= mixer->initial_cross_time.sys.time;
			ratio /= curr_cross_time.dev.time -
				mixer->initial_cross_time.dev.time;

			delta = mixer->initial_cross_time.dev.time * ratio;

			mixer->start_time = mixer->initial_cross_time.sys;

			mixer->start_time.time -= delta;

			cross_timestamp_thresh = curr_cross_time.dev.time * 2;

			if (( mixer->mode == ISAUDK_SILENCE )&&(cross_timestamp_thresh > CROSSTSTAMP_THRESHOLD_STOP )) {
				wait_time = mixer->requested_start_time.time - mixer->start_time.time;
					if (wait_time < 0) {
						mixer->start_time.time = 0xffffff;
					stream_buffer->idx = ISAUDK_INVALIDTIME;
					isaudk_signal_send( stream_buffer->signal );
					return NULL;
				}
				wait_samples = (int64_t)(((double)wait_time/1000000.0)*48.0);
				small_remainder = (((double)wait_time/1000000.0)*48.0 - wait_samples)*(1000000.0/48.0);
				wait_samples -= samples_played;
				wait_buffer_count = wait_samples/(PS16_SAMPLE_COUNT/2);
				remainder_sample_count = wait_samples%(PS16_SAMPLE_COUNT/2);
				buffer_cycles_done = 0;

				samples_to_write = PS16_SAMPLE_COUNT/2;
				offset = 2*remainder_sample_count;
				for (int i = 0; i < wait_buffer_count; i++) {
					write_result = mixer->output->fn->queue_output_buffer
						( mixer->output->ctx, SILENCE, &samples_to_write );
				}


				for (int i = 0; i < offset; i++) {
					SILENCE[i] = 0;
				}
				buffer_from = get_buffer_pointer( &mixer->format,stream_buffer->buffer );
				memcpy( (void *)( &(SILENCE[offset]) ), (const void *)buffer_from, (PS16_SAMPLE_COUNT - offset)*sizeof(int16_t) );

				write_result = mixer->output->fn->queue_output_buffer
					( mixer->output->ctx, SILENCE, &samples_to_write );

				mixer->audio_start_time.time =mixer->start_time.time + wait_time - small_remainder;
				stream_buffer->idx = 0;
				++stream_buffer->read_seq[stream_buffer->idx];
				stream_buffer->remainder = remainder_sample_count;
				mixer->start_time.time = mixer->requested_start_time.time;
				mixer->mode = ISAUDK_AUDIO;
			}


			// After stop threshold is reached stop
			if( cross_timestamp_thresh >
			    CROSSTSTAMP_THRESHOLD_STOP )
				cross_timestamp_thresh = 0;

		}

		if( mixer->playing )
		{
			MIXER_LOOP_LOCK_MIXER;
			mixer->curr_cross_time = curr_cross_time;
			// Signal for more data
			MIXER_LOOP_UNLOCK_MIXER;
		}
		if ( mixer->mode == ISAUDK_AUDIO ) {
			isaudk_signal_send( stream_buffer->signal );
			if( stream_buffer->eos == stream_buffer->idx )
			{
				mixer->output->fn->stop( mixer->output->ctx );
				break;
			}
			stream_buffer->idx = ( stream_buffer->idx + 1 ) %PER_STREAM_BUFFER_COUNT;
		}

	}

	// Exit normally
	mixer->running = false;

	// Send a signal at the end, if we exit abnormally the client may
	// be "hung" waiting for a signal
	isaudk_signal_send( stream_buffer->signal );

	return NULL;
}

#define COND_RETURN_UNLOCK(cond,stmt,retval)	\
	if(( cond ))				\
	{					\
		stmt;				\
		ret = (retval);			\
		goto unlock;			\
	}



isaudk_error_t
isaudk_start_mixer( struct isaudk_mixer_handle *mixer, struct isaudk_system_time start_time )
{
	struct mixer_loop_arg arg;
	isaudk_error_t ret = ISAUDK_SUCCESS;

	if( pthread_mutex_lock( &mixer->mixer_control_lock ) != 0 )
		return ISAUDK_PTHREAD;

	if( mixer->start_req )
	{
		COND_RETURN_UNLOCK( mixer->running,, ISAUDK_SUCCESS )
		else
			COND_RETURN_UNLOCK( mixer->fatal || mixer->fatal_loop,,
					    ISAUDK_FATAL )
			else
				COND_RETURN_UNLOCK( true,,
						    mixer->thread_exit_code );
	}

	COND_RETURN_UNLOCK
		( isaudk_create_signal( &arg.signal ) != ISAUDK_SIGNAL_OK,
		  mixer->fatal = true, ISAUDK_PTHREAD );

	arg.mixer = mixer;
	mixer->exit_req = false;
	mixer->requested_start_time = start_time;

	COND_RETURN_UNLOCK
		( pthread_create( &mixer->thread, NULL, mixer_loop, &arg ) !=
		  0, mixer->fatal = true, ISAUDK_PTHREAD );

	COND_RETURN_UNLOCK
		( isaudk_signal_wait( arg.signal, 0 ) != ISAUDK_SIGNAL_OK,
		  {
			  pthread_cancel( mixer->thread );
			  mixer->fatal = true; }, ISAUDK_PTHREAD )

	mixer->start_req = true;

unlock:
	pthread_mutex_unlock( &mixer->mixer_control_lock );

	return ret;
}

isaudk_error_t
isaudk_drain_mixer( struct isaudk_mixer_handle *mixer )
{
	if( mixer == NULL )
		return ISAUDK_INVALIDARG;

	pthread_join( mixer->thread, NULL );

	return ISAUDK_SUCCESS;
}

void mixer_stop( void )
{
	isaudk_drain_mixer( default_mixer );
}

void isaudk_mixer_get_stream_buffer( isaudk_stream_handle_t stream,
				     isaudk_sample_block_t **sample_buffer,
				     unsigned *count,
				     isaudk_signal_t *signal )
{
	struct per_stream_sample_buffer *buffer;

	buffer = isaudk_stream_get_mixer_private( stream );
	*sample_buffer = buffer->buffer;
	*count = PER_STREAM_BUFFER_COUNT;
	*signal = buffer->signal;
}

uint8_t
isaudk_mixer_get_current_index( isaudk_stream_handle_t stream, bool *roll )
{
	struct per_stream_sample_buffer *buffer;
	uint8_t retval;

	buffer = isaudk_stream_get_mixer_private( stream );
	retval = buffer->idx;

	if( buffer->write_seq[retval] != buffer->read_seq[retval] )
		*roll = true;
	else
		*roll = false;

	return buffer->idx;
}

bool
isaudk_mixer_get_flag( isaudk_stream_handle_t stream )
{
	struct per_stream_sample_buffer *buffer;
	bool retval;

	buffer = isaudk_stream_get_mixer_private( stream );
	retval = buffer->flag;
	return retval;
}

void
isaudk_mixer_clear_flag( isaudk_stream_handle_t stream )
{

	struct per_stream_sample_buffer *buffer;
	buffer = isaudk_stream_get_mixer_private( stream );
	buffer->flag = false;
}

int
isaudk_mixer_get_remainder( isaudk_stream_handle_t stream )
{
	struct per_stream_sample_buffer *buffer;
	int retval;

	buffer = isaudk_stream_get_mixer_private( stream );
	retval = buffer->remainder;
	return retval;
}

isaudk_error_t
isaudk_mixer_get_start_time( isaudk_mixer_handle_t mixer,
			     struct isaudk_system_time *start_time )
{
	struct isaudk_system_time start_time_tmp;
	isaudk_error_t error;

	pthread_mutex_lock( &mixer->mixer_state_lock );
	start_time_tmp = mixer->start_time;
	error = mixer->thread_exit_code;
	pthread_mutex_unlock( &mixer->mixer_state_lock );

	if( error != ISAUDK_SUCCESS )
		return error;

	if( start_time_tmp.time != ULLONG_MAX )
	{
		*start_time = start_time_tmp;
		return ISAUDK_SUCCESS;
	}

	return ISAUDK_AGAIN;
}

isaudk_error_t
isaudk_mixer_get_audio_start_time( isaudk_mixer_handle_t mixer,
			     struct isaudk_system_time *audio_start_time )
{
	struct isaudk_system_time audio_start_time_tmp;
	isaudk_error_t error;

	pthread_mutex_lock( &mixer->mixer_state_lock );
	audio_start_time_tmp = mixer->audio_start_time;
	error = mixer->thread_exit_code;
	pthread_mutex_unlock( &mixer->mixer_state_lock );

	if( error != ISAUDK_SUCCESS )
		return error;

	if( audio_start_time_tmp.time != ULLONG_MAX )
	{
		*audio_start_time = audio_start_time_tmp;
		return ISAUDK_SUCCESS;
	}

	return ISAUDK_AGAIN;
}

isaudk_error_t
isaudk_mixer_get_stream_info( struct isaudk_mixer_handle *handle,
			      struct isaudk_format *format,
			      isaudk_sample_rate_t *rate )
{
	if( format != NULL )
		*format = handle->format;
	if( rate != NULL )
		*rate = handle->rate;

	return ISAUDK_SUCCESS;
}
