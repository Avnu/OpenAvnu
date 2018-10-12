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
* MODULE SUMMARY : H.264 mapping module conforming to 1722A D6 9.4.3.
*/

//////////////////////////////////////////////////////////////////
// WORK IN PROGRESS
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
// WORK IN PROGRESS
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
// WORK IN PROGRESS
//////////////////////////////////////////////////////////////////


#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_avtp_time_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_map_pub.h"
#include "openavb_map_h264_pub.h"

#define	AVB_LOG_COMPONENT	"H.264 Mapping"
#include "openavb_log_pub.h"

// Header sizes
#define AVTP_V0_HEADER_SIZE			12
#define MAP_HEADER_SIZE				16

#define TOTAL_HEADER_SIZE			(AVTP_V0_HEADER_SIZE + MAP_HEADER_SIZE)

#define MAX_PAYLOAD_SIZE 1416

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

// - 1 bytes	Format information 			= 0x02 RTP Video
#define HIDX_FORMAT8				16

// - 1 bytes 	Format subtype				= 0x01 H.264
#define HIDX_FORMAT_SUBTYPE8		17

// - 2 bytes 	Reserved					= 0x0000
#define HIDX_RESV16					18

// - 2 bytes	Stream data length
#define HIDX_STREAM_DATA_LEN16		20

// 1 bit		M3							= binary 0
// 1 bit		M2							= binary 0
// 1 bit		M1							= binary 0
// 1 bit		M0							= Set by interface module
// 2 bits		EVT							= binary 00
// 2 bits		Reserved					= binary 00
#define HIDX_M31_M21_M11_M01_EVT2_RESV2	22

// - 1 byte		Reserved					= binary 0x00
#define HIDX_RESV8					23

// - 4 bytes	h264_timestamp
#define HIDX_H264_TIMESTAMP32		24

// - 4 bytes    h264_timestamp size
#define HIDX_H264_TIMESTAMP_SIZE    4

typedef struct {
	/////////////
	// Config data
	/////////////
	// map_nv_item_count
	U32 itemCount;

	// Transmit interval in frames per second. 0 = default for talker class.
	U32 txInterval;

	// Max payload size
	U32 maxPayloadSize;

	/////////////
	// Variable data
	/////////////
	// Maximum transit time in microseconds
	U32 maxTransitUsec;     

	// Maximum data size. This is the RTP payload size. See RFC 6184 for details.
	U32 maxDataSize;

	// Maximum media queue item size
	U32 itemSize;

} pvt_data_t;



// Each configuration name value pair for this mapping will result in this callback being called.
void openavbMapH264CfgCB(media_q_t *pMediaQ, const char *name, const char *value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return;
		}

		char *pEnd;

		if (strcmp(name, "map_nv_item_count") == 0) {
			pPvtData->itemCount = strtol(value, &pEnd, 10);
		}
		else if (strcmp(name, "map_nv_tx_rate") == 0
			|| strcmp(name, "map_nv_tx_interval") == 0) {
			pPvtData->txInterval = strtol(value, &pEnd, 10);
		}
		else if (strcmp(name, "map_nv_max_payload_size") == 0) {
			U32 size = strtol(value, &pEnd, 10);
			if (size > MAX_PAYLOAD_SIZE) {
				AVB_LOGF_WARNING("map_nv_max_payload_size too large. Parameter set to default: %d", MAX_PAYLOAD_SIZE);
				size = MAX_PAYLOAD_SIZE;
			}
			pPvtData->maxPayloadSize = size;
			pPvtData->maxDataSize = (pPvtData->maxPayloadSize + TOTAL_HEADER_SIZE);
			pPvtData->itemSize =	pPvtData->maxPayloadSize;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

U8 openavbMapH264SubtypeCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return 0x03;        // AVTP Video subtype
}

// Returns the AVTP version used by this mapping
U8 openavbMapH264AvtpVersionCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return 0x00;        // Version 0
}

U16 openavbMapH264MaxDataSizeCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return 0;
		}

		AVB_TRACE_EXIT(AVB_TRACE_MAP);
		return pPvtData->maxDataSize;
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return 0;
}

// Returns the intended transmit interval (in frames per second). 0 = default for talker / class.
U32 openavbMapH264TransmitIntervalCB(media_q_t *pMediaQ)
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

void openavbMapH264GenInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return;
		}

		openavbMediaQSetSize(pMediaQ, pPvtData->itemCount, pPvtData->itemSize);
		openavbMediaQAllocItemMapData(pMediaQ, sizeof(media_q_item_map_h264_pub_data_t), 0);
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// A call to this callback indicates that this mapping module will be
// a talker. Any talker initialization can be done in this function.
void openavbMapH264TxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This talker callback will be called for each AVB observation interval.
tx_cb_ret_t openavbMapH264TxCB(media_q_t *pMediaQ, U8 *pData, U32 *dataLen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	if (pMediaQ && pData && dataLen) {
		U8 *pHdr = pData;
		U8 *pPayload = pData + TOTAL_HEADER_SIZE;
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return TX_CB_RET_PACKET_NOT_READY;
		}

		//pHdr[HIDX_AVTP_TIMESTAMP32] = 0x00;				// Set later
		pHdr[HIDX_FORMAT8] = 0x02;                          // RTP Payload type
		pHdr[HIDX_FORMAT_SUBTYPE8] = 0x01;                  // H.264 subtype
		pHdr[HIDX_RESV16] = 0x0000;           				// Reserved	
		//pHdr[HIDX_STREAM_DATA_LEN16] = 0x0000;			// Set later	
		//pHdr[HIDX_M31_M21_M11_M01_EVT2_RESV2] = 0x00;		// M0 set later
		pHdr[HIDX_RESV8] = 0x00;

		media_q_item_t *pMediaQItem = openavbMediaQTailLock(pMediaQ, TRUE);
		if (pMediaQItem) {
			if (pMediaQItem->dataLen > 0) {
				if (pMediaQItem->dataLen > pPvtData->itemSize) {
					AVB_LOGF_ERROR("Media queue data item size too large. Reported size: %d  Max Size: %d", pMediaQItem->dataLen, pPvtData->itemSize);
					AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
					openavbMediaQTailPull(pMediaQ);
					return TX_CB_RET_PACKET_NOT_READY;
				}

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
				else pHdr[HIDX_AVTP_HIDE7_TU1] &= ~0x01;    // Clear

				// Set the timestamp.
				*(U32 *)(&pHdr[HIDX_AVTP_TIMESTAMP32]) = htonl(openavbAvtpTimeGetAvtpTimestamp(pMediaQItem->pAvtpTime));

				if (((media_q_item_map_h264_pub_data_t *)pMediaQItem->pPubMapData)->lastPacket) {
					pHdr[HIDX_M31_M21_M11_M01_EVT2_RESV2] = 0x10;;
				}
				else {
					pHdr[HIDX_M31_M21_M11_M01_EVT2_RESV2] = 0x00;
				}

				// Set h264_timestamp
				*(U32 *)(&pHdr[HIDX_H264_TIMESTAMP32]) =
						htonl(((media_q_item_map_h264_pub_data_t *)pMediaQItem->pPubMapData)->timestamp);

				// Copy the h264 rtp payload into the outgoing avtp packet.
				memcpy(pPayload, pMediaQItem->pPubData, pMediaQItem->dataLen);

				// Add h264_timestamp size into the H264 data length
				*(U16 *)(&pHdr[HIDX_STREAM_DATA_LEN16]) = htons(pMediaQItem->dataLen + HIDX_H264_TIMESTAMP_SIZE);

				// Set out bound data length (entire packet length)
				*dataLen = pMediaQItem->dataLen + TOTAL_HEADER_SIZE;

				AVB_TRACE_LINE(AVB_TRACE_MAP_LINE);
				AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
				openavbMediaQTailPull(pMediaQ);
				return TX_CB_RET_PACKET_READY;
			}
			openavbMediaQTailPull(pMediaQ);
		}
	}

	if (dataLen) {
		// Set out bound data length (entire packet length)
		*dataLen = 0;
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return TX_CB_RET_PACKET_NOT_READY;
}

// A call to this callback indicates that this mapping module will be
// a listener. Any listener initialization can be done in this function.
void openavbMapH264RxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This callback occurs when running as a listener and data is available.
bool openavbMapH264RxCB(media_q_t *pMediaQ, U8 *pData, U32 dataLen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	if (pMediaQ && pData) {
		U8 *pHdr = pData;
		U8 *pPayload = pData + TOTAL_HEADER_SIZE;


		//pHdr[HIDX_AVTP_TIMESTAMP32]
		//pHdr[HIDX_FORMAT8]
		//pHdr[HIDX_FORMAT_SUBTYPE8]
		//pHdr[HIDX_RESV16]
		//pHdr[HIDX_STREAM_DATA_LEN16]
		U16 payloadLen = ntohs(*(U16 *)(&pHdr[HIDX_STREAM_DATA_LEN16]));
		//pHdr[HIDX_M31_M21_M11_M01_EVT2_RESV2]
		//pHdr[HIDX_RESV8]

		// validate header
		if (payloadLen  > dataLen - TOTAL_HEADER_SIZE) {
			IF_LOG_INTERVAL(1000) AVB_LOG_ERROR("header data len > actual data len");
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return FALSE;
		}

		// Get item pointer in media queue
		media_q_item_t *pMediaQItem = openavbMediaQHeadLock(pMediaQ);
		if (pMediaQItem) {
			// Get the timestamp and place it in the media queue item.
			U32 timestamp = ntohl(*(U32 *)(&pHdr[HIDX_AVTP_TIMESTAMP32]));
			openavbAvtpTimeSetToTimestamp(pMediaQItem->pAvtpTime, timestamp);

			// Set timestamp valid and timestamp uncertain flags
			openavbAvtpTimeSetTimestampValid(pMediaQItem->pAvtpTime, (pHdr[HIDX_AVTP_HIDE7_TV1] & 0x01) ? TRUE : FALSE);
			openavbAvtpTimeSetTimestampUncertain(pMediaQItem->pAvtpTime, (pHdr[HIDX_AVTP_HIDE7_TU1] & 0x01) ? TRUE : FALSE);

			if (pHdr[HIDX_M31_M21_M11_M01_EVT2_RESV2] & 0x10)
				((media_q_item_map_h264_pub_data_t *)pMediaQItem->pPubMapData)->lastPacket = TRUE;
			else
				((media_q_item_map_h264_pub_data_t *)pMediaQItem->pPubMapData)->lastPacket = FALSE;

			((media_q_item_map_h264_pub_data_t *)pMediaQItem->pPubMapData)->timestamp =
					ntohl(*(U32 *)(&pHdr[HIDX_H264_TIMESTAMP32]));

			if (pMediaQItem->itemSize >= payloadLen) {
				memcpy(pMediaQItem->pPubData, pPayload, payloadLen);
				pMediaQItem->dataLen = payloadLen;
			}
			else {
				AVB_LOGF_ERROR("Data to large for media queue (itemSize %d, data size %d).", pMediaQItem->itemSize, payloadLen);
				pMediaQItem->dataLen = 0;
			}

			openavbMediaQHeadPush(pMediaQ);
			AVB_TRACE_LINE(AVB_TRACE_MAP_LINE);
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return TRUE;
		}
		else {
			IF_LOG_INTERVAL(1000) AVB_LOG_ERROR("Media queue full");
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return FALSE;   // Media queue full
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return FALSE;
}

// This callback will be called when the mapping module needs to be closed.
// All cleanup should occur in this function.
void openavbMapH264EndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

void openavbMapH264GenEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Initialization entry point into the mapping module. Will need to be included in the .ini file.
extern DLL_EXPORT bool openavbMapH264Initialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		pMediaQ->pMediaQDataFormat = strdup(MapH264MediaQDataFormat);
		pMediaQ->pPvtMapInfo = calloc(1, sizeof(pvt_data_t));       // Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pMediaQDataFormat || !pMediaQ->pPvtMapInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for mapping module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;

		pMapCB->map_cfg_cb = openavbMapH264CfgCB;
		pMapCB->map_subtype_cb = openavbMapH264SubtypeCB;
		pMapCB->map_avtp_version_cb = openavbMapH264AvtpVersionCB;
		pMapCB->map_max_data_size_cb = openavbMapH264MaxDataSizeCB;
		pMapCB->map_transmit_interval_cb = openavbMapH264TransmitIntervalCB;
		pMapCB->map_gen_init_cb = openavbMapH264GenInitCB;
		pMapCB->map_tx_init_cb = openavbMapH264TxInitCB;
		pMapCB->map_tx_cb = openavbMapH264TxCB;
		pMapCB->map_rx_init_cb = openavbMapH264RxInitCB;
		pMapCB->map_rx_cb = openavbMapH264RxCB;
		pMapCB->map_end_cb = openavbMapH264EndCB;
		pMapCB->map_gen_end_cb = openavbMapH264GenEndCB;

		pPvtData->itemCount = 20;
		pPvtData->txInterval = 0;
		pPvtData->maxTransitUsec = inMaxTransitUsec;

		pPvtData->maxPayloadSize = MAX_PAYLOAD_SIZE;
		pPvtData->maxDataSize = (pPvtData->maxPayloadSize + TOTAL_HEADER_SIZE);
		pPvtData->itemSize = pPvtData->maxPayloadSize;

		openavbMediaQSetMaxLatency(pMediaQ, inMaxTransitUsec);
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return TRUE;
}
