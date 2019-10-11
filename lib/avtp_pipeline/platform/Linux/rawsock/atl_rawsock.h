/*************************************************************************************************************
Copyright (c) 2019, Aquantia Corporation
All rights reserved

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
#ifndef ATL_RAWSOCK_H
#define ATL_RAWSOCK_H

#include "rawsock_impl.h"
#include <pcap/pcap.h>
#include "atl.h"

typedef struct {
	base_rawsock_t base;
	pcap_t *handle;
	device_t *atl_dev;
	struct atl_packet *tx_packet;
	int queue;
	unsigned long txOutOfBuffer;
	unsigned long txOutOfBufferCyclic;
} atl_rawsock_t;

void *atlRawsockOpen(atl_rawsock_t* rawsock, const char *ifname, bool rx_mode, bool tx_mode, U16 ethertype, U32 frame_size, U32 num_frames);

void atlRawsockClose(void *pvRawsock);

bool atlRawsockTxSetMark(void *pvRawsock, int mark);

U8 *atlRawsockGetTxFrame(void *pvRawsock, bool blocking, unsigned int *len);

bool atlRawsockRelTxFrame(void *pvRawsock, U8 *pBuffer);

bool atlRawsockTxFrameReady(void *pvRawsock, U8 *pBuffer, unsigned int len, U64 timeNsec);

int atlRawsockSend(void *pvRawsock);

int atlRawsockTxBufLevel(void *pvRawsock);

unsigned long atlRawsockGetTXOutOfBuffers(void *pvRawsock);

unsigned long atlRawsockGetTXOutOfBuffersCyclic(void *pvRawsock);

#endif
