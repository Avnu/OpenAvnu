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

/////////////////////////////////////////////////////////////////////////////

void openavbSetRxSignalMode(void *pvRawsock, bool rxSignalMode)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.setRxSignalMode)
		rawsock->cb.setRxSignalMode(pvRawsock, rxSignalMode);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
}

void openavbRawsockClose(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.close)
		rawsock->cb.close(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
}

U8 *openavbRawsockGetTxFrame(void *pvRawsock, bool blocking, unsigned int *len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	U8 *ret = NULL;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.getTxFrame)
		ret = rawsock->cb.getTxFrame(pvRawsock, blocking, len);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

bool openavbRawsockTxSetMark(void *pvRawsock, int mark)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	bool ret = false;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.txSetMark)
		ret = rawsock->cb.txSetMark(pvRawsock, mark);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return ret;
}

bool openavbRawsockTxSetHdr(void *pvRawsock, hdr_info_t *pHdr)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	bool ret = false;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.txSetHdr)
		ret = rawsock->cb.txSetHdr(pvRawsock, pHdr);
	else
		ret = baseRawsockTxSetHdr(pvRawsock, pHdr);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return ret;
}

bool openavbRawsockTxFillHdr(void *pvRawsock, U8 *pBuffer, unsigned int *hdrlen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	bool ret = false;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.txFillHdr)
		ret = rawsock->cb.txFillHdr(pvRawsock, pBuffer, hdrlen);
	else
		ret = baseRawsockTxFillHdr(pvRawsock, pBuffer, hdrlen);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

bool openavbRawsockRelTxFrame(void *pvRawsock, U8 *pBuffer)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	bool ret = false;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.relTxFrame)
		ret = rawsock->cb.relTxFrame(pvRawsock, pBuffer);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

bool openavbRawsockTxFrameReady(void *pvRawsock, U8 *pBuffer, unsigned int len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	bool ret = false;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.txFrameReady)
		ret = rawsock->cb.txFrameReady(pvRawsock, pBuffer, len);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

int openavbRawsockSend(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	int ret = -1;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.send)
		ret = rawsock->cb.send(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

int openavbRawsockTxBufLevel(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	int ret = -1;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.txBufLevel)
		ret = rawsock->cb.txBufLevel(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

int openavbRawsockRxBufLevel(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	int ret = -1;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.rxBufLevel)
		ret = rawsock->cb.rxBufLevel(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

U8 *openavbRawsockGetRxFrame(void *pvRawsock, U32 timeout, unsigned int *offset, unsigned int *len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	U8 *ret = NULL;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.getRxFrame)
		ret = rawsock->cb.getRxFrame(pvRawsock, timeout, offset, len);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

// Parse the ethernet frame header.  Returns length of header, or -1 for failure
int openavbRawsockRxParseHdr(void *pvRawsock, U8 *pBuffer, hdr_info_t *pInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	int ret = -1;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.rxParseHdr)
		ret = rawsock->cb.rxParseHdr(pvRawsock, pBuffer, pInfo);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

// Release a RX frame held by the client
bool openavbRawsockRelRxFrame(void *pvRawsock, U8 *pBuffer)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	bool ret = false;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.relRxFrame)
		ret = rawsock->cb.relRxFrame(pvRawsock, pBuffer);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

bool openavbRawsockRxMulticast(void *pvRawsock, bool add_membership, const U8 addr[ETH_ALEN])
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	bool ret = false;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.rxMulticast)
		ret = rawsock->cb.rxMulticast(pvRawsock, add_membership, addr);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return ret;
}

int openavbRawsockGetSocket(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	int ret = -1;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.getSocket)
		ret = rawsock->cb.getSocket(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return ret;
}

bool openavbRawsockGetAddr(void *pvRawsock, U8 addr[ETH_ALEN])
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	bool ret = false;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.getAddr)
		ret = rawsock->cb.getAddr(pvRawsock, addr);
	else
		ret = baseRawsockGetAddr(pvRawsock, addr);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return ret;
}

unsigned long openavbRawsockGetTXOutOfBuffers(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	unsigned long ret = 0;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.getTXOutOfBuffers)
		ret = rawsock->cb.getTXOutOfBuffers(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

unsigned long openavbRawsockGetTXOutOfBuffersCyclic(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	unsigned long ret = 0;

	base_rawsock_t *rawsock = (base_rawsock_t*)pvRawsock;
	if (VALID_RAWSOCK(rawsock) && rawsock->cb.getTXOutOfBuffersCyclic)
		ret = rawsock->cb.getTXOutOfBuffersCyclic(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}
