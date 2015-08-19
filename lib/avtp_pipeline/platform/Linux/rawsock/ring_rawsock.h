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

#ifndef RING_RAWSOCK_H
#define RING_RAWSOCK_H

#include "rawsock_impl.h"

// State information for raw socket
//
typedef struct {
	base_rawsock_t base;

	// the underlying socket
	int sock;

	// number of frames that the ring can hold
	int frameCount;

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

	// Are we losing RX packets?
	bool bLosing;

	// Number of TX buffers we experienced problems with
	unsigned long txOutOfBuffer;
	// Number of TX buffers we experienced problems with from the time when last stats being displayed
	unsigned long txOutOfBufferCyclic;
} ring_rawsock_t;

// Open a rawsock for TX or RX
void* ringRawsockOpen(ring_rawsock_t *rawsock, const char *ifname, bool rx_mode, bool tx_mode, U16 ethertype, U32 frame_size, U32 num_frames);

// Close the rawsock
void ringRawsockClose(void *pvRawsock);

// Get a buffer from the ring to use for TX
U8* ringRawsockGetTxFrame(void *pvRawsock, bool blocking, unsigned int *len);

// Release a TX frame, without marking it as ready to send
bool ringRawsockRelTxFrame(void *pvRawsock, U8 *pBuffer);

// Release a TX frame, and mark it as ready to send
bool ringRawsockTxFrameReady(void *pvRawsock, U8 *pBuffer, unsigned int len);

// Send all packets that are ready (i.e. tell kernel to send them)
int ringRawsockSend(void *pvRawsock);

// Count used TX buffers in ring
int ringRawsockTxBufLevel(void *pvRawsock);

// Count used TX buffers in ring
int ringRawsockRxBufLevel(void *pvRawsock);

// Get a RX frame
U8* ringRawsockGetRxFrame(void *pvRawsock, U32 timeout, unsigned int *offset, unsigned int *len);

// Parse the ethernet frame header.  Returns length of header, or -1 for failure
int ringRawsockRxParseHdr(void *pvRawsock, U8 *pBuffer, hdr_info_t *pInfo);

// Release a RX frame held by the client
bool ringRawsockRelRxFrame(void *pvRawsock, U8 *pBuffer);

unsigned long ringRawsockGetTXOutOfBuffers(void *pvRawsock);

unsigned long ringRawsockGetTXOutOfBuffersCyclic(void *pvRawsock);

#endif
