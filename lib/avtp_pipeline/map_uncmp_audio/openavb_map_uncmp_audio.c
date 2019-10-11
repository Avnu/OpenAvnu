/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
Copyright (c) 2016-2017, Harman International Industries, Incorporated
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
* MODULE SUMMARY : Uncompressed audio mapping module conforming to AVB 61883-6 encapsulation.
*/

#include <stdlib.h>
#include <string.h>
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_avtp_time_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_map_pub.h"
#include "openavb_map_uncmp_audio_pub.h"

// DEBUG Uncomment to turn on logging for just this module.
#define AVB_LOG_ON	1

#define	AVB_LOG_COMPONENT	"61883-6 Mapping"
#include "openavb_log_pub.h"

// Header sizes
#define AVTP_V0_HEADER_SIZE			12
#define MAP_HEADER_SIZE				12
#define CIP_HEADER_SIZE				8

#define TOTAL_HEADER_SIZE			(AVTP_V0_HEADER_SIZE + MAP_HEADER_SIZE + CIP_HEADER_SIZE)

//////
// AVTP Version 0 Header
//////

// This mapping does not directly set or read these.
#define HIDX_AVTP_VERZERO96			0

// - 1 Byte - TV bit (timestamp valid)
#define HIDX_AVTP_HIDE7_TV1			1

// - 1 Byte - TU bit (timestamp uncertain)
#define HIDX_AVTP_HIDE7_TU1			3

//////
// Mapping specific header
//////

// - 4 bytes	avtp_timestamp
#define HIDX_AVTP_TIMESTAMP32		12

// - 4 bytes	gateway_info
#define HIDX_GATEWAY32				16

// - 2 bytes	stream_data_len (save a pointer for later use)
#define HIDX_DATALEN16				20

// 2 bit		tag							= binary 01 (CIP header - included)
// 6 bit		channel						= 31 / 0x1F (Native AVB)
#define HIDX_TAG2_CHANNEL6			22

// 4 bits		tcode						= 0xA
// 4 bits		sy (application-specific)	= 0
#define HIDX_TCODE4_SY4				23

//////
// CIP Header - 2 quadlets (8 bytes)
//////

// 2 bits		cip flag 					= binary 00
// 6 bits		sid (source identifier)		= 63 / 0x3F (On the AVB network)
#define HIDX_CIP2_SID6				24

// 8 bits		dbs (data block size)		= Same as audio frame size. Sample Size * channels
#define HIDX_DBS8					25

// 2 bits		FN (fraction number)		= Bx00
// 3 bits		QPC (quadlet padding count)	= Bx000
// 1 bit		SPH (source packet header)	= Bx0 (not using source packet headers)
// 2 bits		RSV (reserved)				= Bx00
#define HIDX_FN2_QPC3_SPH1_RSV2		26

// 8 bits		DBC (data block counter)	= counter
#define HIDX_DBC8					27

// 2 bits		cip flag (2nd quadlet)		= binary 10
// 6 bits		fmt (stream format)			= binary 010000 (61883-6)
#define HIDX_CIP2_FMT6				28

// 5 bits		fdf (format dependant field)	= 0
// 3 bits		SFC (sample frequency)			= based on rate
#define HIDX_FDF5_SFC3				29

// 2 bytes		syt (synchronization timing) Set to 0xffff according to 1722
#define HIDX_SYT16					30

typedef struct {
	/////////////
	// Config data
	/////////////
	// map_nv_item_count
	U32 itemCount;

	// Transmit interval in frames per second. 0 = default for talker class.
	U32 txInterval;

	// A multiple of how many frames of audio to accept in an media queue item and into the AVTP payload above
	//	the minimal needed.
	U32 packingFactor;

	/////////////
	// Variable data
	/////////////
	U32 maxTransitUsec;     // In microseconds

	U8 cip_sfc;

	U32 AM824_label;

	U32 maxPayloadSize;

	// Data block continuity counter
	U8 DBC;

	avb_audio_mcr_t audioMcr;
#if ATL_LAUNCHTIME_ENABLED
	// Transmit interval in nanoseconds.
	U32 txIntervalNs;
#endif
} pvt_data_t;

static void x_calculateSizes(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		media_q_pub_map_uncmp_audio_info_t *pPubMapInfo = pMediaQ->pPubMapInfo;
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return;
		}

		switch (pPubMapInfo->audioRate) {
			case AVB_AUDIO_RATE_32KHZ:
				pPvtData->cip_sfc = 0;
				pPubMapInfo->sytInterval = 8;
				break;

			case AVB_AUDIO_RATE_44_1KHZ:
				pPvtData->cip_sfc = 1;
				pPubMapInfo->sytInterval = 8;
				break;

			case AVB_AUDIO_RATE_48KHZ:
				pPvtData->cip_sfc = 2;
				pPubMapInfo->sytInterval = 8;
				break;

			case AVB_AUDIO_RATE_88_2KHZ:
				pPvtData->cip_sfc = 3;
				pPubMapInfo->sytInterval = 16;
				break;

			case AVB_AUDIO_RATE_96KHZ:
				pPvtData->cip_sfc = 4;
				pPubMapInfo->sytInterval = 16;
				break;

			case AVB_AUDIO_RATE_176_4KHZ:
				pPvtData->cip_sfc = 5;
				pPubMapInfo->sytInterval = 32;
				break;

			case AVB_AUDIO_RATE_192KHZ:
				pPvtData->cip_sfc = 6;
				pPubMapInfo->sytInterval = 32;
				break;

			default:
				AVB_LOGF_ERROR("Invalid audio frequency configured: %u", pPubMapInfo->audioRate);
				pPvtData->cip_sfc = 2;
				pPubMapInfo->sytInterval = 8;
				break;
		}

		pPubMapInfo->framesPerPacket = (pPubMapInfo->audioRate / pPvtData->txInterval);
		if (pPubMapInfo->framesPerPacket < 1) {
			pPubMapInfo->framesPerPacket = 1;
		}
		if (pPubMapInfo->audioRate % pPvtData->txInterval != 0) {
			AVB_LOGF_WARNING("audio rate (%d) is not an integer multiple of TX rate (%d). Recommend TX rate of (%d)",
				pPubMapInfo->audioRate, pPvtData->txInterval, pPubMapInfo->audioRate / (pPubMapInfo->framesPerPacket + 1));
			pPubMapInfo->framesPerPacket += 1;
		}

		pPubMapInfo->packingFactor = pPvtData->packingFactor;
		if (pPubMapInfo->packingFactor > 1) {
			// Packing multiple packets of sampling into a media queue item
			pPubMapInfo->framesPerItem = pPubMapInfo->framesPerPacket * pPvtData->packingFactor;
		}
		else {
			// No packing. SYT_INTERVAL is used for media queue item size.
			pPubMapInfo->framesPerItem = pPubMapInfo->sytInterval;
		}
		if (pPubMapInfo->framesPerItem < 1) {
			pPubMapInfo->framesPerItem = 1;
		}

		pPubMapInfo->packetSampleSizeBytes = 4;

#if ATL_LAUNCHTIME_ENABLED
		pPvtData->txIntervalNs = 1000000000u / pPvtData->txInterval;
		AVB_LOGF_INFO("LT interval ns:%d", pPvtData->txIntervalNs);
#endif
		AVB_LOGF_INFO("Rate:%d", pPubMapInfo->audioRate);
		AVB_LOGF_INFO("Bits:%d", pPubMapInfo->audioBitDepth);
		AVB_LOGF_INFO("Channels:%d", pPubMapInfo->audioChannels);
		AVB_LOGF_INFO("Packet Interval:%d", pPvtData->txInterval);
		AVB_LOGF_INFO("Frames per packet:%d", pPubMapInfo->framesPerPacket);
		AVB_LOGF_INFO("Packing Factor:%d", pPvtData->packingFactor);
		AVB_LOGF_INFO("Frames per MediaQ Item:%d", pPubMapInfo->framesPerItem);
		AVB_LOGF_INFO("Sample Size Bytes:%d", pPubMapInfo->packetSampleSizeBytes);

		if (pPubMapInfo->audioBitDepth == AVB_AUDIO_BIT_DEPTH_16BIT || pPubMapInfo->audioBitDepth == AVB_AUDIO_BIT_DEPTH_20BIT) {
			// TODO: The 20Bit format was downgraded to 16 bit and therefore 2 bytes
			pPubMapInfo->itemSampleSizeBytes = 2;
		}
		else if (pPubMapInfo->audioBitDepth == AVB_AUDIO_BIT_DEPTH_24BIT) {
			pPubMapInfo->itemSampleSizeBytes = 3;
		}
		else {
			AVB_LOGF_ERROR("Invalid audio format configured: %u", pPubMapInfo->audioBitDepth);
			pPubMapInfo->itemSampleSizeBytes = 1;
		}

		pPubMapInfo->packetFrameSizeBytes = pPubMapInfo->packetSampleSizeBytes * pPubMapInfo->audioChannels;
		pPubMapInfo->packetAudioDataSizeBytes = pPubMapInfo->framesPerPacket * pPubMapInfo->packetFrameSizeBytes;
		pPvtData->maxPayloadSize = pPubMapInfo->packetAudioDataSizeBytes + TOTAL_HEADER_SIZE;

		pPubMapInfo->itemFrameSizeBytes = pPubMapInfo->itemSampleSizeBytes * pPubMapInfo->audioChannels;
		pPubMapInfo->itemSize = pPubMapInfo->itemFrameSizeBytes * pPubMapInfo->framesPerItem;

		switch (pPubMapInfo->audioBitDepth) {
			case AVB_AUDIO_BIT_DEPTH_16BIT:
				pPvtData->AM824_label = 0x42000000;
				break;

			case AVB_AUDIO_BIT_DEPTH_20BIT:
				AVB_LOG_ERROR("20 bit not currently supported. Downgraded to 16 bit.");
				pPvtData->AM824_label = 0x42000000;
				break;

			case AVB_AUDIO_BIT_DEPTH_24BIT:
				pPvtData->AM824_label = 0x40000000;
				break;

			default:
				AVB_LOGF_ERROR("Invalid audio bit depth configured: %u", pPubMapInfo->audioBitDepth);
				pPvtData->AM824_label = 0x40000000;
				break;
		}

	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}


// Each configuration name value pair for this mapping will result in this callback being called.
void openavbMapUncmpAudioCfgCB(media_q_t *pMediaQ, const char *name, const char *value)
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
		else if (strcmp(name, "map_nv_tx_rate") == 0
			|| strcmp(name, "map_nv_tx_interval") == 0) {
			char *pEnd;
			pPvtData->txInterval = strtol(value, &pEnd, 10);
		}
		else if (strcmp(name, "map_nv_packing_factor") == 0) {
			char *pEnd;
			pPvtData->packingFactor = strtol(value, &pEnd, 10);
		}
		else if (strcmp(name, "map_nv_audio_mcr") == 0) {
			char *pEnd;
			pPvtData->audioMcr = (avb_audio_mcr_t)strtol(value, &pEnd, 10);
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

U8 openavbMapUncmpAudioSubtypeCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return 0x00;        // 61883 AVB subtype
}

// Returns the AVTP version used by this mapping
U8 openavbMapUncmpAudioAvtpVersionCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return 0x00;        // Version 0
}

U16 openavbMapUncmpAudioMaxDataSizeCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return 0;
		}

		AVB_TRACE_EXIT(AVB_TRACE_MAP);
		return pPvtData->maxPayloadSize;
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return 0;
}

// Returns the intended transmit interval (in frames per second). 0 = default for talker / class.
U32 openavbMapUncmpAudioTransmitIntervalCB(media_q_t *pMediaQ)
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

void openavbMapUncmpAudioGenInitCB(media_q_t *pMediaQ)
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
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	}

void openavbMapUncmpAudioAVDECCInitCB(media_q_t *pMediaQ, U16 configIdx, U16 descriptorType, U16 descriptorIdx)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

#if ATL_LAUNCHTIME_ENABLED
#define ATL_LT_OFFSET 500000
bool openavbMapUncmpLaunchtimCalculationCB(media_q_t *pMediaQ, U64 *lt)
{
static U64 last_time = 0;
	bool res = false;
	media_q_item_t* pMediaQItem;
	pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	if (!lt) {
		AVB_LOG_ERROR("Mapping module launchtime argument incorrect.");
		return false;
	}
	if (!pPvtData) {
		AVB_LOG_ERROR("Private mapping module data not allocated.");
		return false;
	}

	pMediaQItem = openavbMediaQTailLock(pMediaQ, true);
	if (pMediaQItem) {
		if (pMediaQItem->readIdx == 0) {
			last_time = pMediaQItem->pAvtpTime->timeNsec;
			*lt = last_time + ATL_LT_OFFSET;
			res = true;
		} else if( last_time != 0 ) {
			last_time += pPvtData->txIntervalNs;
			*lt = last_time + ATL_LT_OFFSET;
			res = true;
		}
		openavbMediaQTailUnlock(pMediaQ);
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return res;
}
#endif

// A call to this callback indicates that this mapping module will be
// a talker. Any talker initialization can be done in this function.
void openavbMapUncmpAudioTxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	// Check the media queue for proper allocations to avoid doing it on each tx callback.
	if (!pMediaQ) {
		AVB_LOG_ERROR("Media queue not allocated.");
		AVB_TRACE_EXIT(AVB_TRACE_MAP);
		return;
	}

	media_q_pub_map_uncmp_audio_info_t *pPubMapInfo = pMediaQ->pPubMapInfo;
	if (!pPubMapInfo) {
		AVB_LOG_ERROR("Public mapping module data not allocated.");
		AVB_TRACE_EXIT(AVB_TRACE_MAP);
		return;
	}

	pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
	if (!pPvtData) {
		AVB_LOG_ERROR("Private mapping module data not allocated.");
		AVB_TRACE_EXIT(AVB_TRACE_MAP);
		return;
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This talker callback will be called for each AVB observation interval.
tx_cb_ret_t openavbMapUncmpAudioTxCB(media_q_t *pMediaQ, U8 *pData, U32 *dataLen)
{
	media_q_item_t *pMediaQItem = NULL;
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);

	if (!pData || !dataLen) {
		AVB_LOG_ERROR("Mapping module data or data length argument incorrect.");
		return TX_CB_RET_PACKET_NOT_READY;
	}

	media_q_pub_map_uncmp_audio_info_t *pPubMapInfo = pMediaQ->pPubMapInfo;
	pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;

	if (*dataLen - TOTAL_HEADER_SIZE < pPubMapInfo->packetAudioDataSizeBytes) {
		AVB_LOG_ERROR("Not enough room in packet for audio frames.");
		AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
		return TX_CB_RET_PACKET_NOT_READY;
	}

	if (openavbMediaQIsAvailableBytes(pMediaQ, pPubMapInfo->itemFrameSizeBytes * pPubMapInfo->framesPerPacket, TRUE)) {
		U8 *pHdr = pData;
		U8 *pPayload = pData + TOTAL_HEADER_SIZE;

		//pHdr[HIDX_AVTP_TIMESTAMP32] = 0x00;			// Set later
		*(U32 *)(&pHdr[HIDX_GATEWAY32]) = 0x00000000;
		//pHdr[HIDX_DATALEN16] = 0x00;					// Set Later
		pHdr[HIDX_TAG2_CHANNEL6] = (1 << 6) | 0x1f;
		pHdr[HIDX_TCODE4_SY4] = (0x0a << 4) | 0;

		// Set the majority of the CIP header now.
		pHdr[HIDX_CIP2_SID6] = (0x00 << 6) | 0x3f;
		pHdr[HIDX_DBS8] = pPubMapInfo->audioChannels;

		pHdr[HIDX_FN2_QPC3_SPH1_RSV2] = (0x00 << 6) | (0x00 << 3) | (0x00 << 2) | 0x00;
		// pHdr[HIDX_DBC8] = 0; 						// Set later
		pHdr[HIDX_CIP2_FMT6] = (0x02 << 6) | 0x10;
		pHdr[HIDX_FDF5_SFC3] = 0x00 << 3 | pPvtData->cip_sfc;
		*(U16 *)(&pHdr[HIDX_SYT16]) = 0xffff;

		U32 framesProcessed = 0;
		U8 *pAVTPDataUnit = pPayload;
		bool timestampSet = FALSE;		// index of the timestamp
		U32 sytInt = pPubMapInfo->sytInterval;
		U8 dbc = pPvtData->DBC;
		while (framesProcessed < pPubMapInfo->framesPerPacket) {
			pMediaQItem = openavbMediaQTailLock(pMediaQ, TRUE);
			if (pMediaQItem && pMediaQItem->pPubData && pMediaQItem->dataLen > 0) {
				U8 *pItemData = (U8 *)pMediaQItem->pPubData + pMediaQItem->readIdx;

				if (pMediaQItem->readIdx == 0) {
					// Timestamp from the media queue is always associated with the first data point.

					// PTP walltime already set in the interface module. Just add the max transit time.
					openavbAvtpTimeAddUSec(pMediaQItem->pAvtpTime, pPvtData->maxTransitUsec);

					// Set timestamp valid flag
					if (openavbAvtpTimeTimestampIsValid(pMediaQItem->pAvtpTime))
						pHdr[HIDX_AVTP_HIDE7_TV1] |= 0x01;      // Set
					else {
						pHdr[HIDX_AVTP_HIDE7_TV1] &= ~0x01;     // Clear
					}

					// Set timestamp uncertain flag
					if (openavbAvtpTimeTimestampIsUncertain(pMediaQItem->pAvtpTime))
						pHdr[HIDX_AVTP_HIDE7_TU1] |= 0x01;      // Set
					else
						pHdr[HIDX_AVTP_HIDE7_TU1] &= ~0x01;     // Clear

				}

				while (framesProcessed < pPubMapInfo->framesPerPacket && pMediaQItem->readIdx < pMediaQItem->dataLen) {
					int i1;
					for (i1 = 0; i1 < pPubMapInfo->audioChannels; i1++) {
						if (pPubMapInfo->itemSampleSizeBytes == 2) {
							S32 sample = *(S16 *)pItemData;
							sample &= 0x0000ffff;
							sample = sample << 8;
							sample |= pPvtData->AM824_label;
							sample = htonl(sample);
							*(U32 *)(pAVTPDataUnit) = sample;
							pAVTPDataUnit += 4;
							pItemData += 2;
						}
						else {
							S32 sample = *(S32 *)pItemData;
							sample &= 0x00ffffff;
							sample |= pPvtData->AM824_label;
							sample = htonl(sample);
							*(U32 *)(pAVTPDataUnit) = sample;
							pAVTPDataUnit += 4;
							pItemData += 3;
						}
					}
					framesProcessed++;
					if (dbc % sytInt == 0) {
						*(U32 *)(&pHdr[HIDX_AVTP_TIMESTAMP32]) = htonl(openavbAvtpTimeGetAvtpTimestamp(pMediaQItem->pAvtpTime));

						timestampSet = TRUE;
					}
					dbc++;
					pMediaQItem->readIdx += pPubMapInfo->itemFrameSizeBytes;
				}

				if (pMediaQItem->readIdx >= pMediaQItem->dataLen) {
					// Read the entire item
					openavbMediaQTailPull(pMediaQ);
				}
				else {
					// More to read next interval
					openavbMediaQTailUnlock(pMediaQ);
				}
			}
			else {
				openavbMediaQTailPull(pMediaQ);
			}
		}

		// Check if timestamp was set
		if (!timestampSet) {
			// Timestamp wasn't set so mark it as invalid
			pHdr[HIDX_AVTP_HIDE7_TV1] &= ~0x01;
		}

		// Set the block continutity and data length
		pHdr[HIDX_DBC8] = pPvtData->DBC;
		pPvtData->DBC = dbc;

		*(U16 *)(&pHdr[HIDX_DATALEN16]) = htons((pPubMapInfo->framesPerPacket * pPubMapInfo->packetFrameSizeBytes) + CIP_HEADER_SIZE);

		// Set out bound data length (entire packet length)
		*dataLen = (pPubMapInfo->framesPerPacket * pPubMapInfo->packetFrameSizeBytes) + TOTAL_HEADER_SIZE;

		AVB_TRACE_LINE(AVB_TRACE_MAP_LINE);
		AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
		return TX_CB_RET_PACKET_READY;

	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return TX_CB_RET_PACKET_NOT_READY;
}

// A call to this callback indicates that this mapping module will be
// a listener. Any listener initialization can be done in this function.
void openavbMapUncmpAudioRxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This callback occurs when running as a listener and data is available.
bool openavbMapUncmpAudioRxCB(media_q_t *pMediaQ, U8 *pData, U32 dataLen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	if (pMediaQ && pData) {
		U8 *pHdr = pData;
		U8 *pPayload = pData + TOTAL_HEADER_SIZE;
		media_q_pub_map_uncmp_audio_info_t *pPubMapInfo = pMediaQ->pPubMapInfo;

		//pHdr[HIDX_AVTP_TIMESTAMP32];
		//pHdr[HIDX_GATEWAY32];
		U16 payloadLen = ntohs(*(U16 *)(&pHdr[HIDX_DATALEN16]));

		//pHdr[HIDX_TAG2_CHANNEL6];
		//pHdr[HIDX_TCODE4_SY4];
		//pHdr[HIDX_CIP2_SID6];
		//pHdr[HIDX_DBS8];
		//pHdr[HIDX_FN2_QPC3_SPH1_RSV2];
		U8 dbc = pHdr[HIDX_DBC8];
		//pHdr[HIDX_CIP2_FMT6];
		//pHdr[HIDX_TSF1_RESA7];
		//pHdr[HIDX_RESB8];
		//pHdr[HIDX_RESC8];
		bool tsValid = (pHdr[HIDX_AVTP_HIDE7_TV1] & 0x01) ? TRUE : FALSE;
		bool tsUncertain = (pHdr[HIDX_AVTP_HIDE7_TU1] & 0x01) ? TRUE : FALSE;

		// Per iec61883-6 Secion 7.2
		// index = mod((SYT_INTERVAL - mod(DBC, SYT_INTERVAL)), SYT_INTERVAL)
		// U8 dbcIdx = (pPubMapInfo->sytInterval - (dbc % pPubMapInfo->sytInterval)) % pPubMapInfo->sytInterval;
		// The dbcIdx calculation isn't used from spec because our implmentation only needs to know when the timestamp index is at 0.
		U8 dbcIdx = dbc % pPubMapInfo->sytInterval;
		U8 *pAVTPDataUnit = pPayload;
		U8 *pAVTPDataUnitEnd = pData + AVTP_V0_HEADER_SIZE + MAP_HEADER_SIZE + payloadLen;

		while (((pAVTPDataUnit + pPubMapInfo->packetFrameSizeBytes) <= pAVTPDataUnitEnd)) {
			// Get item pointer in media queue
			media_q_item_t *pMediaQItem = openavbMediaQHeadLock(pMediaQ);
			if (pMediaQItem) {

				U32 itemSizeWritten = 0;
				U8 *pItemData = (U8 *)pMediaQItem->pPubData + pMediaQItem->dataLen;
				U8 *pItemDataEnd = (U8 *)pMediaQItem->pPubData + pMediaQItem->itemSize;

				// Get the timestamp
				U32 timestamp = ntohl(*(U32 *)(&pHdr[HIDX_AVTP_TIMESTAMP32]));
				pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
				if ((pPvtData->audioMcr != AVB_MCR_NONE) && tsValid && !tsUncertain) {
					// MCR mode set and timestamp is valid, and timestamp uncertain is not set
					openavbAvtpTimePushMCR(pMediaQItem->pAvtpTime, timestamp);
				}

				if (pMediaQItem->dataLen == 0) {
					// This is the first set of frames for the media queue item, must align based on SYT_INTERVAL for proper synchronization of listeners
					if (dbcIdx > 0) {
						// Failed to make alignment with this packet. This AVTP packet will be tossed. Once alignment is reached
						// it is expected not to need to toss packets anymore.
						openavbMediaQHeadUnlock(pMediaQ);
						AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
						return TRUE;
					}

					// Set time stamp info on first data write to the media queue
					// place it in the media queue item.
					openavbAvtpTimeSetToTimestamp(pMediaQItem->pAvtpTime, timestamp);

					// Set timestamp valid and timestamp uncertain flags
					openavbAvtpTimeSetTimestampValid(pMediaQItem->pAvtpTime, tsValid);
					openavbAvtpTimeSetTimestampUncertain(pMediaQItem->pAvtpTime, tsUncertain);
				}

				while (((pAVTPDataUnit + pPubMapInfo->packetFrameSizeBytes) <= pAVTPDataUnitEnd) && ((pItemData + pPubMapInfo->itemFrameSizeBytes) <= pItemDataEnd)) {
					int i1;
					for (i1 = 0; i1 < pPubMapInfo->audioChannels; i1++) {
						if (pPubMapInfo->itemSampleSizeBytes == 2) {
							S32 sample = ntohl(*(S32 *)pAVTPDataUnit);
							sample = sample * 1;
							*(S16 *)(pItemData) = (sample & 0x00ffffff) >> 8;
							pAVTPDataUnit += 4;
							pItemData += 2;
							itemSizeWritten += 2;
						}
						else {
							S32 sample = ntohl(*(S32 *)pAVTPDataUnit);
							sample = sample * 1;
							*(S32 *)(pItemData) = sample & 0x00ffffff;
							pAVTPDataUnit += 4;
							pItemData += 3;
							itemSizeWritten += 3;
						}
					}
				}

				pMediaQItem->dataLen += itemSizeWritten;

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
				IF_LOG_INTERVAL(1000) AVB_LOG_INFO("Media queue full");
				AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
				return FALSE;   // Media queue full
			}
		}
		AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
		return TRUE;    // Normal exit
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return FALSE;
}

// This callback will be called when the mapping module needs to be closed.
// All cleanup should occur in this function.
void openavbMapUncmpAudioEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

void openavbMapUncmpAudioGenEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Initialization entry point into the mapping module. Will need to be included in the .ini file.
extern DLL_EXPORT bool openavbMapUncmpAudioInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		pMediaQ->pMediaQDataFormat = strdup(MapUncmpAudioMediaQDataFormat);
		pMediaQ->pPubMapInfo = calloc(1, sizeof(media_q_pub_map_uncmp_audio_info_t));       // Memory freed by the media queue when the media queue is destroyed.
		pMediaQ->pPvtMapInfo = calloc(1, sizeof(pvt_data_t));                               // Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pMediaQDataFormat || !pMediaQ->pPubMapInfo || !pMediaQ->pPvtMapInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for mapping module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;

		pMapCB->map_cfg_cb = openavbMapUncmpAudioCfgCB;
		pMapCB->map_subtype_cb = openavbMapUncmpAudioSubtypeCB;
		pMapCB->map_avtp_version_cb = openavbMapUncmpAudioAvtpVersionCB;
		pMapCB->map_max_data_size_cb = openavbMapUncmpAudioMaxDataSizeCB;
		pMapCB->map_transmit_interval_cb = openavbMapUncmpAudioTransmitIntervalCB;
		pMapCB->map_gen_init_cb = openavbMapUncmpAudioGenInitCB;
		pMapCB->map_tx_init_cb = openavbMapUncmpAudioTxInitCB;
		pMapCB->map_tx_cb = openavbMapUncmpAudioTxCB;
		pMapCB->map_rx_init_cb = openavbMapUncmpAudioRxInitCB;
		pMapCB->map_rx_cb = openavbMapUncmpAudioRxCB;
		pMapCB->map_end_cb = openavbMapUncmpAudioEndCB;
		pMapCB->map_gen_end_cb = openavbMapUncmpAudioGenEndCB;
#if ATL_LAUNCHTIME_ENABLED
		pMapCB->map_lt_calc_cb = openavbMapUncmpLaunchtimCalculationCB;
#endif

		pPvtData->itemCount = 20;
		pPvtData->txInterval = 0;
		pPvtData->packingFactor = 1;
		pPvtData->maxTransitUsec = inMaxTransitUsec;
		pPvtData->DBC = 0;
		pPvtData->audioMcr = AVB_MCR_NONE;

		openavbMediaQSetMaxLatency(pMediaQ, inMaxTransitUsec);
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return TRUE;
}
