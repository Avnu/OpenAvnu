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
 * MODULE SUMMARY : Implements raw socket library.
 *
 * The raw socket library allows its user to send manually-created
 * Etherent frames, and to receive full frames directly off the network
 * interface.
 *
 * AVB needs to do this because Linux doesn't have an in-kernel
 * implementation of the SRP or AVTP protocols.  We handle those
 * protocols in our userspace processes.
 *
 * We use Linux's MMAP method of sending/receiving raw socket frames
 * because it's more efficient than the normal socket read/write method.
 */

// When NATIVE_TX is defined Linux System sockets will not be used for packet transmission. Instead the HAL will be used
// to directly send the packets to the driver. Note: The Linux socket will be be created for compatiblity with the rest of this module
// it just isn't used for transmission. TODO_OPENAVB : The behavior can be improved.
#define NATIVE_TX	1

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/if_packet.h>
#include <assert.h>
#include "openavb_rawsock.h"
#include "openavb_trace.h"
#include <linux/filter.h>

#define	AVB_LOG_COMPONENT	"Raw Socket"
#include "openavb_log.h"

#include "openavb_ether_hal.h"

// CORE TODO: This needs to be centralized; we have multiple defines for 0x8100 and some others like ETHERTYPE_AVTP
#define ETHERTYPE_8021Q 0x8100

// Ethernet header
typedef struct {
	U8 dhost[ETH_ALEN];
	U8 shost[ETH_ALEN];
	u_int16_t  ethertype;
} __attribute__ ((__packed__)) eth_hdr_t;

// VLAN tag
typedef struct {
	u_int16_t 	tpip;
	u_int16_t 	bits;
} __attribute__ ((__packed__)) vlan_tag_t;

// Ethernet header w/VLAN tag
typedef struct {
	U8 dhost[ETH_ALEN];
	U8 shost[ETH_ALEN];
	vlan_tag_t vlan;
	u_int16_t  ethertype;
} __attribute__ ((__packed__)) eth_vlan_hdr_t;
	
// State information for raw socket
//
typedef struct {

	// interface info
	if_info_t ifInfo;

	// saved Ethernet header for TX frames
	union {
		eth_hdr_t      notag;
		eth_vlan_hdr_t tagged;
	} ethHdr;
	unsigned ethHdrLen;

	// Ethertype for TX/RX frames
	unsigned ethertype;

	// the underlying socket
	int sock;

	// number and size of the frames that the ring can hold
	int frameCount;
	int frameSize;

	// size of the header prepended to each frame buffer
	int bufHdrSize;
	// size of the frame buffer (frameSize + bufHdrSize)
	int bufferSize;

	// memory for ring is allocated in blocks by kernel
	int blockSize;
	int blockCount;

	// the memory for the ring
	size_t memSize;
	U8 *pMem;

	// Active slot in the ring - tracked by
	//  block and buffer within that block.
	int blockIndex, bufferIndex;

	// Number of buffers held by client
	int buffersOut;
	// Buffers marked ready, but not yet sent
	int buffersReady;


	// TX usage of the socket
	bool txMode;

	// RX usage of the socket
	bool rxMode;

	// Are we losing RX packets?
	bool bLosing;

	// Number of TX buffers we experienced problems with
	unsigned long txOutOfBuffer;
	// Number of TX buffers we experienced problems with from the time when last stats being displayed
	unsigned long txOutOfBufferCyclic;

#ifdef NATIVE_TX
	eth_frame_t txEthFrame;
#endif

} raw_sock_t;

// Kernel block size
#define BLOCK_SIZE		4096

// Argument validation
#define VALID_RAWSOCK(s) ((s) != NULL && (s)->pMem != (void*)(-1) && (s)->sock != -1)
#define VALID_TX_RAWSOCK(s) (VALID_RAWSOCK(s) && (s)->txMode)
#define VALID_RX_RAWSOCK(s) (VALID_RAWSOCK(s) && (s)->rxMode)

// Get information about an interface
bool openavbCheckInterface(const char *ifname, if_info_t *info)
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
void *openavbRawsockOpen(const char *ifname, bool rx_mode, bool tx_mode, U16 ethertype, U32 frame_size, U32 num_frames)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	AVB_LOGF_DEBUG("Open, rx=%d, tx=%d, ethertype=%x size=%d, num=%d",	rx_mode, tx_mode, ethertype, frame_size, num_frames);

	/* Allocate struct to hold info about the raw socket
	 * that we're going to create.
	 */
	raw_sock_t *rawsock = malloc(sizeof(raw_sock_t));
	if (!rawsock) {
		AVB_LOG_ERROR("Creating rawsock; malloc failed");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}
	memset(rawsock, 0, sizeof(raw_sock_t));
	rawsock->sock = -1;
	rawsock->pMem = (void*)(-1);
	rawsock->rxMode = rx_mode;
	rawsock->txMode = tx_mode;
	rawsock->frameSize = frame_size;
	rawsock->ethertype = ethertype;
	
	// Get info about the network device
	if (!openavbCheckInterface(ifname, &(rawsock->ifInfo))) {
		AVB_LOGF_ERROR("Creating rawsock; bad interface name: %s", ifname);
		free(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	// Deal with frame size.
	if (rawsock->frameSize == 0) {
		// use interface MTU as max frames size, if none specified
		rawsock->frameSize = rawsock->ifInfo.mtu;
	}
	else if (rawsock->frameSize > rawsock->ifInfo.mtu) {
		AVB_LOG_ERROR("Creating raswsock; requested frame size exceeds MTU");
		free(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}
	rawsock->frameSize = TPACKET_ALIGN(rawsock->frameSize);

	// Prepare default Ethernet header.
	rawsock->ethHdrLen = sizeof(eth_hdr_t);
	memset(&(rawsock->ethHdr.notag.dhost), 0xFF, ETH_ALEN);
	memcpy(&(rawsock->ethHdr.notag.shost), &(rawsock->ifInfo.mac), ETH_ALEN);
	rawsock->ethHdr.notag.ethertype = htons(rawsock->ethertype);

	// Create socket
	rawsock->sock = socket(PF_PACKET, SOCK_RAW, htons(rawsock->ethertype));
	if (rawsock->sock == -1) {
		AVB_LOGF_ERROR("Creating rawsock; opening socket: %s", strerror(errno));
		openavbRawsockClose(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	// Allow address reuse
	int temp = 1;
	if(setsockopt(rawsock->sock, SOL_SOCKET, SO_REUSEADDR, &temp, sizeof(int)) < 0) {
		AVB_LOG_ERROR("Creating rawsock; failed to set reuseaddr");
		openavbRawsockClose(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	// Bind to interface
	struct sockaddr_ll my_addr;
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sll_family = PF_PACKET;
	my_addr.sll_protocol = htons(rawsock->ethertype);
	my_addr.sll_ifindex = rawsock->ifInfo.index;

	if (bind(rawsock->sock, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1) {
		AVB_LOGF_ERROR("Creating rawsock; bind socket: %s", strerror(errno));
		openavbRawsockClose(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	// Use version 2 headers for the MMAP packet stuff - avoids 32/64
	// bit problems, gives nanosecond timestamps, and allows rx of vlan id
	int val = TPACKET_V2;
	if (setsockopt(rawsock->sock, SOL_PACKET, PACKET_VERSION, &val, sizeof(val)) < 0) {
		AVB_LOGF_ERROR("Creating rawsock; get PACKET_VERSION: %s", strerror(errno));
		openavbRawsockClose(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	// Get the size of the headers in the ring
	unsigned len = sizeof(val);
	if (getsockopt(rawsock->sock, SOL_PACKET, PACKET_HDRLEN, &val, &len) < 0) {
		AVB_LOGF_ERROR("Creating rawsock; get PACKET_HDRLEN: %s", strerror(errno));
		openavbRawsockClose(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}
	rawsock->bufHdrSize = TPACKET_ALIGN(val);
	
	if (rawsock->rxMode) {
		rawsock->bufHdrSize = rawsock->bufHdrSize + TPACKET_ALIGN(sizeof(struct sockaddr_ll));
	}
	rawsock->bufferSize = rawsock->frameSize + rawsock->bufHdrSize;
	rawsock->frameCount = num_frames;
	AVB_LOGF_DEBUG("frameSize=%d, bufHdrSize=%d(%d+%d) bufferSize=%d, frameCount=%d",
				   rawsock->frameSize, rawsock->bufHdrSize, val, sizeof(struct sockaddr_ll),
				   rawsock->bufferSize, rawsock->frameCount);

	// Get number of bytes in a memory page.  The blocks we ask for
	// must be a multiple of pagesize.  (Actually, it should be
	// (pagesize * 2^N) to avoid wasting memory.)
	int pagesize = getpagesize();
	rawsock->blockSize = pagesize * 4;
	AVB_LOGF_DEBUG("pagesize=%d blockSize=%d", pagesize, rawsock->blockSize);

	// Calculate number of buffers and frames based on blocks
	int buffersPerBlock = rawsock->blockSize / rawsock->bufferSize;
	rawsock->blockCount = rawsock->frameCount / buffersPerBlock + 1;
	rawsock->frameCount = buffersPerBlock * rawsock->blockCount;

	AVB_LOGF_DEBUG("frameCount=%d, buffersPerBlock=%d, blockCount=%d",
				   rawsock->frameCount, buffersPerBlock, rawsock->blockCount);

	// Fill in the kernel structure with our calculated values
	struct tpacket_req s_packet_req;
	memset(&s_packet_req, 0, sizeof(s_packet_req)); 
	s_packet_req.tp_block_size = rawsock->blockSize;
	s_packet_req.tp_frame_size = rawsock->bufferSize;
	s_packet_req.tp_block_nr = rawsock->blockCount;
	s_packet_req.tp_frame_nr = rawsock->frameCount;

	// Ask the kernel to create the TX_RING or RX_RING
	if (rawsock->txMode) {
		if (setsockopt(rawsock->sock, SOL_PACKET, PACKET_TX_RING,
					   (char*)&s_packet_req, sizeof(s_packet_req)) < 0) {
			AVB_LOGF_ERROR("Creating rawsock; TX_RING: %s", strerror(errno));
			openavbRawsockClose(rawsock);
			AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
			return NULL;
		}
		AVB_LOGF_DEBUG("PACKET_%s_RING OK", "TX");
	}
	else {
		if (setsockopt(rawsock->sock, SOL_PACKET, PACKET_RX_RING,
					   (char*)&s_packet_req, sizeof(s_packet_req)) < 0) {
			AVB_LOGF_ERROR("Creating rawsock, RX_RING: %s", strerror(errno));
			openavbRawsockClose(rawsock);
			AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
			return NULL;
		}
		AVB_LOGF_DEBUG("PACKET_%s_RING OK", "TX");
	}

	// Call MMAP to get access to the memory used for the ring
	rawsock->memSize = rawsock->blockCount * rawsock->blockSize;
	AVB_LOGF_DEBUG("memSize=%d (%d, %d), sock=%d",
				   rawsock->memSize,
				   rawsock->blockCount,
				   rawsock->blockSize,
				   rawsock->sock);
	rawsock->pMem = mmap((void*)0, rawsock->memSize, PROT_READ|PROT_WRITE, MAP_SHARED, rawsock->sock, (off_t)0);
	if (rawsock->pMem == (void*)(-1)) {
		AVB_LOGF_ERROR("Creating rawsock; MMAP: %s", strerror(errno));
		openavbRawsockClose(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}
	AVB_LOGF_DEBUG("mmap: %p", rawsock->pMem);

	// Initialize the memory
	memset(rawsock->pMem, 0, rawsock->memSize);

	// Initialize the state of the ring
	rawsock->blockIndex = 0;
	rawsock->bufferIndex = 0;
	rawsock->buffersOut = 0;
	rawsock->buffersReady = 0;
	
	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return rawsock;
}

// Set signal on RX mode
void openavbSetRxSignalMode(void *pvRawsock, bool rxSignalMode)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	// CORE_TODO : Nothing to do on Linux

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
}

// Close the rawsock
void openavbRawsockClose(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	
	if (rawsock) {
		// free ring memory
		if (rawsock->pMem != (void*)(-1)) {
			munmap(rawsock->pMem, rawsock->memSize);
			rawsock->pMem = (void*)(-1);
		}
		// close the socket
		if (rawsock->sock != -1) {
			close(rawsock->sock);
			rawsock->sock = -1;
		}
		// free the state struct
		free(rawsock);
	}
	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
}

// Get a buffer from the ring to use for TX
U8 *openavbRawsockGetTxFrame(void *pvRawsock, bool blocking, unsigned int *len)
{
#ifndef NATIVE_TX
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;

	// Displays only warning when buffer busy after second try
	int bBufferBusyReported = 0;


	if (!VALID_TX_RAWSOCK(rawsock) || len == NULL) {
		AVB_LOG_ERROR("Getting TX frame; bad arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}
	if (rawsock->buffersOut >= rawsock->frameCount) {
		AVB_LOG_ERROR("Getting TX frame; too many TX buffers in use");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}

	// Get pointer to next framebuf.
	volatile struct tpacket2_hdr *pHdr =
		(struct tpacket2_hdr*)(rawsock->pMem
							   + (rawsock->blockIndex * rawsock->blockSize)
							   + (rawsock->bufferIndex * rawsock->bufferSize));
	// And pointer to portion of buffer to be filled with frame
	volatile U8 *pBuffer = (U8*)pHdr + rawsock->bufHdrSize;

	AVB_LOGF_VERBOSE("block=%d, buffer=%d, out=%d, pHdr=%p, pBuffer=%p",
					 rawsock->blockIndex, rawsock->bufferIndex, rawsock->buffersOut,
					 pHdr, pBuffer);
	
	// Check if buffer ready for user
	// In send mode, we want to see TP_STATUS_AVAILABLE
	while (pHdr->tp_status != TP_STATUS_AVAILABLE)
	{
		switch (pHdr->tp_status) {
			case TP_STATUS_SEND_REQUEST:
			case TP_STATUS_SENDING:
				if (blocking) {
#if 0
// We should be able to poll on the socket to wait for the buffer to
// be ready, but it doesn't work (at least on 2.6.37).
// Keep this code, because it may work on newer kernels
					// poll until tx buffer is ready
					struct pollfd pfd;
					pfd.fd = rawsock->sock;
					pfd.events = POLLWRNORM;
					pfd.revents = 0;
					int ret = poll(&pfd, 1, -1);
					if (ret < 0 && errno != EINTR) {
						AVB_LOGF_DEBUG("getting TX frame; poll failed: %s", strerror(errno));
					}
#else
					// Can't poll, so sleep instead to avoid tight loop
					if(0 == bBufferBusyReported) {
						if(!rawsock->txOutOfBuffer) {
							// Display this info only once just to let know that something like this happened
							AVB_LOGF_INFO("Getting TX frame (sock=%d): TX buffer busy", rawsock->sock);
						}

						++rawsock->txOutOfBuffer;
						++rawsock->txOutOfBufferCyclic;
					} else if(1 == bBufferBusyReported) {
						//Display this warning if buffer was busy more than once because it might influence late/lost
						AVB_LOGF_WARNING("Getting TX frame (sock=%d): TX buffer busy after usleep(50) verify if there are any lost/late frames", rawsock->sock);
					}

					++bBufferBusyReported;

					usleep(50);
#endif
				}
				else {
					AVB_LOG_DEBUG("Non-blocking, return NULL");
					AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
					return NULL;
				}
				break;
			case TP_STATUS_WRONG_FORMAT:
			default:
				pHdr->tp_status = TP_STATUS_AVAILABLE;
				break;
		}
	}

	// Remind client how big the frame buffer is
	if (len)
		*len = rawsock->frameSize;
	
	// increment indexes to point to next buffer
	if (++(rawsock->bufferIndex) >= (rawsock->frameCount/rawsock->blockCount)) {
		rawsock->bufferIndex = 0;
		if (++(rawsock->blockIndex) >= rawsock->blockCount) {
			rawsock->blockIndex = 0;
		}
	}

	// increment the count of buffers held by client
	rawsock->buffersOut += 1;

	// warn if too many are out
	if (rawsock->buffersOut >= (rawsock->frameCount * 4)/5) {
		AVB_LOGF_WARNING("Getting TX frame; consider increasing buffers: count=%d, out=%d",
						 rawsock->frameCount, rawsock->buffersOut);
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return (U8*)pBuffer;
#else // NATIVE_TX
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;

	if (!VALID_TX_RAWSOCK(rawsock) || len == NULL) {
		AVB_LOG_ERROR("Getting TX frame; bad arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}

	if (!halGetTxBuf(&rawsock->txEthFrame)) {
		AVB_LOG_ERROR("Getting TX frame. Not available");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}
	*len = rawsock->txEthFrame.length;

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return (U8*)rawsock->txEthFrame.pBuffer;
#endif	// NATIVE_TX
}

// Set the Firewall MARK on the socket
// The mark is used by FQTSS to identify AVTP packets in kernel.
// FQTSS creates a mark that includes the AVB class and stream index.
bool openavbRawsockTxSetMark(void *pvRawsock, int mark)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
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
bool openavbRawsockTxSetHdr(void *pvRawsock, hdr_info_t *pHdr)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	
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
inline bool openavbRawsockTxFillHdr(void *pvRawsock, U8 *pBuffer, unsigned int *hdrlen)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	if (!VALID_TX_RAWSOCK(rawsock)) {
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

// Release a TX frame, without marking it as ready to send
bool openavbRawsockRelTxFrame(void *pvRawsock, U8 *pBuffer)
{
#ifndef NATIVE_TX
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	if (!VALID_TX_RAWSOCK(rawsock) || pBuffer == NULL) {
		AVB_LOG_ERROR("Releasing TX frame; invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	volatile struct tpacket2_hdr *pHdr = (struct tpacket2_hdr*)(pBuffer - rawsock->bufHdrSize);
	AVB_LOGF_VERBOSE("pBuffer=%p, pHdr=%p", pBuffer, pHdr);

	pHdr->tp_len = 0;
	pHdr->tp_status = TP_STATUS_KERNEL;
	rawsock->buffersOut -= 1;

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return TRUE;
#else	// NATIVE_TX
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	if (!VALID_TX_RAWSOCK(rawsock) || pBuffer == NULL) {
		AVB_LOG_ERROR("Releasing TX frame; invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	if (!halRelTxBuf(&rawsock->txEthFrame)) {
		AVB_LOG_ERROR("Releasing TX frame");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return TRUE;
#endif 	// NATIVE_TX
}

// Release a TX frame, and mark it as ready to send
bool openavbRawsockTxFrameReady(void *pvRawsock, U8 *pBuffer, unsigned int len)
{
#ifndef NATIVE_TX
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Marking TX frame ready; invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	volatile struct tpacket2_hdr *pHdr = (struct tpacket2_hdr*)(pBuffer - rawsock->bufHdrSize);
	AVB_LOGF_VERBOSE("pBuffer=%p, pHdr=%p szFrame=%d, len=%d", pBuffer, pHdr, rawsock->frameSize, len);

	assert(len <= rawsock->bufferSize);
	pHdr->tp_len = len;
	pHdr->tp_status = TP_STATUS_SEND_REQUEST;
	rawsock->buffersReady += 1;
	
	if (rawsock->buffersReady >= rawsock->frameCount) {
		AVB_LOG_WARNING("All buffers in ready/unsent state, calling send");
		openavbRawsockSend(pvRawsock);
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return TRUE;
#else 	// NATIVE_TX
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	// Native TX currently suppors only a single frame TX at a time.
	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return TRUE;
#endif	// NATIVE_TX
}

// Send all packets that are ready (i.e. tell kernel to send them)
int openavbRawsockSend(void *pvRawsock)
{
#ifndef NATIVE_TX
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Send; invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return -1;
	}

	// Linux does something dumb to wait for frames to be sent.
	// Without MSG_DONTWAIT, CPU usage is bad.
	int flags = MSG_DONTWAIT;
	int sent = send(rawsock->sock, NULL, 0, flags);
	if (errno == EINTR) {
		// ignore
	}
	else if (sent < 0) {
		AVB_LOGF_ERROR("Send failed: %s", strerror(errno));
		assert(0);
	}
	else {
		AVB_LOGF_VERBOSE("Sent %d bytes, %d frames", sent, rawsock->buffersReady);
		rawsock->buffersOut -= rawsock->buffersReady;
		rawsock->buffersReady = 0;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return sent;
#else 	// NATIVE_TX
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Send; invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return -1;
	}

	if (!halSendTxBuf(&rawsock->txEthFrame)) {
		AVB_LOG_ERROR("Sending TX frame");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return -1;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return 1;
#endif	// NATIVE_TX
}

// Count used TX buffers in ring
int openavbRawsockTxBufLevel(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;

	int buffersPerBlock = rawsock->blockSize / rawsock->bufferSize;

	int iBlock, iBuffer, nInUse = 0;
	volatile struct tpacket2_hdr *pHdr;

	if (!VALID_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("getting buffer level; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	for (iBlock = 0; iBlock < rawsock->blockCount; iBlock++) {
		for (iBuffer = 0; iBuffer < buffersPerBlock; iBuffer++) {

			pHdr = (struct tpacket2_hdr*)(rawsock->pMem
										  + (iBlock * rawsock->blockSize)
										  + (iBuffer * rawsock->bufferSize));

			if (rawsock->txMode) {
				if (pHdr->tp_status == TP_STATUS_SEND_REQUEST
					|| pHdr->tp_status == TP_STATUS_SENDING) 
					nInUse++;
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return nInUse;
}


// Count used TX buffers in ring
int openavbRawsockRxBufLevel(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;

	int buffersPerBlock = rawsock->blockSize / rawsock->bufferSize;

	int iBlock, iBuffer, nInUse = 0;
	volatile struct tpacket2_hdr *pHdr;

	if (!VALID_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("getting buffer level; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	for (iBlock = 0; iBlock < rawsock->blockCount; iBlock++) {
		for (iBuffer = 0; iBuffer < buffersPerBlock; iBuffer++) {

			pHdr = (struct tpacket2_hdr*)(rawsock->pMem
										  + (iBlock * rawsock->blockSize)
										  + (iBuffer * rawsock->bufferSize));

			if (!rawsock->txMode) {
				if (pHdr->tp_status & TP_STATUS_USER)
					nInUse++;
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return nInUse;
}


// Get a RX frame
U8 *openavbRawsockGetRxFrame(void *pvRawsock, U32 timeout, unsigned int *offset, unsigned int *len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	if (!VALID_RX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Getting RX frame; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}
	if (rawsock->buffersOut >= rawsock->frameCount) {
		AVB_LOG_ERROR("Too many RX buffers in use");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}

	// Get pointer to active buffer in ring
	volatile struct tpacket2_hdr *pHdr =
		(struct tpacket2_hdr*)(rawsock->pMem
							   + (rawsock->blockIndex * rawsock->blockSize)
							   + (rawsock->bufferIndex * rawsock->bufferSize));
	volatile U8 *pBuffer = (U8*)pHdr + rawsock->bufHdrSize;

	AVB_LOGF_VERBOSE("block=%d, buffer=%d, out=%d, pHdr=%p, pBuffer=%p",
					 rawsock->blockIndex, rawsock->bufferIndex, rawsock->buffersOut,
					 pHdr, pBuffer);
	
	// Check if buffer ready for user
	// In receive mode, we want TP_STATUS_USER flag set
	if ((pHdr->tp_status & TP_STATUS_USER) == 0)
	{
		struct timespec ts, *pts = NULL;
		struct pollfd pfd;

		// Use poll to wait for "ready to read" condition

		// Poll even if our timeout is 0 - to catch the case where
		// kernel is writing to the wrong slot (see below.)
		if (timeout != OPENAVB_RAWSOCK_BLOCK) {
			ts.tv_sec = timeout / MICROSECONDS_PER_SECOND;
			ts.tv_nsec = (timeout % MICROSECONDS_PER_SECOND) * NANOSECONDS_PER_USEC;
			pts = &ts;
		}

		pfd.fd = rawsock->sock;
		pfd.events = POLLIN;
		pfd.revents = 0;
			
		int ret = ppoll(&pfd, 1, pts, NULL);
		if (ret < 0) {
			if (errno != EINTR) {
				AVB_LOGF_ERROR("Getting RX frame; poll failed: %s", strerror(errno));
			}
			AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
			return NULL;
		}
		if ((pfd.revents & POLLIN) == 0) {
			// timeout
			AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
			return NULL;
		}

		if ((pHdr->tp_status & TP_STATUS_USER) == 0) {
			// Hmmm, this is unexpected.  poll indicated that the
			// socket was ready to read, but the slot in the TX ring
			// that we're looking for the kernel to fill isn't filled.

			// If there aren't any RX buffers held by the application,
			// we can try to fix this sticky situation...
			if (rawsock->buffersOut == 0) {
				// Scan forward through the RX ring, and look for a
				// buffer that's ready for us to read.  The kernel has
				// a bad habit of not starting at the beginning of the
				// ring when the listener process is restarted.
				int nSkipped = 0;
				while((pHdr->tp_status & TP_STATUS_USER) == 0) {
					// Move to next slot in ring.
					// (Increment buffer/block indexes)
					if (++(rawsock->bufferIndex) >= (rawsock->frameCount/rawsock->blockCount)) {
						rawsock->bufferIndex = 0;
						if (++(rawsock->blockIndex) >= rawsock->blockCount) {
							rawsock->blockIndex = 0;
						}
					}
					
					// Adjust pHdr, pBuffer to point to the new slot
					pHdr = (struct tpacket2_hdr*)(rawsock->pMem
												  + (rawsock->blockIndex * rawsock->blockSize)
												  + (rawsock->bufferIndex * rawsock->bufferSize));
					pBuffer = (U8*)pHdr + rawsock->bufHdrSize;

					// If we've scanned all the way around the ring, bail out.
					if (++nSkipped > rawsock->frameCount) {
						AVB_LOG_WARNING("Getting RX frame; no frame after poll");
						AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
						return NULL;
					}
				}

				// We found a slot that's ready.  Hopefully, we're good now.
				AVB_LOGF_WARNING("Getting RX frame; skipped %d empty slots (rawsock=%p)", nSkipped, rawsock);
			}
			else {
				AVB_LOG_WARNING("Getting RX frame; no frame after poll");
				AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
				return NULL;
			}
		}
	}

	AVB_LOGF_VERBOSE("Buffer status=0x%4.4lx", (unsigned long)pHdr->tp_status);
	if (pHdr->tp_status & TP_STATUS_COPY) {
		AVB_LOG_WARNING("Frame too big for receive buffer");
	}

	// Check the "losing" flag.  That indicates that the ring is full,
	// and the kernel had to toss some frames. There is no "winning" flag.
	if ((pHdr->tp_status & TP_STATUS_LOSING)) {
		if (!rawsock->bLosing) {
			AVB_LOG_WARNING("Getting RX frame; mmap buffers full");
			rawsock->bLosing = TRUE;
		}
	}
	else {
		rawsock->bLosing = FALSE;
	}

	// Return pointer to the buffer and length
	*offset = pHdr->tp_mac - rawsock->bufHdrSize;
	*len = pHdr->tp_len;

	// increment indexes for next time
	if (++(rawsock->bufferIndex) >= (rawsock->frameCount/rawsock->blockCount)) {
		rawsock->bufferIndex = 0;
		if (++(rawsock->blockIndex) >= rawsock->blockCount) {
			rawsock->blockIndex = 0;
		}
	}

	// Remember that the client has another buffer
	rawsock->buffersOut += 1;
	
	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return (U8*)pBuffer;
}

// Parse the ethernet frame header.  Returns length of header, or -1 for failure
int openavbRawsockRxParseHdr(void *pvRawsock, U8 *pBuffer, hdr_info_t *pInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	int hdrLen;
	if (!VALID_RX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Parsing Ethernet headers; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return -1;
	}

	volatile struct tpacket2_hdr *pHdr = (struct tpacket2_hdr*)(pBuffer - rawsock->bufHdrSize);
	AVB_LOGF_VERBOSE("pBuffer=%p, pHdr=%p", pBuffer, pHdr);
	
	memset(pInfo, 0, sizeof(hdr_info_t));

    eth_hdr_t *pNoTag = (eth_hdr_t*)((U8*)pHdr + pHdr->tp_mac);
	hdrLen = pHdr->tp_net - pHdr->tp_mac;
	pInfo->shost = pNoTag->shost;
	pInfo->dhost = pNoTag->dhost;
	pInfo->ethertype = ntohs(pNoTag->ethertype);

	if (pInfo->ethertype == ETHERTYPE_8021Q) {
		pInfo->vlan = TRUE;
		pInfo->vlan_vid = pHdr->tp_vlan_tci & 0x0FFF;
		pInfo->vlan_pcp = (pHdr->tp_vlan_tci >> 13) & 0x0007;
		pInfo->ethertype = ntohs(*(U16*)( ((U8*)(&pNoTag->ethertype)) + 4));
		hdrLen += 4;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return hdrLen;
}

// Release a RX frame held by the client
bool openavbRawsockRelRxFrame(void *pvRawsock, U8 *pBuffer)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;

	if (!VALID_RX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Releasing RX frame; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	volatile struct tpacket2_hdr *pHdr = (struct tpacket2_hdr*)(pBuffer - rawsock->bufHdrSize);
	AVB_LOGF_VERBOSE("pBuffer=%p, pHdr=%p", pBuffer, pHdr);

	pHdr->tp_status = TP_STATUS_KERNEL;
	rawsock->buffersOut -= 1;
	
	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return TRUE;
}

// Setup the rawsock to receive multicast packets
bool openavbRawsockRxMulticast(void *pvRawsock, bool add_membership, const U8 addr[ETH_ALEN])
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
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
	mreq.mr_ifindex = rawsock->ifInfo.index;
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
int  openavbRawsockGetSocket(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	if (!rawsock) {
		AVB_LOG_ERROR("Getting socket; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return -1;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return rawsock->sock;
}

// Get the ethernet address of the interface
bool  openavbRawsockGetAddr(void *pvRawsock, U8 addr[ETH_ALEN])
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;
	if (!rawsock) {
		AVB_LOG_ERROR("Getting address; invalid arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return FALSE;
	}

	memcpy(addr, &rawsock->ifInfo.mac.ether_addr_octet, ETH_ALEN);
	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return TRUE;
}

unsigned long openavbRawsockGetTXOutOfBuffers(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	unsigned long counter = 0;
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;

	if(VALID_TX_RAWSOCK(rawsock)) {
		counter = rawsock->txOutOfBuffer;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return counter;
}

unsigned long openavbRawsockGetTXOutOfBuffersCyclic(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	unsigned long counter = 0;
	raw_sock_t *rawsock = (raw_sock_t*)pvRawsock;

	if(VALID_TX_RAWSOCK(rawsock)) {
		counter = rawsock->txOutOfBufferCyclic;
		rawsock->txOutOfBufferCyclic = 0;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return counter;
}
