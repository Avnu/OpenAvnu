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

//! \page play_at Play file at time
//! This application plays a file at the specified time. The command line is:
//! <br><br><tt>
//! play_file_at -st=\<start time\> -if=\<input file\>
//! </tt><br><br>
//! A start time of zero means start now
//!

#include <sdk.h>
#include <args.h>
#include <time.h>
#include <math.h>

#include <sndfile.h>
#include <stdio.h>
#include <string.h>

// Attempt to read a buffers worth of samples, detecting eof (end-of-stream)
// Returns the end-of-stream value, indicating that the stream is finished
static isaudk_error_t
readf_short_eos_detect
( SNDFILE *sndfile, isaudk_stream_handle_t stream, uint8_t channels,
  isaudk_sample_block_t *buffer, uint8_t idx, bool *eos)
{
	sf_count_t samples_read;

	samples_read = PS16_SAMPLE_COUNT/channels;
	samples_read = sf_readf_short
			( sndfile, buffer[idx].PS16, samples_read );

	if( samples_read < (PS16_SAMPLE_COUNT / channels)  ) {
		*eos = true;
	}
	else {
		*eos = false;
	}

	return isaudk_stream_set_buffer_sample_count
		( stream, idx, samples_read, *eos );
}

static isaudk_error_t
readf_short_multi_eos_detect
( SNDFILE *sndfile, isaudk_stream_handle_t stream, uint8_t channels,
  isaudk_sample_block_t *buffer, size_t *start, size_t end, size_t count,
  bool *eos, bool roll )
{
	size_t buffer_index;
	isaudk_error_t err;

	if( end < *start || ( end == *start && roll )) end += count;

	for( buffer_index = *start; buffer_index < end; ++buffer_index )
	{
		err = readf_short_eos_detect
			( sndfile, stream, channels, buffer,
			  buffer_index % count, eos);
		if( err != ISAUDK_SUCCESS )
		{
			if( err == ISAUDK_AGAIN )
			{
				break;
			}
			else
				return err;
		}
		if( *eos ) break;
	}

	*start = buffer_index % count;

	return ISAUDK_SUCCESS;
}


int main( int argc, char **argv )
{

	char *input_file = NULL;
	uint64_t start_time;

	int stop_idx;
	isaudk_parse_error_t parse_error;

	struct isaudk_format format;
	isaudk_stream_handle_t stream_handle;
	isaudk_error_t err;
	SNDFILE *audiofile = NULL;
	SF_INFO af_info, pre_info;
	isaudk_sample_rate_t rate;
	bool eos;
	bool roll;


	isaudk_sample_block_t *buffer;
	size_t buffer_count, buffer_index_prev, buffer_index = 0;
	isaudk_signal_t buffer_signal;

	struct isaudk_system_time requested_start_time, actual_start_time;

	struct isaudk_arg args[] =
	{
		ISAUDK_DECLARE_REQUIRED_ARG(START_TIME,&start_time),
		ISAUDK_DECLARE_REQUIRED_ARG(INPUT_FILE,&input_file),
	};

	stop_idx = isaudk_parse_args( args, sizeof(args)/sizeof(args[0]),
				      argc-1, argv+1, &parse_error );
	if( parse_error != ISAUDK_PARSE_SUCCESS )
	{
		printf( "Error parsing arguments\n" );
		return -1;
	}

	memset( &af_info, 0, sizeof( af_info ));
	memset( &pre_info, 0, sizeof( pre_info ));
	if( input_file != NULL ) {
		audiofile = sf_open( input_file, SFM_READ, &af_info );
	}
	if( audiofile == NULL )
	{
		printf( "Unable to open audio file: %s\n",
			input_file == NULL ? "(unspecified)" : input_file );
		return -1;
	}

	format.encoding = ISAUDK_ENC_PS16;
	format.channels = af_info.channels;
	rate = af_info.samplerate / ISAUDK_RATE_MULTIPLIER;
	err = isaudk_open_stream
		( &stream_handle, &format, rate, ISAUDK_RENDER );

	isaudk_get_stream_buffers( stream_handle, &buffer, &buffer_count,
				   &buffer_signal );

	//fill audio buffer
	buffer_index_prev = 0;
	err = readf_short_multi_eos_detect
		( audiofile, stream_handle, format.channels, buffer,
		  &buffer_index_prev, buffer_count, buffer_count, &eos,
		  false);
	if( err != ISAUDK_SUCCESS )
	{
		printf( "Failed to write audio buffer\n" );
		return -1;
	}


	requested_start_time.time = start_time;
	isaudk_start_stream_at( stream_handle, requested_start_time);

	buffer_index_prev = 0;
	while( !eos )
	{
		// Wait for signal
		isaudk_signal_wait( buffer_signal, 0 );

		// Get HW position
		buffer_index = isaudk_get_stream_buffer_index
			( stream_handle, &roll );
		if (buffer_index == ISAUDK_INVALIDTIME) { break; }

		// Write up to HW position
		err = readf_short_multi_eos_detect
			( audiofile, stream_handle, format.channels, buffer,
			  &buffer_index_prev, buffer_index, buffer_count,
			  &eos, roll);
		if( err != ISAUDK_SUCCESS )
		{
			printf( "Failed to write audio buffer\n" );
			return -1;
		}

	}

	err = isaudk_get_stream_audio_start_time( stream_handle, &actual_start_time );
	printf("%lu/n", actual_start_time.time);

	if (err == ISAUDK_INVALIDTIME) { return -1; }
	return 0;
}
