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

#include "simple_rawsock.h"
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <linux/filter.h>

#include "openavb_trace.h"

#define	AVB_LOG_COMPONENT	"Raw Socket"
#include "openavb_log.h"

// Get information about an interface
bool simpleAvbCheckInterface(const char *ifname, if_info_t *info)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	if (!ifname || !info) {
		AVB_LOG_ERROR("Checking interface; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return FALSE;
	}

	// zap the result struct
	memset(info, 0, sizeof(if_info_t));

	AVB_LOGF_DEBUG("ifname=%s", ifname);
	strncpy(info->name, ifname, IFNAMSIZ - 1);

	// open a throw-away socket - used for our ioctls
	int sk = socket(AF_INET, SOCK_STREAM, 0);
	if (sk == -1) {
		AVB_LOGF_ERROR("Checking interface; socket open failed: %s", strerror(errno));
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return FALSE;
	}

	// set the name of the interface in the ioctl request struct
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);

	// First check if the interface is up
	//  (also proves that the interface exists!)
	int r = ioctl(sk, SIOCGIFFLAGS, &ifr);
	if (r != 0) {
		AVB_LOGF_ERROR("Checking interface; ioctl(SIOCGIFFLAGS) failed: %s", strerror(errno));
		close(sk);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return FALSE;
	}

	if (!(ifr.ifr_flags&IFF_UP)) {
		AVB_LOGF_ERROR("Checking interface; interface is not up: %s", ifname);
		close(sk);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return FALSE;
	}

	// get index for interface
	r = ioctl(sk, SIOCGIFINDEX, &ifr);
	if (r != 0) {
		AVB_LOGF_ERROR("Checking interface; ioctl(SIOCGIFINDEX) failed: %s", strerror(errno));
		close(sk);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return FALSE;
	}
	info->index = ifr.ifr_ifindex;

	// get the MAC address for the interface
	r = ioctl(sk, SIOCGIFHWADDR, &ifr);
	if (r != 0) {
		AVB_LOGF_ERROR("Checking interface; ioctl(SIOCGIFHWADDR) failed: %s", strerror(errno));
		close(sk);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return FALSE;
	}
	memcpy(&info->mac.ether_addr_octet, &ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	// get the MTU for the interface
	r = ioctl(sk, SIOCGIFMTU, &ifr);
	if (r != 0) {
		AVB_LOGF_ERROR("Checking interface; ioctl(SIOCGIFMTU) failed: %s", strerror(errno));
		close(sk);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return FALSE;
	}
	info->mtu = ifr.ifr_mtu;

	// close the temporary socket
	close(sk);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return TRUE;
}

// Open a rawsock for TX or RX
void* simpleRawsockOpen(simple_rawsock_t* rawsock, const char *ifname, bool rx_mode, bool tx_mode, U16 ethertype, U32 frame_size, U32 num_frames)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	AVB_LOGF_DEBUG("Open, rx=%d, tx=%d, ethertype=%x size=%d, num=%d",	rx_mode, tx_mode, ethertype, frame_size, num_frames);

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

	// Prepare default Ethernet header.
	rawsock->base.ethHdrLen = sizeof(eth_hdr_t);
	memset(&(rawsock->base.ethHdr.notag.dhost), 0xFF, ETH_ALEN);
	memcpy(&(rawsock->base.ethHdr.notag.shost), &(rawsock->base.ifInfo.mac), ETH_ALEN);
	rawsock->base.ethHdr.notag.ethertype = htons(rawsock->base.ethertype);

	// Create socket
	rawsock->sock = socket(PF_PACKET, SOCK_RAW, htons(rawsock->base.ethertype));
	if (rawsock->sock == -1) {
		AVB_LOGF_ERROR("Creating rawsock; opening socket: %s", strerror(errno));
		simpleRawsockClose(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	// Allow address reuse
	int temp = 1;
	if(setsockopt(rawsock->sock, SOL_SOCKET, SO_REUSEADDR, &temp, sizeof(int)) < 0) {
		AVB_LOG_ERROR("Creating rawsock; failed to set reuseaddr");
		simpleRawsockClose(rawsock);
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
		simpleRawsockClose(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}


	// fill virtual functions table
	rawsock_cb_t *cb = &rawsock->base.cb;
	cb->close = simpleRawsockClose;
	cb->getTxFrame = simpleRawsockGetTxFrame;
	cb->txSetMark = simpleRawsockTxSetMark;
	cb->txSetHdr = simpleRawsockTxSetHdr;
	cb->txFrameReady = simpleRawsockTxFrameReady;
	cb->getRxFrame = simpleRawsockGetRxFrame;
	cb->rxMulticast = simpleRawsockRxMulticast;
	cb->getSocket = simpleRawsockGetSocket;

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return rawsock;
}

// Close the rawsock
void simpleRawsockClose(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	simple_rawsock_t *rawsock = (simple_rawsock_t*)pvRawsock;

	if (rawsock) {
		// close the socket
		if (rawsock->sock != -1) {
			close(rawsock->sock);
			rawsock->sock = -1;
		}
	}

	baseRawsockClose(rawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
}

// Get a buffer from the ring to use for TX
U8* simpleRawsockGetTxFrame(void *pvRawsock, bool blocking, unsigned int *len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	simple_rawsock_t *rawsock = (simple_rawsock_t*)pvRawsock;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Getting TX frame; bad arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}
//	if (rawsock->buffersOut >= rawsock->frameCount) {
//		AVB_LOG_ERROR("Getting TX frame; too many TX buffers in use");
//		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
//		return NULL;
//	}

	U8 *pBuffer = rawsock->txBuffer;

	// Remind client how big the frame buffer is
	if (len)
		*len = rawsock->base.frameSize;

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return  pBuffer;
}

// Set the Firewall MARK on the socket
// The mark is used by FQTSS to identify AVTP packets in kernel.
// FQTSS creates a mark that includes the AVB class and stream index.
bool simpleRawsockTxSetMark(void *pvRawsock, int mark)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	simple_rawsock_t *rawsock = (simple_rawsock_t*)pvRawsock;
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
bool simpleRawsockTxSetHdr(void *pvRawsock, hdr_info_t *pHdr)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	simple_rawsock_t *rawsock = (simple_rawsock_t*)pvRawsock;

	bool ret = baseRawsockTxSetHdr(pvRawsock, pHdr);
	if (ret && pHdr->vlan) {
		// set the class'es priority on the TX socket
		// (required by Telechips platform for FQTSS Credit Based Shaper to work)
		U32 pcp = pHdr->vlan_pcp;
		if (setsockopt(rawsock->sock, SOL_SOCKET, SO_PRIORITY, (char *)&pcp, sizeof(pcp)) < 0) {
			AVB_LOGF_ERROR("stcRawsockTxSetHdr; SO_PRIORITY setsockopt failed (%d: %s)\n", errno, strerror(errno));
			AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
			return FALSE;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

// Release a TX frame, and send it
bool simpleRawsockTxFrameReady(void *pvRawsock, U8 *pBuffer, unsigned int len, U64 timeNsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	simple_rawsock_t *rawsock = (simple_rawsock_t*)pvRawsock;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Marking TX frame ready; invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	if (timeNsec) {
		IF_LOG_INTERVAL(1000) AVB_LOG_WARNING("launch time is unsupported in simple_rawsock");
	}

	int flags = MSG_DONTWAIT;
	send(rawsock->sock, pBuffer, len, flags);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return TRUE;
}

// Get a RX frame
U8* simpleRawsockGetRxFrame(void *pvRawsock, U32 timeout, unsigned int *offset, unsigned int *len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	simple_rawsock_t *rawsock = (simple_rawsock_t*)pvRawsock;
	if (!VALID_RX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Getting RX frame; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}
//	if (rawsock->buffersOut >= rawsock->frameCount) {
//		AVB_LOG_ERROR("Too many RX buffers in use");
//		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
//		return NULL;
//	}

	int flags = 0;

	U8 *pBuffer = rawsock->rxBuffer;
	*offset = 0;
	*len = recv(rawsock->sock, pBuffer, rawsock->base.frameSize, flags);

	if (*len == -1) {
		AVB_LOGF_ERROR("%s %s", __func__, strerror(errno));
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return pBuffer;
}

// Setup the rawsock to receive multicast packets
bool simpleRawsockRxMulticast(void *pvRawsock, bool add_membership, const U8 addr[ETH_ALEN])
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	simple_rawsock_t *rawsock = (simple_rawsock_t*)pvRawsock;
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
int  simpleRawsockGetSocket(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	simple_rawsock_t *rawsock = (simple_rawsock_t*)pvRawsock;
	if (!rawsock) {
		AVB_LOG_ERROR("Getting socket; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return -1;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return rawsock->sock;
}
