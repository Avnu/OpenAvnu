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
 * Rawsock implemenation which uses libpcap for receiving and transmitting packets.
*/
#include "pcap_rawsock.h"
#include "simple_rawsock.h"
#include "openavb_trace.h"

#define	AVB_LOG_COMPONENT	"Raw Socket"
#include "openavb_log.h"

static pcap_t* open_pcap_dev(const char* ifname, int frameSize, char* errbuf)
{
	pcap_t* handle = pcap_create(ifname, errbuf);
	if (handle) {
		int err;
		err = pcap_set_snaplen(handle, frameSize);
		if (err) AVB_LOGF_WARNING("Cannot set snap len %d", err);

		err = pcap_set_promisc(handle, 1);
		if (err) AVB_LOGF_WARNING("Cannot set promisc %d", err);

		err = pcap_set_immediate_mode(handle, 1);
		if (err) AVB_LOGF_WARNING("Cannot set immediate mode %d", err);

		// we need timeout (here 100ms) otherwise we could block for ever
		err = pcap_set_timeout(handle, 100);
		if (err) AVB_LOGF_WARNING("Cannot set timeout %d", err);

		err = pcap_set_tstamp_precision(handle, PCAP_TSTAMP_PRECISION_NANO);
		if (err) AVB_LOGF_WARNING("Cannot set tstamp nano precision %d", err);

		err = pcap_set_tstamp_type(handle, PCAP_TSTAMP_ADAPTER_UNSYNCED);
		if (err) AVB_LOGF_WARNING("Cannot set tstamp adapter unsynced %d", err);

		err = pcap_activate(handle);
		if (err) AVB_LOGF_WARNING("Cannot activate pcap %d", err);
	}
	return handle;
}

// Open a rawsock for TX or RX
void *pcapRawsockOpen(pcap_rawsock_t* rawsock, const char *ifname, bool rx_mode, bool tx_mode, U16 ethertype, U32 frame_size, U32 num_frames)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	AVB_LOGF_DEBUG("Open, rx=%d, tx=%d, ethertype=%x size=%d, num=%d",	rx_mode, tx_mode, ethertype, frame_size, num_frames);

	baseRawsockOpen(&rawsock->base, ifname, rx_mode, tx_mode, ethertype, frame_size, num_frames);

	if (tx_mode) {
		AVB_LOG_DEBUG("pcap rawsock transmit mode will bypass FQTSS");
	}

	rawsock->handle = 0;

	// Get info about the network device
	if (!simpleAvbCheckInterface(ifname, &(rawsock->base.ifInfo))) {
		AVB_LOGF_ERROR("Creating rawsock; bad interface name: %s", ifname);
		free(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	// Deal with frame size.
	if (rawsock->base.frameSize == 0) {
		// use interface MTU as max frames size, if none specified
		rawsock->base.frameSize = rawsock->base.ifInfo.mtu + ETH_HLEN + VLAN_HLEN;
	}
	else if (rawsock->base.frameSize > rawsock->base.ifInfo.mtu + ETH_HLEN + VLAN_HLEN) {
		AVB_LOG_ERROR("Creating rawsock; requested frame size exceeds MTU");
		free(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	char errbuf[PCAP_ERRBUF_SIZE];
	rawsock->handle = open_pcap_dev(ifname, rawsock->base.frameSize, errbuf);
	if (!rawsock->handle) {
		AVB_LOGF_ERROR("Cannot open device %s: %s", ifname, errbuf);
		free(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	// fill virtual functions table
	rawsock_cb_t *cb = &rawsock->base.cb;
	cb->close = pcapRawsockClose;
	cb->getTxFrame = pcapRawsockGetTxFrame;
	cb->txFrameReady = pcapRawsockTxFrameReady;
	cb->send = pcapRawsockSend;
	cb->getRxFrame = pcapRawsockGetRxFrame;
	cb->rxMulticast = pcapRawsockRxMulticast;
	cb->rxParseHdr = pcapRawsockRxParseHdr;

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return rawsock;
}

void pcapRawsockClose(void *pvRawsock)
{
	pcap_rawsock_t *rawsock = (pcap_rawsock_t*)pvRawsock;

	if (rawsock) {
		pcap_close(rawsock->handle);
	}

	baseRawsockClose(rawsock);
}

U8 *pcapRawsockGetTxFrame(void *pvRawsock, bool blocking, unsigned int *len)
{
	pcap_rawsock_t *rawsock = (pcap_rawsock_t*)pvRawsock;

	if (rawsock) {
		*len = rawsock->base.frameSize;
		return rawsock->txBuffer;
	}

	return false;
}

bool pcapRawsockTxFrameReady(void *pvRawsock, U8 *pBuffer, unsigned int len, U64 timeNsec)
{
	pcap_rawsock_t *rawsock = (pcap_rawsock_t*)pvRawsock;
	int ret = -1;

	if (timeNsec) {
		IF_LOG_INTERVAL(1000) AVB_LOG_WARNING("launch time is unsupported in pcap_rawsock");
	}

	if (rawsock) {
		ret = pcap_sendpacket(rawsock->handle, pBuffer, len);
		if (ret == -1) {
			AVB_LOGF_ERROR("pcap_sendpacket failed: %s", pcap_geterr(rawsock->handle));
		}

	}
	return ret == 0;
}

// Send all packets that are ready (i.e. tell kernel to send them)
int pcapRawsockSend(void *pvRawsock)
{
	// pcapRawsock sends frames in pcapRawsockTxFrameReady

	return 1;
}

U8 *pcapRawsockGetRxFrame(void *pvRawsock, U32 timeout, unsigned int *offset, unsigned int *len)
{
	pcap_rawsock_t *rawsock = (pcap_rawsock_t*)pvRawsock;

	rawsock->rxHeader = 0;
	const u_char *packet = 0;
	int ret;

	if (rawsock) {
		ret = pcap_next_ex(rawsock->handle, &rawsock->rxHeader, &packet);
		switch(ret) {
		case 1:
			*offset = 0;
			*len = rawsock->rxHeader->caplen;
			return (U8*)packet;
		case -1:
			AVB_LOGF_ERROR("pcap_next_ex failed: %s", pcap_geterr(rawsock->handle));
			break;
		case 0:
			// timeout;
			break;
		case -2:
			// no packets to be read from savefile
			// this should not happened
			break;
		default:
			break;
		}
	}

	return NULL;
}

int pcapRawsockRxParseHdr(void* pvRawsock, U8* pBuffer, hdr_info_t* pInfo)
{
	int hdrLen = baseRawsockRxParseHdr(pvRawsock, pBuffer, pInfo);

	pcap_rawsock_t *rawsock = (pcap_rawsock_t*)pvRawsock;
	if (rawsock && rawsock->rxHeader) {
		pInfo->ts.tv_sec = rawsock->rxHeader->ts.tv_sec;
		// we requested nanosecond timestamp precision, so probably we don't have to scale here
		pInfo->ts.tv_nsec = rawsock->rxHeader->ts.tv_usec;
	}
	return hdrLen;
}

// Setup the rawsock to receive multicast packets
bool pcapRawsockRxMulticast(void *pvRawsock, bool add_membership, const U8 addr[ETH_ALEN])
{
	pcap_rawsock_t *rawsock = (pcap_rawsock_t*)pvRawsock;

	struct bpf_program comp_filter_exp;
	char filter_exp[30];

	sprintf(filter_exp, "ether dst %02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

	AVB_LOGF_DEBUG("%s %d %s", __func__, (int)add_membership, filter_exp);

	if (pcap_compile(rawsock->handle, &comp_filter_exp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) < 0) {
		AVB_LOGF_ERROR("Could not parse filter %s: %s", filter_exp, pcap_geterr(rawsock->handle));
		return false;
	}

	if (pcap_setfilter(rawsock->handle, &comp_filter_exp) < 0) {
		AVB_LOGF_ERROR("Could not install filter %s: %s", filter_exp, pcap_geterr(rawsock->handle));
		return false;
	}

	return true;
}
