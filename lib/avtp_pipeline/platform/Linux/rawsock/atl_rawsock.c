/*************************************************************************************************************
Copyright (c) 2019, Aquantia Corporation
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

#include "atl_rawsock.h"
#include "pcap_rawsock.h"
#include "simple_rawsock.h"
#include "avb.h"
#include "openavb_atl.h"
#include "avb_sched.h"

#include "openavb_trace.h"

#define	AVB_LOG_COMPONENT	"Raw Socket"
#include "openavb_log.h"

// needed for gptplocaltime()
extern gPtpTimeData gPtpTD;

void *atlRawsockOpen(atl_rawsock_t* rawsock, const char *ifname, bool rx_mode, bool tx_mode, U16 ethertype, U32 frame_size, U32 num_frames)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);

	if (!pcapRawsockOpen((pcap_rawsock_t*)rawsock, ifname, rx_mode,
		             tx_mode, ethertype, frame_size, num_frames))
	{
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
		return NULL;
	}

	if (tx_mode) {
		// Deal with frame size.
		if (frame_size == 0) {
			// use interface MTU as max frames size, if none specified
			rawsock->base.frameSize = ATL_MTU;
		}
		else if (frame_size > ATL_MTU) {
			AVB_LOGF_ERROR("Creating rawsock; requested frame size exceeds %d", ATL_MTU);
			atlRawsockClose(rawsock);
			AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
			return NULL;
		} else {
			rawsock->base.frameSize = frame_size;
		}

		// ATL setup
		rawsock->atl_dev = atlAcquireDevice(ifname);

		// select class B queue by default
		rawsock->queue = 1;
	}

	// fill virtual functions table
	rawsock_cb_t *cb = &rawsock->base.cb;
	cb->close = atlRawsockClose;
	cb->getTxFrame = atlRawsockGetTxFrame;
	cb->relTxFrame = atlRawsockRelTxFrame;
	cb->txSetMark = atlRawsockTxSetMark;
	cb->txFrameReady = atlRawsockTxFrameReady;
	cb->send = atlRawsockSend;
	cb->txBufLevel = atlRawsockTxBufLevel;
	cb->getTXOutOfBuffers = atlRawsockGetTXOutOfBuffers;
	cb->getTXOutOfBuffersCyclic = atlRawsockGetTXOutOfBuffersCyclic;

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return rawsock;
}

void atlRawsockClose(void *pvRawsock)
{
	atl_rawsock_t *rawsock = (atl_rawsock_t*)pvRawsock;
	if (rawsock->atl_dev) {
		atlReleaseDevice(rawsock->atl_dev);
	}

	pcapRawsockClose((pcap_rawsock_t*)rawsock);
}

bool atlRawsockTxSetMark(void *pvRawsock, int mark)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	atl_rawsock_t *rawsock = (atl_rawsock_t*)pvRawsock;
	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Setting TX mark; invalid argument passed");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	int fwmarkClass = TC_AVB_MARK_CLASS(mark);

	if (fwmarkClass == SR_CLASS_A) {
		rawsock->queue = 0;
	} else if (fwmarkClass == SR_CLASS_B) {
		rawsock->queue = 1;
	} else {
		AVB_LOGF_ERROR("fwmarkClass %d is not proper SR_CLASS", fwmarkClass);
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return TRUE;
}

// Get a buffer from the ring to use for TX
U8 *atlRawsockGetTxFrame(void *pvRawsock, bool blocking, unsigned int *len)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	atl_rawsock_t *rawsock = (atl_rawsock_t*)pvRawsock;

	if (!VALID_TX_RAWSOCK(rawsock) || len == NULL) {
		AVB_LOG_ERROR("Getting TX frame; bad arguments");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return NULL;
	}

	U8 *ret = NULL;
	int bBufferBusyReported = 0;
	U32 iterationCount = 10;

	do {
		rawsock->tx_packet = atlGetTxPacket(rawsock->atl_dev);
		if (!rawsock->tx_packet && blocking) {
			if (0 == bBufferBusyReported) {
				if (!rawsock->txOutOfBuffer) {
					AVB_LOGF_DEBUG("Getting TX frame (%p): TX buffer busy", rawsock);
				}
				++rawsock->txOutOfBuffer;
				++rawsock->txOutOfBufferCyclic;
			} else if (1 == bBufferBusyReported) {
				AVB_LOGF_DEBUG("Getting TX frame (%p): TX buffer busy after usleep(10) verify if there are any late frames", rawsock);
			}
			++bBufferBusyReported;
			usleep(10);
		}
	} while (!rawsock->tx_packet && blocking && iterationCount--);

	if (rawsock->tx_packet) {
		*len = rawsock->base.frameSize;
		ret = rawsock->tx_packet->vaddr;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return ret;
}

// Release a TX frame, without marking it as ready to send
bool atlRawsockRelTxFrame(void *pvRawsock, U8 *pBuffer)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	atl_rawsock_t *rawsock = (atl_rawsock_t*)pvRawsock;
	if (!VALID_TX_RAWSOCK(rawsock) || pBuffer == NULL) {
		AVB_LOG_ERROR("Releasing TX frame; invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return FALSE;
	}

	atlRelTxPacket(rawsock->atl_dev, rawsock->queue, rawsock->tx_packet);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return TRUE;
}

// Release a TX frame, and mark it as ready to send
bool atlRawsockTxFrameReady(void *pvRawsock, U8 *pBuffer, unsigned int len, U64 timeNsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	atl_rawsock_t *rawsock = (atl_rawsock_t*)pvRawsock;

	if (!VALID_TX_RAWSOCK(rawsock)) {
		AVB_LOG_ERROR("Send; invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
		return -1;
	}

	int err;

	rawsock->tx_packet->len = len;

#if ATL_LAUNCHTIME_ENABLED
	gptpmaster2local(&gPtpTD, timeNsec, &rawsock->tx_packet->attime);
#else
    rawsock->tx_packet->attime = 0;
#endif
	err = atl_xmit(rawsock->atl_dev, rawsock->queue, &rawsock->tx_packet);
	if (err) {
		AVB_LOGF_ERROR("atl_xmit failed: %s", strerror(err));
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return !err;
}
// Send all packets that are ready (i.e. tell kernel to send them)
int atlRawsockSend(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);

	// atlRawsock sends frames in atlRawsockTxFrameReady

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return 1;
}

int atlRawsockTxBufLevel(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK_DETAIL);
	atl_rawsock_t *rawsock = (atl_rawsock_t*)pvRawsock;

	int nInUse = atlTxBufLevel(rawsock->atl_dev);

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK_DETAIL);
	return nInUse;
}

unsigned long atlRawsockGetTXOutOfBuffers(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	unsigned long counter = 0;
	atl_rawsock_t *rawsock = (atl_rawsock_t*)pvRawsock;

	if(VALID_TX_RAWSOCK(rawsock)) {
		counter = rawsock->txOutOfBuffer;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return counter;
}

unsigned long atlRawsockGetTXOutOfBuffersCyclic(void *pvRawsock)
{
	AVB_TRACE_ENTRY(AVB_TRACE_RAWSOCK);
	unsigned long counter = 0;
	atl_rawsock_t *rawsock = (atl_rawsock_t*)pvRawsock;

	if(VALID_TX_RAWSOCK(rawsock)) {
		counter = rawsock->txOutOfBufferCyclic;
		rawsock->txOutOfBufferCyclic = 0;
	}

	AVB_TRACE_EXIT(AVB_TRACE_RAWSOCK);
	return counter;
}

