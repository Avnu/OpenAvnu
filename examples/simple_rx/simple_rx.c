/******************************************************************************

  Copyright (c) 2012-2016, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <arpa/inet.h>

#include <pci/pci.h>

#include "igb.h"

#define VERSION_STR "1.0"

#define IGB_BIND_NAMESZ (24)

#define PKT_SZ (1514)

/* Global variable for signal handler */
int *halt_rx_sig;

/* globals */

static const char *version_str = "simple_rx v" VERSION_STR "\n"
	"Copyright (c) 2012-2016, Intel Corporation\n";

unsigned char glob_station_addr[] = { 0, 0, 0, 0, 0, 0 };
unsigned char glob_stream_id[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
/* IEEE 1722 reserved address */
unsigned char glob_l2_dest_addr[] = { 0x91, 0xE0, 0xF0, 0x00, 0x0e, 0x80 };
unsigned char glob_l3_dest_addr[] = { 224, 0, 0, 115 };

void sigint_handler(int signum)
{
	printf("got SIGINT\n");
	*halt_rx_sig = signum;
}

int pci_connect(device_t *igb_dev)
{
	char devpath[IGB_BIND_NAMESZ];
	struct pci_access *pacc;
	struct pci_dev *dev;
	int err;

	memset(igb_dev, 0, sizeof(device_t));
	pacc = pci_alloc();
	pci_init(pacc);
	pci_scan_bus(pacc);
	for (dev = pacc->devices; dev; dev = dev->next) {
		pci_fill_info(dev,
			      PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);
		igb_dev->pci_vendor_id = dev->vendor_id;
		igb_dev->pci_device_id = dev->device_id;
		igb_dev->domain = dev->domain;
		igb_dev->bus = dev->bus;
		igb_dev->dev = dev->dev;
		igb_dev->func = dev->func;
		snprintf(devpath, IGB_BIND_NAMESZ, "%04x:%02x:%02x.%d",
			 dev->domain, dev->bus, dev->dev, dev->func);
		err = igb_probe(igb_dev);

		if (err)
			continue;

		printf("attaching to %s\n", devpath);
		err = igb_attach(devpath, igb_dev);
		if (err || igb_attach_tx(igb_dev)) {
			printf("attach failed! (%s)\n", strerror(errno));
			continue;
		}
		goto out;
	}
	pci_cleanup(pacc);
	return -ENXIO;
 out:	pci_cleanup(pacc);
	return 0;
}

void l3_to_l2_multicast(unsigned char *l2, unsigned char *l3)
{
	 l2[0]  = 0x1;
	 l2[1]  = 0x0;
	 l2[2]  = 0x5e;
	 l2[3]  = l3[1] & 0x7F;
	 l2[4]  = l3[2];
	 l2[5]  = l3[3];
}

int get_mac_address(char *interface)
{
	struct ifreq if_request;
	int lsock;
	int rc;

	lsock = socket(PF_PACKET, SOCK_RAW, htons(0x800));
	if (lsock < 0)
		return -1;

	memset(&if_request, 0, sizeof(if_request));
	strncpy(if_request.ifr_name,
		interface, sizeof(if_request.ifr_name) - 1);
	rc = ioctl(lsock, SIOCGIFHWADDR, &if_request);
	if (rc < 0) {
		close(lsock);
		return -1;
	}

	memcpy(glob_station_addr, if_request.ifr_hwaddr.sa_data,
	       sizeof(glob_station_addr));
	close(lsock);
	return 0;
}

static void usage(void)
{
	fprintf(stderr, "\n"
		"usage: simple_rx [-h] -i interface-name\n"
		"options:\n"
		"    -h  show this message\n"
		"    -i  specify interface for AVB connection\n"
		"    -t  transport equal to 2 for 1722 or 3 for RTP\n"
		"\n%s\n", version_str);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	/* int igb_shm_fd = -1; */
	struct igb_dma_alloc a_page;
	struct igb_packet *tmp_packet;
	struct igb_packet *received_packets;
	struct igb_packet *free_packets;
	char *interface = NULL;
	u_int8_t filter_mask[64];
	device_t igb_dev;
	unsigned i;
	int c, rc = 0;
	int err;

	for (;;) {
		c = getopt(argc, argv, "hi:t:");
		if (c < 0)
			break;
		switch (c) {
		case 'h':
			usage();
			break;
		case 'i':
			if (interface) {
				printf("only one interface per daemon is supported\n");
				usage();
			}
			interface = strdup(optarg);
			break;
		}
	}

	if (optind < argc)
		usage();

	if (interface == NULL)
		usage();

	err = pci_connect(&igb_dev);
	if (err) {
		printf("connect failed (%s) - are you running as root?\n",
		       strerror(errno));
		return errno;
	}
	err = igb_init(&igb_dev);
	if (err) {
		printf("init failed (%s) - is the driver really loaded?\n",
		       strerror(errno));
		return errno;
	}
	/*
	 * allocate a bunch of pages and
	 * stitch into a linked list of igb_packets ...
	 */

	free_packets = NULL;

	for (i = 0; i < 128; i++) {
		tmp_packet = malloc(sizeof(struct igb_packet));
		if (tmp_packet == NULL) {
			printf("failed to allocate igb_packet memory!\n");
			return errno;
		}
		err = igb_dma_malloc_page(&igb_dev, &a_page);
		if (err) {
			printf("malloc failed (%s) - out of memory?\n",
			       strerror(errno));
			return errno;
		}
		tmp_packet->dmatime = 0;
		tmp_packet->attime = 0;
		tmp_packet->flags = 0;
		tmp_packet->map.paddr = a_page.dma_paddr;
		tmp_packet->map.mmap_size = a_page.mmap_size;
		tmp_packet->offset = 0;
		tmp_packet->vaddr = a_page.dma_vaddr + tmp_packet->offset;
		tmp_packet->len = PKT_SZ;
		tmp_packet->next = NULL;

		tmp_packet->next = free_packets;
		free_packets = tmp_packet;
	}

	igb_refresh_buffers(&igb_dev, 0, &free_packets, 128);

	filter_mask[0] = 0x3F; /* 1st 6 bytes of destination MAC address */
	igb_setup_flex_filter(&igb_dev, 0, 0, 6,
			      glob_l2_dest_addr, filter_mask);

	signal(SIGINT, sigint_handler);
	rc = get_mac_address(interface);
	if (rc) {
		printf("failed to open interface\n");
		usage();
	}

	memset(glob_stream_id, 0, sizeof(glob_stream_id));
	memcpy(glob_stream_id, glob_station_addr, sizeof(glob_station_addr));

	*halt_rx_sig = 0;

	rc = nice(-20);

	while (*halt_rx_sig) {
		received_packets = NULL;
		igb_receive(&igb_dev, 0, &received_packets, 1);

		if (received_packets == NULL)
			continue;

		/* print out the received packet? what do do with it? */
		printf("received a packet !\n");
		/* put back for now */
		igb_refresh_buffers(&igb_dev, 0, &received_packets, 1);

	}
	rc = nice(0);

	*halt_rx_sig = 1;

	igb_clear_flex_filter(&igb_dev, 0);
	igb_dma_free_page(&igb_dev, &a_page);
	err = igb_detach(&igb_dev);

	return EXIT_SUCCESS;
}
