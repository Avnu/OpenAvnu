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
 * MODULE : ADP - AVDECC Discovery Protocol Message Handler
 * MODULE SUMMARY : Implements the 1722.1 (AVDECC) discovery protocol message handlers
 ******************************************************************
 */

#include "openavb_platform.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define	AVB_LOG_COMPONENT	"ADP"
#include "openavb_log.h"

#include "openavb_debug.h"
#include "openavb_rawsock.h"
#include "openavb_avtp.h"
#include "openavb_srp.h"
#include "openavb_adp.h"
#include "openavb_adp_sm_advertise_interface.h"
#include "openavb_acmp_sm_listener.h"

#ifdef AVB_PTP_AVAILABLE
#include "openavb_ptp_api.h"

// PTP declarations
openavbRC openavbPtpInitializeSharedMemory  ();
void     openavbPtpReleaseSharedMemory     ();
openavbRC openavbPtpUpdateSharedMemoryEntry ();
openavbRC openavbPtpFindLatestSharedMemoryEntry(U32 *index);
extern gmChangeTable_t openavbPtpGMChageTable;
#else
#include "openavb_grandmaster_osal_pub.h"
#endif // AVB_PTP_AVAILABLE


#define INVALID_SOCKET (-1)

// ADP Multicast address
#define ADP_PROTOCOL_ADDR "91:E0:F0:01:00:00"

// message length
#define AVTP_HDR_LEN 12
#define ADP_DATA_LEN 56
#define ADP_FRAME_LEN (ETH_HDR_LEN_VLAN + AVTP_HDR_LEN + ADP_DATA_LEN)

// number of buffers (arbitrary, and rounded up by rawsock)
#define ADP_NUM_BUFFERS 2

// do cast from ether_addr to U8*
#define ADDR_PTR(A) (U8*)(&((A)->ether_addr_octet))

extern openavb_avdecc_cfg_t gAvdeccCfg;

extern MUTEX_HANDLE(openavbAdpMutex);
#define ADP_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAdpMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define ADP_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAdpMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

static void *rxSock = NULL;
static void *txSock = NULL;
static struct ether_addr intfAddr;
static struct ether_addr adpAddr;

extern openavb_adp_sm_global_vars_t openavbAdpSMGlobalVars;

THREAD_TYPE(openavbAdpMessageRxThread);
THREAD_DEFINITON(openavbAdpMessageRxThread);

static bool bRunning = FALSE;

void openavbAdpCloseSocket()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	if (rxSock) {
		openavbRawsockClose(rxSock);
		rxSock = NULL;
	}
	if (txSock) {
		openavbRawsockClose(txSock);
		txSock = NULL;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

bool openavbAdpOpenSocket(const char* ifname, U16 vlanID, U8 vlanPCP)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	hdr_info_t hdr;

#ifndef UBUNTU
	// This is the normal case for most of our supported platforms
	rxSock = openavbRawsockOpen(ifname, TRUE, FALSE, ETHERTYPE_8021Q, ADP_FRAME_LEN, ADP_NUM_BUFFERS);
#else
	rxSock = openavbRawsockOpen(ifname, TRUE, FALSE, ETHERTYPE_AVTP, ADP_FRAME_LEN, ADP_NUM_BUFFERS);
#endif
	txSock = openavbRawsockOpen(ifname, FALSE, TRUE, ETHERTYPE_AVTP, ADP_FRAME_LEN, ADP_NUM_BUFFERS);

	if (txSock && rxSock
		&& openavbRawsockGetAddr(txSock, ADDR_PTR(&intfAddr))
		&& ether_aton_r(ADP_PROTOCOL_ADDR, &adpAddr)
		&& openavbRawsockRxMulticast(rxSock, TRUE, ADDR_PTR(&adpAddr)))
	{
		if (!openavbRawsockRxAVTPSubtype(rxSock, OPENAVB_ADP_AVTP_SUBTYPE | 0x80)) {
			AVB_LOG_DEBUG("RX AVTP Subtype not supported");
		}

		memset(&hdr, 0, sizeof(hdr_info_t));
		hdr.shost = ADDR_PTR(&intfAddr);
		hdr.dhost = ADDR_PTR(&adpAddr);
		hdr.ethertype = ETHERTYPE_AVTP;
		if (vlanID != 0 || vlanPCP != 0) {
			hdr.vlan = TRUE;
			hdr.vlan_pcp = vlanPCP;
			hdr.vlan_vid = vlanID;
			AVB_LOGF_DEBUG("VLAN pcp=%d vid=%d", hdr.vlan_pcp, hdr.vlan_vid);
		}
		if (!openavbRawsockTxSetHdr(txSock, &hdr)) {
			AVB_LOG_ERROR("TX socket Header Failure");
			openavbAdpCloseSocket();
			AVB_TRACE_EXIT(AVB_TRACE_ADP);
			return false;
		}

		AVB_TRACE_EXIT(AVB_TRACE_ADP);
		return true;
	}

	AVB_LOG_ERROR("Invalid socket");
	openavbAdpCloseSocket();

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
	return false;
}

static void openavbAdpMessageRxFrameParse(U8* payload, int payload_len, hdr_info_t *hdr)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	openavb_adp_control_header_t adpHeader;
	openavb_adp_data_unit_t adpPdu;

#if 0
	AVB_LOGF_DEBUG("openavbAdpMessageRxFrameParse packet data (length %d):", payload_len);
	AVB_LOG_BUFFER(AVB_LOG_LEVEL_DEBUG, payload, payload_len, 16);
#endif

	U8 *pSrc = payload;
	{
		// AVTP Control Header
		openavb_adp_control_header_t *pDst = &adpHeader;

		BIT_B2DNTOHB(pDst->cd, pSrc, 0x80, 7, 0);
		BIT_B2DNTOHB(pDst->subtype, pSrc, 0x7f, 0, 1);
		BIT_B2DNTOHB(pDst->sv, pSrc, 0x80, 7, 0);
		BIT_B2DNTOHB(pDst->version, pSrc, 0x70, 4, 0);
		BIT_B2DNTOHB(pDst->message_type, pSrc, 0x0f, 0, 1);
		BIT_B2DNTOHB(pDst->valid_time, pSrc, 0xf800, 11, 0);
		BIT_B2DNTOHS(pDst->control_data_length, pSrc, 0x07ff, 0, 2);
		OCT_B2DMEMCP(pDst->entity_id, pSrc);
	}

	if (adpHeader.subtype == OPENAVB_ADP_AVTP_SUBTYPE &&
			(adpHeader.message_type == OPENAVB_ADP_MESSAGE_TYPE_ENTITY_DISCOVER ||
			 (gAvdeccCfg.bFastConnectSupported && adpHeader.message_type == OPENAVB_ADP_MESSAGE_TYPE_ENTITY_AVAILABLE))) {
		// ADP PDU
		openavb_adp_data_unit_t *pDst = &adpPdu;

		OCT_B2DMEMCP(pDst->entity_model_id, pSrc);
		OCT_B2DNTOHL(pDst->entity_capabilities, pSrc);
		OCT_B2DNTOHS(pDst->talker_stream_sources, pSrc);
		OCT_B2DNTOHS(pDst->talker_capabilities, pSrc);
		OCT_B2DNTOHS(pDst->listener_stream_sinks, pSrc);
		OCT_B2DNTOHS(pDst->listener_capabilities, pSrc);
		OCT_B2DNTOHL(pDst->controller_capabilities, pSrc);
		OCT_B2DNTOHL(pDst->available_index, pSrc);
		OCT_B2DMEMCP(pDst->gptp_grandmaster_id, pSrc);
		OCT_B2DNTOHB(pDst->gptp_domain_number, pSrc);
		OCT_B2DMEMCP(pDst->reserved0, pSrc);
		OCT_B2DNTOHS(pDst->identify_control_index, pSrc);
		OCT_B2DNTOHS(pDst->interface_index, pSrc);
		OCT_B2DMEMCP(pDst->association_id, pSrc);
		OCT_B2DMEMCP(pDst->reserved1, pSrc);

		if (adpHeader.message_type == OPENAVB_ADP_MESSAGE_TYPE_ENTITY_DISCOVER) {
			// Update the interface state machine
			openavbAdpSMAdvertiseInterfaceSet_entityID(adpHeader.entity_id);
			openavbAdpSMAdvertiseInterfaceSet_rcvdDiscover(TRUE);
		}
		else {
			// See if Fast Connect is waiting for this device to be available.
			if (adpPdu.talker_stream_sources > 0) {
				openavbAcmpSMListenerSet_talkerTestFastConnect(adpHeader.entity_id);
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

static void openavbAdpMessageRxFrameReceive(U32 timeoutUsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

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
				// parse the PDU only for ADP messages
				if (*(pFrame + offset) == (0x80 | OPENAVB_ADP_AVTP_SUBTYPE)) {
					if (memcmp(hdrInfo.shost, ADDR_PTR(&intfAddr), 6) != 0) { // Not from us!
						openavbAdpMessageRxFrameParse(pFrame + offset, len - offset, &hdrInfo);
					}
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

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}


void openavbAdpMessageTxFrame(U8 msgType, U8 *destAddr)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	U8 *pBuf;
	U32 size;
	unsigned int hdrlen = 0;

	pBuf = openavbRawsockGetTxFrame(txSock, TRUE, &size);

	if (!pBuf) {
		AVB_LOG_ERROR("No TX buffer");
		AVB_TRACE_EXIT(AVB_TRACE_ADP);
		return;
	}

	if (size < ADP_FRAME_LEN) {
		AVB_LOG_ERROR("TX buffer too small");
		openavbRawsockRelTxFrame(txSock, pBuf);
		pBuf = NULL;
		AVB_TRACE_EXIT(AVB_TRACE_ADP);
		return;
	}

	memset(pBuf, 0, ADP_FRAME_LEN);
	openavbRawsockTxFillHdr(txSock, pBuf, &hdrlen);

	if (destAddr)
		memcpy(pBuf, destAddr, ETH_ALEN);

	ADP_LOCK();
	U8 *pDst = pBuf + hdrlen;
	{
		// AVTP Control Header
		openavb_adp_control_header_t *pSrc = &openavbAdpSMGlobalVars.entityInfo.header;
		BIT_D2BHTONB(pDst, pSrc->cd, 7, 0);
		BIT_D2BHTONB(pDst, pSrc->subtype, 0, 1);
		BIT_D2BHTONB(pDst, pSrc->sv, 7, 0);
		BIT_D2BHTONB(pDst, pSrc->version, 4, 0);
		BIT_D2BHTONB(pDst, msgType, 0, 1);
		BIT_D2BHTONS(pDst, pSrc->valid_time, 11, 0);
		BIT_D2BHTONS(pDst, pSrc->control_data_length, 0, 2);
		OCT_D2BMEMCP(pDst, pSrc->entity_id);
	}

	{
		// ADP PDU
		openavb_adp_data_unit_t *pSrc = &openavbAdpSMGlobalVars.entityInfo.pdu;
		OCT_D2BMEMCP(pDst, pSrc->entity_model_id);
		OCT_D2BHTONL(pDst, pSrc->entity_capabilities);
		OCT_D2BHTONS(pDst, pSrc->talker_stream_sources);
		OCT_D2BHTONS(pDst, pSrc->talker_capabilities);
		OCT_D2BHTONS(pDst, pSrc->listener_stream_sinks);
		OCT_D2BHTONS(pDst, pSrc->listener_capabilities);
		OCT_D2BHTONL(pDst, pSrc->controller_capabilities);
		OCT_D2BHTONL(pDst, pSrc->available_index);
		OCT_D2BMEMCP(pDst, pSrc->gptp_grandmaster_id);
		OCT_D2BHTONB(pDst, pSrc->gptp_domain_number);
		OCT_D2BMEMCP(pDst, pSrc->reserved0);
		OCT_D2BHTONS(pDst, pSrc->identify_control_index);
		OCT_D2BHTONS(pDst, pSrc->interface_index);
		OCT_D2BMEMCP(pDst, pSrc->association_id);
		OCT_D2BMEMCP(pDst, pSrc->reserved1);
	}
	ADP_UNLOCK();

#if 0
	AVB_LOGF_DEBUG("openavbAdpMessageTxFrame packet data (length %d):", hdrlen + AVTP_HDR_LEN + ADP_DATA_LEN);
	AVB_LOG_BUFFER(AVB_LOG_LEVEL_DEBUG, pBuf, hdrlen + AVTP_HDR_LEN + ADP_DATA_LEN, 16);
#endif

	openavbRawsockTxFrameReady(txSock, pBuf, hdrlen + AVTP_HDR_LEN + ADP_DATA_LEN, 0);
	openavbRawsockSend(txSock);

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

void* openavbAdpMessageRxThreadFn(void *pv)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	AVB_LOG_DEBUG("ADP Thread Started");
	while (bRunning) {
		// Try to get and process an ADP discovery message.
		openavbAdpMessageRxFrameReceive(MICROSECONDS_PER_SECOND);
	}
	AVB_LOG_DEBUG("ADP Thread Done");

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
	return NULL;
}

openavbRC openavbAdpMessageHandlerStart()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	bRunning = TRUE;

	if (openavbAdpOpenSocket((const char *)gAvdeccCfg.ifname, gAvdeccCfg.vlanID, gAvdeccCfg.vlanPCP)) {

		// Start the RX thread
		bool errResult;
		THREAD_CREATE(openavbAdpMessageRxThread, openavbAdpMessageRxThread, NULL, openavbAdpMessageRxThreadFn, NULL);
		THREAD_CHECK_ERROR(openavbAdpMessageRxThread, "Thread / task creation failed", errResult);
		if (errResult) {
			bRunning = FALSE;
			openavbAdpCloseSocket();
			AVB_RC_TRACE_RET(OPENAVB_AVDECC_FAILURE, AVB_TRACE_ADP);
		}

		AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_ADP);
	}

	bRunning = FALSE;
	AVB_RC_TRACE_RET(OPENAVB_AVDECC_FAILURE, AVB_TRACE_ADP);
}

void openavbAdpMessageHandlerStop()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	if (bRunning) {
		bRunning = FALSE;
		THREAD_JOIN(openavbAdpMessageRxThread, NULL);
		openavbAdpCloseSocket();
	}

	AVB_TRACE_EXIT(AVB_TRACE_ADP);
}

openavbRC openavbAdpMessageSend(U8 messageType)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ADP);

	// Note: this entire process of the GM ID is not as the 1722.1 spec expects.
	//  The openavbAdpSMAdvertiseInterfaceSet_advertisedGrandmasterID() should be called when the
	//  stack detects a GM change that will trigger an advertise. Instead we are detecting
	//  the GM change when we have are sending an advertise message. This means we will not have a timely
	//  new advertise in the event of a GM change.  Additionally the handling the advertiseInterface var
	//  of GM ID is not normal since the GM ID is being placed directly into the PDU rather than getting
	//  pulled from the state machine var.
	//  AVDECC_TODO: This logic should change to detect GM change else where in the system and call the
	//  expected openavbAdpSMAdvertiseInterfaceSet_advertisedGrandmasterID() to start the advertise process.
#ifdef AVB_PTP_AVAILABLE
	openavb_adp_data_unit_t *pPdu = &openavbAdpSMGlobalVars.entityInfo.pdu;
	openavbRC  retCode = OPENAVB_PTP_FAILURE;
	U32 ptpSharedMemoryEntryIndex;
	retCode = openavbPtpFindLatestSharedMemoryEntry(&ptpSharedMemoryEntryIndex);
	if (IS_OPENAVB_FAILURE(retCode)) {
		AVB_LOG_INFO("Failed to find PTP shared memory entry.");
	}
	else {
		if (memcmp(pPdu->gptp_grandmaster_id, openavbPtpGMChageTable.entry[ptpSharedMemoryEntryIndex].gmId, sizeof(pPdu->gptp_grandmaster_id))) {
			memcpy(pPdu->gptp_grandmaster_id, openavbPtpGMChageTable.entry[ptpSharedMemoryEntryIndex].gmId, sizeof(pPdu->gptp_grandmaster_id));
			openavbAdpSMAdvertiseInterfaceSet_advertisedGrandmasterID(pPdu->gptp_grandmaster_id);
		}
	}
#else
	openavb_adp_data_unit_t *pPdu = &openavbAdpSMGlobalVars.entityInfo.pdu;
	uint8_t current_grandmaster_id[8];
	if (!osalAVBGrandmasterGetCurrent(current_grandmaster_id, &(pPdu->gptp_domain_number)))
	{
		AVB_LOG_ERROR("osalAVBGrandmasterGetCurrent failure");
	}
	else if (memcmp(pPdu->gptp_grandmaster_id, current_grandmaster_id, sizeof(pPdu->gptp_grandmaster_id)))
	{
		memcpy(pPdu->gptp_grandmaster_id, current_grandmaster_id, sizeof(pPdu->gptp_grandmaster_id));
		openavbAdpSMAdvertiseInterfaceSet_advertisedGrandmasterID(pPdu->gptp_grandmaster_id);
	}
#endif

	openavbAdpMessageTxFrame(messageType, NULL);
	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_ADP);
}

