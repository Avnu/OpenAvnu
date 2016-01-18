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

#ifndef SIMPLE_RAWSOCK_H
#define SIMPLE_RAWSOCK_H

#include "rawsock_impl.h"

// State information for raw socket
//
typedef struct {
	base_rawsock_t base;

	// the underlying socket
	int sock;

	// buffer for sending frames
	U8 txBuffer[1518];

	// buffer for receiving frames
	U8 rxBuffer[1518];
} simple_rawsock_t;

bool simpleAvbCheckInterface(const char *ifname, if_info_t *info);

// Open a rawsock for TX or RX
void* simpleRawsockOpen(simple_rawsock_t *rawsock, const char *ifname, bool rx_mode, bool tx_mode, U16 ethertype, U32 frame_size, U32 num_frames);

// Close the rawsock
void simpleRawsockClose(void *pvRawsock);

// Get a buffer from the simple to use for TX
U8* simpleRawsockGetTxFrame(void *pvRawsock, bool blocking, unsigned int *len);

// Set the Firewall MARK on the socket
// The mark is used by FQTSS to identify AVTP packets in kernel.
// FQTSS creates a mark that includes the AVB class and stream index.
bool simpleRawsockTxSetMark(void *pvRawsock, int mark);

// Pre-set the ethernet header information that will be used on TX frames
bool simpleRawsockTxSetHdr(void *pvRawsock, hdr_info_t *pHdr);

// Release a TX frame, and mark it as ready to send
bool simpleRawsockTxFrameReady(void *pvRawsock, U8 *pBuffer, unsigned int len);

// Get a RX frame
U8* simpleRawsockGetRxFrame(void *pvRawsock, U32 timeout, unsigned int *offset, unsigned int *len);

// Setup the rawsock to receive multicast packets
bool simpleRawsockRxMulticast(void *pvRawsock, bool add_membership, const U8 addr[ETH_ALEN]);

// Get the socket used for this rawsock; can be used for poll/select
int  simpleRawsockGetSocket(void *pvRawsock);

#endif
