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

//! \page record_at Record file at time
//!
//! This application records a file at the specified time. The command line is:
//! <br><br><tt>
//! record_file_at -st=\<start time\> -of=\<output file\>
//! </tt><br><br>
//! A start time of zero means start now
//!
//! At exit the actual start time is printed to standard out

#include <sdk.h>
#include <args.h>

#include <sndfile.h>
#include <stdio.h>
#include <string.h>

#define NANOSEC_PER_MILLISEC	(1000000ULL)
#define NANOSEC_PER_SEC		(1000000000)
#define DEFAULT_DURATION	(5000)

// Returns sample count (round up) given milliseconds
static uint32_t
milliseconds_to_samples( uint32_t millis, isaudk_sample_rate_t rate )
{
	uint64_t ret;
	uint32_t tmp;

	ret = millis*NANOSEC_PER_MILLISEC;
	tmp = NANOSEC_PER_SEC / ( rate * ISAUDK_RATE_MULTIPLIER );
	ret = ret / tmp + (ret % tmp > 0 ? 1 : 0);

	return ret;
}

static isaudk_error_t
write_buffer_to_disk( SNDFILE *sndfile, isaudk_stream_handle_t stream,
		      uint8_t channels, isaudk_sample_block_t *buffer,
		      size_t *start_idx, size_t stop_idx, size_t count,
		      bool roll, size_t *sample_count )
{
	size_t current_idx;
	isaudk_error_t err;
	sf_count_t sf_count;

	if( stop_idx < *start_idx || ( stop_idx == *start_idx && roll ))
		stop_idx += count;

	for( current_idx = *start_idx; current_idx < stop_idx; ++current_idx )
	{
		sf_count = sf_writef_short
			( sndfile, buffer[current_idx % count].PS16,
			  PS16_SAMPLE_COUNT / channels );
		if( sf_count != PS16_SAMPLE_COUNT / channels )
			return ISAUDK_FATAL;

		*sample_count = *sample_count + sf_count;

		err = isaudk_stream_mark_buffer_done
			( stream, current_idx % count );
		if( err != ISAUDK_SUCCESS )
		{
			if( err == ISAUDK_AGAIN )
				break;

			else
				return err;

		}
	}
	*start_idx = current_idx % count;

	return ISAUDK_SUCCESS;
}

int main( int argc, char **argv )
{
	char *output_file = NULL;
	uint64_t start_time;
	uint32_t duration = DEFAULT_DURATION;

	int stop_idx;
	isaudk_parse_error_t parse_error;

	SNDFILE *audiofile = NULL;
	SF_INFO af_info;

	isaudk_stream_handle_t stream_handle;
	struct isaudk_format format;
	isaudk_sample_rate_t rate;

	isaudk_sample_block_t *buffer;
	size_t buffer_count;
	isaudk_signal_t buffer_signal;
	size_t capture_index, prev_capture_index;
	bool roll;
	struct isaudk_system_time actual_start_time;

	size_t sample_count;

	struct isaudk_arg args[] =
	{
		ISAUDK_DECLARE_REQUIRED_ARG(START_TIME,&start_time),
		ISAUDK_DECLARE_REQUIRED_ARG(OUTPUT_FILE,&output_file),
		ISAUDK_DECLARE_OPTIONAL_ARG(DURATION,&duration),
	};

	stop_idx = isaudk_parse_args( args, sizeof(args)/sizeof(args[0]),
				      argc-1, argv+1, &parse_error );
	if( parse_error != ISAUDK_PARSE_SUCCESS )
	{
		printf( "Error parsing arguments\n" );
		return -1;
	}

	memset( &af_info, 0, sizeof( af_info ));
	isaudk_open_default_stream( &stream_handle, ISAUDK_CAPTURE );
	isaudk_get_stream_info( stream_handle, &format, &rate, NULL );
	af_info.samplerate = rate * ISAUDK_RATE_MULTIPLIER;
	af_info.channels = format.channels;
	af_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	if( output_file != NULL )
		audiofile = sf_open( output_file, SFM_WRITE, &af_info );
	if( audiofile == NULL )
	{
		printf( "Unable to open audio file: %s\n",
			output_file == NULL ? "(unspecified)" : output_file );
		return -1;
	}

	isaudk_get_stream_buffers( stream_handle, &buffer, &buffer_count,
				   &buffer_signal );

	isaudk_start_stream_at( stream_handle, ISAUDK_PLAY_IMMED );

	prev_capture_index = 0;
	sample_count = 0;
	while( sample_count < milliseconds_to_samples( duration, rate ))
	{
		isaudk_signal_wait( buffer_signal, 0 );

		capture_index = isaudk_get_stream_buffer_index
			( stream_handle, &roll );

		write_buffer_to_disk( audiofile, stream_handle,
				      format.channels, buffer,
				      &prev_capture_index, capture_index,
				      buffer_count, roll, &sample_count );
	}

	isaudk_get_stream_start_time( stream_handle, &actual_start_time );
	printf( "Start time: %lu\n", actual_start_time.time );

	sf_close( audiofile );

	return 0;
}
