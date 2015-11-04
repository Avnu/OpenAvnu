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
#ifndef IGB_RAWSOCK_H
#define IGB_RAWSOCK_H

#include "rawsock_impl.h"
#include <pcap/pcap.h>
#include "igb.h"

typedef struct {
	base_rawsock_t base;
	pcap_t *handle;
	device_t *igb_dev;
	struct igb_packet *tx_packet;
	int queue;
	unsigned long txOutOfBuffer;
	unsigned long txOutOfBufferCyclic;
} igb_rawsock_t;

void *igbRawsockOpen(igb_rawsock_t* rawsock, const char *ifname, bool rx_mode, bool tx_mode, U16 ethertype, U32 frame_size, U32 num_frames);

void igbRawsockClose(void *pvRawsock);

bool igbRawsockTxSetMark(void *pvRawsock, int mark);

U8 *igbRawsockGetTxFrame(void *pvRawsock, bool blocking, unsigned int *len);

bool igbRawsockRelTxFrame(void *pvRawsock, U8 *pBuffer);

bool igbRawsockTxFrameReady(void *pvRawsock, U8 *pBuffer, unsigned int len);

int igbRawsockSend(void *pvRawsock);

int igbRawsockTxBufLevel(void *pvRawsock);

unsigned long igbRawsockGetTXOutOfBuffers(void *pvRawsock);

unsigned long igbRawsockGetTXOutOfBuffersCyclic(void *pvRawsock);

#endif
