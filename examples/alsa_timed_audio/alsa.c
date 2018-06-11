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

#include <audio_output.h>
#include <audio_input.h>
#include <stack.h>
#include <init.h>

#include <asoundlib.h>

#include <limits.h>

void alsa_init( void ) _isaudk_io_init;

#define DEFAULT_ALSA_DEVICE "hw:0,0"

#define ALSA_PERIOD_TIME	( 20000		/* microsec */)
#define ALSA_PERIODS_PER_BUFFER	( 15 ) /* ALSA always allocates 1 extra */

typedef enum
{
	ISAUDK_ALSA_INTERNAL_ERROR,
	ISAUDK_ALSA_FORMAT_ERROR,
	ISAUDK_ALSA_OK,
} isaudk_alsa_error_t;

struct alsa_context
{
	snd_pcm_t			*alsa_handle;
	snd_pcm_status_t		*alsa_status;
	char *devname;
	isaudk_alsa_error_t	 last_error;
	const char		*last_error_func;
	unsigned short		 last_error_line;
};

struct isaudk_input_context
{
	struct alsa_context ctx;
};

struct isaudk_output_context
{
	struct alsa_context ctx;
};

const struct isaudk_output *default_alsa_output;

const struct isaudk_output *isaudk_get_default_alsa_output()
{
	return default_alsa_output;
}

const struct isaudk_input *default_alsa_input;

const struct isaudk_input *isaudk_get_default_alsa_input()
{
	return default_alsa_input;
}

static inline void
alsa_no_error( struct alsa_context *ctx )
{
	ctx->last_error = ISAUDK_ALSA_OK;
	ctx->last_error_func = "";
	ctx->last_error_line = 0;
}

static inline void
_alsa_set_error( struct alsa_context *ctx, isaudk_alsa_error_t error,
		const char *func, unsigned short line )
{
	ctx->last_error = error;
	ctx->last_error_func = func;
	ctx->last_error_line = line;
}

#define alsa_set_error( ctx, error )			\
	_alsa_set_error( ctx, error, __FUNCTION__, __LINE__ );

#define CHECK_ALSA_RESULT( err_code, ctx )				\
	if( err < 0 )							\
	{								\
		alsa_set_error( ctx, err_code );			\
		return false;						\
	}

static bool
_alsa_set_audio_param( struct alsa_context *ctx,
		       struct isaudk_format *format,
		       unsigned rate )
{
        snd_pcm_hw_params_t *hwparams;
        snd_pcm_sw_params_t *swparams;
	snd_pcm_audio_tstamp_config_t tstamp_config;
	snd_pcm_format_t alsa_format;

	unsigned int period_time;
	unsigned int periods_per_buffer;
	int dir;
	int err;

	if( format == NULL )
	{
		alsa_set_error( ctx, ISAUDK_ALSA_FORMAT_ERROR );
		return false;
	}
	switch( format->encoding )
	{
	default:
		// Unsupported format request
		alsa_set_error( ctx, ISAUDK_ALSA_FORMAT_ERROR );
		return false;
	case ISAUDK_ENC_PS16:
		alsa_format = SND_PCM_FORMAT_S16;
		break;
	}

        snd_pcm_hw_params_alloca( &hwparams );
        snd_pcm_sw_params_alloca( &swparams );

	err = snd_pcm_hw_params_any( ctx->alsa_handle, hwparams );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	err = snd_pcm_hw_params_set_rate_resample( ctx->alsa_handle, hwparams,
						   0 );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	err = snd_pcm_hw_params_set_access( ctx->alsa_handle, hwparams,
					    SND_PCM_ACCESS_MMAP_INTERLEAVED );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	err = snd_pcm_hw_params_set_format( ctx->alsa_handle, hwparams,
					    alsa_format );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_FORMAT_ERROR, ctx );

	err = snd_pcm_hw_params_set_channels
		( ctx->alsa_handle, hwparams, format->channels );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_FORMAT_ERROR, ctx );

	err = snd_pcm_hw_params_set_rate
		( ctx->alsa_handle, hwparams, rate*ISAUDK_RATE_MULTIPLIER, 0 );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_FORMAT_ERROR, ctx );

	dir = 1;
	period_time = ALSA_PERIOD_TIME;
	err = snd_pcm_hw_params_set_period_time_near
		( ctx->alsa_handle, hwparams, &period_time, &dir );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	dir = 1;
	periods_per_buffer = ALSA_PERIODS_PER_BUFFER;
	err = snd_pcm_hw_params_set_periods_near
		( ctx->alsa_handle, hwparams, &periods_per_buffer, &dir );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	err = snd_pcm_hw_params( ctx->alsa_handle, hwparams );

	err = snd_pcm_sw_params_current( ctx->alsa_handle, swparams );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	err = snd_pcm_sw_params_set_tstamp_mode
		( ctx->alsa_handle, swparams, SND_PCM_TSTAMP_ENABLE );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	err = snd_pcm_sw_params_set_tstamp_type
		( ctx->alsa_handle, swparams,
		  //		  SND_PCM_TSTAMP_TYPE_GETTIMEOFDAY );
		  SND_PCM_TSTAMP_TYPE_MONOTONIC_RAW );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	tstamp_config.type_requested = 5;
	tstamp_config.report_delay = 0;
	snd_pcm_status_set_audio_htstamp_config
		( ctx->alsa_status, &tstamp_config );

	err = snd_pcm_sw_params_set_start_threshold
		( ctx->alsa_handle, swparams, INT_MAX );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	err = snd_pcm_sw_params( ctx->alsa_handle, swparams );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	alsa_no_error( ctx );
	return true;
}

static bool
alsa_set_audio_output_param( struct isaudk_output_context *ctx,
			     struct isaudk_format *format,
			     unsigned rate )
{
	int err;

	err = snd_pcm_open( &ctx->ctx.alsa_handle, ctx->ctx.devname,
			    SND_PCM_STREAM_PLAYBACK, 0 );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, &ctx->ctx );

	return _alsa_set_audio_param( &ctx->ctx, format, rate );
}

static bool
alsa_set_audio_input_param( struct isaudk_input_context *ctx,
			     struct isaudk_format *format,
			     unsigned rate )
{
	int err;

	err = snd_pcm_open( &ctx->ctx.alsa_handle, ctx->ctx.devname,
			    SND_PCM_STREAM_CAPTURE, 0 );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, &ctx->ctx );

	return _alsa_set_audio_param( &ctx->ctx, format, rate );
}

#define NSEC_PER_SEC (1000000000)

#define snd_htstamp_to_systime( ts )				\
	((struct isaudk_system_time)					\
	{ .time = (ts)->tv_sec*NSEC_PER_SEC*1ULL + (ts)->tv_nsec })

#define snd_htstamp_to_audiotime( ts )				\
	((struct isaudk_audio_time)					\
	{ .time = (ts)->tv_sec*NSEC_PER_SEC*1ULL + (ts)->tv_nsec })

bool _alsa_get_cross_tstamp( struct alsa_context *ctx,
			     struct isaudk_cross_time *time )
{
	int err;
	snd_htimestamp_t sys_ts, audio_ts;
	snd_pcm_state_t state;

	err = snd_pcm_status( ctx->alsa_handle, ctx->alsa_status );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	state = snd_pcm_status_get_state( ctx->alsa_status );
	switch( state )
	{
		// These states will give valid results
	case SND_PCM_STATE_RUNNING:
	case SND_PCM_STATE_DRAINING:
		break;
		// Other states will not
	default:
		return false;
	}

	snd_pcm_status_get_htstamp( ctx->alsa_status, &sys_ts );
	snd_pcm_status_get_audio_htstamp( ctx->alsa_status, &audio_ts );
	time->sys = snd_htstamp_to_systime( &sys_ts );
	time->dev = snd_htstamp_to_audiotime( &audio_ts );

	alsa_no_error( ctx );
	return true;
}

bool _alsa_start( struct alsa_context *ctx )
{
	int err;

	alsa_no_error( ctx );
	err = snd_pcm_start( ctx->alsa_handle );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	alsa_no_error( ctx );
	return true;
}

bool _alsa_stop( struct alsa_context *ctx )
{
	int err;

	err = snd_pcm_drain( ctx->alsa_handle );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	err = snd_pcm_close( ctx->alsa_handle );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, ctx );

	alsa_no_error( ctx );
	return true;
}

bool alsa_get_output_cross_tstamp( struct isaudk_output_context *ctx,
				   struct isaudk_cross_time *time )
{
	return _alsa_get_cross_tstamp( &ctx->ctx, time );
}

bool alsa_start_output( isaudk_output_context_t ctx )
{
	return _alsa_start( &ctx->ctx );
}

bool alsa_stop_output( isaudk_output_context_t ctx )
{
	return _alsa_stop( &ctx->ctx );
}

bool alsa_queue_output_buffer( isaudk_output_context_t ctx,
			       void *buffer, unsigned *count )
{
	snd_pcm_sframes_t err;

	err = snd_pcm_mmap_writei( ctx->ctx.alsa_handle, buffer, *count );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, &ctx->ctx );

	*count = err;
	alsa_no_error( &ctx->ctx );
	return true;
}

static struct isaudk_output_fn default_alsa_output_fn =
{
	.set_audio_param		= alsa_set_audio_output_param,
	.get_cross_tstamp		= alsa_get_output_cross_tstamp,
	.start				= alsa_start_output,
	.stop				= alsa_stop_output,
	.queue_output_buffer		= alsa_queue_output_buffer,
};

void alsa_delete_output( struct isaudk_output *output )
{
	if( output == NULL )
		return;
	else
	{
		if( output->ctx != NULL )
		{
			if( output->ctx->ctx.devname != NULL )
				free(( void * ) output->ctx->ctx.devname );
			snd_pcm_status_free( output->ctx->ctx.alsa_status );
			free(( void * ) output->ctx );
		}
		free(( void * ) output );
	}
}

struct isaudk_output *
alsa_open_output( char *devname )
{
	struct isaudk_output *output;
	int err;

	output = (__typeof__( output )) malloc(( size_t ) sizeof( *output ));
	if( output == NULL )
		goto error;

	output->ctx = (__typeof__( output->ctx ))
		malloc(( size_t ) sizeof( *output->ctx ));
	if( output->ctx == NULL )
		goto error;

	output->ctx->ctx.devname = (__typeof__( output->ctx->ctx.devname ))
		malloc(( size_t ) sizeof( *output->ctx->ctx.devname )*
		       strlen( devname ) + 1 );
	if( output->ctx->ctx.devname == NULL )
		goto error;
	strncpy( output->ctx->ctx.devname, DEFAULT_ALSA_DEVICE, 7 );

	err = snd_pcm_status_malloc( &output->ctx->ctx.alsa_status );
	if( err == 0 )
	{
		alsa_no_error( &output->ctx->ctx );
		output->fn = &default_alsa_output_fn;
		return output;
	}

error:
	alsa_delete_output( output );
	return NULL;
}

bool alsa_get_input_cross_tstamp( struct isaudk_input_context *ctx,
				   struct isaudk_cross_time *time )
{
	return _alsa_get_cross_tstamp( &ctx->ctx, time );
}

bool alsa_start_input( isaudk_input_context_t ctx )
{
	return _alsa_start( &ctx->ctx );
}

bool alsa_stop_input( isaudk_input_context_t ctx )
{
	return _alsa_stop( &ctx->ctx );
}

bool alsa_read_input_buffer( isaudk_input_context_t ctx,
			     void *buffer, unsigned *count )
{
	snd_pcm_sframes_t err;

	err = snd_pcm_mmap_readi( ctx->ctx.alsa_handle, buffer, *count );
	CHECK_ALSA_RESULT( ISAUDK_ALSA_INTERNAL_ERROR, &ctx->ctx );

	*count = err;
	alsa_no_error( &ctx->ctx );
	return true;
}

static struct isaudk_input_fn default_alsa_input_fn =
{
	.set_audio_param		= alsa_set_audio_input_param,
	.get_cross_tstamp		= alsa_get_input_cross_tstamp,
	.start				= alsa_start_input,
	.stop				= alsa_stop_input,
	.read_input_buffer		= alsa_read_input_buffer,
};

void alsa_delete_input( struct isaudk_input *input )
{
	if( input == NULL )
		return;
	else
	{
		if( input->ctx != NULL )
		{
			if( input->ctx->ctx.devname != NULL )
				free(( void * ) input->ctx->ctx.devname );
			snd_pcm_status_free( input->ctx->ctx.alsa_status );
			free(( void * ) input->ctx );
		}
		free(( void * ) input );
	}
}

struct isaudk_input *
alsa_open_input( char *devname )
{
	struct isaudk_input *input;
	int err;

	input = (__typeof__( input )) malloc(( size_t ) sizeof( *input ));
	if( input == NULL )
		goto error;

	input->ctx = (__typeof__( input->ctx ))
		malloc(( size_t ) sizeof( *input->ctx ));
	if( input->ctx == NULL )
		goto error;

	input->ctx->ctx.devname = (__typeof__( input->ctx->ctx.devname ))
		malloc(( size_t ) sizeof( *input->ctx->ctx.devname )*
		       strlen( devname ) + 1 );
	if( input->ctx->ctx.devname == NULL )
		goto error;
	strncpy( input->ctx->ctx.devname, DEFAULT_ALSA_DEVICE, 7 );

	err = snd_pcm_status_malloc( &input->ctx->ctx.alsa_status );
	if( err == 0 )
	{
		alsa_no_error( &input->ctx->ctx );
		input->fn = &default_alsa_input_fn;
		return input;
	}

error:
	alsa_delete_input( input );
	return NULL;
}

void
alsa_init( void )
{
	default_alsa_output = alsa_open_output( DEFAULT_ALSA_DEVICE );
	default_alsa_input = alsa_open_input( DEFAULT_ALSA_DEVICE );
}
