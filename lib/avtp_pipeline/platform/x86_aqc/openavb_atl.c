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

#include "openavb_atl.h"
#include "openavb_osal.h"
#include "avb_atl.h"
#include "atl.h"

#define	AVB_LOG_COMPONENT	"HAL Ethernet"
#include "openavb_pub.h"
#include "openavb_log.h"
#include "openavb_trace.h"


static pthread_mutex_t gAtlDeviceMutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK()  	pthread_mutex_lock(&gAtlDeviceMutex)
#define UNLOCK()	pthread_mutex_unlock(&gAtlDeviceMutex)

static struct atl_dma_alloc g_pages[ATL_PAGES];
static struct atl_packet *g_free_packets;

static device_t *atl_dev = NULL;
static int atl_dev_users = 0; // time uses it

static int g_totalBuffers = 0;
static int g_usedBuffers = -1;

static int count_packets(struct atl_packet *packet)
{
	int count=0;
	while (packet) {
		count++;
		packet = packet->next;
	}
	return count;
}

static struct atl_packet* alloc_page(device_t* dev, struct atl_dma_alloc *a_page)
{
	int err = atl_dma_malloc_page(dev, a_page);
	if (err) {
		AVB_LOGF_ERROR("atl_dma_malloc_page failed: %s", strerror(err));
		return NULL;
	}

	struct atl_packet *free_packets;
	struct atl_packet a_packet;

	a_packet.attime = a_packet.flags = 0;
	a_packet.map.paddr = a_page->dma_paddr;
	a_packet.map.mmap_size = a_page->mmap_size;
	a_packet.offset = 0;
	a_packet.vaddr = a_page->dma_vaddr + a_packet.offset;
	a_packet.len = ATL_MTU;
	a_packet.next = NULL;

	free_packets = NULL;

	/* divide the dma page into buffers for packets */
	int i;
	for (i = 0; i < a_page->mmap_size / ATL_MTU; i++) {
		struct atl_packet *tmp_packet = malloc(sizeof(struct atl_packet));
		if (!tmp_packet) {
			AVB_LOG_ERROR("failed to allocate atl_packet memory!");
			return false;
		}
		*tmp_packet = a_packet;
		tmp_packet->offset = (i * ATL_MTU);
		tmp_packet->vaddr += tmp_packet->offset;
		tmp_packet->next = free_packets;
		memset(tmp_packet->vaddr, 0, ATL_MTU);
		free_packets = tmp_packet;
	}
	return free_packets;
}

device_t *atlAcquireDevice(const char *ifname)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);

	LOCK();
	if (!atl_dev) {
		device_t *tmp_dev = calloc(1, sizeof(device_t));
		const char *complex_ifname = NULL;
		if (!tmp_dev) {
			AVB_LOGF_ERROR("Cannot allocate memory for device: %s", strerror(errno));
			goto unlock;
		}

		complex_ifname = strchr(ifname, 0x3a);
		if( complex_ifname != NULL && strlen(complex_ifname) > 1 ) {
			tmp_dev->ifname = strdup(&complex_ifname[1]);
		} else {
			tmp_dev->ifname = strdup(ifname);
		}

#ifdef AVB_LOG_ON
		atl_init_avb_log(&__avbLogFn, "AQ_AVTP");
#endif // AVB_LOG_ON

		int err = pci_connect(tmp_dev);
		if (err) {
			AVB_LOGF_ERROR("connect failed (%s) - are you running as root?", strerror(err));
			goto unlock;
		}

		err = atl_init(tmp_dev);
		if (err) {
			AVB_LOGF_ERROR("init failed (%s) - is the driver really loaded?", strerror(err));
			atl_detach(tmp_dev);
			goto unlock;
		}


		int i;
		for (i = 0; i < ATL_PAGES; i++) {
			struct atl_packet* free_packets = alloc_page(tmp_dev, &g_pages[i]);
			if (!g_free_packets) {
				g_free_packets = free_packets;
			} else {
				struct atl_packet* last_packet = g_free_packets;
				while (last_packet->next) {
					last_packet = last_packet->next;
				}
				last_packet->next = free_packets;
			}
		}

		g_totalBuffers = count_packets(g_free_packets);

		AVB_LOGF_INFO("TX buffers: %d", g_totalBuffers);

		AVB_LOGF_INFO("ATL launch time feature is %s", ATL_LAUNCHTIME_ENABLED ? "ENABLED" : "DISABLED");

		atl_dev = tmp_dev;
unlock:
		if (!atl_dev)
			free(tmp_dev);
	}

	if (atl_dev) {
		atl_dev_users += 1;
		AVB_LOGF_DEBUG("atl_dev_users %d", atl_dev_users);
	}
	UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return atl_dev;
}

void atlReleaseDevice(device_t* dev)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);

    struct atl_packet *cleaned_packets;
    struct atl_packet *tmp_packet;

	LOCK();

	atl_dev_users -= 1;
	AVB_LOGF_DEBUG("atl_dev_users %d", atl_dev_users);

	if (atl_dev && atl_dev_users <= 0) {
		int i;

		atl_stop_tx(dev, 0, &cleaned_packets);

		tmp_packet = cleaned_packets;

		while( tmp_packet ) {
			cleaned_packets = tmp_packet->next;
			if( tmp_packet->extra ) {
				free(tmp_packet->extra);
			}
			free(tmp_packet);
			tmp_packet = cleaned_packets;
		}

		tmp_packet = g_free_packets;
		while( tmp_packet ) {
			g_free_packets = tmp_packet->next;
			if( tmp_packet->extra ) {
				free(tmp_packet->extra);
			}
			free(tmp_packet);
			tmp_packet = g_free_packets;
		}

		for (i = 0; i < ATL_PAGES; i++)
			atl_dma_free_page(atl_dev, &g_pages[i]);

		atl_detach(atl_dev);
		free(atl_dev);
		atl_dev = NULL;
		g_free_packets = NULL;
	}

	UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
}

struct atl_packet *atlGetTxPacket(device_t* dev)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);

	LOCK();

	struct atl_packet* tx_packet = g_free_packets;
	if (!tx_packet && atl_dev) {
		struct atl_packet *cleaned_packets;
		atl_clean(dev, &cleaned_packets);
		while (cleaned_packets) {
			struct atl_packet *tmp_packet = cleaned_packets;
			cleaned_packets = cleaned_packets->next;
			tmp_packet->next = g_free_packets;
			g_free_packets = tmp_packet;
		}
		tx_packet = g_free_packets;

		g_usedBuffers = g_totalBuffers - count_packets(g_free_packets);
	}

	if (tx_packet) {
		g_free_packets = tx_packet->next;
		tx_packet->next = NULL;
	}
	UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return tx_packet;
}

void atlRelTxPacket(device_t* dev, int queue, struct atl_packet *tx_packet)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);

	LOCK();
	tx_packet->next = g_free_packets;
	g_free_packets = tx_packet;

	UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
}

int atlTxBufLevel(device_t *dev)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return g_usedBuffers;
}
