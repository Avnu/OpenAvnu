/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 
1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
Attributions: The inih library portion of the source code is licensed from 
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt. 
Complete license and copyright information can be found at 
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

/*
* MODULE SUMMARY : General Audio Types Public
*/

#ifndef AVB_AUDIO_PUB_H
#define AVB_AUDIO_PUB_H 1

/** \file
 * General audio types
 */

/** Audio rate
 */
typedef enum {
	/// 8000
	AVB_AUDIO_RATE_8KHZ 		= 8000,
	/// 11025
	AVB_AUDIO_RATE_11_025KHZ 	= 11025,
	/// 16000
	AVB_AUDIO_RATE_16KHZ 		= 16000,
	/// 22050
	AVB_AUDIO_RATE_22_05KHZ		= 22050,
	/// 32000
	AVB_AUDIO_RATE_32KHZ		= 32000,
	/// 44100
	AVB_AUDIO_RATE_44_1KHZ		= 44100,
	/// 48000
	AVB_AUDIO_RATE_48KHZ		= 48000,
	/// 64000
	AVB_AUDIO_RATE_64KHZ		= 64000,
	/// 88200
	AVB_AUDIO_RATE_88_2KHZ		= 88200,
	/// 96000
	AVB_AUDIO_RATE_96KHZ		= 96000,
	/// 176400
	AVB_AUDIO_RATE_176_4KHZ		= 176400,
	/// 192000
	AVB_AUDIO_RATE_192KHZ		= 192000
} avb_audio_rate_t;

/** Defines what type is data.
 *
 * Information is needed together with endianes and bit depth to configure the
 * sample format correctly
 */
typedef enum {
	/// Data type undefined
	AVB_AUDIO_TYPE_UNSPEC,
	/// Data type int
	AVB_AUDIO_TYPE_INT,
	/// Data type unsigned int
	AVB_AUDIO_TYPE_UINT,
	/// Data type float
	AVB_AUDIO_TYPE_FLOAT,
} avb_audio_type_t;

/** Defines endianess of data.
 *
 * Information is needed together with data type and bit depth to configure the
 * sample format correctly
 */
typedef enum {
	/// Unspecified
	AVB_AUDIO_ENDIAN_UNSPEC,
	/// Little endian
	AVB_AUDIO_ENDIAN_LITTLE,
	/// Big endian
	AVB_AUDIO_ENDIAN_BIG,
} avb_audio_endian_t;

/** Bit depth of audio.
 *
 * Information is needed together with endianes and data type to configure the
 * sample format correctly
 */
typedef enum {
	/// 1 bit
	AVB_AUDIO_BIT_DEPTH_1BIT	= 1,
	/// 8 bit
	AVB_AUDIO_BIT_DEPTH_8BIT	= 8,
	/// 16 bit
	AVB_AUDIO_BIT_DEPTH_16BIT	= 16,
	/// 20 bit
	AVB_AUDIO_BIT_DEPTH_20BIT	= 20,
	/// 24 bit
	AVB_AUDIO_BIT_DEPTH_24BIT	= 24,
	/// 32 bit
	AVB_AUDIO_BIT_DEPTH_32BIT	= 32,
	/// 48 bit
	AVB_AUDIO_BIT_DEPTH_48BIT	= 48,
	/// 64 bit
	AVB_AUDIO_BIT_DEPTH_64BIT	= 64
} avb_audio_bit_depth_t;

/** Number of channels
 */
typedef enum {
	/// 1 channel
	AVB_AUDIO_CHANNELS_1		= 1,
	/// 2 channels
	AVB_AUDIO_CHANNELS_2		= 2,
	/// 3 channels
	AVB_AUDIO_CHANNELS_3		= 3,
	/// 4 channels
	AVB_AUDIO_CHANNELS_4		= 4,
	/// 5 channels
	AVB_AUDIO_CHANNELS_5		= 5,
	/// 6 channels
	AVB_AUDIO_CHANNELS_6		= 6,
	/// 7 channels
	AVB_AUDIO_CHANNELS_7		= 7,
	/// 8 channels
	AVB_AUDIO_CHANNELS_8		= 8
} avb_audio_channels_t;

/** Media Clock Recovery.
 */
typedef enum {
	/// No Media Clock Recovery is Done, this is the default
	AVB_MCR_NONE,
	/// Media Clock Recovery done by using AVTP timestamps
	AVB_MCR_AVTP_TIMESTAMP,
	/// Media Clock Recovery done by using 1722(a), Clock Reference Stream (CRS)
	AVB_MCR_CRS
}avb_audio_mcr_t;

#endif // AVB_AUDIO_PUB_H
