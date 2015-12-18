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

#ifndef RAWSOCK_IMPL_H
#define RAWSOCK_IMPL_H

#include "openavb_rawsock.h"

// CORE TODO: This needs to be centralized; we have multiple defines for 0x8100 and some others like ETHERTYPE_AVTP
#define ETHERTYPE_8021Q 0x8100

#define VLAN_HLEN	4	// The additional bytes required by VLAN
				// (in addition to the Ethernet header)

// Ethernet header
typedef struct {
	U8 dhost[ETH_ALEN];
	U8 shost[ETH_ALEN];
	u_int16_t  ethertype;
} __attribute__ ((__packed__)) eth_hdr_t;

// VLAN tag
typedef struct {
	u_int16_t	tpip;
	u_int16_t	bits;
} __attribute__ ((__packed__)) vlan_tag_t;

// Ethernet header w/VLAN tag
typedef struct {
	U8 dhost[ETH_ALEN];
	U8 shost[ETH_ALEN];
	vlan_tag_t vlan;
	u_int16_t  ethertype;
} __attribute__ ((__packed__)) eth_vlan_hdr_t;

typedef struct {
	void (*setRxSignalMode)(void* rawsock, bool rxSignalMode);
	void (*close)(void* rawsock);
	int (*getSocket)(void* rawsock);
	bool (*getAddr)(void* rawsock, U8 addr[ETH_ALEN]);
	U8* (*getRxFrame)(void* rawsock, U32 usecTimeout, U32* offset, U32* len);
	int (*rxParseHdr)(void* rawsock, U8* pBuffer, hdr_info_t* pInfo);
	bool (*relRxFrame)(void* rawsock, U8* pFrame);
	bool (*rxMulticast)(void* rawsock, bool add_membership, const U8 buf[ETH_ALEN]);
	bool (*rxAVTPSubtype)(void* rawsock, U8 subtype);
	bool (*txSetHdr)(void* rawsock, hdr_info_t* pInfo);
	bool (*txFillHdr)(void* rawsock, U8* pBuffer, U32* hdrlen);
	bool (*txSetMark)(void* rawsock, int prio);
	U8* (*getTxFrame)(void* rawsock, bool blocking, U32* size);
	bool (*relTxFrame)(void* rawsock, U8* pBuffer);
	bool (*txFrameReady)(void* rawsock, U8* pFrame, U32 len);
	int (*send)(void* rawsock);
	int (*txBufLevel)(void* rawsock);
	int (*rxBufLevel)(void* rawsock);
	unsigned long (*getTXOutOfBuffers)(void* pvRawsock);
	unsigned long (*getTXOutOfBuffersCyclic)(void* pvRawsock);
} rawsock_cb_t;

// State information for raw socket
//
typedef struct base_rawsock {
	// implementation callbacks
	rawsock_cb_t cb;

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

	// size of ethernet frame
	int frameSize;

	// TX usage of the socket
	bool txMode;

	// RX usage of the socket
	bool rxMode;

} base_rawsock_t;

// Argument validation
#define VALID_RAWSOCK(s) ((s) != NULL)
#define VALID_TX_RAWSOCK(s) (VALID_RAWSOCK(s) && ((base_rawsock_t*)s)->txMode)
#define VALID_RX_RAWSOCK(s) (VALID_RAWSOCK(s) && ((base_rawsock_t*)s)->rxMode)

void* baseRawsockOpen(base_rawsock_t* rawsock, const char *ifname, bool rx_mode, bool tx_mode, U16 ethertype, U32 frame_size, U32 num_frames);
void baseRawsockClose(void* rawsock);
bool baseRawsockTxSetHdr(void *pvRawsock, hdr_info_t *pHdr);
bool baseRawsockTxFillHdr(void *pvRawsock, U8 *pBuffer, unsigned int *hdrlen);
bool baseRawsockGetAddr(void *pvRawsock, U8 addr[ETH_ALEN]);
int baseRawsockRxParseHdr(void *pvRawsock, U8 *pBuffer, hdr_info_t *pInfo);

#endif // RAWSOCK_IMPL_H
