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
 * MODULE SUMMARY : Implementation for AAF mapping module
 *
 * AAF is defined in IEEE 1722-rev1/D12 (still in draft as of Feb 2015).
 */

#include <stdlib.h>
#include <string.h>
#include "openavb_mcr_hal_pub.h"
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_avtp_time_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_map_pub.h"
#include "openavb_map_aaf_audio_pub.h"

#define	AVB_LOG_COMPONENT	"AAF Mapping"
#include "openavb_log_pub.h"

#define AVTP_SUBTYPE_AAF			2

// Header sizes (bytes)
#define AVTP_V0_HEADER_SIZE			12
#define AAF_HEADER_SIZE				12
#define TOTAL_HEADER_SIZE			(AVTP_V0_HEADER_SIZE + AAF_HEADER_SIZE)

// - 1 Byte - TV bit (timestamp valid)
#define HIDX_AVTP_HIDE7_TV1			1

// - 1 Byte - TU bit (timestamp uncertain)
#define HIDX_AVTP_HIDE7_TU1			3

// - 2 bytes	Stream data length
#define HIDX_STREAM_DATA_LEN16		20

// - 1 Byte - SP bit (sparse mode)
#define HIDX_AVTP_HIDE7_SP			22
#define SP_M0_BIT					(1 << 4)

typedef enum {
	AAF_RATE_UNSPEC = 0,
	AAF_RATE_8K,
	AAF_RATE_16K,
	AAF_RATE_32K,
	AAF_RATE_44K1,
	AAF_RATE_48K,
	AAF_RATE_88K2,
	AAF_RATE_96K,
	AAF_RATE_176K4,
	AAF_RATE_192K,
} aaf_nominal_sample_rate_t;

typedef enum {
	AAF_FORMAT_UNSPEC = 0,
	AAF_FORMAT_FLOAT_32,
	AAF_FORMAT_INT_32,
	AAF_FORMAT_INT_24,
	AAF_FORMAT_INT_16,
} aaf_sample_format_t;

typedef enum {
	AAF_STATIC_CHANNELS_LAYOUT	= 0,
	AAF_MONO_CHANNELS_LAYOUT	= 1,
	AAF_STEREO_CHANNELS_LAYOUT	= 2,
	AAF_5_1_CHANNELS_LAYOUT		= 3,
	AAF_7_1_CHANNELS_LAYOUT		= 4,
	AAF_MAX_CHANNELS_LAYOUT		= 15,
} aaf_automotive_channels_layout_t;

typedef struct {
	/////////////
	// Config data
	/////////////
	// map_nv_item_count
	U32 itemCount;

	// Transmit interval in frames per second. 0 = default for talker class.
	U32 txInterval;

	// A multiple of how many frames of audio to accept in an media queue item and
	// into the AVTP payload above the minimal needed.
	U32 packingFactor;

	// MCR mode
	avb_audio_mcr_t audioMcr;

	// MCR timestamp interval	
	U32 mcrTimestampInterval;

	// MCR clock recovery interval	
	U32 mcrRecoveryInterval;
	
	/////////////
	// Variable data
	/////////////
	U32 maxTransitUsec;     // In microseconds

	aaf_nominal_sample_rate_t 	aaf_rate;
	aaf_sample_format_t			aaf_format;
	U8							aaf_bit_depth;
	U32 payloadSize;

	U8 aaf_event_field;

	bool dataValid;

	U32 intervalCounter;

	U32 sparseMode;

	bool mediaQItemSyncTS;

} pvt_data_t;

static void x_calculateSizes(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		media_q_pub_map_aaf_audio_info_t *pPubMapInfo = pMediaQ->pPubMapInfo;
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return;
		}

		switch (pPubMapInfo->audioRate) {
			case AVB_AUDIO_RATE_8KHZ:
				pPvtData->aaf_rate = AAF_RATE_8K;
				break;
			case AVB_AUDIO_RATE_16KHZ:
				pPvtData->aaf_rate = AAF_RATE_16K;
				break;
			case AVB_AUDIO_RATE_32KHZ:
				pPvtData->aaf_rate = AAF_RATE_32K;
				break;
			case AVB_AUDIO_RATE_44_1KHZ:
				pPvtData->aaf_rate = AAF_RATE_44K1;
				break;
			case AVB_AUDIO_RATE_48KHZ:
				pPvtData->aaf_rate = AAF_RATE_48K;
				break;
			case AVB_AUDIO_RATE_88_2KHZ:
				pPvtData->aaf_rate = AAF_RATE_88K2;
				break;
			case AVB_AUDIO_RATE_96KHZ:
				pPvtData->aaf_rate = AAF_RATE_96K;
				break;
			case AVB_AUDIO_RATE_176_4KHZ:
				pPvtData->aaf_rate = AAF_RATE_176K4;
				break;
			case AVB_AUDIO_RATE_192KHZ:
				pPvtData->aaf_rate = AAF_RATE_192K;
				break;
			default:
				AVB_LOG_ERROR("Invalid audio frequency configured");
				pPvtData->aaf_rate = AAF_RATE_UNSPEC;
				break;
		}
		AVB_LOGF_INFO("aaf_rate=%d (%dKhz)", pPvtData->aaf_rate, pPubMapInfo->audioRate);

		char *typeStr = "int";
		if (pPubMapInfo->audioType == AVB_AUDIO_TYPE_FLOAT) {
			typeStr = "float";
			switch (pPubMapInfo->audioBitDepth) {
				case AVB_AUDIO_BIT_DEPTH_32BIT:
					pPvtData->aaf_format = AAF_FORMAT_FLOAT_32;
					pPubMapInfo->itemSampleSizeBytes = 4;
					pPubMapInfo->packetSampleSizeBytes = 4;
					pPvtData->aaf_bit_depth = 32;
					break;
				default:
					AVB_LOG_ERROR("Invalid audio bit-depth configured for float");
					pPvtData->aaf_format = AAF_FORMAT_UNSPEC;
					break;
			}
		}
		else {
			switch (pPubMapInfo->audioBitDepth) {
				case AVB_AUDIO_BIT_DEPTH_32BIT:
					pPvtData->aaf_format = AAF_FORMAT_INT_32;
					pPubMapInfo->itemSampleSizeBytes = 4;
					pPubMapInfo->packetSampleSizeBytes = 4;
					pPvtData->aaf_bit_depth = 32;
					break;
				case AVB_AUDIO_BIT_DEPTH_24BIT:
					pPvtData->aaf_format = AAF_FORMAT_INT_24;
					pPubMapInfo->itemSampleSizeBytes = 3;
					pPubMapInfo->packetSampleSizeBytes = 3;
					pPvtData->aaf_bit_depth = 24;
					break;
				case AVB_AUDIO_BIT_DEPTH_16BIT:
					pPvtData->aaf_format = AAF_FORMAT_INT_16;
					pPubMapInfo->itemSampleSizeBytes = 2;
					pPubMapInfo->packetSampleSizeBytes = 2;
					pPvtData->aaf_bit_depth = 16;
					break;
#if 0
					// should work - test content?
				case AVB_AUDIO_BIT_DEPTH_20BIT:
					pPvtData->aaf_format = AAF_FORMAT_INT_24;
					pPubMapInfo->itemSampleSizeBytes = 3;
					pPubMapInfo->packetSampleSizeBytes = 3;
					pPvtData->aaf_bit_depth = 20;
					break;
					// would require byte-by-byte copy
				case AVB_AUDIO_BIT_DEPTH_8BIT:
					pPvtData->aaf_format = AAF_FORMAT_INT_24;
					pPubMapInfo->itemSampleSizeBytes = 1;
					pPubMapInfo->packetSampleSizeBytes = 2;
					pPvtData->aaf_bit_depth = 8;
					break;
#endif
				default:
					AVB_LOG_ERROR("Invalid audio bit-depth configured");
					pPvtData->aaf_format = AAF_FORMAT_UNSPEC;
					break;
			}
		}
		AVB_LOGF_INFO("aaf_format=%d (%s%d)",
			pPvtData->aaf_format, typeStr, pPubMapInfo->audioBitDepth);

		// Audio frames per packet
		pPubMapInfo->framesPerPacket = (pPubMapInfo->audioRate / pPvtData->txInterval);
		if (pPubMapInfo->audioRate % pPvtData->txInterval != 0) {
			AVB_LOGF_WARNING("Audio rate (%d) is not integer multiple of TX interval (%d)",
				pPubMapInfo->audioRate, pPvtData->txInterval);
			pPubMapInfo->framesPerPacket += 1;
		}
		AVB_LOGF_INFO("Frames/packet = %d", pPubMapInfo->framesPerPacket);

		// AAF packet size calculations
		pPubMapInfo->packetFrameSizeBytes = pPubMapInfo->packetSampleSizeBytes * pPubMapInfo->audioChannels;
		pPvtData->payloadSize = pPubMapInfo->framesPerPacket * pPubMapInfo->packetFrameSizeBytes;
		AVB_LOGF_INFO("packet: sampleSz=%d * channels=%d => frameSz=%d * %d => payloadSz=%d",
			pPubMapInfo->packetSampleSizeBytes,
			pPubMapInfo->audioChannels,
			pPubMapInfo->packetFrameSizeBytes,
			pPubMapInfo->framesPerPacket,
			pPvtData->payloadSize);

		// MediaQ item size calculations
		pPubMapInfo->packingFactor = pPvtData->packingFactor;
		pPubMapInfo->framesPerItem = pPubMapInfo->framesPerPacket * pPvtData->packingFactor;
		pPubMapInfo->itemFrameSizeBytes = pPubMapInfo->itemSampleSizeBytes * pPubMapInfo->audioChannels;
		pPubMapInfo->itemSize = pPubMapInfo->itemFrameSizeBytes * pPubMapInfo->framesPerItem;
		AVB_LOGF_INFO("item: sampleSz=%d * channels=%d => frameSz=%d * %d * packing=%d => itemSz=%d",
			pPubMapInfo->itemSampleSizeBytes,
			pPubMapInfo->audioChannels,
			pPubMapInfo->itemFrameSizeBytes,
			pPubMapInfo->framesPerPacket,
			pPubMapInfo->packingFactor,
			pPubMapInfo->itemSize);
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}


// Each configuration name value pair for this mapping will result in this callback being called.
void openavbMapAVTPAudioCfgCB(media_q_t *pMediaQ, const char *name, const char *value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return;
		}

		if (strcmp(name, "map_nv_item_count") == 0) {
			char *pEnd;
			pPvtData->itemCount = strtol(value, &pEnd, 10);
		}
		else if (strcmp(name, "map_nv_packing_factor") == 0) {
			char *pEnd;
			pPvtData->packingFactor = strtol(value, &pEnd, 10);
		}
		else if (strcmp(name, "map_nv_tx_rate") == 0
			|| strcmp(name, "map_nv_tx_interval") == 0) {
			char *pEnd;
			pPvtData->txInterval = strtol(value, &pEnd, 10);
		}
		else if (strcmp(name, "map_nv_sparse_mode") == 0) {
			char* pEnd;
			U32 tmp;
			tmp = strtol(value, &pEnd, 10);
			if (*pEnd == '\0' && tmp == 1) {
				pPvtData->sparseMode = TS_SPARSE_MODE_ENABLED;
			}
			else if (*pEnd == '\0' && tmp == 0) {
				pPvtData->sparseMode = TS_SPARSE_MODE_DISABLED;
			}
		}
		else if (strcmp(name, "map_nv_audio_mcr") == 0) {
			char *pEnd;
			pPvtData->audioMcr = (avb_audio_mcr_t)strtol(value, &pEnd, 10);
		}
		else if (strcmp(name, "map_nv_mcr_timestamp_interval") == 0) {
			char *pEnd;
			pPvtData->mcrTimestampInterval = strtol(value, &pEnd, 10);
		}
		else if (strcmp(name, "map_nv_mcr_recovery_interval") == 0) {
			char *pEnd;
			pPvtData->mcrRecoveryInterval = strtol(value, &pEnd, 10);
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

U8 openavbMapAVTPAudioSubtypeCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return AVTP_SUBTYPE_AAF;        // AAF AVB subtype
}

// Returns the AVTP version used by this mapping
U8 openavbMapAVTPAudioAvtpVersionCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return 0x00;        // Version 0
}

U16 openavbMapAVTPAudioMaxDataSizeCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return 0;
		}

		AVB_TRACE_EXIT(AVB_TRACE_MAP);
		return pPvtData->payloadSize + TOTAL_HEADER_SIZE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return 0;
}

// Returns the intended transmit interval (in frames per second). 0 = default for talker / class.
U32 openavbMapAVTPAudioTransmitIntervalCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return 0;
		}

		AVB_TRACE_EXIT(AVB_TRACE_MAP);
		return pPvtData->txInterval;
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return 0;
}

void openavbMapAVTPAudioGenInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	if (pMediaQ) {
		media_q_pub_map_uncmp_audio_info_t *pPubMapInfo = pMediaQ->pPubMapInfo;
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return;
		}

		x_calculateSizes(pMediaQ);
		openavbMediaQSetSize(pMediaQ, pPvtData->itemCount, pPubMapInfo->itemSize);

		pPvtData->dataValid = TRUE;

		pPubMapInfo->sparseMode = pPvtData->sparseMode;
		if (pPubMapInfo->sparseMode == TS_SPARSE_MODE_ENABLED) {
			AVB_LOG_INFO("Sparse timestamping mode: enabled");
		} else {
			AVB_LOG_INFO("Sparse timestamping mode: disabled");
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// A call to this callback indicates that this mapping module will be
// a talker. Any talker initialization can be done in this function.
void openavbMapAVTPAudioTxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// CORE_TODO: This callback should be updated to work in a similiar way the uncompressed audio mapping. With allowing AVTP packets to be built 
//  from multiple media queue items. This allows interface to set into the media queue blocks of audio frames to properly correspond to
//  a SYT_INTERVAL. Additionally the public data member sytInterval needs to be set in the same way the uncompressed audio mapping does.
// This talker callback will be called for each AVB observation interval.
tx_cb_ret_t openavbMapAVTPAudioTxCB(media_q_t *pMediaQ, U8 *pData, U32 *dataLen)
{
	media_q_item_t *pMediaQItem = NULL;
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);

	if (!pData || !dataLen) {
		AVB_LOG_ERROR("Mapping module data or data length argument incorrect.");
		return TX_CB_RET_PACKET_NOT_READY;
	}

	if (pMediaQ)
		pMediaQItem = openavbMediaQTailLock(pMediaQ, TRUE);

	if (pMediaQItem) {
		if (pMediaQItem->dataLen > 0) {

			U32 tmp32;
			U8 *pHdrV0 = pData;
			U32 *pHdr = (U32 *)(pData + AVTP_V0_HEADER_SIZE);
			U8  *pPayload = pData + TOTAL_HEADER_SIZE;
			media_q_pub_map_aaf_audio_info_t *pPubMapInfo = pMediaQ->pPubMapInfo;
			pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
			if (!pPvtData) {
				AVB_LOG_ERROR("Private mapping module data not allocated.");
				return TX_CB_RET_PACKET_NOT_READY;
			}
			// timestamp set in the interface module, here just validate
			if (openavbAvtpTimeTimestampIsValid(pMediaQItem->pAvtpTime)) {

				// Add the max transit time.
				openavbAvtpTimeAddUSec(pMediaQItem->pAvtpTime, pPvtData->maxTransitUsec);

				// Set timestamp valid flag
				pHdrV0[HIDX_AVTP_HIDE7_TV1] |= 0x01;

				// Set (clear) timestamp uncertain flag
				if (openavbAvtpTimeTimestampIsUncertain(pMediaQItem->pAvtpTime))
					pHdrV0[HIDX_AVTP_HIDE7_TU1] |= 0x01;
				else pHdrV0[HIDX_AVTP_HIDE7_TU1] &= ~0x01;

				// - 4 bytes	avtp_timestamp
				*pHdr++ = htonl(openavbAvtpTimeGetAvtpTimestamp(pMediaQItem->pAvtpTime));

				openavbAvtpTimeSetTimestampValid(pMediaQItem->pAvtpTime, FALSE);
			}
			else {
				// Clear timestamp valid flag
				pHdrV0[HIDX_AVTP_HIDE7_TV1] &= ~0x01;
				pHdr++; // Move past the timestamp field
			}

			// - 4 bytes	format info (format, sample rate, channels per frame, bit depth)
			tmp32 = pPvtData->aaf_format << 24;
			tmp32 |= pPvtData->aaf_rate  << 20;
			tmp32 |= pPubMapInfo->audioChannels << 8;
			tmp32 |= pPvtData->aaf_bit_depth;
			*pHdr++ = htonl(tmp32);

			// - 4 bytes	packet info (data length, evt field)
			tmp32 = pPvtData->payloadSize << 16;
			tmp32 |= pPvtData->aaf_event_field << 8;
			*pHdr++ = htonl(tmp32);

			// Set (clear) sparse mode flag
			if (pPvtData->sparseMode == TS_SPARSE_MODE_ENABLED) {
				pHdrV0[HIDX_AVTP_HIDE7_SP] |= SP_M0_BIT;
			} else {
				pHdrV0[HIDX_AVTP_HIDE7_SP] &= ~SP_M0_BIT;
			}

			if ((*dataLen - TOTAL_HEADER_SIZE) < pPvtData->payloadSize) {
				AVB_LOG_ERROR("Not enough room in packet for payload");
				AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
				return TX_CB_RET_PACKET_NOT_READY;
			}

			if ((pMediaQItem->dataLen - pMediaQItem->readIdx) < pPvtData->payloadSize) {
				// This should not happen so we will just toss it away.
				AVB_LOG_ERROR("Not enough data in media queue item for packet");
				openavbMediaQTailPull(pMediaQ);
				AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
				return TX_CB_RET_PACKET_NOT_READY;
			}

			memcpy(pPayload, (uint8_t *)pMediaQItem->pPubData + pMediaQItem->readIdx, pPvtData->payloadSize);

			pMediaQItem->readIdx += pPvtData->payloadSize;
			if (pMediaQItem->readIdx >= pMediaQItem->dataLen) {
				// Finished reading the entire item
				openavbMediaQTailPull(pMediaQ);
			}
			else {
				// More to read next interval
				openavbMediaQTailUnlock(pMediaQ);
			}

			// Set outbound data length (entire packet length)
			*dataLen = pPvtData->payloadSize + TOTAL_HEADER_SIZE;

			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return TX_CB_RET_PACKET_READY;
		}
		else {
			openavbMediaQTailPull(pMediaQ);
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return TX_CB_RET_PACKET_NOT_READY;  // No payload
		}

	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return TX_CB_RET_PACKET_NOT_READY;
}

// A call to this callback indicates that this mapping module will be
// a listener. Any listener initialization can be done in this function.
void openavbMapAVTPAudioRxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return;
		}
		if (pPvtData->audioMcr != AVB_MCR_NONE) {
			HAL_INIT_MCR_V2(pPvtData->txInterval, pPvtData->packingFactor, pPvtData->mcrTimestampInterval, pPvtData->mcrRecoveryInterval);
		}
		bool badPckFctrValue = FALSE;
		if (pPvtData->sparseMode == TS_SPARSE_MODE_ENABLED) {
			// sparse mode enabled so check packing factor
			// listener should work correct for packing_factors:
			// 1, 2, 4, 8, 16, 24, 32, 40, 48, (+8) ...
			if (pPvtData->packingFactor < 8) {
				// check if power of 2
				if ((pPvtData->packingFactor & (pPvtData->packingFactor - 1)) != 0) {
					badPckFctrValue = TRUE;
				}
			} else {
				// check if multiple of 8
				if (pPvtData->packingFactor % 8 != 0) {
					badPckFctrValue = TRUE;
				}
			}
			if (badPckFctrValue) {
				AVB_LOGF_WARNING("Wrong packing factor value set (%d) for sparse timestamping mode", pPvtData->packingFactor);
			}
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This callback occurs when running as a listener and data is available.
bool openavbMapAVTPAudioRxCB(media_q_t *pMediaQ, U8 *pData, U32 dataLen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	if (pMediaQ && pData) {
		U8 *pHdrV0 = pData;
		U32 *pHdr = (U32 *)(pData + AVTP_V0_HEADER_SIZE);
		U8  *pPayload = pData + TOTAL_HEADER_SIZE;
		media_q_pub_map_aaf_audio_info_t *pPubMapInfo = pMediaQ->pPubMapInfo;
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return FALSE;
		}

		int tmp;
		bool dataValid = TRUE;

		U32 timestamp = ntohl(*pHdr++);
		U32 format_info = ntohl(*pHdr++);
		U32 packet_info = ntohl(*pHdr++);

		bool listenerSparseMode = (pPvtData->sparseMode == TS_SPARSE_MODE_ENABLED) ? TRUE : FALSE;
		bool streamSparseMode = (pHdrV0[HIDX_AVTP_HIDE7_SP] & SP_M0_BIT) ? TRUE : FALSE;
		U16 payloadLen = ntohs(*(U16 *)(&pHdrV0[HIDX_STREAM_DATA_LEN16]));

		if (payloadLen  > dataLen - TOTAL_HEADER_SIZE) {
			if (pPvtData->dataValid)
				AVB_LOGF_ERROR("header data len %d > actual data len %d",
					       payloadLen, dataLen - TOTAL_HEADER_SIZE);
			dataValid = FALSE;
		}

		if ((tmp = ((format_info >> 24) & 0xFF)) != pPvtData->aaf_format) {
			if (pPvtData->dataValid)
				AVB_LOGF_ERROR("Listener format %d doesn't match received data (%d)",
					pPvtData->aaf_format, tmp);
			dataValid = FALSE;
		}
		if ((tmp = ((format_info >> 20) & 0x0F)) != pPvtData->aaf_rate) {
			if (pPvtData->dataValid)
				AVB_LOGF_ERROR("Listener sample rate (%d) doesn't match received data (%d)",
					pPvtData->aaf_rate, tmp);
			dataValid = FALSE;
		}
		if ((tmp = ((format_info >> 8) & 0x3FF)) != pPubMapInfo->audioChannels) {
			if (pPvtData->dataValid)
				AVB_LOGF_ERROR("Listener channel count (%d) doesn't match received data (%d)",
					pPubMapInfo->audioChannels, tmp);
			dataValid = FALSE;
		}
		if ((tmp = (format_info & 0xFF)) != pPvtData->aaf_bit_depth) {
			if (pPvtData->dataValid)
				AVB_LOGF_ERROR("Listener bit depth (%d) doesn't match received data (%d)",
					pPvtData->aaf_bit_depth, tmp);
			dataValid = FALSE;
		}
		if ((tmp = ((packet_info >> 16) & 0xFFFF)) != pPvtData->payloadSize) {
			if (pPvtData->dataValid)
				AVB_LOGF_ERROR("Listener payload size (%d) doesn't match received data (%d)",
					pPvtData->payloadSize, tmp);
			dataValid = FALSE;
		}
		if ((tmp = ((packet_info >> 8) & 0x0F)) != pPvtData->aaf_event_field) {
			if (pPvtData->dataValid)
				AVB_LOGF_ERROR("Listener event field (%d) doesn't match received data (%d)",
					pPvtData->aaf_event_field, tmp);
		}
		if (streamSparseMode != listenerSparseMode) {
			if (pPvtData->dataValid)
				AVB_LOGF_ERROR("Listener sparse mode (%d) doesn't match stream sparse mode (%d)",
					listenerSparseMode, streamSparseMode);
			dataValid = FALSE;
		}

		if (dataValid) {
			if (!pPvtData->dataValid) {
				AVB_LOG_INFO("RX data valid, stream un-muted");
				pPvtData->dataValid = TRUE;
			}

			// Get item pointer in media queue
			media_q_item_t *pMediaQItem = openavbMediaQHeadLock(pMediaQ);
			if (pMediaQItem) {
				// set timestamp if first data written to item
				if (pMediaQItem->dataLen == 0) {

					// Set timestamp valid flag
					openavbAvtpTimeSetTimestampValid(pMediaQItem->pAvtpTime, (pHdrV0[HIDX_AVTP_HIDE7_TV1] & 0x01) ? TRUE : FALSE);

					if (openavbAvtpTimeTimestampIsValid(pMediaQItem->pAvtpTime)) {
						// Get the timestamp and place it in the media queue item.
						openavbAvtpTimeSetToTimestamp(pMediaQItem->pAvtpTime, timestamp);

						openavbAvtpTimeSubUSec(pMediaQItem->pAvtpTime, pPubMapInfo->presentationLatencyUSec);

						// Set timestamp uncertain flag
						openavbAvtpTimeSetTimestampUncertain(pMediaQItem->pAvtpTime, (pHdrV0[HIDX_AVTP_HIDE7_TU1] & 0x01) ? TRUE : FALSE);
						// Set flag to inform that MediaQ is synchronized with timestamped packets
						 pPvtData->mediaQItemSyncTS = TRUE;
					}
					else if (!pPvtData->mediaQItemSyncTS) {
						//we need packet with valid TS for first data written to item
						AVB_LOG_DEBUG("Timestamp not valid for MediaQItem - initial packets dropped");
						IF_LOG_INTERVAL(1000) AVB_LOG_ERROR("Timestamp not valid for MediaQItem - initial packets dropped");
						dataValid = FALSE;
					}
				}
				if (dataValid) {
					if (pPubMapInfo->intf_rx_translate_cb) {
						pPubMapInfo->intf_rx_translate_cb(pMediaQ, pPayload, pPvtData->payloadSize);
					}

					memcpy((uint8_t *)pMediaQItem->pPubData + pMediaQItem->dataLen, pPayload, pPvtData->payloadSize);
					pMediaQItem->dataLen += pPvtData->payloadSize;
				}

				if (pMediaQItem->dataLen < pMediaQItem->itemSize) {
					// More data can be written to the item
					openavbMediaQHeadUnlock(pMediaQ);
				}
				else {
					// The item is full push it.
					openavbMediaQHeadPush(pMediaQ);
				}

				AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
				return TRUE;    // Normal exit
			}
			else {
				IF_LOG_INTERVAL(1000) AVB_LOG_ERROR("Media queue full");
				AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
				return FALSE;   // Media queue full
			}
		}
		else {
			if (pPvtData->dataValid) {
				AVB_LOG_INFO("RX data invalid, stream muted");
				pPvtData->dataValid = FALSE;
			}
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return FALSE;
}

// This callback will be called when the mapping module needs to be closed.
// All cleanup should occur in this function.
void openavbMapAVTPAudioEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return;
		}

		if (pPvtData->audioMcr != AVB_MCR_NONE) {
			HAL_CLOSE_MCR_V2();
		}

		pPvtData->mediaQItemSyncTS = FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

void openavbMapAVTPAudioGenEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Initialization entry point into the mapping module. Will need to be included in the .ini file.
extern DLL_EXPORT bool openavbMapAVTPAudioInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		pMediaQ->pMediaQDataFormat = strdup(MapAVTPAudioMediaQDataFormat);
		pMediaQ->pPubMapInfo = calloc(1, sizeof(media_q_pub_map_aaf_audio_info_t));      // Memory freed by the media queue when the media queue is destroyed.
		pMediaQ->pPvtMapInfo = calloc(1, sizeof(pvt_data_t));                            // Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pMediaQDataFormat || !pMediaQ->pPubMapInfo || !pMediaQ->pPvtMapInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for mapping module");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;

		pMapCB->map_cfg_cb = openavbMapAVTPAudioCfgCB;
		pMapCB->map_subtype_cb = openavbMapAVTPAudioSubtypeCB;
		pMapCB->map_avtp_version_cb = openavbMapAVTPAudioAvtpVersionCB;
		pMapCB->map_max_data_size_cb = openavbMapAVTPAudioMaxDataSizeCB;
		pMapCB->map_transmit_interval_cb = openavbMapAVTPAudioTransmitIntervalCB;
		pMapCB->map_gen_init_cb = openavbMapAVTPAudioGenInitCB;
		pMapCB->map_tx_init_cb = openavbMapAVTPAudioTxInitCB;
		pMapCB->map_tx_cb = openavbMapAVTPAudioTxCB;
		pMapCB->map_rx_init_cb = openavbMapAVTPAudioRxInitCB;
		pMapCB->map_rx_cb = openavbMapAVTPAudioRxCB;
		pMapCB->map_end_cb = openavbMapAVTPAudioEndCB;
		pMapCB->map_gen_end_cb = openavbMapAVTPAudioGenEndCB;

		pPvtData->itemCount = 20;
		pPvtData->txInterval = 4000;  // default to something that wont cause divide by zero
		pPvtData->packingFactor = 1;
		pPvtData->maxTransitUsec = inMaxTransitUsec;
		pPvtData->sparseMode = TS_SPARSE_MODE_DISABLED;
		pPvtData->mcrTimestampInterval = 144;
		pPvtData->mcrRecoveryInterval = 512;
		pPvtData->aaf_event_field = AAF_STATIC_CHANNELS_LAYOUT;
		pPvtData->intervalCounter = 0;
		pPvtData->mediaQItemSyncTS = FALSE;
		openavbMediaQSetMaxLatency(pMediaQ, inMaxTransitUsec);
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return TRUE;
}
