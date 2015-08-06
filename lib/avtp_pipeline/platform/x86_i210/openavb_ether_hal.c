/*************************************************************************************************************
Copyright (c) 2012-2013, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
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

#include <pci/pci.h>
#include "igb.h"
#include "avb.h"


#include "openavb_ether_hal.h"
#include "openavb_osal.h"

#define	AVB_LOG_COMPONENT	"HAL Ethernet"
#include "openavb_pub.h"
#include "openavb_log.h"
#include "openavb_trace.h"

#define MAX_MTU 1536

static pthread_mutex_t gHALTimeInitMutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK()  	pthread_mutex_lock(&gHALTimeInitMutex)
#define UNLOCK()	pthread_mutex_unlock(&gHALTimeInitMutex)

// TODO_OPENAVB : This should settable via function call or dynamic if per stream buffers are used.
#define MAX_PACKET_SIZE	100

static bool bInitialized = FALSE;
static device_t gIgbDev;

// Related to packet buffers. Naming borrowed form OpenAVB examples.
static size_t gMmaxPacketSize = 0;
static struct igb_dma_alloc a_page;
static struct igb_packet a_packet;
static struct igb_packet *tmp_packet;
static struct igb_packet *cleaned_packets;
static struct igb_packet *free_packets;

static bool x_HalEtherInit(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	int rslt;

	rslt = pci_connect(&gIgbDev);
	if (rslt) {
		AVB_LOGF_ERROR("connect failed (%s) - are you running as root?", strerror(errno));
		AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
		return FALSE;
	}

	rslt = igb_init(&gIgbDev);
	if (rslt) {
		AVB_LOGF_ERROR("init failed (%s) - is the driver really loaded?\n", strerror(errno));
		AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
		return FALSE;
	}

	// TODO_OPENAVB : This allocation of a single page of results in 4KB of memory. For a single stream and/small packet sizes
	//  this may be enough. However, this needs to be changed to a more flexible system either allocating a larger number of 
	//  pages that all streams can use or a separate page for each stream. In either case the control of packet memory allocation
	//  should handled via parameter either in this init function or a separate alloc function.
	rslt = igb_dma_malloc_page(&gIgbDev, &a_page);
	if (rslt) {
		AVB_LOGF_ERROR("malloc failed (%s) - out of memory?", strerror(errno));
		AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
		return FALSE;
	}

	{
		// Create list of igb_packets
		gMmaxPacketSize = MAX_PACKET_SIZE;

		a_packet.dmatime = 0;
		a_packet.attime = 0;
		a_packet.flags = 0;
		a_packet.map.paddr = a_page.dma_paddr;
		a_packet.map.mmap_size = a_page.mmap_size;
		a_packet.offset = 0;
		a_packet.vaddr = a_page.dma_vaddr + a_packet.offset;
		a_packet.len = gMmaxPacketSize;
		a_packet.next = NULL;
		free_packets = NULL;

		// Divide the DMA page into separate packets
		int i1;
		for (i1 = 1; i1 < ((a_page.mmap_size) / gMmaxPacketSize); i1++) {
			tmp_packet = malloc(sizeof(struct igb_packet));
			if (!tmp_packet) {
				AVB_LOG_ERROR("failed to allocate igb_packet memory!");
				AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
				return FALSE;
			}

			*tmp_packet = a_packet;
			tmp_packet->offset = (i1 * gMmaxPacketSize);
			tmp_packet->vaddr += tmp_packet->offset;
			tmp_packet->next = free_packets;

			memset(tmp_packet->vaddr, 0, gMmaxPacketSize);
			free_packets = tmp_packet;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return TRUE;
}


// Initialize HAL layer Ethernet driver
bool halEthernetInitialize(U8 *macAddr, bool gmacAutoNegotiate)
{
	LOCK();
	if (!bInitialized) {
		if (x_HalEtherInit())
			bInitialized = TRUE;
	}
	UNLOCK();

	return bInitialized;
}

bool halEthernetFinalize(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);

	// TODO_OPENAVB : shutdown igb

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return TRUE;
}

bool halEtherAddMulticast(U8 *multicastAddr, bool add)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return TRUE;	
}

U8 *halGetRxBufAVTP(U32 *frameSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return NULL;
}

// Get next Rx packet. NOTE: Expected to be called from a single task (socket task)
U8 *halGetRxBufAVB(U32 *frameSize, bool *bPtpCBUsed)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return NULL;
}

U8 *halGetRxBufGEN(U32 *frameSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return NULL;
}


bool halGetTxBuf(eth_frame_t *pEtherFrame)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	bool rslt = FALSE;

	LOCK();

	tmp_packet = free_packets;
	if (!tmp_packet) {
		igb_clean(&gIgbDev, &cleaned_packets);
		while (cleaned_packets) {
			tmp_packet = cleaned_packets;
			cleaned_packets = cleaned_packets->next;
			tmp_packet->next = free_packets;
			free_packets = tmp_packet;
		}
		tmp_packet = free_packets;
	}
	if (!tmp_packet) {
		pEtherFrame->pPvtData = NULL;
		pEtherFrame->length = 0;
		pEtherFrame->pBuffer = NULL;

		rslt = FALSE;
	}
	else {
		free_packets = tmp_packet->next;

		pEtherFrame->pPvtData = tmp_packet;
		pEtherFrame->length = tmp_packet->len;
		pEtherFrame->pBuffer = tmp_packet->vaddr + tmp_packet->offset;

		rslt = TRUE;
	}

	UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return rslt;
}

bool halRelTxBuf(eth_frame_t *pEtherFrame)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);

	if (!pEtherFrame->pPvtData) {
		AVB_LOG_ERROR("Invalid packet to release");
		AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
		return FALSE;
	}

	LOCK();

	tmp_packet = pEtherFrame->pPvtData;
	tmp_packet->next = free_packets;
	free_packets = tmp_packet;

	UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return TRUE;
}


bool halSendTxBuf(eth_frame_t *pEtherFrame)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	bool rslt = FALSE;

	if (!pEtherFrame->pPvtData) {
		AVB_LOG_ERROR("Invalid packet to send");
		AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
		return FALSE;
	}

	if (pEtherFrame->length > gMmaxPacketSize) {
		AVB_LOG_ERROR("Packet size too large");
		AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
		return FALSE;
	}

	LOCK();

	// TODO_OPENAVB : Select the queue (param 2) based on SR Class from pEtherFrame->fwmark
	tmp_packet = pEtherFrame->pPvtData;
	tmp_packet->len = pEtherFrame->length;

	// Setting packet launch time to now so that it will send ASAP. The talker is pacing the packets.
	// TODO_OPENAVB : An improvement may be to allow talker pacing to be driven by packet availability 
	//  and allow the I210 to pace via launch time.
	u_int64_t now;
	if (igb_get_wallclock(&gIgbDev, &now, NULL) > 0) {
		AVB_LOG_ERROR("Failed to get local time");
		AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
		return FALSE;
	}
	tmp_packet->attime = now;

	int err = igb_xmit(&gIgbDev, 0, pEtherFrame->pPvtData);
	if (!err) {
		rslt = TRUE;
	}
	else {
		if (err == ENOSPC) {
			// Place back on list.
			tmp_packet->next = free_packets;
			free_packets = tmp_packet;
		}
		rslt = FALSE;
	}

	UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return rslt;
}

// Is the link up. Returns TRUE if it is.
bool halIsLinkUp(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
    return FALSE;
}

// Returns the MTU
U32 halGetMTU(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return MAX_MTU;
}

U8 *halGetMacAddr(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
    return NULL;
}

bool halGetLocaltime(U64 *localTime64)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	if (igb_get_wallclock(&gIgbDev, localTime64, NULL ) > 0) {
		AVB_LOG_ERROR("Failed to get wallclock time");
		AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
		return FALSE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return TRUE;
}

void halTrafficShaperAddStream(int class, uint32_t streamsBytesPerSec)
{

// TODO_OPENAVB : it appears the igb_set_class_bandwidth() helper function is designed about the concept of a single Class A and single Class B 
// stream. Perhaps it could be used in the short term, however, a final solution may be to talk to the igb driver directly supply the E1000 reg
// values directly.
// 
//	igb_set_class_bandwidth(dev)

// TODO_OPENAVB
//	if (class == AVB_CLASS_B) {
// 	}
//	else if (class == AVB_CLASS_A) {
//	}
}

void halTrafficShaperRemoveStream(int class, uint32_t streamsBytesPerSec)
{

// TODO_OPENAVB : it appears the igb_set_class_bandwidth() helper function is designed about the concept of a single Class A and single Class B 
// stream. Perhaps it could be used in the short term, however, a final solution may be to talk to the igb driver directly supply the E1000 reg
// values directly.
// 
//	igb_set_class_bandwidth(dev)

// TODO_OPENAVB
//	if (class == AVB_CLASS_B) {
//  	}
//	else if (class == AVB_CLASS_A) {
//	}
}




