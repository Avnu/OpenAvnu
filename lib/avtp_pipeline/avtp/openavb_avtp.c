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
* MODULE SUMMARY : Implements main functions for AVTP.  Includes
* functions to create/destroy and AVTP stream, and to send or receive
* data from that AVTP stream.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include "openavb_platform.h"
#include "openavb_types.h"
#include "openavb_trace.h"
#include "openavb_avtp.h"
#include "openavb_rawsock.h"
#include "openavb_mediaq.h"

#define	AVB_LOG_COMPONENT	"AVTP"
#include "openavb_log.h"

// Maximum time that AVTP RX/TX calls should block before returning
#define AVTP_MAX_BLOCK_USEC (1 * MICROSECONDS_PER_SECOND)

/*
 * This is broken out into a function, so that we can close and reopen
 * the socket if we detect a problem receiving frames.
 */
static openavbRC openAvtpSock(avtp_stream_t *pStream)
{
	if (pStream->tx) {
		pStream->rawsock = openavbRawsockOpen(pStream->ifname, FALSE, TRUE, ETHERTYPE_AVTP, pStream->frameLen, pStream->nbuffers);
	}
	else {
#ifndef UBUNTU
		// This is the normal case for most of our supported platforms
		pStream->rawsock = openavbRawsockOpen(pStream->ifname, TRUE, FALSE, ETHERTYPE_8021Q, pStream->frameLen, pStream->nbuffers);
#else
		pStream->rawsock = openavbRawsockOpen(pStream->ifname, TRUE, FALSE, ETHERTYPE_AVTP, pStream->frameLen, pStream->nbuffers);
#endif
	}

	if (pStream->rawsock != NULL) {
		openavbSetRxSignalMode(pStream->rawsock, pStream->bRxSignalMode);

		if (!pStream->tx) {
			// Set the multicast address that we want to receive
			openavbRawsockRxMulticast(pStream->rawsock, TRUE, pStream->dest_addr.ether_addr_octet);
		}
		AVB_RC_RET(OPENAVB_AVTP_SUCCESS);
	}

	AVB_RC_LOG_RET(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVB_RC_RAWSOCK_OPEN));
}


// Evaluate the AVTP timestamp. Only valid for common AVTP stream subtypes
#define HIDX_AVTP_HIDE7_TV1			1
#define HIDX_AVTP_HIDE7_TU1			3
#define HIDX_AVTP_TIMESPAMP32		12
static void processTimestampEval(avtp_stream_t *pStream, U8 *pHdr)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP_DETAIL);

	if (pStream->tsEval) {
		bool tsValid =  (pHdr[HIDX_AVTP_HIDE7_TV1] & 0x01) ? TRUE : FALSE;
		bool tsUncertain = (pHdr[HIDX_AVTP_HIDE7_TU1] & 0x01) ? TRUE : FALSE;

		if (tsValid && !tsUncertain) {
			U32 ts = ntohl(*(U32 *)(&pHdr[HIDX_AVTP_TIMESPAMP32]));
			U32 tsSmoothed = openavbTimestampEvalTimestamp(pStream->tsEval, ts);
			if (tsSmoothed != ts) {
				*(U32 *)(&pHdr[HIDX_AVTP_TIMESPAMP32]) = htonl(tsSmoothed);
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVTP_DETAIL);
}


/* Initialize AVTP for talking
 */
openavbRC openavbAvtpTxInit(
	media_q_t *pMediaQ,
	openavb_map_cb_t *pMapCB,
	openavb_intf_cb_t *pIntfCB,
	char *ifname,
	AVBStreamID_t *streamID,
	U8 *destAddr,
	U32 max_transit_usec,
	U32 fwmark,
	U16 vlanID,
	U8  vlanPCP,
	U16 nbuffers,
	void **pStream_out)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP);
	AVB_LOG_DEBUG("Initialize");

	*pStream_out = NULL;

	// Malloc the structure to hold state information
	avtp_stream_t *pStream = calloc(1, sizeof(avtp_stream_t));
	if (!pStream) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVB_RC_OUT_OF_MEMORY), AVB_TRACE_AVTP);
	}
	pStream->tx = TRUE;

	pStream->pMediaQ = pMediaQ;
	pStream->pMapCB = pMapCB;
	pStream->pIntfCB = pIntfCB;

	pStream->pMapCB->map_tx_init_cb(pStream->pMediaQ);
	pStream->pIntfCB->intf_tx_init_cb(pStream->pMediaQ);
	if (pStream->pIntfCB->intf_set_stream_uid_cb) {
		pStream->pIntfCB->intf_set_stream_uid_cb(pStream->pMediaQ, streamID->uniqueID);
	}

	// Set the frame length
	pStream->frameLen = pStream->pMapCB->map_max_data_size_cb(pStream->pMediaQ) + ETH_HDR_LEN_VLAN;

	// and the latency
	pStream->max_transit_usec = max_transit_usec;

	// and save other stuff needed to (re)open the socket
	pStream->ifname = strdup(ifname);
	pStream->nbuffers = nbuffers;

	// Open a raw socket
	openavbRC rc = openAvtpSock(pStream);
	if (IS_OPENAVB_FAILURE(rc)) {
		free(pStream);
		AVB_RC_LOG_TRACE_RET(rc, AVB_TRACE_AVTP);
	}

	// Create the AVTP frame header for the frames we'll send
	hdr_info_t hdrInfo;

	U8 srcAddr[ETH_ALEN];
	if (openavbRawsockGetAddr(pStream->rawsock, srcAddr)) {
		hdrInfo.shost = srcAddr;
	}
	else {
		openavbRawsockClose(pStream->rawsock);
		free(pStream);
		AVB_LOG_ERROR("Failed to get source MAC address");
		AVB_RC_TRACE_RET(OPENAVB_AVTP_FAILURE, AVB_TRACE_AVTP);
	}

	hdrInfo.dhost = destAddr;
	if (vlanPCP != 0 || vlanID != 0) {
		hdrInfo.vlan = TRUE;
		hdrInfo.vlan_pcp = vlanPCP;
		hdrInfo.vlan_vid = vlanID;
		AVB_LOGF_DEBUG("VLAN pcp=%d vid=%d", hdrInfo.vlan_pcp, hdrInfo.vlan_vid);
	}
	else {
		hdrInfo.vlan = FALSE;
	}
	openavbRawsockTxSetHdr(pStream->rawsock, &hdrInfo);

	// Remember the AVTP subtype and streamID
	pStream->subtype = pStream->pMapCB->map_subtype_cb();

	memcpy(pStream->streamIDnet, streamID->addr, ETH_ALEN);
        U16 *pStreamUID = (U16 *)((U8 *)(pStream->streamIDnet) + ETH_ALEN);
       *pStreamUID = htons(streamID->uniqueID);

	// Set the fwmark - used to steer packets into the right traffic control queue
	openavbRawsockTxSetMark(pStream->rawsock, fwmark);

	*pStream_out = (void *)pStream;
	AVB_RC_TRACE_RET(OPENAVB_AVTP_SUCCESS, AVB_TRACE_AVTP);
}

#ifdef OPENAVB_AVTP_REPORT_RX_STATS
static void inline rxDeliveryStats(avtp_rx_info_t *rxInfo,
	struct timespec *tmNow,
	U32 early, U32 late)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP);

	rxInfo->rxCnt++;

	if (late > 0) {
		rxInfo->lateCnt++;
		if (late > rxInfo->maxLate)
			rxInfo->maxLate = late;
	}
	if (early > 0) {
		rxInfo->earlyCnt++;
		if (early > rxInfo->maxEarly)
			rxInfo->maxEarly = early;
	}

	if (rxInfo->lastTime.tv_sec == 0) {
		rxInfo->lastTime.tv_sec = tmNow->tv_sec;
		rxInfo->lastTime.tv_nsec = tmNow->tv_nsec;
	}
	else if ((tmNow->tv_sec > (rxInfo->lastTime.tv_sec + OPENAVB_AVTP_REPORT_INTERVAL))
		|| ((tmNow->tv_sec == (rxInfo->lastTime.tv_sec + OPENAVB_AVTP_REPORT_INTERVAL))
			&& (tmNow->tv_nsec > rxInfo->lastTime.tv_nsec))) {
		AVB_LOGF_INFO("Stream %d seconds, %lu samples: %lu late, max=%lums, %lu early, max=%lums",
			OPENAVB_AVTP_REPORT_INTERVAL, (unsigned long)rxInfo->rxCnt,
			(unsigned long)rxInfo->lateCnt, (unsigned long)rxInfo->maxLate / NANOSECONDS_PER_MSEC,
			(unsigned long)rxInfo->earlyCnt, (unsigned long)rxInfo->maxEarly / NANOSECONDS_PER_MSEC);
		rxInfo->maxLate = 0;
		rxInfo->lateCnt = 0;
		rxInfo->maxEarly = 0;
		rxInfo->earlyCnt = 0;
		rxInfo->rxCnt = 0;
		rxInfo->lastTime.tv_sec = tmNow->tv_sec;
		rxInfo->lastTime.tv_nsec = tmNow->tv_nsec;
	}

#if 0
	if (++txCnt >= 1000) {}
#endif
	AVB_TRACE_EXIT(AVB_TRACE_AVTP);
}
#endif

static openavbRC fillAvtpHdr(avtp_stream_t *pStream, U8 *pFill)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP_DETAIL);

	switch (pStream->pMapCB->map_avtp_version_cb()) {
		default:
			AVB_RC_LOG_RET(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVBAVTP_RC_INVALID_AVTP_VERSION));
		case 0:
			//
			// - 1 bit 		cd (control/data indicator)	= 0 (stream data)
			// - 7 bits 	subtype  					= as configured
			*pFill++ = pStream->subtype & 0x7F;
			// - 1 bit 		sv (stream valid)			= 1
			// - 3 bits 	AVTP version				= binary 000
			// - 1 bit		mr (media restart)			= toggled when clock changes
			// - 1 bit		r (reserved)				= 0
			// - 1 bit		gv (gateway valid)			= 0
			// - 1 bit		tv (timestamp valid)		= 1
			// TODO: set mr correctly
			*pFill++ = 0x81;
			// - 8 bits		sequence num				= increments with each frame
			*pFill++ = pStream->avtp_sequence_num;
			// - 7 bits		reserved					= 0;
			// - 1 bit		tu (timestamp uncertain)	= 1 when no PTP sync
			// TODO: set tu correctly
			*pFill++ = 0;
			// - 8 bytes    stream_id
			memcpy(pFill, (U8 *)&pStream->streamIDnet, 8);
			break;
	}
	AVB_RC_TRACE_RET(OPENAVB_AVTP_SUCCESS, AVB_TRACE_AVTP_DETAIL);
}

/* Send a frame
 */
openavbRC openavbAvtpTx(void *pv, bool bSend, bool txBlockingInIntf)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP_DETAIL);

	avtp_stream_t *pStream = (avtp_stream_t *)pv;
	if (!pStream) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AVTP_DETAIL);
	}

	U8 * pAvtpFrame,*pFill;
	U32 avtpFrameLen, frameLen;
	tx_cb_ret_t txCBResult = TX_CB_RET_PACKET_NOT_READY;

	// Get a TX buf if we don't already have one.
	//   (We keep the TX buf in our stream data, so that if we don't
	//    get data from the mapping module, we can use the buf next time.)
	if (!pStream->pBuf) {

		pStream->pBuf = (U8 *)openavbRawsockGetTxFrame(pStream->rawsock, TRUE, &frameLen);
		if (pStream->pBuf) {
			assert(frameLen >= pStream->frameLen);
			// Fill in the Ethernet header
			openavbRawsockTxFillHdr(pStream->rawsock, pStream->pBuf, &pStream->ethHdrLen);
		}
	}

	if (pStream->pBuf) {
		// AVTP frame starts right after the Ethernet header
		pAvtpFrame = pFill = pStream->pBuf + pStream->ethHdrLen;
		avtpFrameLen = pStream->frameLen - pStream->ethHdrLen;

		// Fill the AVTP Header. This must be done before calling the interface and mapping modules.
		openavbRC rc = fillAvtpHdr(pStream, pFill);
		if (IS_OPENAVB_FAILURE(rc)) {
			AVB_RC_LOG_TRACE_RET(rc, AVB_TRACE_AVTP_DETAIL);
		}

		U64 timeNsec = 0;

		if (!txBlockingInIntf) {
			// Call interface module to read data
			pStream->pIntfCB->intf_tx_cb(pStream->pMediaQ);

#if IGB_LAUNCHTIME_ENABLED
			// lets get unmodified timestamp from mediaq item about to be sent by mapping
			media_q_item_t* item = openavbMediaQTailLock(pStream->pMediaQ, true);
			if (item) {
				timeNsec = item->pAvtpTime->timeNsec;
				openavbMediaQTailUnlock(pStream->pMediaQ);
			}
#elif ATL_LAUNCHTIME_ENABLED
			if( pStream->pMapCB->map_lt_calc_cb ) {
				pStream->pMapCB->map_lt_calc_cb(pStream->pMediaQ, &timeNsec);
			}
#endif

			// Call mapping module to move data into AVTP frame
			txCBResult = pStream->pMapCB->map_tx_cb(pStream->pMediaQ, pAvtpFrame, &avtpFrameLen);

			pStream->bytes += avtpFrameLen;
		}
		else {

#if IGB_LAUNCHTIME_ENABLED
			// lets get unmodified timestamp from mediaq item about to be sent by mapping
			media_q_item_t* item = openavbMediaQTailLock(pStream->pMediaQ, true);
			if (item) {
				timeNsec = item->pAvtpTime->timeNsec;
				openavbMediaQTailUnlock(pStream->pMediaQ);
			}
#elif ATL_LAUNCHTIME_ENABLED
			if( pStream->pMapCB->map_lt_calc_cb ) {
				pStream->pMapCB->map_lt_calc_cb(pStream->pMediaQ, &timeNsec);
			}
#endif

			// Blocking in interface mode. Pull from media queue for tx first
			if ((txCBResult = pStream->pMapCB->map_tx_cb(pStream->pMediaQ, pAvtpFrame, &avtpFrameLen)) == TX_CB_RET_PACKET_NOT_READY) {
				// Call interface module to read data
				pStream->pIntfCB->intf_tx_cb(pStream->pMediaQ);
			}
			else {
				pStream->bytes += avtpFrameLen;
			}
		}

		// If we got data from the mapping module and stream is not paused,
		// notify the raw sockets.
		if (txCBResult != TX_CB_RET_PACKET_NOT_READY && !pStream->bPause) {

			if (pStream->tsEval) {
				processTimestampEval(pStream, pAvtpFrame);
			}

			// Increment the sequence number now that we are sure this is a good packet.
			pStream->avtp_sequence_num++;
			// Mark the frame "ready to send".
			openavbRawsockTxFrameReady(pStream->rawsock, pStream->pBuf, avtpFrameLen + pStream->ethHdrLen, timeNsec);
			// Send if requested
			if (bSend)
				openavbRawsockSend(pStream->rawsock);
			// Drop our reference to it
			pStream->pBuf = NULL;
		}
		else {
			AVB_RC_TRACE_RET(OPENAVB_AVTP_FAILURE, AVB_TRACE_AVTP_DETAIL);
		}
	}
	else {
		AVB_RC_TRACE_RET(OPENAVB_AVTP_FAILURE, AVB_TRACE_AVTP_DETAIL);
	}

	AVB_RC_TRACE_RET(OPENAVB_AVTP_SUCCESS, AVB_TRACE_AVTP_DETAIL);
}

openavbRC openavbAvtpRxInit(
	media_q_t *pMediaQ,
	openavb_map_cb_t *pMapCB,
	openavb_intf_cb_t *pIntfCB,
	char *ifname,
	AVBStreamID_t *streamID,
	U8 *daddr,
	U16 nbuffers,
	bool rxSignalMode,
	void **pStream_out)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP);
	AVB_LOG_DEBUG("Initialize");

	*pStream_out = NULL;

	if (!pMapCB) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVBAVTP_RC_MAPPING_CB_NOT_SET), AVB_TRACE_AVTP);
	}
	if (!pIntfCB) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVBAVTP_RC_INTERFACE_CB_NOT_SET), AVB_TRACE_AVTP);
	}
	if (!daddr) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AVTP);
	}

	avtp_stream_t *pStream = calloc(1, sizeof(avtp_stream_t));
	if (!pStream) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVB_RC_OUT_OF_MEMORY), AVB_TRACE_AVTP);
	}
	pStream->tx = FALSE;
	pStream->nLost = -1;

	pStream->pMediaQ = pMediaQ;
	pStream->pMapCB = pMapCB;
	pStream->pIntfCB = pIntfCB;

	pStream->pMapCB->map_rx_init_cb(pStream->pMediaQ);
	pStream->pIntfCB->intf_rx_init_cb(pStream->pMediaQ);
	if (pStream->pIntfCB->intf_set_stream_uid_cb) {
		pStream->pIntfCB->intf_set_stream_uid_cb(pStream->pMediaQ, streamID->uniqueID);
	}

	// Set the frame length
	pStream->frameLen = pStream->pMapCB->map_max_data_size_cb(pStream->pMediaQ) + ETH_HDR_LEN_VLAN;

	// Save the streamID
	memcpy(pStream->streamIDnet, streamID->addr, ETH_ALEN);
        U16 *pStreamUID = (U16 *)((U8 *)(pStream->streamIDnet) + ETH_ALEN);
       *pStreamUID = htons(streamID->uniqueID);

	// and the destination MAC address
	memcpy(pStream->dest_addr.ether_addr_octet, daddr, ETH_ALEN);

	// and other stuff needed to (re)open the socket
	pStream->ifname = strdup(ifname);
	pStream->nbuffers = nbuffers;
	pStream->bRxSignalMode = rxSignalMode;

	openavbRC rc = openAvtpSock(pStream);
	if (IS_OPENAVB_FAILURE(rc)) {
		free(pStream);
		AVB_RC_LOG_TRACE_RET(rc, AVB_TRACE_AVTP);
	}

	// Save the AVTP subtype
	pStream->subtype = pStream->pMapCB->map_subtype_cb();

	*pStream_out = (void *)pStream;
	AVB_RC_TRACE_RET(OPENAVB_AVTP_SUCCESS, AVB_TRACE_AVTP);
}

static void x_avtpRxFrame(avtp_stream_t *pStream, U8 *pFrame, U32 frameLen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP_DETAIL);
	IF_LOG_INTERVAL(4096) AVB_LOGF_DEBUG("pFrame=%p, len=%u", pFrame, frameLen);
	U8 subtype, flags, flags2, rxSeq, nLost, avtpVersion;
	U8 *pRead = pFrame;

	// AVTP Header
	//
	// Check control/data bit.  We only expect data packets.
	if (0 == (*pRead & 0x80)) {
		// - 7 bits 	subtype
		subtype = *pRead++ & 0x7F;
		flags   = *pRead++;
		avtpVersion = (flags >> 4) & 0x07;

		// Check AVTPDU version, BZ 106
		if (0 == avtpVersion) {

			rxSeq = *pRead++;

			if (pStream->nLost == -1) {
				// first frame received, don't check for mismatch
				pStream->nLost = 0;
			}
			else if (pStream->avtp_sequence_num != rxSeq) {
				nLost = (rxSeq - pStream->avtp_sequence_num)
					+ (rxSeq < pStream->avtp_sequence_num ? 256 : 0);
				AVB_LOGF_INFO("AVTP sequence mismatch: expected: %3u,\tgot: %3u,\tlost %3d",
					pStream->avtp_sequence_num, rxSeq, nLost);
				pStream->nLost += nLost;
			}
			pStream->avtp_sequence_num = rxSeq + 1;

			pStream->bytes += frameLen;

			flags2 = *pRead++;
			IF_LOG_INTERVAL(4096) AVB_LOGF_DEBUG("subtype=%u, sv=%u, ver=%u, mr=%u, tv=%u tu=%u",
				subtype, flags & 0x80, avtpVersion,
				flags & 0x08, flags & 0x01, flags2 & 0x01);

			pRead += 8;

			if (pStream->tsEval) {
				processTimestampEval(pStream, pFrame);
			}

			pStream->pMapCB->map_rx_cb(pStream->pMediaQ, pFrame, frameLen);

			// NOTE : This is a redundant call. It is handled in avtpTryRx()
			// pStream->pIntfCB->intf_rx_cb(pStream->pMediaQ);

			pStream->info.rx.bComplete = TRUE;

			// to prevent unused variable warnings
			(void)subtype;
			(void)flags2;
		}
		else {
			AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVBAVTP_RC_INVALID_AVTP_VERSION));
		}
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVBAVTP_RC_IGNORING_CONTROL_PACKET));
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVTP_DETAIL);
}

/*
 * Try to receive some data.
 *
 * Keeps state information in pStream.
 * Look at pStream->info for the received data.
 */
static void avtpTryRx(avtp_stream_t *pStream)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP_DETAIL);

	U8         *pBuf = NULL;   // pointer to buffer containing rcvd frame, if any
	U8         *pAvtpPdu;      // pointer to AVTP PDU within Ethernet frame
	U32         offsetToFrame; // offset into pBuf where Ethernet frame begins (bytes)
	U32         frameLen;      // length of the Ethernet frame (bytes)
	int         hdrLen;        // length of the Ethernet frame header (bytes)
	U32         avtpPduLen;    // length of the AVTP PDU (bytes)
	hdr_info_t  hdrInfo;       // Ethernet header contents
	U32         timeout;

	while (!pBuf) {
		if (!openavbMediaQUsecTillTail(pStream->pMediaQ, &timeout)) {
			// No mediaQ item available therefore wait for a new packet
			timeout = AVTP_MAX_BLOCK_USEC;
			pBuf = (U8 *)openavbRawsockGetRxFrame(pStream->rawsock, timeout, &offsetToFrame, &frameLen);
			if (!pBuf) {
				AVB_TRACE_EXIT(AVB_TRACE_AVTP_DETAIL);
				return;
			}
		}
		else if (timeout == 0) {
			// Process the pending media queue item and after check for available incoming packets
			pStream->pIntfCB->intf_rx_cb(pStream->pMediaQ);

			// Previously would check for new packets but disabled to favor presentation times.
			// pBuf = (U8 *)openavbRawsockGetRxFrame(pStream->rawsock, OPENAVB_RAWSOCK_NONBLOCK, &offsetToFrame, &frameLen);
		}
		else {
			if (timeout > AVTP_MAX_BLOCK_USEC)
				timeout = AVTP_MAX_BLOCK_USEC;
			if (timeout < RAWSOCK_MIN_TIMEOUT_USEC)
				timeout = RAWSOCK_MIN_TIMEOUT_USEC;

			pBuf = (U8 *)openavbRawsockGetRxFrame(pStream->rawsock, timeout, &offsetToFrame, &frameLen);
			if (!pBuf)
				pStream->pIntfCB->intf_rx_cb(pStream->pMediaQ);
		}
	}

	hdrLen = openavbRawsockRxParseHdr(pStream->rawsock, pBuf, &hdrInfo);
	if (hdrLen < 0) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVBAVTP_RC_PARSING_FRAME_HEADER));
	}
	else {
		pAvtpPdu = pBuf + offsetToFrame + hdrLen;
		avtpPduLen = frameLen - hdrLen;
		x_avtpRxFrame(pStream, pAvtpPdu, avtpPduLen);
	}
	openavbRawsockRelRxFrame(pStream->rawsock, pBuf);

	AVB_TRACE_EXIT(AVB_TRACE_AVTP_DETAIL);
}

int openavbAvtpTxBufferLevel(void *pv)
{
	avtp_stream_t *pStream = (avtp_stream_t *)pv;
	if (!pStream) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		return 0;
	}
	return openavbRawsockTxBufLevel(pStream->rawsock);
}

int openavbAvtpRxBufferLevel(void *pv)
{
	avtp_stream_t *pStream = (avtp_stream_t *)pv;
	if (!pStream) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		return 0;
	}
	return openavbRawsockRxBufLevel(pStream->rawsock);
}

int openavbAvtpLost(void *pv)
{
	avtp_stream_t *pStream = (avtp_stream_t *)pv;
	if (!pStream) {
		// Quietly return. Since this can be called before a stream is available.
		return 0;
	}
	int count = pStream->nLost;
	pStream->nLost = 0;
	return count;
}

U64 openavbAvtpBytes(void *pv)
{
	avtp_stream_t *pStream = (avtp_stream_t *)pv;
	if (!pStream) {
		// Quietly return. Since this can be called before a stream is available.
		return 0;
	}

	U64 bytes = pStream->bytes;
	pStream->bytes = 0;
	return bytes;
}

openavbRC openavbAvtpRx(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP_DETAIL);

	avtp_stream_t *pStream = (avtp_stream_t *)pv;
	if (!pStream) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AVTP_DETAIL);
	}

	// Check our socket, and potentially receive some data.
	avtpTryRx(pStream);

	// See if there's a complete (re-assembled) data sample.
	if (pStream->info.rx.bComplete) {
		pStream->info.rx.bComplete = FALSE;
		AVB_RC_TRACE_RET(OPENAVB_AVTP_SUCCESS, AVB_TRACE_AVTP_DETAIL);
	}

	AVB_RC_TRACE_RET(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVBAVTP_RC_NO_FRAMES_PROCESSED), AVB_TRACE_AVTP_DETAIL);
}

void openavbAvtpConfigTimsstampEval(void *handle, U32 tsInterval, U32 reportInterval, bool smoothing, U32 tsMaxJitter, U32 tsMaxDrift)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP);

	avtp_stream_t *pStream = (avtp_stream_t *)handle;
	if (!pStream) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AVTP);
		return;
	}

	pStream->tsEval = openavbTimestampEvalNew();
	openavbTimestampEvalInitialize(pStream->tsEval, tsInterval);
	openavbTimestampEvalSetReport(pStream->tsEval, reportInterval);
	if (smoothing) {
		openavbTimestampEvalSetSmoothing(pStream->tsEval, tsMaxJitter, tsMaxDrift);
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVTP);
}

void openavbAvtpPause(void *handle, bool bPause)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP);

	avtp_stream_t *pStream = (avtp_stream_t *)handle;
	if (!pStream) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AVTP);
		return;
	}

	pStream->bPause = bPause;

	// AVDECC_TODO:  Do something with the bPause value!

	AVB_TRACE_EXIT(AVB_TRACE_AVTP);
}

void openavbAvtpShutdownTalker(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP);
	AVB_LOG_DEBUG("Shutdown");

	avtp_stream_t *pStream = (avtp_stream_t *)pv;
	if (pStream) {
		pStream->pIntfCB->intf_end_cb(pStream->pMediaQ);
		pStream->pMapCB->map_end_cb(pStream->pMediaQ);

		// close the rawsock
		if (pStream->rawsock) {
			openavbRawsockClose(pStream->rawsock);
			pStream->rawsock = NULL;
		}

		if (pStream->ifname)
			free(pStream->ifname);

		// free the malloc'd stream info
		free(pStream);
	}
	AVB_TRACE_EXIT(AVB_TRACE_AVTP);
	return;
}

void openavbAvtpShutdownListener(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP);
	AVB_LOG_DEBUG("Shutdown");

	avtp_stream_t *pStream = (avtp_stream_t *)pv;
	if (pStream) {
		// close the rawsock
		if (pStream->rawsock) {
			openavbRawsockClose(pStream->rawsock);
			pStream->rawsock = NULL;
		}

		pStream->pIntfCB->intf_end_cb(pStream->pMediaQ);
		pStream->pMapCB->map_end_cb(pStream->pMediaQ);

		if (pStream->ifname)
			free(pStream->ifname);

		// free the malloc'd stream info
		free(pStream);
	}
	AVB_TRACE_EXIT(AVB_TRACE_AVTP);
	return;
}
