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
* MODULE SUMMARY : Pipe mapping module.
* 
* The Pipe mapping is an AVB "experimental" mapping format. It will
* pass throught any data from interface modules unchanged.
* This can be useful for development, testing or custom solutions.
* 
*----------------------------------------------------------------*
* 
* HEADERS
* 
*  -+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-
* |C|             |S|     |M| |G|T|               |             |T|   
* |D|subtype      |V|vers |R|R|V|V|sequence number|reserved     |U|   
* --+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+--
* |                                                               |   
* |                                                               |   
* -                                                               -
* |                                                               |   
* |stream id                                                      |   
* --+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+--
* |                                                               |   
* |AVTP timestamp                                                 |   
* --+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+--
* |               |                               |               |   
* |                          Vendor_eui_1                         |
* --+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+--
* |                                                               |   
* |          Data Length          |         Vendor_eui_2          |
*  -+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-
* 
* CD 				: Standard AVTP
* Subtype 			: Vendor specific 0x6f (Introduced in 1722A(D) 
* SV				: Standard AVTP
* Ver`				: Standard AVTP
* MR				: Standard AVTP
* R					: Standard AVTP
* GV				: Standard AVTP
* TV				: Standard AVTP
* Sequence number	: Standard AVTP
* Reserved			: Standard AVTP
* TU				: Standard AVTP
* Stream ID			: Standard AVTP
* AVTP timestamp	: Standard AVTP
* Vendor_eui_1		: Vendor specific (includes OPENAVB format : Pipe Mapping 0x01)
* Data Length		: Length of the data payload
* Vendor_eui_2		: Vendor specific
* 
*/

#include "openavb_platform_pub.h"
#include <stdlib.h>
#include <string.h>
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_avtp_time_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_map_pub.h"
#include "openavb_map_pipe_pub.h"

#define	AVB_LOG_COMPONENT	"Pipe Mapping"
#include "openavb_log_pub.h"

#define AVTP_V0_HEADER_SIZE			12
#define MAP_HEADER_SIZE				12

#define TOTAL_HEADER_SIZE			(AVTP_V0_HEADER_SIZE + MAP_HEADER_SIZE)

//////
// AVTP Version 0 Header
//////

#define HIDX_AVTP_VERZERO96			0

// - 1 Byte - TV bit (timestamp valid)
#define HIDX_AVTP_HIDE7_TV1			1

// - 1 Byte - TU bit (timestamp uncertain)
#define HIDX_AVTP_HIDE7_TU1			3

// - 4 bytes	avtp_timestamp
#define HIDX_AVTP_TIMESPAMP32		12

// - 2 bytes	stream_data_len
#define HIDX_AVTP_DATALEN16			20

//////
// Mapping specific header
//////

// - 1 byte		OPENAVB format
#define HIDX_OPENAVB_FORMAT8			16

// - 2 bytes	vendor specific
#define HIDX_VENDOR2_EUI16			22

typedef struct {
	/////////////
	// Config data
	/////////////
	// map_nv_item_count
	U32 itemCount;

	// Transmit interval in frames per second. 0 = default for talker class.
	U32 txInterval;

	// map_nv_push_header
	bool push_header;

	// map_nv_pull_header
	bool pull_header;


	/////////////
	// Variable data
	/////////////
	// Max payload size
	U32 maxPayloadSize;

	// Maximum data size
	U32 maxDataSize;

	// Maximum media queue item size
	U32 itemSize;

	// Maximum transit time
	U32 maxTransitUsec;     // In microseconds

} pvt_data_t;

// Each configuration name value pair for this mapping will result in this callback being called.
void openavbMapPipeCfgCB(media_q_t *pMediaQ, const char *name, const char *value)
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
		else if (strcmp(name, "map_nv_max_payload_size") == 0) {
			char *pEnd;
			pPvtData->maxPayloadSize = strtol(value, &pEnd, 10);
			pPvtData->maxDataSize = (pPvtData->maxPayloadSize + TOTAL_HEADER_SIZE);
			pPvtData->itemSize =	pPvtData->maxDataSize;
		}
		else if (strcmp(name, "map_nv_push_header") == 0) {
			char *pEnd;
			long tmp;
			tmp = strtol(value, &pEnd, 10);
			if (*pEnd == '\0' && tmp == 1) {
				pPvtData->push_header = (tmp == 1);
			}
		}
		else if (strcmp(name, "map_nv_pull_header") == 0) {
			char *pEnd;
			long tmp;
			tmp = strtol(value, &pEnd, 10);
			if (*pEnd == '\0' && tmp == 1) {
				pPvtData->pull_header = (tmp == 1);
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

U8 openavbMapPipeSubtypeCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return 0x6F;        // Vendor Specific AVTP subtype
}

// Returns the AVTP version used by this mapping
U8 openavbMapPipeAvtpVersionCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return 0x00;        // Version 0
}

U16 openavbMapPipeMaxDataSizeCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return 0;
		}

		AVB_TRACE_EXIT(AVB_TRACE_MAP);
		return pPvtData->maxDataSize + TOTAL_HEADER_SIZE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return 0;
}

// Returns the intended transmit interval (in frames per second). 0 = default for talker / class.
U32 openavbMapPipeTransmitIntervalCB(media_q_t *pMediaQ)
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

void openavbMapPipeGenInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return;
		}

		openavbMediaQSetSize(pMediaQ, pPvtData->itemCount, pPvtData->itemSize);
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// A call to this callback indicates that this mapping module will be
// a talker. Any talker initialization can be done in this function.
void openavbMapPipeTxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This talker callback will be called for each AVB observation interval.
tx_cb_ret_t openavbMapPipeTxCB(media_q_t *pMediaQ, U8 *pData, U32 *dataLen)
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

		media_q_item_t *pMediaQItem = openavbMediaQTailLock(pMediaQ, TRUE);

		if (pMediaQItem) {
			if (pMediaQItem->dataLen > 0) {
				if (pMediaQItem->dataLen > pPvtData->maxDataSize) {
					AVB_LOGF_ERROR("Media queue data item size too large. Reported size: %d  Max Size: %d", pMediaQItem->dataLen, pPvtData->maxDataSize);
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
				else pHdr[HIDX_AVTP_HIDE7_TU1] &= ~0x01;     // Clear

				*(U32 *)(&pHdr[HIDX_AVTP_TIMESPAMP32]) = htonl(openavbAvtpTimeGetAvtpTimestamp(pMediaQItem->pAvtpTime));
				*(U32 *)(&pHdr[HIDX_OPENAVB_FORMAT8]) = 0x00000000;
				pHdr[HIDX_OPENAVB_FORMAT8] = MAP_PIPE_OPENAVB_FORMAT;
				// for alignment
				U16 payloadLen = htons(pMediaQItem->dataLen);;
				memcpy(&pHdr[HIDX_AVTP_DATALEN16], &payloadLen, sizeof(U16));
				
				*(U16 *)(&pHdr[HIDX_VENDOR2_EUI16]) = 0x0000;

				if (pPvtData->pull_header) {
					memcpy(pData, pMediaQItem->pPubData, pMediaQItem->dataLen);
					*dataLen = pMediaQItem->dataLen;
				}
				else {
					memcpy(pPayload, pMediaQItem->pPubData, pMediaQItem->dataLen);
					*dataLen = pMediaQItem->dataLen + TOTAL_HEADER_SIZE;
				}

				openavbMediaQTailPull(pMediaQ);
				AVB_TRACE_LINE(AVB_TRACE_MAP_LINE);
				AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
				return TX_CB_RET_PACKET_READY;
			}
			else {
				openavbMediaQTailPull(pMediaQ);
				AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
				return TX_CB_RET_PACKET_NOT_READY;  // No payload
			}
		}
		else {
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return TX_CB_RET_PACKET_NOT_READY;  // Media queue empty
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return TX_CB_RET_PACKET_NOT_READY;
}

// A call to this callback indicates that this mapping module will be
// a listener. Any listener initialization can be done in this function.
void openavbMapPipeRxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This callback occurs when running as a listener and data is available.
bool openavbMapPipeRxCB(media_q_t *pMediaQ, U8 *pData, U32 dataLen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	if (pMediaQ && pData) {
		const U8 *pHdr = pData;
		U8 *pPayload = pData + TOTAL_HEADER_SIZE;
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return FALSE;
		}

		media_q_item_t *pMediaQItem = openavbMediaQHeadLock(pMediaQ);
		if (pMediaQItem) {
			U32 timestamp = ntohl(*(U32 *)(&pHdr[HIDX_AVTP_TIMESPAMP32]));
			openavbAvtpTimeSetToTimestamp(pMediaQItem->pAvtpTime, timestamp);

			// Set timestamp valid and timestamp uncertain flags
			openavbAvtpTimeSetTimestampValid(pMediaQItem->pAvtpTime, (pHdr[HIDX_AVTP_HIDE7_TV1] & 0x01) ? TRUE : FALSE);
			openavbAvtpTimeSetTimestampUncertain(pMediaQItem->pAvtpTime, (pHdr[HIDX_AVTP_HIDE7_TU1] & 0x01) ? TRUE : FALSE);

			if (pPvtData->push_header) {
				if (pMediaQItem->itemSize >= dataLen) {
					memcpy(pMediaQItem->pPubData, pData, dataLen);
					pMediaQItem->dataLen = dataLen;
				}
				else {
					AVB_LOG_ERROR("Data to large for media queue.");
					pMediaQItem->dataLen = 0;
				}
			}
			else {
				// for alignment
				U16 payloadLen;
				memcpy(&payloadLen, &pHdr[HIDX_AVTP_DATALEN16], sizeof(U16));
				payloadLen = ntohs(payloadLen);

				if (pMediaQItem->itemSize >= payloadLen) {
					memcpy(pMediaQItem->pPubData, pPayload, payloadLen);
					pMediaQItem->dataLen = payloadLen;
				}
				else {
					AVB_LOG_ERROR("Data to large for media queue.");
					pMediaQItem->dataLen = 0;
				}
			}

			openavbMediaQHeadPush(pMediaQ);
			AVB_TRACE_LINE(AVB_TRACE_MAP_LINE);
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return TRUE;
		}
		else {
			IF_LOG_INTERVAL(1000) AVB_LOG_INFO("Media queue full");
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return FALSE;   // Media queue full
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return FALSE;
}

// This callback will be called when the mapping module needs to be closed.
// All cleanup should occur in this function.
void openavbMapPipeEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

void openavbMapPipeGenEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Initialization entry point into the mapping module. Will need to be included in the .ini file.
extern DLL_EXPORT bool openavbMapPipeInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		pMediaQ->pMediaQDataFormat = strdup(MapPipeMediaQDataFormat);
		pMediaQ->pPvtMapInfo = calloc(1, sizeof(pvt_data_t));       // Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pMediaQDataFormat || !pMediaQ->pPvtMapInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for mapping module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;

		pMapCB->map_cfg_cb = openavbMapPipeCfgCB;
		pMapCB->map_subtype_cb = openavbMapPipeSubtypeCB;
		pMapCB->map_avtp_version_cb = openavbMapPipeAvtpVersionCB;
		pMapCB->map_max_data_size_cb = openavbMapPipeMaxDataSizeCB;
		pMapCB->map_transmit_interval_cb = openavbMapPipeTransmitIntervalCB;
		pMapCB->map_gen_init_cb = openavbMapPipeGenInitCB;
		pMapCB->map_tx_init_cb = openavbMapPipeTxInitCB;
		pMapCB->map_tx_cb = openavbMapPipeTxCB;
		pMapCB->map_rx_init_cb = openavbMapPipeRxInitCB;
		pMapCB->map_rx_cb = openavbMapPipeRxCB;
		pMapCB->map_end_cb = openavbMapPipeEndCB;
		pMapCB->map_gen_end_cb = openavbMapPipeGenEndCB;

		pPvtData->itemCount = 20;
		pPvtData->txInterval = 0;
		pPvtData->push_header = FALSE;
		pPvtData->pull_header = FALSE;
		pPvtData->maxTransitUsec = inMaxTransitUsec;

		pPvtData->maxPayloadSize = 1024;
		pPvtData->maxDataSize = (pPvtData->maxPayloadSize + TOTAL_HEADER_SIZE);
		pPvtData->itemSize = pPvtData->maxDataSize;

		openavbMediaQSetMaxLatency(pMediaQ, inMaxTransitUsec);
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return TRUE;
}

