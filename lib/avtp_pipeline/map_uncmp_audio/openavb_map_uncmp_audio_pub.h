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
* HEADER SUMMARY : Uncompressed Audio mapping module public interface
* 
* Refer to IEC 61883-6 for details of the "source packet" structure.
* 
* The protocol_specific_header and CIP header.
* 
* map_nv_tx_rate must be set in the .ini file.
*/

#ifndef OPENAVB_MAP_UNCMP_AUDIO_PUB_H
#define OPENAVB_MAP_UNCMP_AUDIO_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_audio_pub.h"
#include "openavb_intf_pub.h"

/** \file
 * Uncompressed Audio mapping module public interface.
 *
 * Refer to IEC 61883-6 for details of the "source packet" structure.
 * The protocol_specific_header and CIP header.
 * map_nv_tx_rate must be set in the .ini file.
 */

/** \note A define is used for the MediaQDataFormat identifier because it is
 * needed in separate execution units (static / dynamic libraries) that is why
 * a single static (static/extern pattern) definition can not be used.
 */
#define MapUncmpAudioMediaQDataFormat "UncmpAudio"

/** Defines AAF timestamping mode:
 * - TS_SPARSE_MODE_DISABLED - timestamp is valid in every avtp packet
 * - TS_SPARSE_MODE_ENABLED - timestamp is valid in every 8th avtp packet
 */
typedef enum {
	/// Unspecified
	TS_SPARSE_MODE_UNSPEC		= 0,
	/// Disabled
	TS_SPARSE_MODE_DISABLED		= 1,
	/// Enabled
	TS_SPARSE_MODE_ENABLED		= 8
} avb_audio_sparse_mode_t;

/** Contains detailed information of the audio format.
 * \note Interface module has to set during the RX and TX init callbacks:
 * - audioRate,
 * - audioType,
 * - audioBitDepth,
 * - audioEndian,
 * - audioChannels,
 * - sparseMode.
 * \note The rest of fields mapping module will set these during the RX and TX
 * init callbacks. The interface module can use these during the RX and TX
 * callbacks.
 */
typedef struct {
	/// Rate of audio
	avb_audio_rate_t audioRate;
	/// Sample data type
	avb_audio_type_t audioType;
	/// Bit depth of audio
	avb_audio_bit_depth_t audioBitDepth;
	/// Sample endianess
	avb_audio_endian_t audioEndian;
	/// Number of channels
	avb_audio_channels_t audioChannels;
	/// Sparse timestamping mode
	avb_audio_sparse_mode_t sparseMode;

	// The mapping module will set these during the RX and TX init callbacks
	// The interface module can use these during the RX and TX callbacks.
	/// Number of frames for one data packet
	U32 framesPerPacket;
	/// Size of one sample in bytes
	U32 packetSampleSizeBytes;
	/// Size of one frame in bytes (framesPerPacket * audioChannels)
	U32 packetFrameSizeBytes;
	/// Size of packet (packetFrameSizeBytes * framesPerPacket)
	U32 packetAudioDataSizeBytes;
	/// Number of frames of audio to accept in one Media Queue Item
	U32 packingFactor;
	/// Number of frames per one Media Queue Item
	U32 framesPerItem;
	/// Item sample size in bytes (usually the same as packetSampleSizeBytes)
	U32 itemSampleSizeBytes;
	/// Media Queue Item Frame size in bytes
	U32 itemFrameSizeBytes;
	/// Media Queue Item size
	U32 itemSize;
	/// synchronization time interval
	U32 sytInterval;

	/// CB for interface modules to do translations in place before data is moved into the mediaQ on rx.	
	openavb_intf_rx_translate_cb_t	intf_rx_translate_cb;

	/// Interface Module may set this presentation latency which listener mapping modules will use to adjust the presetnation time
	S32 presentationLatencyUSec;
	
} media_q_pub_map_uncmp_audio_info_t;

#endif  // OPENAVB_MAP_UNCMP_AUDIO_PUB_H
