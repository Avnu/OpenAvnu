/*************************************************************************************************************
Copyright (c) 2012-2013, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
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

#include "stc_avb_platform.h"
#include "stc_sockets.h"
#include <stdlib.h>
#include <string.h>
#include "stc_rawsock.h"
#include "stc_avb_trace.h"

#define	AVB_LOG_COMPONENT	"Raw Socket"
#include "stc_avb_log.h"
#include "stc_rawsock_tcal.h"

// State information for raw socket
//
typedef struct {

	// interface info
	if_info_t ifInfo;

	// saved Ethernet header for TX frames
	union {
		eth_hdr_t      notag;
		eth_vlan_hdr_t tagged;
	}
	ethHdr;
	unsigned ethHdrLen;

	// Ethertype for TX/RX frames
	unsigned ethertype;

	// the underlying socket
	int sock;

	// number and size of the frames requested
	int frameSize;

	eth_frame_t *pTxFrame;
	eth_frame_t *pRxFrame;

	// Task 
	thread_type *pTask;					// Used by Task Manager suspend/resume

	// TX usage of the socket
	bool txMode;

	// RX usage of the socket
	bool rxMode;
} raw_sock_t;

// Kernel block size
#define BLOCK_SIZE		4096

// Argument validation
#define VALID_RAWSOCK(s) ((s) != NULL && (s)->sock != -1)
#define VALID_TX_RAWSOCK(s) (VALID_RAWSOCK(s) && (s)->txMode)
#define VALID_RX_RAWSOCK(s) (VALID_RAWSOCK(s) && (s)->rxMode)

// Get information about an interface
bool stcAvbCheckInterface(const char *ifname, if_info_t *info){
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	bool retVal = FALSE;

	if (!ifname || !info) {
		AVB_LOG_ERROR("Checking interface; invalid arguments");
	}
	else {
		// zap the result struct
		memset(info, 0, sizeof(if_info_t));

		AVB_LOGF_DEBUG("ifname=%s", ifname);
		strncpy(info->name, ifname, IFNAMSIZ - 1);

		// TODO_OPENAVB: Link is checked at Ethernet init time.
		// is interface up?
		//if(!halIsLinkUp()) {
		//	AVB_LOGF_ERROR("Checking interface; interface is not up: %s", ifname);
		//}
		//else {
			info->index = 0; // index not used

			osalGetMacAddr(info->mac.ether_addr_octet);
			info->mtu = halGetMTU();

			retVal = TRUE;
		//}
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return retVal;
}


// Open a rawsock for TX or RX
void *stcRawsockOpen(const char *ifname, bool rx_mode, bool tx_mode, U16 ethertype, U32 frame_size, U32 num_frames) {
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	raw_sock_t *rawsock = NULL;

	AVB_LOGF_DEBUG("Open, rx=%d, tx=%d, ethertype=%x size=%d, num=%d",	rx_mode, tx_mode, ethertype, frame_size, num_frames);

	do {
		/* Allocate struct to hold info about the raw socket
		 * that we're going to create.
		 */
		rawsock = calloc(1, sizeof(raw_sock_t));
		if (!rawsock) {
			AVB_LOG_ERROR("Creating rawsock; malloc failed");
			break;
		}

		rawsock->sock = -1;
		rawsock->rxMode = rx_mode;
		rawsock->txMode = tx_mode;
		rawsock->frameSize = frame_size;
		rawsock->ethertype = ethertype;

		// Get info about the network device
		if (!stcAvbCheckInterface(ifname, &(rawsock->ifInfo))) {
			AVB_LOGF_ERROR("Creating rawsock; bad interface name: %s", ifname);
			free(rawsock);
			rawsock = NULL;
			break;
		}

		// Deal with frame size.
		if (rawsock->frameSize == 0) {
			// use interface MTU as max frames size, if none specified
			rawsock->frameSize = rawsock->ifInfo.mtu;
		}
		else if (rawsock->frameSize > rawsock->ifInfo.mtu) {
			AVB_LOG_ERROR("Creating raswsock; requested frame size exceeds MTU");
			free(rawsock);
			rawsock = NULL;
			break;
		}
		else {
			rawsock->frameSize = TPACKET_ALIGN(rawsock->frameSize);
		}

		// Prepare default Ethernet header.
		rawsock->ethHdrLen = sizeof(eth_hdr_t);
		memset(&(rawsock->ethHdr.notag.dhost), 0xFF, ETH_ALEN);
		stc_safe_memcpy(&(rawsock->ethHdr.notag.shost), &(rawsock->ifInfo.mac), ETH_ALEN);
		rawsock->ethHdr.notag.ethertype = htons(rawsock->ethertype);

		// Create socket
		rawsock->sock = osalSocket(PF_PACKET, SOCK_RAW, htons(rawsock->ethertype));
		if (rawsock->sock == -1) {
			AVB_LOG_ERROR("Creating rawsock; opening socket:");
			free(rawsock);
			rawsock = NULL;
			break;
		}

		// request Tx frames
		if (tx_mode) {
			if (osalSetTxMode(rawsock->sock, num_frames, frame_size) == -1) {
				AVB_LOG_ERROR("Creating rawsock: Allocating tx buffers");
				stcRawsockClose(rawsock);
				rawsock = NULL;
				break;
			}
		}

		// request Rx frames
		if (rx_mode) {
			if (osalSetRxMode(rawsock->sock, num_frames, frame_size) == -1) {
				AVB_LOG_ERROR("Creating rawsock: Allocating rx buffers");
				stcRawsockClose(rawsock);
				rawsock = NULL;
				break;
			}
		}

		rawsock->pTask = stcTaskMgrGetCurrentTask();

		if (osalBind(rawsock->sock) == -1) {
			AVB_LOG_ERROR("Creating rawsock; bind socket");
			stcRawsockClose(rawsock);
			rawsock = NULL;
			break;
		}

	} while (0);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return rawsock;
}

// Set signal on RX mode
void stcSetRxSignalMode(void *pvRawsock, bool rxSignalMode)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;

	if (rawsock) {
		if (rawsock->sock != -1) {
			osalSetRxSignalMode(rawsock->sock, rxSignalMode);
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
}


// Close the rawsock
void stcRawsockClose(void *pvRawsock){
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;

	if (rawsock) {
		// close the socket
		if (rawsock->sock != -1) {
			osalCloseSocket(rawsock->sock);
			rawsock->sock = -1;
		}

		// free the state struct
		free(rawsock);
	}
	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
}

// Get a buffer from the ring to use for TX
U8 *stcRawsockGetTxFrame(void *pvRawsock, bool blocking, U32 *len){
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	U8 *pBuffer = NULL;

	if (!VALID_TX_RAWSOCK(rawsock) || len == NULL) {
		AVB_LOG_ERROR("Getting TX frame; bad arguments");
	}
	else {
		do {
			rawsock->pTxFrame = osalSocketGetTxBuf(rawsock->sock);
			if (rawsock->pTxFrame) {
				pBuffer = rawsock->pTxFrame->data;
				break;				
			}
			else {
				IF_LOG_INTERVAL(1000) {
					AVB_LOG_ERROR("Getting TX frame: too many TX buffers in use");
			  	}
				break;
			}
			//if (blocking) {
			//	SLEEP_MSEC(1);
			//}
		} while (1);

		if (pBuffer) {
			// Remind client how big the frame buffer is
			if (len) {
				*len = rawsock->pTxFrame->length;
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return pBuffer;
}

// Set the Firewall MARK on the socket
// The mark is used by FQTSS to identify AVTP packets.
// FQTSS creates a mark that includes the AVB class and streamBytesPerSec
bool stcRawsockTxSetMark(void *pvRawsock, int mark){
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	bool retval = FALSE;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Setting TX mark; invalid argument passed");
	}
	else if (osalSetsockopt(rawsock->sock, SOL_SOCKET, SO_MARK, &mark, sizeof(mark)) < 0) {
		AVB_LOG_ERROR("Setting TX mark; osalSetsockopt failed");
	}
	else {
		AVB_LOGF_DEBUG("SO_MARK=%d OK", mark);
		retval = TRUE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return retval;
}

// Pre-set the Ethernet header information that will be used on TX frames
bool stcRawsockTxSetHdr(void *pvRawsock, hdr_info_t *pHdr){
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	bool retVal = FALSE;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Setting TX header; invalid argument");
	}
	else {
		// source address
		if (pHdr->shost) {
			stc_safe_memcpy(&(rawsock->ethHdr.notag.shost), pHdr->shost, ETH_ALEN);
		}
		// destination address
		if (pHdr->dhost) {
			stc_safe_memcpy(&(rawsock->ethHdr.notag.dhost), pHdr->dhost, ETH_ALEN);
		}

		// VLAN tag?
		if (!pHdr->vlan) {
			// No, set ethertype in normal location
			rawsock->ethHdr.notag.ethertype = htons(rawsock->ethertype);
			// and set ethernet header length
			rawsock->ethHdrLen = sizeof(eth_hdr_t);
		}
		else {
			// Add VLAN tag
			AVB_LOGF_DEBUG("VLAN=%d pcp=%d vid=%d", pHdr->vlan_vid, pHdr->vlan_pcp, pHdr->vlan_vid);

			// Build bitfield with vlan_pcp and vlan_vid.
			// I think CFI bit is alway 0
			uint16_t bits = 0;
			bits |= (pHdr->vlan_pcp << 13) & 0xE000;
			bits |= pHdr->vlan_vid & 0x0FFF;

			// Create VLAN tag
			rawsock->ethHdr.tagged.vlan.tpip = htons(ETHERTYPE_VLAN);
			rawsock->ethHdr.tagged.vlan.bits = htons(bits);
			rawsock->ethHdr.tagged.ethertype = htons(rawsock->ethertype);
			// and set Ethernet header length
			rawsock->ethHdrLen = sizeof(eth_vlan_hdr_t);
		}
		retVal = TRUE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return retVal;
}

// Copy the pre-set header to the outgoing frame
bool stcRawsockTxFillHdr(void *pvRawsock, U8 *pBuffer, U32 *hdrlen) {
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	bool retVal = FALSE;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Filling TX header; invalid argument");
	}
	else {
		// Copy the default Ethernet header into the buffer
		if (hdrlen)
			*hdrlen = rawsock->ethHdrLen;
		stc_safe_memcpy((char*)pBuffer, &(rawsock->ethHdr), rawsock->ethHdrLen);

		retVal = TRUE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return retVal;
}

// Release a TX frame, without marking it as ready to send
bool stcRawsockRelTxFrame(void *pvRawsock, U8 *pBuffer){
	bool retVal = TRUE;
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	
	if (!VALID_TX_RAWSOCK(rawsock) || pBuffer == NULL) {
		AVB_LOG_ERROR("Releasing TX frame; invalid argument");
		retVal = FALSE;
	}
	else if (osalSocketRelTxBuf(rawsock->sock)) {
		AVB_LOG_ERROR("Releasing TX frame");
		retVal = FALSE;
	}

	rawsock->pTxFrame = NULL;
	
	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return retVal;
}

// Ready Tx frame. pBuffer parameter not used in this implementation.
bool stcRawsockTxFrameReady(void *pvRawsock, U8 *pBuffer, U32 len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	bool retVal = FALSE;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Marking TX frame ready; invalid argument");
	}
	else {
		rawsock->pTxFrame->length = len;

		retVal = TRUE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return retVal;
}

// Send all packets that are ready (i.e. tell kernel to send them)
int stcRawsockSend(void *pvRawsock) {
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	int sent = -1;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Send; invalid argument");
	}
	else {
		osalSocketPushTxBuf(rawsock->sock);
		rawsock->pTxFrame = NULL;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return sent;
}

int stcRawsockTxBufLevel(void *pvRawsock){
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	int retVal = 0;

	if (!VALID_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("getting buffer level; invalid arguments");
	}
	else {
		retVal = osalGetTxSockLevel(rawsock->sock);
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return retVal;
}

int stcRawsockRxBufLevel(void *pvRawsock){
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	int retVal = 0;

	if (!VALID_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("getting buffer level; invalid arguments");
	}
	else {
		retVal = osalGetRxSockLevel(rawsock->sock);
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return retVal;
}

// Get a RX frame
U8 *stcRawsockGetRxFrame(void *pvRawsock, U32 timeoutUSec, U32 *offset, U32 *len) {
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	U8 *pBuffer = NULL;

	if (!VALID_RX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Getting RX frame; invalid arguments");
	}
	else {
		while (!rawsock->pRxFrame) {
			rawsock->pRxFrame = osalSocketGetRxBuf(rawsock->sock);
			if (!rawsock->pRxFrame) {
				if (timeoutUSec == STC_RAWSOCK_NONBLOCK) {
					AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
					return NULL;
				}
				if (timeoutUSec == (U32)STC_RAWSOCK_BLOCK) {
					TASK_MGR_SUSPEND(rawsock->pTask);
					rawsock->pRxFrame = osalSocketGetRxBuf(rawsock->sock);
				}
				else {
					TASK_MGR_NANOSLEEP(rawsock->pTask, timeoutUSec * 1000);
					rawsock->pRxFrame = osalSocketGetRxBuf(rawsock->sock);
					break;		// Assumed timed out or packet ready. Caller will sort it out.
				}
			}
		}
		
		if (rawsock->pRxFrame) {
			*offset = 0;
			*len = rawsock->pRxFrame->length;
			pBuffer = rawsock->pRxFrame->data;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return pBuffer;
}

// Parse the Ethernet frame header.  Returns length of header, or -1 for failure
int stcRawsockRxParseHdr(void *pvRawsock, U8 *pBuffer, hdr_info_t *pInfo){
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	// Current usage of this function of only to determine the header length. 
	// and the only usage of this port is always has a VLAN tag therefore 
	// the implementation is hidden
	// ATNEL_TODO PERF this block isn't really needed consider using the simple case.
#if 0	
	int pduOffset = -1;
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	if (!VALID_RX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Parsing Ethernet headers; invalid arguments");
	}
	else {

		memset(pInfo, 0, sizeof(hdr_info_t));

		pduOffset = sizeof(eth_hdr_t);

		eth_hdr_t *pNoTag = (eth_hdr_t*)pBuffer;
		pInfo->shost = pNoTag->shost;
		pInfo->dhost = pNoTag->dhost;
		pInfo->ethertype = ntohs(pNoTag->ethertype);

		if (pInfo->ethertype == ETHERTYPE_VLAN) {
			uint16_t tp_vlan_tci = ntohs(*(uint16_t *)(pBuffer + pduOffset));
			pInfo->vlan = TRUE;
			pInfo->vlan_vid = tp_vlan_tci & 0x0FFF;
			pInfo->vlan_pcp = (tp_vlan_tci >> 13) & 0x0007;
			pInfo->ethertype = ntohs(*(uint16_t *)(pBuffer + pduOffset + 2));
			pduOffset += 4; // sizeof 802.1Q header
		}
	}
#else
	int pduOffset = sizeof(eth_hdr_t) + 4;
#endif	
	
	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return pduOffset;
}

// Release a RX frame held by the client
bool stcRawsockRelRxFrame(void *pvRawsock, U8 *pBuffer){
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	bool retVal = FALSE;

	if (!VALID_RX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Releasing RX frame; invalid arguments");
	}
	else {
		retVal = osalSocketPullRxBuf(rawsock->sock);
		rawsock->pRxFrame = NULL;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return retVal;
}

// Setup the rawsock to receive multicast packets
bool stcRawsockRxMulticast(void *pvRawsock, bool add_membership, const U8 addr[ETH_ALEN]){
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	bool retVal = FALSE;

	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	if (!VALID_RX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Setting multicast; invalid arguments");
	}
	else {
		if (osalSetsockopt(rawsock->sock, SOL_SOCKET, (add_membership ? PACKET_ADD_MEMBERSHIP : PACKET_DROP_MEMBERSHIP), addr, ETH_ALEN) < 0) {
			AVB_LOG_ERROR("Setting multicast at HAL");
		}
		else {
			retVal = TRUE;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return retVal;
}

bool stcRawsockRxAVTPSubtype(void *pvRawsock, U8 subtype){
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	bool retVal = FALSE;

	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	if (!VALID_RX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Setting AVTP subtype: invalid arguments");
	}
	else {
		if (osalSetAvtpSubtype(rawsock->sock, subtype) < 0) {
			AVB_LOG_ERROR("Setting AVTP subtype filter");
		}
		else {
			retVal = TRUE;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return retVal;
}

// Get the socket used for this rawsock; can be used for poll/select
int stcRawsockGetSocket(void *pvRawsock){
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	int retVal = -1;

	if (!rawsock) {
		AVB_LOG_ERROR("Getting socket; invalid arguments");
	}
	else {
		retVal = rawsock->sock;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return retVal;
}

// Get the Ethernet address of the interface
bool  stcRawsockGetAddr(void *pvRawsock, U8 addr[ETH_ALEN]){
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	bool retVal = FALSE;
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	if (!rawsock) {
		AVB_LOG_ERROR("Getting address; invalid arguments");
	}
	else {
		stc_safe_memcpy(addr, &rawsock->ifInfo.mac.ether_addr_octet, ETH_ALEN);
		retVal = TRUE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return retVal;
}
