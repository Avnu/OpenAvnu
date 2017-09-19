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
 ******************************************************************
 * MODULE : ACMP - AVDECC Connection Management Protocol Message Handler
 * MODULE SUMMARY : Implements the 1722.1 ACMP - AVDECC Connection Management Protocol message handlers
 ******************************************************************
 */

#include "openavb_platform.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define	AVB_LOG_COMPONENT	"ACMP"
#include "openavb_log.h"

#include "openavb_debug.h"
#include "openavb_rawsock.h"
#include "openavb_avtp.h"
#include "openavb_srp.h"
#include "openavb_acmp.h"
#include "openavb_acmp_sm_controller.h"
#include "openavb_acmp_sm_listener.h"
#include "openavb_acmp_sm_talker.h"

#define INVALID_SOCKET (-1)

// ACMP Multicast address
#define ACMP_PROTOCOL_ADDR "91:E0:F0:01:00:00"

// message length
#define AVTP_HDR_LEN 12
#define ACMP_DATA_LEN 44
#define ACMP_FRAME_LEN (ETH_HDR_LEN_VLAN + AVTP_HDR_LEN + ACMP_DATA_LEN)

// number of buffers
#define ACMP_NUM_TX_BUFFERS 2
#define ACMP_NUM_RX_BUFFERS 20

// do cast from ether_addr to U8*
#define ADDR_PTR(A) (U8*)(&((A)->ether_addr_octet))

extern openavb_avdecc_cfg_t gAvdeccCfg;

extern MUTEX_HANDLE(openavbAcmpMutex);
#define ACMP_LOCK() MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAcmpMutex); MUTEX_LOG_ERR("Mutex lock failure");
#define ACMP_UNLOCK() MUTEX_UNLOCK(openavbAcmpMutex);

static void *rxSock = NULL;
static void *txSock = NULL;
static struct ether_addr intfAddr;
static struct ether_addr acmpAddr;

extern openavb_acmp_sm_global_vars_t openavbAcmpSMGlobalVars;

THREAD_TYPE(openavbAcmpMessageRxThread);
THREAD_DEFINITON(openavbAcmpMessageRxThread);

static bool bRunning = FALSE;

void openavbAcmpCloseSocket()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	if (rxSock) {
		openavbRawsockClose(rxSock);
		rxSock = NULL;
	}
	if (txSock) {
		openavbRawsockClose(txSock);
		txSock = NULL;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

bool openavbAcmpOpenSocket(const char* ifname, U16 vlanID, U8 vlanPCP)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	hdr_info_t hdr;

	rxSock = openavbRawsockOpen(ifname, TRUE, FALSE, ETHERTYPE_AVTP, ACMP_FRAME_LEN, ACMP_NUM_RX_BUFFERS);
	txSock = openavbRawsockOpen(ifname, FALSE, TRUE, ETHERTYPE_AVTP, ACMP_FRAME_LEN, ACMP_NUM_TX_BUFFERS);

	if (txSock && rxSock
		&& openavbRawsockGetAddr(txSock, ADDR_PTR(&intfAddr))
		&& ether_aton_r(ACMP_PROTOCOL_ADDR, &acmpAddr)
		&& openavbRawsockRxMulticast(rxSock, TRUE, ADDR_PTR(&acmpAddr)))
	{
		if (!openavbRawsockRxAVTPSubtype(rxSock, OPENAVB_ACMP_AVTP_SUBTYPE | 0x80)) {
			AVB_LOG_DEBUG("RX AVTP Subtype not supported");
		}

		memset(&hdr, 0, sizeof(hdr_info_t));
		hdr.shost = ADDR_PTR(&intfAddr);
		hdr.dhost = ADDR_PTR(&acmpAddr);
		hdr.ethertype = ETHERTYPE_AVTP;
		if (vlanID != 0 || vlanPCP != 0) {
			hdr.vlan = TRUE;
			hdr.vlan_pcp = vlanPCP;
			hdr.vlan_vid = vlanID;
			AVB_LOGF_DEBUG("VLAN pcp=%d vid=%d", hdr.vlan_pcp, hdr.vlan_vid);
		}
		if (!openavbRawsockTxSetHdr(txSock, &hdr)) {
			AVB_LOG_ERROR("TX socket Header Failure");
			openavbAcmpCloseSocket();
			AVB_TRACE_EXIT(AVB_TRACE_ACMP);
			return false;
		}

		AVB_TRACE_EXIT(AVB_TRACE_ACMP);
		return true;
	}

	AVB_LOG_ERROR("Invalid socket");
	openavbAcmpCloseSocket();

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return false;
}

static void openavbAcmpMessageRxFrameParse(U8* payload, int payload_len, hdr_info_t *hdr)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	openavb_acmp_control_header_t acmpHeader;
	openavb_acmp_ACMPCommandResponse_t acmpCommandResponse;

#if 0
	AVB_LOGF_DEBUG("openavbAcmpMessageRxFrameParse packet data (length %d):", payload_len);
	AVB_LOG_BUFFER(AVB_LOG_LEVEL_DEBUG, payload, payload_len, 16);
#endif

	U8 *pSrc = payload;
	{
		openavb_acmp_control_header_t *pDst1 = &acmpHeader;
		openavb_acmp_ACMPCommandResponse_t *pDst2 = &acmpCommandResponse;

		memset(&acmpHeader, 0, sizeof(acmpHeader));
		memset(&acmpCommandResponse, 0, sizeof(acmpCommandResponse));

		// AVTP Control Header
		BIT_B2DNTOHB(pDst1->cd, pSrc, 0x80, 7, 0);
		BIT_B2DNTOHB(pDst1->subtype, pSrc, 0x7f, 0, 1);
		BIT_B2DNTOHB(pDst1->sv, pSrc, 0x80, 7, 0);
		BIT_B2DNTOHB(pDst1->version, pSrc, 0x70, 4, 0);
		BIT_B2DNTOHB(pDst2->message_type, pSrc, 0x0f, 0, 1);
		BIT_B2DNTOHB(pDst2->status, pSrc, 0xf800, 11, 0);
		BIT_B2DNTOHS(pDst1->control_data_length, pSrc, 0x07ff, 0, 2);
		OCT_B2DMEMCP(pDst2->stream_id, pSrc);

		// ACMP PDU
		OCT_B2DMEMCP(pDst2->controller_entity_id, pSrc);
		OCT_B2DMEMCP(pDst2->talker_entity_id, pSrc);
		OCT_B2DMEMCP(pDst2->listener_entity_id, pSrc);
		OCT_B2DNTOHS(pDst2->talker_unique_id, pSrc);
		OCT_B2DNTOHS(pDst2->listener_unique_id, pSrc);
		OCT_B2DMEMCP(pDst2->stream_dest_mac, pSrc);
		OCT_B2DNTOHS(pDst2->connection_count, pSrc);
		OCT_B2DNTOHS(pDst2->sequence_id, pSrc);
		OCT_B2DNTOHS(pDst2->flags, pSrc);
		OCT_B2DNTOHS(pDst2->stream_vlan_id, pSrc);
	}

	// Update the state machine
	switch (acmpCommandResponse.message_type) {
		case OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_TX_COMMAND:
			openavbAcmpSMTalkerSet_rcvdConnectTXCmd(&acmpCommandResponse);
			break;
		case OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_TX_RESPONSE:
			openavbAcmpSMListenerSet_rcvdConnectTXResp(&acmpCommandResponse);
			break;
		case OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_TX_COMMAND:
			openavbAcmpSMTalkerSet_rcvdDisconnectTXCmd(&acmpCommandResponse);
			break;
		case OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_TX_RESPONSE:
			openavbAcmpSMListenerSet_rcvdDisconnectTXResp(&acmpCommandResponse);
			break;
		case OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_STATE_COMMAND:
			openavbAcmpSMTalkerSet_rcvdGetTXState(&acmpCommandResponse);
			break;
		case OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_STATE_RESPONSE:
			openavbAcmpSMControllerSet_rcvdResponse(&acmpCommandResponse);
			break;
		case OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_RX_COMMAND:
			openavbAcmpSMListenerSet_rcvdConnectRXCmd(&acmpCommandResponse);
			break;
		case OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_RX_RESPONSE:
			openavbAcmpSMControllerSet_rcvdResponse(&acmpCommandResponse);
			break;
		case OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_RX_COMMAND:
			openavbAcmpSMListenerSet_rcvdDisconnectRXCmd(&acmpCommandResponse);
			break;
		case OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_RX_RESPONSE:
			openavbAcmpSMControllerSet_rcvdResponse(&acmpCommandResponse);
			break;
		case OPENAVB_ACMP_MESSAGE_TYPE_GET_RX_STATE_COMMAND:
			openavbAcmpSMListenerSet_rcvdGetRXState(&acmpCommandResponse);
			break;
		case OPENAVB_ACMP_MESSAGE_TYPE_GET_RX_STATE_RESPONSE:
			openavbAcmpSMControllerSet_rcvdResponse(&acmpCommandResponse);
			break;
		case OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_CONNECTION_COMMAND:
			openavbAcmpSMTalkerSet_rcvdGetTXConnectionCmd(&acmpCommandResponse);
			break;
		case OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_CONNECTION_RESPONSE:
			openavbAcmpSMControllerSet_rcvdResponse(&acmpCommandResponse);
			break;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

static void openavbAcmpMessageRxFrameReceive(U32 timeoutUsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	hdr_info_t hdrInfo;
	unsigned int offset, len;
	U8 *pBuf, *pFrame;

	memset(&hdrInfo, 0, sizeof(hdr_info_t));

	pBuf = (U8 *)openavbRawsockGetRxFrame(rxSock, timeoutUsec, &offset, &len);
	if (pBuf) {
		pFrame = pBuf + offset;

		offset = openavbRawsockRxParseHdr(rxSock, pBuf, &hdrInfo);
		{
#ifndef UBUNTU
			if (hdrInfo.ethertype == ETHERTYPE_8021Q) {
				// Oh!  Need to look past the VLAN tag
				U16 vlan_bits = ntohs(*(U16 *)(pFrame + offset));
				hdrInfo.vlan = TRUE;
				hdrInfo.vlan_vid = vlan_bits & 0x0FFF;
				hdrInfo.vlan_pcp = (vlan_bits >> 13) & 0x0007;
				offset += 2;
				hdrInfo.ethertype = ntohs(*(U16 *)(pFrame + offset));
				offset += 2;
			}
#endif

			// Make sure that this is an AVTP packet
			// (Should always be AVTP if it's to our AVTP-specific multicast address)
			if (hdrInfo.ethertype == ETHERTYPE_AVTP) {
				// parse the PDU only for ACMP messages
				if (*(pFrame + offset) == (0x80 | OPENAVB_ACMP_AVTP_SUBTYPE)) {
					openavbAcmpMessageRxFrameParse(pFrame + offset, len - offset, &hdrInfo);
				}
			}
			else {
				AVB_LOG_WARNING("Received non-AVTP frame!");
				AVB_LOGF_DEBUG("Unexpected packet data (length %d):", len);
				AVB_LOG_BUFFER(AVB_LOG_LEVEL_DEBUG, pFrame, len, 16);
			}
		}

		// Release the frame
		openavbRawsockRelRxFrame(rxSock, pBuf);
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void openavbAcmpMessageTxFrame(U8 messageType, openavb_acmp_ACMPCommandResponse_t *pACMPCommandResponse, U8 status)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	U8 *pBuf;
	U32 size;
	unsigned int hdrlen = 0;

	pBuf = openavbRawsockGetTxFrame(txSock, TRUE, &size);

	if (!pBuf) {
		AVB_LOG_ERROR("No TX buffer");
		AVB_TRACE_EXIT(AVB_TRACE_ACMP);
		return;		// AVDECC_TODO - return error result
	}

	if (size < ACMP_FRAME_LEN) {
		AVB_LOG_ERROR("TX buffer too small");
		openavbRawsockRelTxFrame(txSock, pBuf);
		pBuf = NULL;
		AVB_TRACE_EXIT(AVB_TRACE_ACMP);
		return;		// AVDECC_TODO - return error result
	}

	memset(pBuf, 0, ACMP_FRAME_LEN);
	openavbRawsockTxFillHdr(txSock, pBuf, &hdrlen);

	ACMP_LOCK();
	U8 *pDst = pBuf + hdrlen;
	{
		openavb_acmp_control_header_t *pSrc1 = &openavbAcmpSMGlobalVars.controlHeader;
		openavb_acmp_ACMPCommandResponse_t *pSrc2 = pACMPCommandResponse;

		// AVTP Control Header
		BIT_D2BHTONB(pDst, pSrc1->cd, 7, 0);
		BIT_D2BHTONB(pDst, pSrc1->subtype, 0, 1);
		BIT_D2BHTONB(pDst, pSrc1->sv, 7, 0);
		BIT_D2BHTONB(pDst, pSrc1->version, 4, 0);
		BIT_D2BHTONB(pDst, messageType, 0, 1);
		BIT_D2BHTONS(pDst, status, 11, 0);
		BIT_D2BHTONS(pDst, pSrc1->control_data_length, 0, 2);
		OCT_D2BMEMCP(pDst, pSrc2->stream_id);

		// ACMP PDU
		OCT_D2BMEMCP(pDst, pSrc2->controller_entity_id);
		OCT_D2BMEMCP(pDst, pSrc2->talker_entity_id);
		OCT_D2BMEMCP(pDst, pSrc2->listener_entity_id);
		OCT_D2BHTONS(pDst, pSrc2->talker_unique_id);
		OCT_D2BHTONS(pDst, pSrc2->listener_unique_id);
		OCT_D2BMEMCP(pDst, pSrc2->stream_dest_mac);
		OCT_D2BHTONS(pDst, pSrc2->connection_count);
		OCT_D2BHTONS(pDst, pSrc2->sequence_id);
		OCT_D2BHTONS(pDst, pSrc2->flags);
		OCT_D2BHTONS(pDst, pSrc2->stream_vlan_id);
	}
	ACMP_UNLOCK();

#if 0
	AVB_LOGF_DEBUG("openavbAcmpMessageTxFrame packet data (length %d):", hdrlen + AVTP_HDR_LEN + ACMP_DATA_LEN);
	AVB_LOG_BUFFER(AVB_LOG_LEVEL_DEBUG, pBuf, hdrlen + AVTP_HDR_LEN + ACMP_DATA_LEN, 16);
#endif

	openavbRawsockTxFrameReady(txSock, pBuf, hdrlen + AVTP_HDR_LEN + ACMP_DATA_LEN, 0);
	openavbRawsockSend(txSock);

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

void* openavbAcmpMessageRxThreadFn(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	AVB_LOG_DEBUG("ACMP Thread Started");
	while (bRunning) {
		// Try to get and process an ACMP.
		openavbAcmpMessageRxFrameReceive(MICROSECONDS_PER_SECOND);
	}
	AVB_LOG_DEBUG("ACMP Thread Done");

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
	return NULL;
}

openavbRC openavbAcmpMessageHandlerStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	bRunning = TRUE;

	if (openavbAcmpOpenSocket((const char *)gAvdeccCfg.ifname, gAvdeccCfg.vlanID, gAvdeccCfg.vlanPCP)) {

		// Start the RX thread
		bool errResult;
		THREAD_CREATE(openavbAcmpMessageRxThread, openavbAcmpMessageRxThread, NULL, openavbAcmpMessageRxThreadFn, NULL);
		THREAD_CHECK_ERROR(openavbAcmpMessageRxThread, "Thread / task creation failed", errResult);
		if (errResult) {
			bRunning = FALSE;
			openavbAcmpCloseSocket();
			AVB_RC_TRACE_RET(OPENAVB_AVDECC_FAILURE, AVB_TRACE_ACMP);
		}

		AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_ACMP);
	}

	bRunning = FALSE;
	AVB_RC_TRACE_RET(OPENAVB_AVDECC_FAILURE, AVB_TRACE_ACMP);
}

void openavbAcmpMessageHandlerStop()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);

	if (bRunning) {
		bRunning = FALSE;
		THREAD_JOIN(openavbAcmpMessageRxThread, NULL);
		openavbAcmpCloseSocket();
	}

	AVB_TRACE_EXIT(AVB_TRACE_ACMP);
}

openavbRC openavbAcmpMessageSend(U8 messageType, openavb_acmp_ACMPCommandResponse_t *pACMPCommandResponse, U8 status)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ACMP);
	openavbAcmpMessageTxFrame(messageType, pACMPCommandResponse, status);
	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_ACMP);
}

