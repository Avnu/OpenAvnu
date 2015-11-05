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
* MODULE SUMMARY : Mpeg2 TS mapping module conforming to AVB 61883-4 encapsulation.
*/

#include <stdlib.h>
#include <string.h>
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_avtp_time_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_map_pub.h"
#include "openavb_map_mpeg2ts_pub.h"
#include "openavb_types.h"
#include <assert.h>

#define	AVB_LOG_COMPONENT	"MPEG2TS Mapping"
#include "openavb_log_pub.h"

// Base MPEG2 Transport Stream packet size
#define MPEG2_TS_PKT_SIZE			188
// MPEG2TS sync byte
#define MPEG2_TS_SYNC_BYTE			0x47

// GStreamer likes to pass 4096-byte buffers, so we want a buffer
// bigger than that.  And, we're more efficient if the interface layer
// passes us whole TS packets, so let's use a default value that's a
// multiple of both 188 and 192
#define MPEG2TS_MQITEM_SIZE			9024
#define MPEG2TS_MQITEM_COUNT		20

// MPEG2TS packets with 4 byte timestamp (192 bytes) are called "source packets" in AVTP
#define MPEGTS_SRC_PKT_HDR_SIZE		4
#define MPEGTS_SRC_PKT_SIZE 		(MPEG2_TS_PKT_SIZE + MPEGTS_SRC_PKT_HDR_SIZE)

// AVTP payload: we allow for 5 source packets (192 * 5) plus the CIP header (8) = 968
// TODO: make the number of packet configurable
#define AVTP_DFLT_SRC_PKTS				7

// AVTP Header sizes
#define AVTP_V0_HEADER_SIZE			12
#define MAP_HEADER_SIZE				12
#define CIP_HEADER_SIZE				8

#define TOTAL_HEADER_SIZE			(AVTP_V0_HEADER_SIZE + MAP_HEADER_SIZE + CIP_HEADER_SIZE)

#define BLOCKS_PER_SRC_PKT			8
#define BLOCKS_PER_AVTP_PKT			(BLOCKS_PER_SRC_PKT * AVTP_NUM_SRC_PKTS)

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

// 8 bits		dbs (data block size)		= 6 (6 quadlets per block)
#define HIDX_DBS8					25

// 2 bits		FN (fraction number)		= Bx11 (8 data blocks)
// 3 bits		QPC (quadlet padding count)	= Bx000
// 1 bit		SPH (source packet header)	= Bx1 (using source packet headers)
// 2 bits		RSV (reserved)				= Bx00
#define HIDX_FN2_QPC3_SPH1_RSV2		26

// 8 bits		DBC (data block counter)	= counter
#define HIDX_DBC8					27

// 2 bits		cip flag (2nd quadlet)		= binary 10
// 6 bits		fmt (stream format)			= binary 100000 (MPEG2-TS)
#define HIDX_CIP2_FMT6				28

// 3 bytes		fdf (format dependant)
// i.e. for MPEG2-TS
// 1 bit		tsf (time shift flag)		= 0
// 23 bits		reserved				    = 0
#define HIDX_TSF1_RESA7				29
#define HIDX_RESB8					30
#define HIDX_RESC8					31

#define DEFAULT_SRC_PKTS_PER_AVTP_FRAME 5

typedef struct {
	/////////////
	// Config data
	/////////////

	// map_nv_item_count, map_nv_item_size
	// number of items to allocate in MediaQ, size of data in each item
	unsigned itemCount, itemSize;

	// map_nv_ts_packet_size
	// size of transport stream packets passed to/from interface module (188 or 192)
	unsigned tsPacketSize;

	// Talker-only config

	// map_nv_num_source_packets
	// Number of source packets to send in AVTP frame
	unsigned numSourcePackets;

	// map_nv_tx_rate
	// Transmit rate in frames per second. 0 = default for talker class.
	unsigned txRate;

	/////////////
	// Variable data
	/////////////
	unsigned maxTransitUsec;        // In microseconds

	// Data block continuity counter
	U32 DBC;

	// Saved partial source packet
	char savedBytes[200];
	int  nSavedBytes;

	// Is the input stream out of synch?
	bool unsynched;

	unsigned int srcBitrate;

} pvt_data_t;


// Each configuration name value pair for this mapping will result in this callback being called.
void openavbMapMpeg2tsCfgCB(media_q_t *pMediaQ, const char *name, const char *value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return;
		}

		char *pEnd;
		unsigned long tmp;
		bool nameOK = TRUE, valueOK = FALSE;

		if (strcmp(name, "map_nv_item_count") == 0) {
			tmp = strtoul(value, &pEnd, 10);
			if (pEnd != value && *pEnd == '\0') {
				pPvtData->itemCount = tmp;
				valueOK = TRUE;
			}
		}
		else if (strcmp(name, "map_nv_item_size") == 0) {
			tmp = strtoul(value, &pEnd, 10);
			if (pEnd != value && *pEnd == '\0') {
				pPvtData->itemSize = tmp;
				valueOK = TRUE;
			}
		}
		else if (strcmp(name, "map_nv_ts_packet_size") == 0) {
			tmp = strtoul(value, &pEnd, 10);
			if (pEnd != value && *pEnd == '\0' && (tmp == 188 || tmp == 192)) {
				pPvtData->tsPacketSize = tmp;
				valueOK = TRUE;
			}
		}
		else if (strcmp(name, "map_nv_num_source_packets") == 0) {
			tmp = strtoul(value, &pEnd, 10);
			if (pEnd != value && *pEnd == '\0') {
				pPvtData->numSourcePackets = tmp;
				valueOK = TRUE;
			}
		}
		else if (strcmp(name, "map_nv_tx_rate") == 0
			|| strcmp(name, "map_nv_tx_interval") == 0) {
			tmp = strtoul(value, &pEnd, 10);
			if (pEnd != value && *pEnd == '\0') {
				pPvtData->txRate = tmp;
				valueOK = TRUE;
			}
		}
		else {
			AVB_LOGF_WARNING("Unknown configuration item: %s", name);
			nameOK = FALSE;
		}

		if (nameOK && !valueOK) {
			AVB_LOGF_WARNING("Bad value for configuration item: %s = %s", name, value);
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

U8 openavbMapMpeg2tsSubtypeCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return 0x00;        // 61883 AVB subtype
}

// Returns the AVTP version used by this mapping
U8 openavbMapMpeg2tsAvtpVersionCB()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return 0x00;        // Version 0
}

U16 openavbMapMpeg2tsMaxDataSizeCB(media_q_t *pMediaQ)
{
	U16 retval = 0;
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return 0;
		}

		retval = MPEGTS_SRC_PKT_SIZE * pPvtData->numSourcePackets + TOTAL_HEADER_SIZE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return retval;
}

// Returns the intended transmit interval (in frames per second). 0 = default for talker / class.
U32 openavbMapMpeg2tsTransmitIntervalCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return 0;
		}

		return pPvtData->txRate;
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return 0;
}

void openavbMapMpeg2tsGenInitCB(media_q_t *pMediaQ)
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
void openavbMapMpeg2tsTxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

static int syncScan(media_q_item_t *pMediaQItem, int startIdx)
{
	char *data = pMediaQItem->pPubData;
	int offset;
	for (offset = startIdx; offset < pMediaQItem->dataLen; offset++) {
		if (data[offset] == MPEG2_TS_SYNC_BYTE)
			break;
	}

	AVB_LOGF_WARNING("Dropped %d bytes", offset - startIdx);
	if (offset >= pMediaQItem->dataLen)
		offset = -1;

	return offset;
}

// This talker callback will be called for each AVB observation interval.
tx_cb_ret_t openavbMapMpeg2tsTxCB(media_q_t *pMediaQ, U8 *pData, U32 *dataLen)
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

		//pHdr[HIDX_AVTP_TIMESTAMP32] = 0x00;			// Set later
		*(U32 *)(&pHdr[HIDX_GATEWAY32]) = 0x00000000;
		//pHdr[HIDX_DATALEN16] = 0x00;					// Set Later
		pHdr[HIDX_TAG2_CHANNEL6] = (1 << 6) | 0x1f;
		pHdr[HIDX_TCODE4_SY4] = (0x0a << 4) | 0;

		// Set the majority of the CIP header now.
		pHdr[HIDX_CIP2_SID6] = (0x00 << 6) | 0x3f;
		pHdr[HIDX_DBS8] = 0x06;
		pHdr[HIDX_FN2_QPC3_SPH1_RSV2] = (0x03 << 6) | (0x00 << 3) | (0x01 << 2) | 0x00;
		// pHdr[HIDX_DBC8] = 0; // This will be set later.
		pHdr[HIDX_CIP2_FMT6] = (0x02 << 6) | 0xa0;
		pHdr[HIDX_TSF1_RESA7] = (0x01 << 7) | 0x00;
		pHdr[HIDX_RESB8] = 0x00;
		pHdr[HIDX_RESC8] = 0x00;

		media_q_item_t *pMediaQItem = openavbMediaQTailLock(pMediaQ, TRUE);
		int sourcePacketsAdded = 0;
		int nItemBytes, nAvailBytes;
		int offset, bytesNeeded;
		U32 timestamp;
		bool moreSourcePackets = TRUE;

		while (pMediaQItem && moreSourcePackets) {

			if (pPvtData->unsynched) {
				// Scan forward, looking for next sync byte.
				offset = syncScan(pMediaQItem, 0);
				if (offset >= 0)
					pMediaQItem->readIdx = offset;
				else {
					pPvtData->unsynched = TRUE;
					pMediaQItem->dataLen = 0;
					pMediaQItem->readIdx = 0;
				}
			}

			nItemBytes = pMediaQItem->dataLen - pMediaQItem->readIdx;
			nAvailBytes = nItemBytes + pPvtData->nSavedBytes;

			if (nItemBytes == 0) {
				// Empty MQ item, ignore
			}
			else if (nAvailBytes >= pPvtData->tsPacketSize) {
				// PTP walltime already set in the interface module. Just add the max transit time.
				// If this is a new mq item, add the AVTP transit time to the timestamp
				if (pMediaQItem->readIdx == 0) {
					openavbAvtpTimeAddUSec(pMediaQItem->pAvtpTime, pPvtData->maxTransitUsec);
				}

				timestamp = openavbAvtpTimeGetAvtpTimestamp(pMediaQItem->pAvtpTime);

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

				// Set the timestamp.
				if (sourcePacketsAdded == 0) {
					// TODO: I think this is wrong with source packets
					*(U32 *)(&pHdr[HIDX_AVTP_TIMESTAMP32]) = htonl(timestamp);
				}

				/* Copy the TS packets into the outgoing AVTP frame
				 */
				offset = 0, bytesNeeded = pPvtData->tsPacketSize;

				// If getting 188-byte packets from interface, need to add source packet header
				if (pPvtData->tsPacketSize == MPEG2_TS_PKT_SIZE) {
					// Set the timestamp on this source packet
					*((U32 *)pPayload) = htonl(timestamp);
					offset = MPEGTS_SRC_PKT_HDR_SIZE;
				}

				// Use any leftover data from last MQ item
				if (pPvtData->nSavedBytes) {
					memcpy(pPayload + offset, pPvtData->savedBytes, pPvtData->nSavedBytes);
					offset += pPvtData->nSavedBytes;
					bytesNeeded -= pPvtData->nSavedBytes;
					pPvtData->nSavedBytes = 0;
				}

				// Now, copy data from current MQ item
				memcpy(pPayload + offset, pMediaQItem->pPubData + pMediaQItem->readIdx, bytesNeeded);

				// Check that the data we've copied is synchronized
				/// i.e. that the transport stream packet starts
				//  where we think it should
				if (pPayload[4] == MPEG2_TS_SYNC_BYTE) {
					// OK, now we can update the read index
					pMediaQItem->readIdx += bytesNeeded;
					// and move the payload ptr for the next source packet
					pPayload += MPEGTS_SRC_PKT_SIZE;

					// Keep track of how many source packets have been added to the outgoing packet
					sourcePacketsAdded++;
				}
				else {
					AVB_LOG_WARNING("Alignment problem");

					// Scan forward, looking for next sync byte.
					// Start scan after 4-byte header if getting 192 byte packets.
					// Ignore saved data if there was any, start from what's in current item.
					offset = syncScan(pMediaQItem, pMediaQItem->readIdx + (pPvtData->tsPacketSize - MPEG2_TS_PKT_SIZE) + 1);
					if (offset >= 0)
						pMediaQItem->readIdx = offset;
					else {
						pPvtData->unsynched = TRUE;
						pMediaQItem->dataLen = 0;
						pMediaQItem->readIdx = 0;
					}
				}
			}
			else {
				// Arghhh - a partial packet.
				assert(pPvtData->nSavedBytes + nItemBytes <= MPEG2_TS_PKT_SIZE);

				memcpy(pPvtData->savedBytes + pPvtData->nSavedBytes,
					pMediaQItem->pPubData + pMediaQItem->readIdx,
					nItemBytes);
				pPvtData->nSavedBytes += nItemBytes;

				pMediaQItem->dataLen = 0;
				pMediaQItem->readIdx = 0;
			}

			// Have we reached our target?
			if (sourcePacketsAdded >= pPvtData->numSourcePackets)
				moreSourcePackets = FALSE;

			// Have we used up the data in the MQ item?
			if (pMediaQItem->dataLen - pMediaQItem->readIdx == 0) {
				// release used-up item
				openavbMediaQTailPull(pMediaQ);

				// and get a new one, if needed
				if (moreSourcePackets)
					pMediaQItem = openavbMediaQTailLock(pMediaQ, TRUE);
				else pMediaQItem = NULL;
			}

		} // while (pMediaQItem && moreSourcePackets)

		if (pMediaQItem) {
			// holds more data, leave it in the queue
			openavbMediaQTailUnlock(pMediaQ);
		}

		if (sourcePacketsAdded > 0) {

			// Set the block continuity and data length
			pHdr[HIDX_DBC8] = pPvtData->DBC;
			pPvtData->DBC = (pPvtData->DBC + (sourcePacketsAdded * BLOCKS_PER_SRC_PKT)) & 0x000000ff;

			U16 datalen = (sourcePacketsAdded * MPEGTS_SRC_PKT_SIZE) + CIP_HEADER_SIZE;
			*(U16 *)(pHdr + HIDX_DATALEN16) = htons(datalen);

			// Set out bound data length (entire packet length)
			*dataLen = (sourcePacketsAdded * MPEGTS_SRC_PKT_SIZE) + TOTAL_HEADER_SIZE;

			AVB_TRACE_LINE(AVB_TRACE_MAP_LINE);
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return TX_CB_RET_PACKET_READY;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return TX_CB_RET_PACKET_NOT_READY;
}

// A call to this callback indicates that this mapping module will be
// a listener. Any listener initialization can be done in this function.
void openavbMapMpeg2tsRxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

// This callback occurs when running as a listener and data is available.
bool openavbMapMpeg2tsRxCB(media_q_t *pMediaQ, U8 *pData, U32 dataLen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP_DETAIL);
	if (pMediaQ && pData) {
		U8 *pHdr = pData;
		U8 *pPayload = pData + TOTAL_HEADER_SIZE;
		U8 sourcePacketCount = 0;
		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private mapping module data not allocated.");
			return FALSE;
		}

		//pHdr[HIDX_AVTP_TIMESTAMP32];
		//pHdr[HIDX_GATEWAY32];
		//pHdr[HIDX_DATALEN16];

		//pHdr[HIDX_TAG2_CHANNEL6];
		//pHdr[HIDX_TCODE4_SY4];
		//pHdr[HIDX_CIP2_SID6];
		//pHdr[HIDX_DBS8];
		//pHdr[HIDX_FN2_QPC3_SPH1_RSV2];

		// Only support full source packets.
		// TODO: This mapper could be enhanced to support partial source packets and assemble the datablocks
		//	recieved across multiple avtp packets.
		if ((pHdr[HIDX_DBC8] % 8) > 0) {
			AVB_LOG_ERROR("Unsupported MPEG2TS block count received, payload discarded.");
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return FALSE;
		}

		// TODO: validate DBC

		// Determine the number of source packets bundled into the AVTP payload.
		U16 datalen = ntohs(*(U16 *)(pHdr + HIDX_DATALEN16));
		sourcePacketCount = (datalen - CIP_HEADER_SIZE) / MPEGTS_SRC_PKT_SIZE;

		//pHdr[HIDX_CIP2_FMT6];
		//pHdr[HIDX_TSF1_RESA7];
		//pHdr[HIDX_RESB8];
		//pHdr[HIDX_RESC8];

		if (sourcePacketCount > 0) {
			// Get item in media queue
			media_q_item_t *pMediaQItem = openavbMediaQHeadLock(pMediaQ);
			if (pMediaQItem) {
				if (pMediaQItem->dataLen == 0) {
					// Get the timestamp and place it in the media queue item.
					U32 timestamp = ntohl(*(U32 *)(&pHdr[HIDX_AVTP_TIMESTAMP32]));
					openavbAvtpTimeSetToTimestamp(pMediaQItem->pAvtpTime, timestamp);

					// Set timestamp valid and timestamp uncertain flags
					openavbAvtpTimeSetTimestampValid(pMediaQItem->pAvtpTime, (pHdr[HIDX_AVTP_HIDE7_TV1] & 0x01) ? TRUE : FALSE);
					openavbAvtpTimeSetTimestampUncertain(pMediaQItem->pAvtpTime, (pHdr[HIDX_AVTP_HIDE7_TU1] & 0x01) ? TRUE : FALSE);
				}

				int i;
				for (i = 0; i < sourcePacketCount; i++) {
					if (pMediaQItem->itemSize - pMediaQItem->dataLen >= pPvtData->tsPacketSize) {
						memcpy(pMediaQItem->pPubData + pMediaQItem->dataLen,
							pPayload + MPEGTS_SRC_PKT_SIZE - pPvtData->tsPacketSize,
							pPvtData->tsPacketSize);
						pMediaQItem->dataLen += pPvtData->tsPacketSize;
						pPayload += MPEGTS_SRC_PKT_SIZE;
					}
					else {
						AVB_LOG_ERROR("Data too large for media queue");
						pMediaQItem->dataLen = 0;
						break;
					}
				}

				// TODO: try to pack more data into MQ item?
				openavbMediaQHeadPush(pMediaQ);
			}
			else {
				// Media queue full, data dropped
				IF_LOG_INTERVAL(1000) AVB_LOG_ERROR("Media queue full");
				AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
				return FALSE;
			}
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
	return FALSE;
}

// This callback will be called when the mapping module needs to be closed.
// All cleanup should occur in this function.
void openavbMapMpeg2tsEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAP);
}

void openavbMapMpeg2tsGenEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbMapMpeg2tsSetSrcBitrateCB(media_q_t *pMediaQ, unsigned int bitrate)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	((pvt_data_t*)pMediaQ->pPvtMapInfo)->srcBitrate = (unsigned int)((double)bitrate * ((double)MPEGTS_SRC_PKT_SIZE)/((double)MPEG2_TS_PKT_SIZE));
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

unsigned int calculateBitrate(const int intervalsPerSecond, const int num_source_packets, const int number_of_frames)
{
	unsigned int bitrate = intervalsPerSecond * num_source_packets * number_of_frames * (MPEGTS_SRC_PKT_SIZE) * 8;
	return bitrate;
}

unsigned int openavbMapMpeg2tsGetMaxIntervalFramesCB(media_q_t *pMediaQ, SRClassIdx_t sr_class)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	unsigned int interval_frames = 1;
	unsigned int classRate = 0;
	switch (sr_class) {
	case SR_CLASS_A:
		classRate = 8000;
		break;
	case SR_CLASS_B:
		classRate = 4000;
		break;
	case MAX_AVB_SR_CLASSES:	// Case included to avoid warning on some toolchains
		break;
	}

	((pvt_data_t*)pMediaQ->pPvtMapInfo)->numSourcePackets = 1;
	while (calculateBitrate(classRate,((pvt_data_t*)pMediaQ->pPvtMapInfo)->numSourcePackets,interval_frames) < ((pvt_data_t*)pMediaQ->pPvtMapInfo)->srcBitrate)
	{
		++((pvt_data_t*)pMediaQ->pPvtMapInfo)->numSourcePackets;
		if (((pvt_data_t*)pMediaQ->pPvtMapInfo)->numSourcePackets > 7)
		{
			((pvt_data_t*)pMediaQ->pPvtMapInfo)->numSourcePackets = 1;
			++interval_frames;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return interval_frames;
}

// Initialization entry point into the mapping module. Will need to be included in the .ini file.
extern DLL_EXPORT bool openavbMapMpeg2tsInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAP);

	if (pMediaQ) {
		pMediaQ->pMediaQDataFormat = strdup(MapMpeg2tsMediaQDataFormat);
		pMediaQ->pPvtMapInfo = calloc(1, sizeof(pvt_data_t));       // Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pMediaQDataFormat || !pMediaQ->pPvtMapInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for mapping module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtMapInfo;

		pMapCB->map_cfg_cb = openavbMapMpeg2tsCfgCB;
		pMapCB->map_subtype_cb = openavbMapMpeg2tsSubtypeCB;
		pMapCB->map_avtp_version_cb = openavbMapMpeg2tsAvtpVersionCB;
		pMapCB->map_max_data_size_cb = openavbMapMpeg2tsMaxDataSizeCB;
		pMapCB->map_transmit_interval_cb = openavbMapMpeg2tsTransmitIntervalCB;
		pMapCB->map_gen_init_cb = openavbMapMpeg2tsGenInitCB;
		pMapCB->map_tx_init_cb = openavbMapMpeg2tsTxInitCB;
		pMapCB->map_tx_cb = openavbMapMpeg2tsTxCB;
		pMapCB->map_rx_init_cb = openavbMapMpeg2tsRxInitCB;
		pMapCB->map_rx_cb = openavbMapMpeg2tsRxCB;
		pMapCB->map_end_cb = openavbMapMpeg2tsEndCB;
		pMapCB->map_gen_end_cb = openavbMapMpeg2tsGenEndCB;
		pMapCB->map_set_src_bitrate_cb = openavbMapMpeg2tsSetSrcBitrateCB;
		pMapCB->map_get_max_interval_frames_cb = openavbMapMpeg2tsGetMaxIntervalFramesCB;

		pPvtData->tsPacketSize = MPEG2_TS_PKT_SIZE;
		pPvtData->numSourcePackets = AVTP_DFLT_SRC_PKTS;
		pPvtData->itemCount = MPEG2TS_MQITEM_COUNT;
		pPvtData->itemSize = MPEG2TS_MQITEM_SIZE;
		pPvtData->txRate = 0;
		pPvtData->maxTransitUsec = inMaxTransitUsec;
		pPvtData->DBC = 0;

		openavbMediaQSetMaxLatency(pMediaQ, inMaxTransitUsec);
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAP);
	return TRUE;
}
