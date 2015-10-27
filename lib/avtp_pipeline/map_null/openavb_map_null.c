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
* MODULE SUMMARY : NULL mapping module.
* 
* This NULL mapping does not pack or unpack any media data in AVB packer.
* It does however exercise the various functions and callback and can be
* used as an example or a template for new mapping modules.
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
* |               |                                               |   
* |OPENAVB format     |OPENAVB reserved                                   |   
* --+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+--
* |                                                               |   
* |OPENAVB format specific                                            |   
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
* OPENAVB format		: Null Mapping 0x00
* OPENAVB reserved		:
* OPENAVB format spec.	}
* 
*/

#include <stdlib.h>
#include <string.h>
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_avtp_time_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_map_pub.h"
#include "openavb_map_null_pub.h"

#define	AVB_LOG_COMPONENT	"Null Mapping"
#include "openavb_log_pub.h"

#define AVTP_V0_HEADER_SIZE			12
#define MAP_HEADER_SIZE				12

#define TOTAL_HEADER_SIZE			(AVTP_V0_HEADER_SIZE + MAP_HEADER_SIZE)

#define MAX_PAYLOAD_SIZE 			14
#define MAX_DATA_SIZE 				(MAX_PAYLOAD_SIZE + TOTAL_HEADER_SIZE)

#define ITEM_SIZE					MAX_DATA_SIZE


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
#define HIDX_AVTP_TIMESPAMP32		12

// - 1 byte		OPENAVB format
#define HIDX_OPENAVB_FORMAT8			16

// - 3 bytes	OPENAVB reserved
#define HIDX_OPENAVB_RESERVEDA8			17
#define HIDX_OPENAVB_RESERVEDB8			18
#define HIDX_OPENAVB_RESERVEDC8			19

// - 4 bytes	OPENAVB format specific
#define HIDX_OPENAVB_FORMAT_SPEC32		20

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

} pvt_data_t;

// Each configuration name value pair for this mapping will result in this callback being called.
void openavbMapNullCfgCB(media_q_t *pMediaQ, const char *name, const char *value)
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

// Returns the AVB subtype for this mapping
U8 openavbMapNullSubtypeCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return 0x7F;        // Experimental AVB subtype
}

// Returns the AVTP version used by this mapping
U8 openavbMapNullAvtpVersionCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return 0x00;        // Version 0
}

// Returns the max data size
U16 openavbMapNullMaxDataSizeCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return MAX_DATA_SIZE;
}

// Returns the intended transmit interval (in frames per second). 0 = default for talker / class.
U32 openavbMapNullTransmitIntervalCB(media_q_t *pMediaQ)
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

void openavbMapNullGenInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return;
		}

		openavbMediaQSetSize(pMediaQ, pPvtData->itemCount, ITEM_SIZE);
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// A call to this callback indicates that this mapping module will be
// a talker. Any talker initialization can be done in this function.
void openavbMapNullTxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This talker callback will be called for each AVB observation interval.
tx_cb_ret_t openavbMapNullTxCB(media_q_t *pMediaQ, U8 *pData, U32 *dataLen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	if (pMediaQ && pData && dataLen) {
		U8 *pHdr = pData;
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return TX_CB_RET_PACKET_NOT_READY;
		}

		media_q_item_t *pMediaQItem = openavbMediaQTailLock(pMediaQ, TRUE);
		if (pMediaQItem) {
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
			pHdr[HIDX_OPENAVB_FORMAT8] = MAP_NULL_OPENAVB_FORMAT;
			pHdr[HIDX_OPENAVB_RESERVEDA8] = 0x00;
			pHdr[HIDX_OPENAVB_RESERVEDB8] = 0x00;
			pHdr[HIDX_OPENAVB_RESERVEDC8] = 0x00;
			*(U32 *)(&pHdr[HIDX_OPENAVB_FORMAT_SPEC32]) = 0x00000000;

			*dataLen = TOTAL_HEADER_SIZE;   // No data
			openavbMediaQTailPull(pMediaQ);
			AVB_TRACE_LINE(AVB_TRACE_MAP_LINE);
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return TX_CB_RET_PACKET_READY;
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
void openavbMapNullRxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This callback occurs when running as a listener and data is available.
bool openavbMapNullRxCB(media_q_t *pMediaQ, U8 *pData, U32 dataLen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	if (pMediaQ && pData) {
		U8 *pHdr = pData;

		media_q_item_t *pMediaQItem = openavbMediaQHeadLock(pMediaQ);
		if (pMediaQItem) {
			// Get the timestamp and place it in the media queue item.
			U32 timestamp = ntohl(*(U32 *)(&pHdr[HIDX_AVTP_TIMESPAMP32]));
			openavbAvtpTimeSetToTimestamp(pMediaQItem->pAvtpTime, timestamp);

			// Set timestamp valid and timestamp uncertain flags
			openavbAvtpTimeSetTimestampValid(pMediaQItem->pAvtpTime, (pHdr[HIDX_AVTP_HIDE7_TV1] & 0x01) ? TRUE : FALSE);
			openavbAvtpTimeSetTimestampUncertain(pMediaQItem->pAvtpTime, (pHdr[HIDX_AVTP_HIDE7_TU1] & 0x01) ? TRUE : FALSE);

			pMediaQItem->dataLen = 0;       // Regardless of what is sent nullify the data to the interface.
			openavbMediaQHeadPush(pMediaQ);
			AVB_TRACE_LINE(AVB_TRACE_MAP_LINE);
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return TRUE;
		}
		else {
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return FALSE;   // Media queue full
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return FALSE;
}

// This callback will be called when the mapping module needs to be closed.
// All cleanup should occur in this function.
void openavbMapNullEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

void openavbMapNullGenEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Initialization entry point into the mapping module. Will need to be included in the .ini file.
extern DLL_EXPORT bool openavbMapNullInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		pMediaQ->pMediaQDataFormat = strdup(MapNullMediaQDataFormat);
		pMediaQ->pPvtMapInfo = calloc(1, sizeof(pvt_data_t));       // Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pMediaQDataFormat || !pMediaQ->pPvtMapInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for mapping module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;

		pMapCB->map_cfg_cb = openavbMapNullCfgCB;
		pMapCB->map_subtype_cb = openavbMapNullSubtypeCB;
		pMapCB->map_avtp_version_cb = openavbMapNullAvtpVersionCB;
		pMapCB->map_max_data_size_cb = openavbMapNullMaxDataSizeCB;
		pMapCB->map_transmit_interval_cb = openavbMapNullTransmitIntervalCB;
		pMapCB->map_gen_init_cb = openavbMapNullGenInitCB;
		pMapCB->map_tx_init_cb = openavbMapNullTxInitCB;
		pMapCB->map_tx_cb = openavbMapNullTxCB;
		pMapCB->map_rx_init_cb = openavbMapNullRxInitCB;
		pMapCB->map_rx_cb = openavbMapNullRxCB;
		pMapCB->map_end_cb = openavbMapNullEndCB;
		pMapCB->map_gen_end_cb = openavbMapNullGenEndCB;

		pPvtData->itemCount = 20;
		pPvtData->txInterval = 0;
		pPvtData->maxTransitUsec = inMaxTransitUsec;

		openavbMediaQSetMaxLatency(pMediaQ, inMaxTransitUsec);
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return TRUE;
}
