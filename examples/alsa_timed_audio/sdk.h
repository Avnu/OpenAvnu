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

#ifndef SDK_H
#define SDK_H

#include <bool.h>

#include <thread_signal.h>

#include <stdint.h>
#include <unistd.h>

//! \addtogroup StreamAPI Stream API
//! The Stream API is is used to create time synchronized audio applications.
//! It implements a mixer and capture module that are time-aware. The API is
//! similar in scope (albeit simplified) to PulseAudio adding calls to start
//! audio at a requested system time and returns the actual start time. It
//! provides a simplified API as well as a mixer that is able to mix streams
//! with a precision of +/- .5 sample.

//! \typedef isaudk_stream_handle_t
//! \brief Audio stream reference
//! \detail	Each is either capture or render
//!
//! \typedef isaudk_mixer_handle_t
//! \brief Audio mixer reference.
//! \details	One per audio output device. Default mixer is sufficient for
//!		most users
//!
//! \typedef isaudk_capture_handle_t
//! \brief Audio capture reference.
//! \details	One per audio input device. Default capture is sufficient for
//!		most users
//!
typedef struct isaudk_stream_handle*	isaudk_stream_handle_t;
typedef struct isaudk_mixer_handle*	isaudk_mixer_handle_t;
typedef struct isaudk_capture_handle*	isaudk_capture_handle_t;

//! \enum isaudk_direction_t
//! \brief Stream direction.
//! \details	Either render or capture
typedef enum
{
	ISAUDK_RENDER,		//!< Render stream
	ISAUDK_CAPTURE,		//!< Capture stream
} isaudk_direction_t;

// \enum isaudk_mode_t
// \either pre-play mode (playing zeros)
// \or audio-play mode (playing audio file)
typedef enum
{
	ISAUDK_SILENCE,		//play silence
	ISAUDK_AUDIO,		//play audio
} isaudk_mode_t;

//! \enum isaudk_error_t
//! \brief Error return value.
typedef enum
{
	ISAUDK_SUCCESS,	    	//!< No error
	ISAUDK_NOMEMORY,	//!< Failed to allocate dynamic memory
	ISAUDK_PTHREAD,		//!< Error internal to pthread library
	ISAUDK_BADFORMAT,	//!< Requested format is unavailable
	ISAUDK_INVALIDARG,	//!< One or more arguments are invalid
	ISAUDK_UNIMPL,		//!< Feature not implemented
	ISAUDK_AGAIN,		//!< Procedure may succeed if called again
	ISAUDK_FATAL,		//!< Error is unrecoverable, system error
	ISAUDK_INVALIDTIME = 255,	//!< An invalid start time was requested
} isaudk_error_t;

//! \enum isaudk_encoding_t
//! \brief Frame sample encoding.
//! \details	Includes frame layout (e.g. default is interleaved)
typedef enum
{
	ISAUDK_ENC_NONE,	//!< Defer format
	ISAUDK_ENC_PS16 = 0x02,	//!< PCM signed 16 bit
} isaudk_encoding_t;

//! \def PS16_SAMPLE_COUNT
//! \brief 16 bit signed sample count per sample block.
//!
//! \def PS16_JUMBO_SAMPLE_COUNT
//! \brief 16 bit signed sample count per jumbo sample block.
//!
#define PS16_SAMPLE_COUNT 2016
#define PS16_JUMBO_SAMPLE_COUNT 24576

//! \struct isaudk_format
//! \brief Audio frame format.
//! \details	Consists of sample encoding (including frame layout e.g.
//!		interleaved is default) and number of channels
struct isaudk_format
{
	isaudk_encoding_t encoding; //!< per-sample encoding / frame layout
	uint8_t channels;	    //!< channel count
};

//! \def ISAUDK_RATE_MULTIPLIER
//! \brief Multiplier for sample rate constant.
//! \details	Results in number of samples per second
//!
//! \typedef isaudk_sample_rate_t
//! \brief Sample rate.
//! \details	multiply by rate multiplier for samples per second
//!		\see ISAUDK_RATE_MULTIPLIER
//! \var _48K_SAMPLE_RATE
//! \brief 48 KHz sample rate
#define ISAUDK_RATE_MULTIPLIER 25
typedef unsigned short isaudk_sample_rate_t;
extern const isaudk_sample_rate_t _48K_SAMPLE_RATE;


//! \union isaudk_sample_block_t
//! \brief Sample block.
//! \details	Maximum 4096 byte block containing a whole number of audio
//!		frames for common channel configuration and sample sizes
typedef union
{
	int16_t PS16[2*PS16_SAMPLE_COUNT]; //!< 16 bit signed sample values
} isaudk_sample_block_t;

//! \union isaudk_jumbo_sample_block_t
//! \brief Large sample block.
//! \details	Maximum 12 x 4096 byte block containing a whole number of audio
//!		frames for common channel configuration and sample sizes.
//!		Contains several milliseconds of audio at high sample rates
//!		and 8 channel audio
typedef union
{
	int16_t PS16[PS16_JUMBO_SAMPLE_COUNT];
} isaudk_jumbo_sample_block_t;

//! \struct isaudk_system_time
//! \brief Local system time.
//! \details	In terms of POSIX-ish CLOCK_MONOTONIC_RAW
struct isaudk_system_time
{
	uint64_t time; //!< 64-bit system time scalar
};

//! \var ISAUDK_PLAY_IMMED
//! \brief system time indicating that playback or record begins immediately
extern const struct isaudk_system_time ISAUDK_PLAY_IMMED;

//! \struct isaudk_audio_time
//! \brief Audio device time.
//! \details	Is the sample count multiplied by nominal nanoseconds per
//!		sample. Includes fractional samples.
struct isaudk_audio_time
{
	uint64_t time;
};

//! \struct isaudk_cross_time
//! \brief Correlated audio and system time.
//! \details	Captured simultaneously. Degree of simultaneity depends on
//!		timestamp function of underlying audio driver. If available,
//!		ALSA can capture timestamps using hardware
struct isaudk_cross_time
{
	struct isaudk_system_time	sys;
	struct isaudk_audio_time	dev;
};

//! \brief Check cross-timestamps for equality
//!
//! \param a one cross-timestamp
//! \param b another cross-timestamp
//!
//! \return true if cross-timestamps are equal
static inline bool
isaudk_crosstime_equal( const struct isaudk_cross_time *a,
			const struct isaudk_cross_time *b )
{
	if( a->sys.time != b->sys.time )
		return false;
	if( a->dev.time != b->dev.time )
		return false;

	return true;
}

//! \brief Opens render stream.
//! \details	Using specified mixer.
//! \ingroup StreamAPI
//!
//! \param handle[out]		stream_handle
//! \param format[in]		audio frame format
//! \param rate[in]		sample rate
//! \param mixer[in]		mixer handle (NULL: use default)
//!
//! \return Return code
isaudk_error_t
isaudk_open_stream_mixer( isaudk_stream_handle_t *handle,
			  struct isaudk_format *format,
			  isaudk_sample_rate_t rate,
			  isaudk_mixer_handle_t mixer );

//! \brief Opens capture stream.
//! \details	Using specified capture module.
//! \ingroup StreamAPI
//!
//! \param handle[out]		stream_handle
//! \param format[in]		audio frame format
//! \param rate[in]		sample rate
//! \param capture[in]		capture module handle (NULL: use default)
//!
//! \return Return code
isaudk_error_t
isaudk_open_stream_capture( isaudk_stream_handle_t *handle,
			  struct isaudk_format *format,
			  isaudk_sample_rate_t rate,
			  isaudk_capture_handle_t capture );

//! \brief Opens stream.
//! \details	Using default mixer or capture module.
//! \ingroup StreamAPI
//!
//! \param handle[out]		stream_handle
//! \param format[in]		audio frame format
//! \param rate[in]		sample rate
//! \param direction[in]	specify capture or render \see
//!				isaudk_direction_t
//! \return Return code
isaudk_error_t
isaudk_open_stream( isaudk_stream_handle_t *handle,
		    struct isaudk_format *format,
		    isaudk_sample_rate_t rate,
		    isaudk_direction_t direction );

//! \brief Opens stream using default stream format/rate.
//! \ingroup StreamAPI
//!
//! \param handle[out]		stream_handle
//! \param direction[in]	specify capture or render \see
//!				isaudk_direction_t
//! \return Return code
isaudk_error_t
isaudk_open_default_stream( isaudk_stream_handle_t *handle,
			    isaudk_direction_t direction );

//! \brief Get stream info.
//! \details	Useful after stream is opened using default parameters
//!		\see isaudk_open_default_stream
//! \ingroup StreamAPI
//!
//! \param handle[in]		stream_handle
//! \param format[out]		audio frame format
//! \param rate[out]		sample rate
//! \param direction[out]	specify capture or render \see
//!				isaudk_direction_t
//! \return Return code
isaudk_error_t
isaudk_get_stream_info( isaudk_stream_handle_t handle,
			struct isaudk_format *format,
			isaudk_sample_rate_t *rate,
			isaudk_direction_t *direction );

//! \brief Get buffers.
//! \details	Exchange data with mixer or capture module.
//! \ingroup StreamAPI
//!
//! \param handle[in]		stream_handle
//! \param buffer[out]		audio buffer array
//! \param count[out]		buffer array size
//! \param signal[out]		signals when mixer is ready for more data or
//!				capture stream is ready for reading
//! \return Return code
void
isaudk_get_stream_buffers( isaudk_stream_handle_t handle,
			   isaudk_sample_block_t **buffer, size_t *count,
			   isaudk_signal_t *signal );

//! \brief Get buffers.
//! \details	Useful after stream is opened using default parameters
//!		\see isaudk_open_default_stream
//! \ingroup StreamAPI
//!
//! \param handle[in]		stream_handle
//! \param buffer[out]		audio buffer array
//! \param count[out]		buffer array size
//! \param signal[out]		signals when mixer is ready for more data or
//!				capture stream is ready for reading
//! \return Return code
isaudk_error_t
isaudk_stream_set_buffer_sample_count( isaudk_stream_handle_t handle,
				       size_t idx, size_t count, bool eos );

//! \brief Get current buffer index for render.
//! \details	Useful after stream is opened using default parameters
//!		\see isaudk_open_default_stream
//! \ingroup StreamAPI
//!
//! \param handle[in]		stream_handle
//! \param roll[out]		Current buffer index is ready to read, may
//!				indicate that wrap-around has occured
//! \return Buffer index
uint8_t
isaudk_get_stream_buffer_index( isaudk_stream_handle_t handle, bool *roll );

//! \brief Mark buffer index as consumed.
//! \ingroup StreamAPI
//!
//! \param handle[in]		stream_handle
//! \param idx[in]		Buffer index consumed
//!
//! \return Return code
isaudk_error_t
isaudk_stream_mark_buffer_done( isaudk_stream_handle_t stream, size_t idx );

//! \brief Request stream start time.
//! \details	Is accurated within 1 samples period. \see ISAUDK_PLAY_IMMED
//! \ingroup StreamAPI
//!
//! \param handle[in]		stream_handle
//! \param start_time[out]	Start time in nanoseconds
//!
//! \return Return code
isaudk_error_t
isaudk_start_stream_at( isaudk_stream_handle_t handle,
			struct isaudk_system_time start_time );

//! \brief Get actual stream start time.
//! \details	In nanosecond units, accurate within system clock tick
//!		usually sub-microsecond
//! \ingroup StreamAPI
//!
//! \param handle[in]		stream_handle
//! \param roll[out]		Current buffer index is ready to read, may
//!				indicate that wrap-around has occured
//! \return Return code
isaudk_error_t
isaudk_get_stream_start_time( isaudk_stream_handle_t handle,
		       struct isaudk_system_time *start_time );
isaudk_error_t
isaudk_get_stream_audio_start_time( isaudk_stream_handle_t handle,
		       struct isaudk_system_time *start_time );

#endif/*SDK_H*/
