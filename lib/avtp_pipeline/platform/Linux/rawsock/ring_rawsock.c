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

#include "ring_rawsock.h"
#include "simple_rawsock.h"
#include <linux/if_packet.h>

#include "openavb_trace.h"

#define	AVB_LOG_COMPONENT	"Raw Socket"
#include "openavb_log.h"


// Open a rawsock for TX or RX
void* ringRawsockOpen(ring_rawsock_t *rawsock, const char *ifname, bool rx_mode, bool tx_mode, U16 ethertype, U32 frame_size, U32 num_frames)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	if (!simpleRawsockOpen((simple_rawsock_t*)rawsock, ifname, rx_mode,
			       tx_mode, ethertype, frame_size, num_frames))
	{
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	rawsock->pMem = (void*)(-1);

	// Use version 2 headers for the MMAP packet stuff - avoids 32/64
	// bit problems, gives nanosecond timestamps, and allows rx of vlan id
	int val = TPACKET_V2;
	if (setsockopt(rawsock->sock, SOL_PACKET, PACKET_VERSION, &val, sizeof(val)) < 0) {
		AVB_LOGF_ERROR("Creating rawsock; get PACKET_VERSION: %s", strerror(errno));
		ringRawsockClose(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	// Get the size of the headers in the ring
	unsigned len = sizeof(val);
	if (getsockopt(rawsock->sock, SOL_PACKET, PACKET_HDRLEN, &val, &len) < 0) {
		AVB_LOGF_ERROR("Creating rawsock; get PACKET_HDRLEN: %s", strerror(errno));
		ringRawsockClose(rawsock);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}
	rawsock->bufHdrSize = TPACKET_ALIGN(val);

	if (rawsock->base.rxMode) {
		rawsock->bufHdrSize = rawsock->bufHdrSize + TPACKET_ALIGN(sizeof(struct sockaddr_ll));
	}
	rawsock->bufferSize = rawsock->base.frameSize + rawsock->bufHdrSize;
	rawsock->frameCount = num_frames;
	AVB_LOGF_DEBUG("frameSize=%d, bufHdrSize=%d(%d+%zu) bufferSize=%d, frameCount=%d",
				   rawsock->base.frameSize, rawsock->bufHdrSize, val, sizeof(struct sockaddr_ll),
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
	if (rawsock->base.txMode) {
		if (setsockopt(rawsock->sock, SOL_PACKET, PACKET_TX_RING,
					   (char*)&s_packet_req, sizeof(s_packet_req)) < 0) {
			AVB_LOGF_ERROR("Creating rawsock; TX_RING: %s", strerror(errno));
			ringRawsockClose(rawsock);
			AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
			return NULL;
		}
		AVB_LOGF_DEBUG("PACKET_%s_RING OK", "TX");
	}
	else {
		if (setsockopt(rawsock->sock, SOL_PACKET, PACKET_RX_RING,
					   (char*)&s_packet_req, sizeof(s_packet_req)) < 0) {
			AVB_LOGF_ERROR("Creating rawsock, RX_RING: %s", strerror(errno));
			ringRawsockClose(rawsock);
			AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
			return NULL;
		}
		AVB_LOGF_DEBUG("PACKET_%s_RING OK", "TX");
	}

	// Call MMAP to get access to the memory used for the ring
	rawsock->memSize = rawsock->blockCount * rawsock->blockSize;
	AVB_LOGF_DEBUG("memSize=%zu (%d, %d), sock=%d",
				   rawsock->memSize,
				   rawsock->blockCount,
				   rawsock->blockSize,
				   rawsock->sock);
	rawsock->pMem = mmap((void*)0, rawsock->memSize, PROT_READ|PROT_WRITE, MAP_SHARED, rawsock->sock, (off_t)0);
	if (rawsock->pMem == (void*)(-1)) {
		AVB_LOGF_ERROR("Creating rawsock; MMAP: %s", strerror(errno));
		ringRawsockClose(rawsock);
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

	// fill virtual functions table
	rawsock_cb_t *cb = &rawsock->base.cb;
	cb->close = ringRawsockClose;
	cb->getTxFrame = ringRawsockGetTxFrame;
	cb->relTxFrame = ringRawsockRelTxFrame;
	cb->txFrameReady = ringRawsockTxFrameReady;
	cb->send = ringRawsockSend;
	cb->txBufLevel = ringRawsockTxBufLevel;
	cb->rxBufLevel = ringRawsockRxBufLevel;
	cb->getRxFrame = ringRawsockGetRxFrame;
	cb->rxParseHdr = ringRawsockRxParseHdr;
	cb->relRxFrame = ringRawsockRelRxFrame;
	cb->getTXOutOfBuffers = ringRawsockGetTXOutOfBuffers;
	cb->getTXOutOfBuffersCyclic = ringRawsockGetTXOutOfBuffersCyclic;

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return rawsock;
}

// Close the rawsock
void ringRawsockClose(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	ring_rawsock_t *rawsock = (ring_rawsock_t*)pvRawsock;

	if (rawsock) {
		// free ring memory
		if (rawsock->pMem != (void*)(-1)) {
			munmap(rawsock->pMem, rawsock->memSize);
			rawsock->pMem = (void*)(-1);
		}
	}

	simpleRawsockClose(pvRawsock);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
}

// Get a buffer from the ring to use for TX
U8* ringRawsockGetTxFrame(void *pvRawsock, bool blocking, unsigned int *len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	ring_rawsock_t *rawsock = (ring_rawsock_t*)pvRawsock;

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
		*len = rawsock->base.frameSize;

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
}

// Release a TX frame, without marking it as ready to send
bool ringRawsockRelTxFrame(void *pvRawsock, U8 *pBuffer)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	ring_rawsock_t *rawsock = (ring_rawsock_t*)pvRawsock;
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
}

// Release a TX frame, and mark it as ready to send
bool ringRawsockTxFrameReady(void *pvRawsock, U8 *pBuffer, unsigned int len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	ring_rawsock_t *rawsock = (ring_rawsock_t*)pvRawsock;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Marking TX frame ready; invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	volatile struct tpacket2_hdr *pHdr = (struct tpacket2_hdr*)(pBuffer - rawsock->bufHdrSize);
	AVB_LOGF_VERBOSE("pBuffer=%p, pHdr=%p szFrame=%d, len=%d", pBuffer, pHdr, rawsock->base.frameSize, len);

	assert(len <= rawsock->bufferSize);
	pHdr->tp_len = len;
	pHdr->tp_status = TP_STATUS_SEND_REQUEST;
	rawsock->buffersReady += 1;

	if (rawsock->buffersReady >= rawsock->frameCount) {
		AVB_LOG_WARNING("All buffers in ready/unsent state, calling send");
		ringRawsockSend(pvRawsock);
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return TRUE;
}

// Send all packets that are ready (i.e. tell kernel to send them)
int ringRawsockSend(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	ring_rawsock_t *rawsock = (ring_rawsock_t*)pvRawsock;
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
}

// Count used TX buffers in ring
int ringRawsockTxBufLevel(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	ring_rawsock_t *rawsock = (ring_rawsock_t*)pvRawsock;

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

			if (rawsock->base.txMode) {
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
int ringRawsockRxBufLevel(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	ring_rawsock_t *rawsock = (ring_rawsock_t*)pvRawsock;

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

			if (!rawsock->base.txMode) {
				if (pHdr->tp_status & TP_STATUS_USER)
					nInUse++;
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return nInUse;
}

// Get a RX frame
U8* ringRawsockGetRxFrame(void *pvRawsock, U32 timeout, unsigned int *offset, unsigned int *len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	ring_rawsock_t *rawsock = (ring_rawsock_t*)pvRawsock;
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

	// increment indexes for next time
	if (++(rawsock->bufferIndex) >= (rawsock->frameCount/rawsock->blockCount)) {
		rawsock->bufferIndex = 0;
		if (++(rawsock->blockIndex) >= rawsock->blockCount) {
			rawsock->blockIndex = 0;
		}
	}

	// Remember that the client has another buffer
	rawsock->buffersOut += 1;

	if (pHdr->tp_snaplen < pHdr->tp_len) {
		IF_LOG_INTERVAL(1000) AVB_LOGF_WARNING("Getting RX frame; partial frame ignored (len %d, snaplen %d)", pHdr->tp_len, pHdr->tp_snaplen);
		ringRawsockRelRxFrame(rawsock, (U8*)pBuffer);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}

	// Return pointer to the buffer and length
	*offset = pHdr->tp_mac - rawsock->bufHdrSize;
	*len = pHdr->tp_snaplen;

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return (U8*)pBuffer;
}

// Parse the ethernet frame header.  Returns length of header, or -1 for failure
int ringRawsockRxParseHdr(void *pvRawsock, U8 *pBuffer, hdr_info_t *pInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	ring_rawsock_t *rawsock = (ring_rawsock_t*)pvRawsock;
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
bool ringRawsockRelRxFrame(void *pvRawsock, U8 *pBuffer)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	ring_rawsock_t *rawsock = (ring_rawsock_t*)pvRawsock;

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

unsigned long ringRawsockGetTXOutOfBuffers(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	unsigned long counter = 0;
	ring_rawsock_t *rawsock = (ring_rawsock_t*)pvRawsock;

	if(VALID_TX_RAWSOCK(rawsock)) {
		counter = rawsock->txOutOfBuffer;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return counter;
}

unsigned long ringRawsockGetTXOutOfBuffersCyclic(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	unsigned long counter = 0;
	ring_rawsock_t *rawsock = (ring_rawsock_t*)pvRawsock;

	if(VALID_TX_RAWSOCK(rawsock)) {
		counter = rawsock->txOutOfBufferCyclic;
		rawsock->txOutOfBufferCyclic = 0;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return counter;
}
