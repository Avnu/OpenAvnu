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

#include "rawsock_impl.h"
#include "openavb_trace.h"

#define	AVB_LOG_COMPONENT	"Raw Socket"
#include "openavb_log.h"

void baseRawsockSetRxSignalMode(void *rawsock, bool rxSignalMode) {}
int baseRawsockGetSocket(void *rawsock) { return -1; }
U8 *baseRawsockGetRxFrame(void *rawsock, U32 usecTimeout, U32 *offset, U32 *len) { return NULL; }
bool baseRawsockRelRxFrame(void *rawsock, U8 *pFrame) { return false; }
bool baseRawsockRxMulticast(void *rawsock, bool add_membership, const U8 buf[]) { return false; }
bool baseRawsockRxAVTPSubtype(void *rawsock, U8 subtype) { return false; }
bool baseRawsockTxSetMark(void *rawsock, int prio) { return false; }
U8 *baseRawsockGetTxFrame(void *rawsock, bool blocking, U32 *size) { return NULL; }
bool baseRawsockRelTxFrame(void *rawsock, U8 *pBuffer) { return false; }
bool baseRawsockTxFrameReady(void *rawsock, U8 *pFrame, U32 len) { return false; }
int baseRawsockSend(void *rawsock) { return -1; }
int baseRawsockTxBufLevel(void *rawsock) { return -1; }
int baseRawsockRxBufLevel(void *rawsock) { return -1; }
unsigned long baseRawsockGetTXOutOfBuffers(void *pvRawsock) { return 0; }
unsigned long baseRawsockGetTXOutOfBuffersCyclic(void *pvRawsock) { return 0; }

void* baseRawsockOpen(base_rawsock_t* rawsock, const char *ifname, bool rx_mode, bool tx_mode, U16 ethertype, U32 frame_size, U32 num_frames)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	rawsock->rxMode = rx_mode;
	rawsock->txMode = tx_mode;
	rawsock->frameSize = frame_size;
	rawsock->ethertype = ethertype;

	// fill virtual functions table
	rawsock_cb_t *cb = &rawsock->cb;
	cb->setRxSignalMode = baseRawsockSetRxSignalMode;
	cb->close = baseRawsockClose;
	cb->getSocket = baseRawsockGetSocket;
	cb->getAddr = baseRawsockGetAddr;
	cb->getRxFrame = baseRawsockGetRxFrame;
	cb->rxParseHdr = baseRawsockRxParseHdr;
	cb->relRxFrame = baseRawsockRelRxFrame;
	cb->rxMulticast = baseRawsockRxMulticast;
	cb->rxAVTPSubtype = baseRawsockRxAVTPSubtype;
	cb->txSetHdr = baseRawsockTxSetHdr;
	cb->txFillHdr = baseRawsockTxFillHdr;
	cb->txSetMark = baseRawsockTxSetMark;
	cb->getTxFrame = baseRawsockGetTxFrame;
	cb->relTxFrame = baseRawsockRelTxFrame;
	cb->txFrameReady = baseRawsockTxFrameReady;
	cb->send = baseRawsockSend;
	cb->txBufLevel = baseRawsockTxBufLevel;
	cb->rxBufLevel = baseRawsockRxBufLevel;
	cb->getTXOutOfBuffers = baseRawsockGetTXOutOfBuffers;
	cb->getTXOutOfBuffersCyclic = baseRawsockGetTXOutOfBuffersCyclic;


	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return rawsock;
}

void baseRawsockClose(void *rawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	// free the state struct
	free(rawsock);
	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
}

// Pre-set the ethernet header information that will be used on TX frames
bool baseRawsockTxSetHdr(void *pvRawsock, hdr_info_t *pHdr)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Setting TX header; invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	// source address
	if (pHdr->shost) {
		memcpy(&(rawsock->ethHdr.notag.shost), pHdr->shost, ETH_ALEN);
	}
	// destination address
	if (pHdr->dhost) {
		memcpy(&(rawsock->ethHdr.notag.dhost), pHdr->dhost, ETH_ALEN);
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
		u_int16_t bits = 0;
		bits |= (pHdr->vlan_pcp << 13) & 0xE000;
		bits |= pHdr->vlan_vid & 0x0FFF;

		// Create VLAN tag
		rawsock->ethHdr.tagged.vlan.tpip = htons(ETHERTYPE_VLAN);
		rawsock->ethHdr.tagged.vlan.bits = htons(bits);
		rawsock->ethHdr.tagged.ethertype = htons(rawsock->ethertype);
		// and set ethernet header length
		rawsock->ethHdrLen = sizeof(eth_vlan_hdr_t);
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return TRUE;
}

// Copy the pre-set header to the outgoing frame
bool baseRawsockTxFillHdr(void *pvRawsock, U8 *pBuffer, unsigned int *hdrlen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (!rawsock) {
		AVB_LOG_ERROR("Filling TX header; invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	// Copy the default Ethernet header into the buffer
	if (hdrlen)
		*hdrlen = rawsock->ethHdrLen;
	memcpy((char*)pBuffer, &(rawsock->ethHdr), rawsock->ethHdrLen);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return TRUE;
}

// Get the ethernet address of the interface
bool baseRawsockGetAddr(void *pvRawsock, U8 addr[ETH_ALEN])
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (!rawsock) {
		AVB_LOG_ERROR("Getting address; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return FALSE;
	}

	memcpy(addr, &rawsock->ifInfo.mac.ether_addr_octet, ETH_ALEN);
	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return TRUE;
}

// Parse the ethernet frame header.  Returns length of header, or -1 for failure
int baseRawsockRxParseHdr(void *pvRawsock, U8 *pBuffer, hdr_info_t *pInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	eth_hdr_t *eth_hdr = (eth_hdr_t*)pBuffer;
	pInfo->dhost = eth_hdr->dhost;
	pInfo->shost = eth_hdr->shost;
	pInfo->ethertype = ntohs(eth_hdr->ethertype);
	int hdrLen = sizeof(eth_hdr_t);

	if (pInfo->ethertype == ETHERTYPE_8021Q) {
		pInfo->vlan = TRUE;
		// TODO extract vlan_vid and vlan_pcp
		hdrLen += 4;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return hdrLen;
}


/////////////////////////////////////////////////////////////////////////////

void openavbSetRxSignalMode(void *pvRawsock, bool rxSignalMode)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	((base_rawsock_t*)pvRawsock)->cb.setRxSignalMode(pvRawsock, rxSignalMode);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
}

void openavbRawsockClose(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	((base_rawsock_t*)pvRawsock)->cb.close(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
}

U8 *openavbRawsockGetTxFrame(void *pvRawsock, bool blocking, unsigned int *len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	U8 *ret = ((base_rawsock_t*)pvRawsock)->cb.getTxFrame(pvRawsock, blocking, len);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

bool openavbRawsockTxSetMark(void *pvRawsock, int mark)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	bool ret = ((base_rawsock_t*)pvRawsock)->cb.txSetMark(pvRawsock, mark);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return ret;
}

bool openavbRawsockTxSetHdr(void *pvRawsock, hdr_info_t *pHdr)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	bool ret = ((base_rawsock_t*)pvRawsock)->cb.txSetHdr(pvRawsock, pHdr);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return ret;
}

bool openavbRawsockTxFillHdr(void *pvRawsock, U8 *pBuffer, unsigned int *hdrlen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	bool ret = ((base_rawsock_t*)pvRawsock)->cb.txFillHdr(pvRawsock, pBuffer, hdrlen);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

bool openavbRawsockRelTxFrame(void *pvRawsock, U8 *pBuffer)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	bool ret = ((base_rawsock_t*)pvRawsock)->cb.relTxFrame(pvRawsock, pBuffer);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

bool openavbRawsockTxFrameReady(void *pvRawsock, U8 *pBuffer, unsigned int len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	bool ret = ((base_rawsock_t*)pvRawsock)->cb.txFrameReady(pvRawsock, pBuffer, len);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

int openavbRawsockSend(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	int ret = ((base_rawsock_t*)pvRawsock)->cb.send(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

int openavbRawsockTxBufLevel(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	int ret = ((base_rawsock_t*)pvRawsock)->cb.txBufLevel(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

int openavbRawsockRxBufLevel(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	int ret = ((base_rawsock_t*)pvRawsock)->cb.rxBufLevel(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

U8 *openavbRawsockGetRxFrame(void *pvRawsock, U32 timeout, unsigned int *offset, unsigned int *len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	U8 *ret = ((base_rawsock_t*)pvRawsock)->cb.getRxFrame(pvRawsock, timeout, offset, len);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

int openavbRawsockRxParseHdr(void *pvRawsock, U8 *pBuffer, hdr_info_t *pInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	int ret = ((base_rawsock_t*)pvRawsock)->cb.rxParseHdr(pvRawsock, pBuffer, pInfo);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

bool openavbRawsockRelRxFrame(void *pvRawsock, U8 *pBuffer)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	bool ret = ((base_rawsock_t*)pvRawsock)->cb.relRxFrame(pvRawsock, pBuffer);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

bool openavbRawsockRxMulticast(void *pvRawsock, bool add_membership, const U8 addr[ETH_ALEN])
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	bool ret = ((base_rawsock_t*)pvRawsock)->cb.rxMulticast(pvRawsock, add_membership, addr);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return ret;
}

bool openavbRawsockRxAVTPSubtype(void *pvRawsock, U8 subtype)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	bool ret = ((base_rawsock_t*)pvRawsock)->cb.rxAVTPSubtype(pvRawsock, subtype);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return ret;
}

int openavbRawsockGetSocket(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	int ret = ((base_rawsock_t*)pvRawsock)->cb.getSocket(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return ret;
}

bool openavbRawsockGetAddr(void *pvRawsock, U8 addr[ETH_ALEN])
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	bool ret = ((base_rawsock_t*)pvRawsock)->cb.getAddr(pvRawsock, addr);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return ret;
}

unsigned long openavbRawsockGetTXOutOfBuffers(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	unsigned long ret = ((base_rawsock_t*)pvRawsock)->cb.getTXOutOfBuffers(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

unsigned long openavbRawsockGetTXOutOfBuffersCyclic(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	unsigned long ret = ((base_rawsock_t*)pvRawsock)->cb.getTXOutOfBuffersCyclic(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}
