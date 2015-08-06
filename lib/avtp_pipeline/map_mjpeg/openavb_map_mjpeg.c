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
* MODULE SUMMARY : Motion Jpeg mapping module conforming to 1722A RTP payload encapsulation.
*/

#include <stdlib.h>
#include <string.h>
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_avtp_time_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_map_pub.h"
#include "openavb_map_mjpeg_pub.h"

#define	AVB_LOG_COMPONENT	"MJPEG Mapping"
#include "openavb_log_pub.h"

// Header sizes
#define AVTP_V0_HEADER_SIZE			12
#define MAP_HEADER_SIZE				12

#define TOTAL_HEADER_SIZE			(AVTP_V0_HEADER_SIZE + MAP_HEADER_SIZE)

// MJPEG Payload is a JPEG fragment per packet plus its headers
//#define MAX_JPEG_PAYLOAD_SIZE 		1024
#define MAX_JPEG_PAYLOAD_SIZE 		1412

#define MAX_DATA_SIZE 				(MAX_JPEG_PAYLOAD_SIZE + TOTAL_HEADER_SIZE);

#define ITEM_SIZE					MAX_JPEG_PAYLOAD_SIZE

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

// - 1 byte 	Format subtype				= 0x00
#define HIDX_FORMAT_SUBTYPE8		17

// - 2 bytes	Reserved					= binary 0x0000
#define HIDX_RESV16					18

// - 2 bytes	Stream data length
#define HIDX_STREAM_DATA_LEN16		20

// 1 bit		M3							= binary 0
// 1 bit		M2							= binary 0
// 1 bit		M1							= binary 0
// 1 bit		M0							= binary 0
// 2 bit		evt							= binary 00
// 2 bit		Reserved					= binary 00
#define HIDX_M11_M01_EVT2_RESV2		22

// - 1 byte		Reserved					= binary 0x00
#define HIDX_RESV8					23

typedef struct {
	/////////////
	// Config data
	/////////////
	// map_nv_item_count
	U32 itemCount;

	// Transmit interval in frames per second. 0 = default for talker class.
	U32 txInterval;

	/////////////
	// Variable data
	/////////////
	U32 maxTransitUsec;     // In microseconds

	U32 timestamp;
	bool tsvalid;
} pvt_data_t;



// Each configuration name value pair for this mapping will result in this callback being called.
void openavbMapMjpegCfgCB(media_q_t *pMediaQ, const char *name, const char *value)
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
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

U8 openavbMapMjpegSubtypeCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return 0x03;        // AVTP Video subtype
}

// Returns the AVTP version used by this mapping
U8 openavbMapMjpegAvtpVersionCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return 0x00;        // Version 0
}

U16 openavbMapMjpegMaxDataSizeCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return MAX_DATA_SIZE;
}

// Returns the intended transmit interval (in frames per second). 0 = default for talker / class.
U32 openavbMapMjpegTransmitIntervalCB(media_q_t *pMediaQ)
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

void openavbMapMjpegGenInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return;
		}

		pPvtData->timestamp = 0;
		pPvtData->tsvalid = FALSE;

		openavbMediaQSetSize(pMediaQ, pPvtData->itemCount, ITEM_SIZE);
		openavbMediaQAllocItemMapData(pMediaQ, sizeof(media_q_item_map_mjpeg_pub_data_t), 0);
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// A call to this callback indicates that this mapping module will be
// a talker. Any talker initialization can be done in this function.
void openavbMapMjpegTxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This talker callback will be called for each AVB observation interval.
tx_cb_ret_t openavbMapMjpegTxCB(media_q_t *pMediaQ, U8 *pData, U32 *dataLen)
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
		*(U16 *)(&pHdr[HIDX_FORMAT_SUBTYPE8]) = 0x00;       // MJPEG format (RFC 2435)
															//pHdr[HIDX_STREAM_DATA_LEN16] = 0x00;				// Set later
		pHdr[HIDX_RESV16] = 0x00;
		pHdr[HIDX_RESV16+1] = 0x00;
		pHdr[HIDX_M11_M01_EVT2_RESV2] = 0x00;
		pHdr[HIDX_RESV8] = 0x00;

		media_q_item_t *pMediaQItem = openavbMediaQTailLock(pMediaQ, TRUE);
		if (pMediaQItem) {
			if (pMediaQItem->dataLen > 0) {
				if (pMediaQItem->dataLen > ITEM_SIZE) {
					AVB_LOGF_ERROR("Media queue data item size too large. Reported size: %d  Max Size: %d", pMediaQItem->dataLen, ITEM_SIZE);
					AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
					openavbMediaQTailPull(pMediaQ);
					return TX_CB_RET_PACKET_NOT_READY;
				}

				// PTP walltime already set in the interface module. Just add the max transit time.
				openavbAvtpTimeAddUSec(pMediaQItem->pAvtpTime, pPvtData->maxTransitUsec);

				// Set timestamp valid flag
				if (openavbAvtpTimeTimestampIsValid(pMediaQItem->pAvtpTime)) {
					pHdr[HIDX_AVTP_HIDE7_TV1] |= 0x01;      // Set
				}
				else {
					pHdr[HIDX_AVTP_HIDE7_TV1] &= ~0x01;     // Clear
				}

				// Set timestamp uncertain flag
				if (openavbAvtpTimeTimestampIsUncertain(pMediaQItem->pAvtpTime)) {
					pHdr[HIDX_AVTP_HIDE7_TU1] |= 0x01;      // Set
				}
				else {
					pHdr[HIDX_AVTP_HIDE7_TU1] &= ~0x01;     // Clear
				}

				// Set the timestamp.
				// 1722a-D6: The avtp_timestamp represents the presentation time associated with the given frame. The same avtp_timestamp
				// shall appear in each fragment of a given frame. The M0 marker bit shall be set in the last packet of a frame.
				if (!pPvtData->tsvalid)
				{
					pPvtData->timestamp = openavbAvtpTimeGetAvtpTimestamp(pMediaQItem->pAvtpTime);
					pPvtData->tsvalid = TRUE;
				}
				*(U32 *)(&pHdr[HIDX_AVTP_TIMESTAMP32]) = htonl(pPvtData->timestamp);

				if (((media_q_item_map_mjpeg_pub_data_t *)pMediaQItem->pPubMapData)->lastFragment) {
					pHdr[HIDX_M11_M01_EVT2_RESV2] = 0x00 | (1 << 4);
					pPvtData->tsvalid = FALSE;
				}
				else {
					pHdr[HIDX_M11_M01_EVT2_RESV2] = 0x00;
				}

				// Copy the JPEG fragment into the outgoing avtp packet.
				memcpy(pPayload, pMediaQItem->pPubData, pMediaQItem->dataLen);

				*(U16 *)(&pHdr[HIDX_STREAM_DATA_LEN16]) = pMediaQItem->dataLen;

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
void openavbMapMjpegRxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This callback occurs when running as a listener and data is available.
bool openavbMapMjpegRxCB(media_q_t *pMediaQ, U8 *pData, U32 dataLen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	if (pMediaQ && pData) {
		U8 *pHdr = pData;
		U8 *pPayload = pData + TOTAL_HEADER_SIZE;

//		U16 payloadLen = ntohs(*(U16 *)(&pHdr[HIDX_STREAM_DATA_LEN16]));
		U16 payloadLen = *(U16 *)(&pHdr[HIDX_STREAM_DATA_LEN16]);

		// Get item pointer in media queue
		media_q_item_t *pMediaQItem = openavbMediaQHeadLock(pMediaQ);
		if (pMediaQItem) {
			// Get the timestamp and place it in the media queue item.
			U32 timestamp = ntohl(*(U32 *)(&pHdr[HIDX_AVTP_TIMESTAMP32]));
			openavbAvtpTimeSetToTimestamp(pMediaQItem->pAvtpTime, timestamp);

			// Set timestamp valid and timestamp uncertain flags
			openavbAvtpTimeSetTimestampValid(pMediaQItem->pAvtpTime, (pHdr[HIDX_AVTP_HIDE7_TV1] & 0x01) ? TRUE : FALSE);
			openavbAvtpTimeSetTimestampUncertain(pMediaQItem->pAvtpTime, (pHdr[HIDX_AVTP_HIDE7_TU1] & 0x01) ? TRUE : FALSE);

			if (pHdr[HIDX_M11_M01_EVT2_RESV2] & 0x10) {
				((media_q_item_map_mjpeg_pub_data_t *)pMediaQItem->pPubMapData)->lastFragment = TRUE;
			}
			else {
				((media_q_item_map_mjpeg_pub_data_t *)pMediaQItem->pPubMapData)->lastFragment = FALSE;
			}

			if (pMediaQItem->itemSize >= dataLen - TOTAL_HEADER_SIZE) {
				memcpy(pMediaQItem->pPubData, pPayload, payloadLen);
				pMediaQItem->dataLen = dataLen - TOTAL_HEADER_SIZE;
			}
			else {
				AVB_LOG_ERROR("Data to large for media queue.");
				pMediaQItem->dataLen = 0;
			}

			openavbMediaQHeadPush(pMediaQ);
			AVB_TRACE_LINE(AVB_TRACE_MAP_LINE);
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return TRUE;
		}
		else {
			IF_LOG_INTERVAL(1000) AVB_LOG_ERROR("Media queue full.");
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return FALSE;   // Media queue full
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return FALSE;
}

// This callback will be called when the mapping module needs to be closed.
// All cleanup should occur in this function.
void openavbMapMjpegEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

void openavbMapMjpegGenEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Initialization entry point into the mapping module. Will need to be included in the .ini file.
extern DLL_EXPORT bool openavbMapMjpegInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		pMediaQ->pMediaQDataFormat = strdup(MapMjpegMediaQDataFormat);
		pMediaQ->pPvtMapInfo = calloc(1, sizeof(pvt_data_t));       // Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pMediaQDataFormat || !pMediaQ->pPvtMapInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for mapping module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;

		pMapCB->map_cfg_cb = openavbMapMjpegCfgCB;
		pMapCB->map_subtype_cb = openavbMapMjpegSubtypeCB;
		pMapCB->map_avtp_version_cb = openavbMapMjpegAvtpVersionCB;
		pMapCB->map_max_data_size_cb = openavbMapMjpegMaxDataSizeCB;
		pMapCB->map_transmit_interval_cb = openavbMapMjpegTransmitIntervalCB;
		pMapCB->map_gen_init_cb = openavbMapMjpegGenInitCB;
		pMapCB->map_tx_init_cb = openavbMapMjpegTxInitCB;
		pMapCB->map_tx_cb = openavbMapMjpegTxCB;
		pMapCB->map_rx_init_cb = openavbMapMjpegRxInitCB;
		pMapCB->map_rx_cb = openavbMapMjpegRxCB;
		pMapCB->map_end_cb = openavbMapMjpegEndCB;
		pMapCB->map_gen_end_cb = openavbMapMjpegGenEndCB;

		pPvtData->itemCount = 20;
		pPvtData->txInterval = 0;
		pPvtData->maxTransitUsec = inMaxTransitUsec;

		openavbMediaQSetMaxLatency(pMediaQ, inMaxTransitUsec);
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return TRUE;
}
