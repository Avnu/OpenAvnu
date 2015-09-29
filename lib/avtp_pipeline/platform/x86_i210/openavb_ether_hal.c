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

#include "openavb_ether_hal.h"
#include "openavb_osal.h"
#include "avb.h"
#include "igb.h"

#define	AVB_LOG_COMPONENT	"HAL Ethernet"
#include "openavb_pub.h"
#include "openavb_log.h"
#include "openavb_trace.h"


static pthread_mutex_t gIgbDeviceMutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK()  	pthread_mutex_lock(&gIgbDeviceMutex)
#define UNLOCK()	pthread_mutex_unlock(&gIgbDeviceMutex)

static struct igb_dma_alloc g_pages[IGB_PAGES];
static struct igb_packet *g_free_packets;

static device_t *igb_dev = NULL;
static int igb_dev_users = 0; // time uses it

static int g_totalBuffers = 0;
static int g_usedBuffers = -1;

static int count_packets(struct igb_packet *packet)
{
	int count=0;
	while (packet) {
		count++;
		packet = packet->next;
	}
	return count;
}

static struct igb_packet* alloc_page(device_t* dev, struct igb_dma_alloc *a_page)
{
	int err = igb_dma_malloc_page(dev, a_page);
	if (err) {
		AVB_LOGF_ERROR("igb_dma_malloc_page failed: %s", strerror(err));
		return NULL;
	}

	struct igb_packet *free_packets;
	struct igb_packet a_packet;

	a_packet.dmatime = a_packet.attime = a_packet.flags = 0;
	a_packet.map.paddr = a_page->dma_paddr;
	a_packet.map.mmap_size = a_page->mmap_size;
	a_packet.offset = 0;
	a_packet.vaddr = a_page->dma_vaddr + a_packet.offset;
	a_packet.len = IGB_MTU;
	a_packet.next = NULL;

	free_packets = NULL;

	/* divide the dma page into buffers for packets */
	int i;
	for (i = 0; i < a_page->mmap_size / IGB_MTU; i++) {
		struct igb_packet *tmp_packet = malloc(sizeof(struct igb_packet));
		if (!tmp_packet) {
			AVB_LOG_ERROR("failed to allocate igb_packet memory!");
			return false;
		}
		*tmp_packet = a_packet;
		tmp_packet->offset = (i * IGB_MTU);
		tmp_packet->vaddr += tmp_packet->offset;
		tmp_packet->next = free_packets;
		memset(tmp_packet->vaddr, 0, IGB_MTU);
		free_packets = tmp_packet;
	}
	return free_packets;
}

device_t *igbAcquireDevice()
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);

	LOCK();
	if (!igb_dev) {
		device_t *tmp_dev = malloc(sizeof(device_t));
		if (!tmp_dev) {
			AVB_LOGF_ERROR("Cannot allocate memory for device: %s", strerror(errno));
			goto unlock;
		}

		int err = pci_connect(tmp_dev);
		if (err) {
			AVB_LOGF_ERROR("connect failed (%s) - are you running as root?", strerror(err));
			goto unlock;
		}

		err = igb_init(tmp_dev);
		if (err) {
			AVB_LOGF_ERROR("init failed (%s) - is the driver really loaded?", strerror(err));
			igb_detach(tmp_dev);
			goto unlock;
		}


		int i;
		for (i = 0; i < IGB_PAGES; i++) {
			struct igb_packet* free_packets = alloc_page(tmp_dev, &g_pages[i]);
			if (!g_free_packets) {
				g_free_packets = free_packets;
			} else {
				struct igb_packet* last_packet = g_free_packets;
				while (last_packet->next) {
					last_packet = last_packet->next;
				}
				last_packet->next = free_packets;
			}
		}

		g_totalBuffers = count_packets(g_free_packets);

		AVB_LOGF_INFO("TX buffers: %d", g_totalBuffers);

		igbControlLaunchTime(tmp_dev, IGB_LAUNCHTIME_ENABLED);

		AVB_LOGF_INFO("IGB launch time feature is %s", IGB_LAUNCHTIME_ENABLED ? "ENABLED" : "DISABLED");

		igb_dev = tmp_dev;
unlock:
		if (!igb_dev)
			free(tmp_dev);
	}

	if (igb_dev) {
		igb_dev_users += 1;
		AVB_LOGF_DEBUG("igb_dev_users %d", igb_dev_users);
	}
	UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return igb_dev;
}

void igbReleaseDevice(device_t* dev)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);

	LOCK();

	igb_dev_users -= 1;
	AVB_LOGF_DEBUG("igb_dev_users %d", igb_dev_users);

	if (igb_dev && igb_dev_users <= 0) {
		int i;
		for (i = 0; i < IGB_PAGES; i++)
			igb_dma_free_page(igb_dev, &g_pages[i]);

		igb_detach(igb_dev);
		free(igb_dev);
		igb_dev = NULL;
	}

	UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
}

struct igb_packet *igbGetTxPacket(device_t* dev)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);

	LOCK();

	struct igb_packet* tx_packet = g_free_packets;

	if (!tx_packet) {
		struct igb_packet *cleaned_packets;
		igb_clean(dev, &cleaned_packets);
		while (cleaned_packets) {
			struct igb_packet *tmp_packet = cleaned_packets;
			cleaned_packets = cleaned_packets->next;
			tmp_packet->next = g_free_packets;
			g_free_packets = tmp_packet;
		}
		tx_packet = g_free_packets;

		g_usedBuffers = g_totalBuffers - count_packets(g_free_packets);
	}

	if (tx_packet) {
		g_free_packets = tx_packet->next;
	}

	UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return tx_packet;
}

void igbRelTxPacket(device_t* dev, int queue, struct igb_packet *tx_packet)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);

	LOCK();

	tx_packet->next = g_free_packets;
	g_free_packets = tx_packet;

	UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
}

int igbTxBufLevel(device_t *dev)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return g_usedBuffers;
}

bool igbGetMacAddr(U8 mac_addr[ETH_ALEN])
{
	int err = igb_get_mac_addr(igb_dev, mac_addr);
	if (err) {
		AVB_LOGF_ERROR("igb_get_mac_addr() failed: %s", strerror(err));
	}
	return !err;
}

bool igbControlLaunchTime(device_t *dev, int enable)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);

	const uint32_t E1000_TQAVCTRL = 0x03570;
	const uint32_t E1000_TQAVCTRL_LAUNCH_VALID = 0x00000200;

	int err;
	uint32_t regVal;

	err = igb_lock(dev);
	if (err)
		goto error;

	igb_readreg(dev, E1000_TQAVCTRL, &regVal);

	if (enable)
		regVal |= E1000_TQAVCTRL_LAUNCH_VALID;
	else
		regVal &= ~E1000_TQAVCTRL_LAUNCH_VALID;

	igb_writereg(dev, E1000_TQAVCTRL, regVal);

	err = igb_unlock(dev);

error:
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return !err;
}

