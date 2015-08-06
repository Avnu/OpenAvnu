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
* MODULE SUMMARY : Control mapping module.
* 
* The Control mapping is an AVTP control subtype with an vender specific
* type defined to carry control command payloads between custom interface
* modules. This is compliant with 1722a-D6
* 
*----------------------------------------------------------------*
* 
* HEADERS
* 
*  -+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-
* |C|             |S|     |M| |F|T|               |             |T|   
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
* |                                                               |   
* |format info                                                    |   
* --+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+--
* |                               |proto  |       |number of      | 
* |stream data length             |type   |reserve|messages       | 
* --+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+--
* |OPENAVB control    |                               |               | 
* |format         |OPENAVB data length                |OPENAVB reserved   | 
* --+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+--
* 
******* Standard AVTP header fields for the AVTP_CONTROL subtype IEEE 1722a Section 10.
* 
* CD 				: Standard AVTP
* Subtype 			: AVTP Control 0x04
* SV				: Standard AVTP
* Ver`				: Standard AVTP
* MR				: Standard AVTP
* R					: Standard AVTP
* FV				: Standard AVTP Set to zero (0)
* TV				: Standard AVTP
* Sequence number	: Standard AVTP
* Reserved			: Standard AVTP
* TU				: Standard AVTP
* Stream ID			: Standard AVTP
* AVTP timestamp	: Standard AVTP
* Format info		: Standard AVTP (unused for AVTP control streams)
* Data Length		: Length of the data payload
* Protocol type 	: Set to "F" for CTL_PROPRIETARY
* Reserved 			: Standard AVTP
* Number of messages: This mapping module will always transport just 1 message.
* 
******* OPENAVB Vendor headers
* 
* OPENAVB Ctrl format	: OPENAVB specific control format. 0x01 for this mapping.
* OPENAVB data length	: Data length of the OPENAVB control payload.
* OPENAVB reserved		: reserved
* 
*/

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_avtp_time_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_map_pub.h"
#include "openavb_map_ctrl_pub.h"

#define	AVB_LOG_COMPONENT	"Control Mapping"
#include "openavb_log_pub.h"

#define AVTP_V0_HEADER_SIZE			12
#define SUBTYPE_HEADER_SIZE			12
#define OPENAVB_FORMAT_HEADER_SIZE		4

#define AVTP_HEADER_SIZE			(AVTP_V0_HEADER_SIZE + SUBTYPE_HEADER_SIZE)
#define TOTAL_HEADER_SIZE			(AVTP_HEADER_SIZE + OPENAVB_FORMAT_HEADER_SIZE)

//////
// AVTP Version 0 Header
//////

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

// - 4 bytes	format information
#define HIDX_AVTP_FORMAT_INFO32		16

// - 2 bytes	data length
#define HIDX_AVTP_DATA_LENGTH16		20

// - 1 byte		protocol type | Reserved
#define HIDX_PROTO4_RESERVED4		22

// - 1 byte		Number of messages
#define HIDX_NUMMESS8				23


//////
// OPENAVB specific header
//////

// - 1 byte		OPENAVB format
#define HIDX_OPENAVB_CTRL_FORMAT8		24

// - 2 bytes	stream_data_len
#define HIDX_OPENAVB_CTRL_DATALEN16		25

// - 1 bytes	OPENAVB reserved
#define HIDX_OPENAVB_RESERVEDA8			27


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
void openavbMapCtrlCfgCB(media_q_t *pMediaQ, const char *name, const char *value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	if (pMediaQ) {
		char *pEnd;

		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return;
		}

		if (strcmp(name, "map_nv_item_count") == 0) {
			pPvtData->itemCount = strtol(value, &pEnd, 10);
		}
		else if (strcmp(name, "map_nv_tx_rate") == 0 
			|| strcmp(name, "map_nv_tx_interval") == 0) {
			pPvtData->txInterval = strtol(value, &pEnd, 10);
		}
		else if (strcmp(name, "map_nv_max_payload_size") == 0) {
			pPvtData->maxPayloadSize = strtol(value, &pEnd, 10);
			pPvtData->maxDataSize = (pPvtData->maxPayloadSize + TOTAL_HEADER_SIZE);
			pPvtData->itemSize =	pPvtData->maxDataSize;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

U8 openavbMapCtrlSubtypeCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return 0x04;        // AVTP Control subtype 1722a 5.2.1.2 subtype field
}

// Returns the AVTP version used by this mapping
U8 openavbMapCtrlAvtpVersionCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return 0x00;        // Version 0
}

U16 openavbMapCtrlMaxDataSizeCB(media_q_t *pMediaQ)
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
U32 openavbMapCtrlTransmitIntervalCB(media_q_t *pMediaQ)
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

void openavbMapCtrlGenInitCB(media_q_t *pMediaQ)
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
void openavbMapCtrlTxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This talker callback will be called for each AVB observation interval.
tx_cb_ret_t openavbMapCtrlTxCB(media_q_t *pMediaQ, U8 *pData, U32 *dataLen)
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
				if (pMediaQItem->dataLen > pPvtData->maxPayloadSize) {
					AVB_LOGF_ERROR("Media queue data item size too large. Reported size: %d  Max Size: %d", pMediaQItem->dataLen, pPvtData->maxPayloadSize);
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
				else 
					pHdr[HIDX_AVTP_HIDE7_TU1] &= ~0x01;     // Clear

				*(U32 *)(&pHdr[HIDX_AVTP_TIMESPAMP32]) = htonl(openavbAvtpTimeGetAvtpTimestamp(pMediaQItem->pAvtpTime));
				*(U32 *)(&pHdr[HIDX_AVTP_FORMAT_INFO32]) = 0x00000000;
				*(U16 *)(&pHdr[HIDX_AVTP_DATA_LENGTH16]) = htons(pMediaQItem->dataLen + OPENAVB_FORMAT_HEADER_SIZE);
				pHdr[HIDX_PROTO4_RESERVED4] = 0xF0;      	// 1722a 10.2.2 protocol_type field. F for CTL_PROPRIETARY
				pHdr[HIDX_NUMMESS8] = 0x01;      			// Always 1 control message

				pHdr[HIDX_OPENAVB_CTRL_FORMAT8] = MAP_CTRL_OPENAVB_FORMAT;
				*(U16 *)(&pHdr[HIDX_OPENAVB_CTRL_DATALEN16]) = htons(pMediaQItem->dataLen);
				pHdr[HIDX_OPENAVB_RESERVEDA8] = 0x00;

				memcpy(pPayload, pMediaQItem->pPubData, pMediaQItem->dataLen);
				*dataLen = pMediaQItem->dataLen + TOTAL_HEADER_SIZE;

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
void openavbMapCtrlRxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This callback occurs when running as a listener and data is available.
bool openavbMapCtrlRxCB(media_q_t *pMediaQ, U8 *pData, U32 dataLen)
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

			if (pHdr[HIDX_OPENAVB_CTRL_FORMAT8] == MAP_CTRL_OPENAVB_FORMAT) {
				U16 payloadLen = ntohs(*(U16 *)(&pHdr[HIDX_OPENAVB_CTRL_DATALEN16]));
				if (pMediaQItem->itemSize >= payloadLen) {
					memcpy(pMediaQItem->pPubData, pPayload, payloadLen);
					pMediaQItem->dataLen = payloadLen;
				}
				else {
					AVB_LOG_ERROR("Data to large for media queue.");
					pMediaQItem->dataLen = 0;
				}
			}
			else {
				AVB_LOG_ERROR("Unexpected control stream format.");
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
void openavbMapCtrlEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

void openavbMapCtrlGenEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Initialization entry point into the mapping module. Will need to be included in the .ini file.
extern DLL_EXPORT bool openavbMapCtrlInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		pMediaQ->pMediaQDataFormat = strdup(MapCtrlMediaQDataFormat);
		pMediaQ->pPvtMapInfo = calloc(1, sizeof(pvt_data_t));       // Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pMediaQDataFormat || !pMediaQ->pPvtMapInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for mapping module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;

		pMapCB->map_cfg_cb = openavbMapCtrlCfgCB;
		pMapCB->map_subtype_cb = openavbMapCtrlSubtypeCB;
		pMapCB->map_avtp_version_cb = openavbMapCtrlAvtpVersionCB;
		pMapCB->map_max_data_size_cb = openavbMapCtrlMaxDataSizeCB;
		pMapCB->map_transmit_interval_cb = openavbMapCtrlTransmitIntervalCB;
		pMapCB->map_gen_init_cb = openavbMapCtrlGenInitCB;
		pMapCB->map_tx_init_cb = openavbMapCtrlTxInitCB;
		pMapCB->map_tx_cb = openavbMapCtrlTxCB;
		pMapCB->map_rx_init_cb = openavbMapCtrlRxInitCB;
		pMapCB->map_rx_cb = openavbMapCtrlRxCB;
		pMapCB->map_end_cb = openavbMapCtrlEndCB;
		pMapCB->map_gen_end_cb = openavbMapCtrlGenEndCB;

		pPvtData->itemCount = 20;
		pPvtData->txInterval = 0;
		pPvtData->maxTransitUsec = inMaxTransitUsec;

		pPvtData->maxPayloadSize = 1024;
		pPvtData->maxDataSize = (pPvtData->maxPayloadSize + TOTAL_HEADER_SIZE);
		pPvtData->itemSize = pPvtData->maxDataSize;

		openavbMediaQSetMaxLatency(pMediaQ, inMaxTransitUsec);
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return TRUE;
}

