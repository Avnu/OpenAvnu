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

#include "sendmmsg_rawsock.h"

#include "simple_rawsock.h"
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <linux/filter.h>

#define AVB_LOG_LEVEL AVB_LOG_LEVEL_INFO

#include "openavb_trace.h"

#define	AVB_LOG_COMPONENT	"Raw Socket"
#include "openavb_log.h"


#if USE_LAUNCHTIME

#ifndef SCM_TIMEDLAUNCH
#define SCM_TIMEDLAUNCH 0x04
#endif

#endif /* if USE_LAUNCHTIME */


static void fillmsghdr(struct msghdr *msg, struct iovec *iov,
#if USE_LAUNCHTIME
					   unsigned char *cmsgbuf, uint64_t time,
#endif
					   void *pktdata, size_t pktlen)
{
	msg->msg_name = NULL;
	msg->msg_namelen = 0;

	iov->iov_base = pktdata;
	iov->iov_len = pktlen;
	msg->msg_iov = iov;
	msg->msg_iovlen = 1;

#if USE_LAUNCHTIME
	{
		struct cmsghdr *cmsg;
		uint64_t *tsptr;

		msg->msg_control = cmsgbuf;
		msg->msg_controllen = CMSG_LEN(sizeof time);

		cmsg = CMSG_FIRSTHDR(msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_TIMEDLAUNCH;
		cmsg->cmsg_len = CMSG_LEN(sizeof time);

		tsptr = (uint64_t *)CMSG_DATA(cmsg);
		*tsptr = time;
	}
#else
	msg->msg_control = NULL;
	msg->msg_controllen = 0;
#endif

	msg->msg_flags = 0;
}

// Open a rawsock for TX or RX
void* sendmmsgRawsockOpen(sendmmsg_rawsock_t* rawsock, const char *ifname, bool rx_mode, bool tx_mode, U16 ethertype, U32 frame_size, U32 num_frames)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	AVB_LOGF_DEBUG("Open, ifname=%s, rx=%d, tx=%d, ethertype=%x size=%d, num=%d",
				   ifname, rx_mode, tx_mode, ethertype, frame_size, num_frames);

	baseRawsockOpen(&rawsock->base, ifname, rx_mode, tx_mode, ethertype, frame_size, num_frames);

	rawsock->sock = -1;

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
		AVB_LOG_ERROR("Creating raswsock; requested frame size exceeds MTU");
		free(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}
	rawsock->base.frameSize = TPACKET_ALIGN(rawsock->base.frameSize);
	assert(rawsock->base.frameSize <= MAX_FRAME_SIZE);

	// Prepare default Ethernet header.
	rawsock->base.ethHdrLen = sizeof(eth_hdr_t);
	memset(&(rawsock->base.ethHdr.notag.dhost), 0xFF, ETH_ALEN);
	memcpy(&(rawsock->base.ethHdr.notag.shost), &(rawsock->base.ifInfo.mac), ETH_ALEN);
	rawsock->base.ethHdr.notag.ethertype = htons(rawsock->base.ethertype);

	// Create socket
	rawsock->sock = socket(PF_PACKET, SOCK_RAW, htons(rawsock->base.ethertype));
	if (rawsock->sock == -1) {
		AVB_LOGF_ERROR("Creating rawsock; opening socket: %s", strerror(errno));
		sendmmsgRawsockClose(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	// Allow address reuse
	int temp = 1;
	if(setsockopt(rawsock->sock, SOL_SOCKET, SO_REUSEADDR, &temp, sizeof(int)) < 0) {
		AVB_LOG_ERROR("Creating rawsock; failed to set reuseaddr");
		sendmmsgRawsockClose(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	// Bind to interface
	struct sockaddr_ll my_addr;
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sll_family = PF_PACKET;
	my_addr.sll_protocol = htons(rawsock->base.ethertype);
	my_addr.sll_ifindex = rawsock->base.ifInfo.index;

	if (bind(rawsock->sock, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1) {
		AVB_LOGF_ERROR("Creating rawsock; bind socket: %s", strerror(errno));
		sendmmsgRawsockClose(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	// Clear our buffers and other tracking data
	memset(rawsock->mmsg, 0, sizeof(rawsock->mmsg));
	memset(rawsock->miov, 0, sizeof(rawsock->miov));
	memset(rawsock->pktbuf, 0, sizeof(rawsock->pktbuf));
#if USE_LAUNCHTIME
	memset(rawsock->cmsgbuf, 0, sizeof(rawsock->cmsgbuf));
#endif

	rawsock->buffersOut = 0;
	rawsock->buffersReady = 0;
	rawsock->frameCount = MSG_COUNT;

	// fill virtual functions table
	rawsock_cb_t *cb = &rawsock->base.cb;
	cb->close = sendmmsgRawsockClose;
	cb->getTxFrame = sendmmsgRawsockGetTxFrame;
	cb->txSetMark = sendmmsgRawsockTxSetMark;
	cb->txSetHdr = sendmmsgRawsockTxSetHdr;
	cb->txFrameReady = sendmmsgRawsockTxFrameReady;
	cb->send = sendmmsgRawsockSend;
	cb->getRxFrame = sendmmsgRawsockGetRxFrame;
	cb->rxMulticast = sendmmsgRawsockRxMulticast;
	cb->getSocket = sendmmsgRawsockGetSocket;

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return rawsock;
}

// Close the rawsock
void sendmmsgRawsockClose(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	sendmmsg_rawsock_t *rawsock = (sendmmsg_rawsock_t*)pvRawsock;

	if (rawsock) {
		if (rawsock->sock != -1) {
			close(rawsock->sock);
			rawsock->sock = -1;
		}
	}

	baseRawsockClose(rawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
}

// Get a buffer from the ring to use for TX
U8* sendmmsgRawsockGetTxFrame(void *pvRawsock, bool blocking, unsigned int *len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	sendmmsg_rawsock_t *rawsock = (sendmmsg_rawsock_t*)pvRawsock;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Getting TX frame; bad arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}
	if (rawsock->buffersOut >= rawsock->frameCount) {
		AVB_LOG_ERROR("Getting TX frame; too many TX buffers in use");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}

	U8 *pBuffer = rawsock->pktbuf[rawsock->buffersOut];
	rawsock->buffersOut += 1;

	// Remind client how big the frame buffer is
	if (len)
		*len = rawsock->base.frameSize;

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return  pBuffer;
}

// Set the Firewall MARK on the socket
// The mark is used by FQTSS to identify AVTP packets in kernel.
// FQTSS creates a mark that includes the AVB class and stream index.
bool sendmmsgRawsockTxSetMark(void *pvRawsock, int mark)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	sendmmsg_rawsock_t *rawsock = (sendmmsg_rawsock_t*)pvRawsock;
	bool retval = FALSE;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Setting TX mark; invalid argument passed");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	if (setsockopt(rawsock->sock, SOL_SOCKET, SO_MARK, &mark, sizeof(mark)) < 0) {
		AVB_LOGF_ERROR("Setting TX mark; setsockopt failed: %s", strerror(errno));
	}
	else {
		AVB_LOGF_DEBUG("SO_MARK=%d OK", mark);
		retval = TRUE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return retval;
}

// Pre-set the ethernet header information that will be used on TX frames
bool sendmmsgRawsockTxSetHdr(void *pvRawsock, hdr_info_t *pHdr)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	sendmmsg_rawsock_t *rawsock = (sendmmsg_rawsock_t*)pvRawsock;

	bool ret = baseRawsockTxSetHdr(pvRawsock, pHdr);
	if (ret && pHdr->vlan) {
		// set the class'es priority on the TX socket
		// (required by Telechips platform for FQTSS Credit Based Shaper to work)
		U32 pcp = pHdr->vlan_pcp;
		if (setsockopt(rawsock->sock, SOL_SOCKET, SO_PRIORITY, (char *)&pcp, sizeof(pcp)) < 0) {
			AVB_LOGF_ERROR("openavbRawsockTxSetHdr; SO_PRIORITY setsockopt failed (%d: %s)\n", errno, strerror(errno));
			AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
			return FALSE;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

// Release a TX frame, mark it ready to send
bool sendmmsgRawsockTxFrameReady(void *pvRawsock, U8 *pBuffer, unsigned int len, U64 timeNsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	sendmmsg_rawsock_t *rawsock = (sendmmsg_rawsock_t*)pvRawsock;
	int bufidx;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Marking TX frame ready; invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	bufidx = rawsock->buffersReady;
	assert(pBuffer == rawsock->pktbuf[bufidx]);

#if USE_LAUNCHTIME
	if (!timeNsec) {
		IF_LOG_INTERVAL(1000) AVB_LOG_WARNING("launch time is enabled but not passed to TxFrameReady");
	}
	fillmsghdr(&(rawsock->mmsg[bufidx].msg_hdr), &(rawsock->miov[bufidx]), rawsock->cmsgbuf[bufidx],
			   timeNsec, rawsock->pktbuf[bufidx], len);
#else
	if (timeNsec) {
		IF_LOG_INTERVAL(1000) AVB_LOG_WARNING("launch time is not enabled but was passed to TxFrameReady");
	}
	fillmsghdr(&(rawsock->mmsg[bufidx].msg_hdr), &(rawsock->miov[bufidx]), rawsock->pktbuf[bufidx], len);
#endif


	rawsock->buffersReady += 1;

	if (rawsock->buffersReady >= rawsock->frameCount) {
		AVB_LOG_DEBUG("All TxFrame slots marked ready");
		//sendmmsgRawsockSend(rawsock);
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return TRUE;
}

// Send all packets that are ready (i.e. tell kernel to send them)
int sendmmsgRawsockSend(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	sendmmsg_rawsock_t *rawsock = (sendmmsg_rawsock_t*)pvRawsock;
	int sz, bytes;

	if (!VALID_TX_RAWSOCK(rawsock)) {
			AVB_LOG_ERROR("Marking TX frame ready; invalid argument");
			AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
			return -1;
	}

	if (rawsock->buffersOut != rawsock->buffersReady) {
		AVB_LOGF_ERROR("Tried to send with %d bufs out, %d bufs ready", rawsock->buffersOut, rawsock->buffersReady);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return -1;
	}

	IF_LOG_INTERVAL(1000) AVB_LOGF_DEBUG("Send with %d of %d buffers ready", rawsock->buffersReady, rawsock->frameCount);
	sz = sendmmsg(rawsock->sock, rawsock->mmsg, rawsock->buffersReady, 0);
	if (sz < 0) {
		AVB_LOGF_ERROR("Call to sendmmsg failed! Error code was %d", sz);
		bytes = sz;
	} else {
		int i;
		for (i = 0, bytes = 0; i < sz; i++) {
			bytes += rawsock->mmsg[i].msg_len;
		}
		if (sz < rawsock->buffersReady) {
			AVB_LOGF_WARNING("Only sent %ld of %d messages; dropping others", sz, rawsock->buffersReady);
		}
	}

	rawsock->buffersOut = rawsock->buffersReady = 0;

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return bytes;
}

// Get a RX frame
U8* sendmmsgRawsockGetRxFrame(void *pvRawsock, U32 timeout, unsigned int *offset, unsigned int *len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	sendmmsg_rawsock_t *rawsock = (sendmmsg_rawsock_t*)pvRawsock;
	if (!VALID_RX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Getting RX frame; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}
// For switching to recvmmsg eventually
//	if (rawsock->rxbuffersOut >= rawsock->rxframeCount) {
//		AVB_LOG_ERROR("Too many RX buffers in use");
//		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
//		return NULL;
//	}

	int flags = 0;

	U8 *pBuffer = rawsock->rxBuffer;
	*offset = 0;
	*len = recv(rawsock->sock, pBuffer, rawsock->base.frameSize, flags);

	if (*len == (unsigned int)(-1)) {
		AVB_LOGF_ERROR("%s %s", __func__, strerror(errno));
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return pBuffer;
}

// Setup the rawsock to receive multicast packets
bool sendmmsgRawsockRxMulticast(void *pvRawsock, bool add_membership, const U8 addr[ETH_ALEN])
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	sendmmsg_rawsock_t *rawsock = (sendmmsg_rawsock_t*)pvRawsock;
	if (!VALID_RX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Setting multicast; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	struct ether_addr mcast_addr;
	memcpy(mcast_addr.ether_addr_octet, addr, ETH_ALEN);

	// Fill in the structure for the multicast ioctl
	struct packet_mreq mreq;
	memset(&mreq, 0, sizeof(struct packet_mreq));
	mreq.mr_ifindex = rawsock->base.ifInfo.index;
	mreq.mr_type = PACKET_MR_MULTICAST;
	mreq.mr_alen = ETH_ALEN;
	memcpy(&mreq.mr_address, &mcast_addr.ether_addr_octet, ETH_ALEN);

	// And call the ioctl to add/drop the multicast address
	int action = (add_membership ? PACKET_ADD_MEMBERSHIP : PACKET_DROP_MEMBERSHIP);
	if (setsockopt(rawsock->sock, SOL_PACKET, action,
					(void*)&mreq, sizeof(struct packet_mreq)) < 0) {
		AVB_LOGF_ERROR("Setting multicast; setsockopt(%s) failed: %s",
					   (add_membership ? "PACKET_ADD_MEMBERSHIP" : "PACKET_DROP_MEMBERSHIP"),
					   strerror(errno));

		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	// In addition to adding the multicast membership, we also want to
	//	add a packet filter to restrict the packets that we'll receive
	//	on this socket.  Multicast memeberships are global - not
	//	per-socket, so without the filter, this socket would receieve
	//	packets for all the multicast addresses added by all other
	//	sockets.
	//
	if (add_membership)
	{
		// Here's the template packet filter code.
		// It was produced by running:
		//   tcpdump -dd ether dest host 91:e0:01:02:03:04
		struct sock_filter bpfCode[] = {
			{ 0x20, 0, 0, 0x00000002 },
			{ 0x15, 0, 3, 0x01020304 },   // last 4 bytes of dest mac
			{ 0x28, 0, 0, 0x00000000 },
			{ 0x15, 0, 1, 0x000091e0 },   // first 2 bytes of dest mac
			{ 0x06, 0, 0, 0x0000ffff },
			{ 0x06, 0, 0, 0x00000000 }
		};

		// We need to graft our multicast dest address into bpfCode
		U32 tmp; U8 *buf = (U8*)&tmp;
		memcpy(buf, mcast_addr.ether_addr_octet + 2, 4);
		bpfCode[1].k = ntohl(tmp);
		memset(buf, 0, 4);
		memcpy(buf + 2, mcast_addr.ether_addr_octet, 2);
		bpfCode[3].k = ntohl(tmp);

		// Now wrap the filter code in the appropriate structure
		struct sock_fprog filter;
		memset(&filter, 0, sizeof(filter));
		filter.len = 6;
		filter.filter = bpfCode;

		// And attach it to our socket
		if (setsockopt(rawsock->sock, SOL_SOCKET, SO_ATTACH_FILTER,
						&filter, sizeof(filter)) < 0) {
			AVB_LOGF_ERROR("Setting multicast; setsockopt(SO_ATTACH_FILTER) failed: %s", strerror(errno));
		}
	}
	else {
		if (setsockopt(rawsock->sock, SOL_SOCKET, SO_DETACH_FILTER, NULL, 0) < 0) {
			AVB_LOGF_ERROR("Setting multicast; setsockopt(SO_DETACH_FILTER) failed: %s", strerror(errno));
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return TRUE;
}

// Get the socket used for this rawsock; can be used for poll/select
int  sendmmsgRawsockGetSocket(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	sendmmsg_rawsock_t *rawsock = (sendmmsg_rawsock_t*)pvRawsock;
	if (!rawsock) {
		AVB_LOG_ERROR("Getting socket; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return -1;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return rawsock->sock;
}
