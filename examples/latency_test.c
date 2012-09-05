/******************************************************************************

  Copyright (c) 2001-2012, Intel Corporation 
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
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/user.h>

#include <sys/un.h>

#include <pci/pci.h>

#include "igb.h"

#define IGB_BIND_NAMESZ 24

device_t 	igb_dev;
volatile int	halt_tx;

void
sigint_handler (int signum)
{
	printf("halting ...\n");
	halt_tx = signum;
}

int
connect()
{
	struct	pci_access	*pacc;
	struct	pci_dev	*dev;
	int		err;
	char	devpath[IGB_BIND_NAMESZ];

	memset(&igb_dev, 0, sizeof(device_t));

	pacc	=	pci_alloc();
	pci_init(pacc);	
	pci_scan_bus(pacc);
	for	(dev=pacc->devices;	dev;	dev=dev->next)
	{
		pci_fill_info(dev,	PCI_FILL_IDENT	|	PCI_FILL_BASES	|	PCI_FILL_CLASS);

		igb_dev.pci_vendor_id = dev->vendor_id;
		igb_dev.pci_device_id = dev->device_id;
		igb_dev.domain = dev->domain;
		igb_dev.bus = dev->bus;
		igb_dev.dev = dev->dev;
		igb_dev.func = dev->func;

		snprintf(devpath, IGB_BIND_NAMESZ, "%04x:%02x:%02x.%d", \
			dev->domain, dev->bus, dev->dev, dev->func );
		printf("probing %s\n", devpath);

		err = igb_probe(&igb_dev);

		if (err) {
			continue;
		}

		printf ("attaching to %s\n", devpath);
		err = igb_attach(devpath, &igb_dev);

		if (err) {
			printf ("attach failed! (%s)\n", strerror(errno));
			continue;
		}
		printf("attach successful to %s\n", devpath);
		goto out;
	}

	pci_cleanup(pacc);
	return	ENXIO;

out:
	pci_cleanup(pacc);
	return	0;

}

#define VERSION_STR	"1.0"

static const char *version_str =
"latency test v" VERSION_STR "\n"
"Copyright (c) 2012, Intel Corporation\n";


static void
usage( void ) {
	fprintf(stderr, 
		"\n"
		"usage: latency_test [-hr] [-d <obs>]"
		"\n"
		"options:\n"
		"    -h  show this message\n"
		"    -d  observation window (seconds)\n"
		"    -p  delay from sw schedule to hw transmit in usec (defaults to 20 msec)\n"
		"\n"
		"%s"
		"\n", version_str);
	exit(1);
}

#define PACKET_IPG	(20000000) /* 20 msec */

int
main(int argc, char *argv[]) {
	int	c;
	unsigned	i;
	unsigned	tqavctl;
	int	err;
	struct igb_dma_alloc	a_page;
	struct igb_packet	a_packet;
	struct igb_packet	*tmp_packet;
	struct igb_packet	*cleaned_packets;
	struct igb_packet	*free_packets;
	u_int64_t	last_time;
	u_int64_t	rdtsc0;
	unsigned int	captured_latency;
	int		obs_window = 1;
	int		obs_window_count;
	int		ipg = PACKET_IPG / 1000;

	unsigned int		min = 0xFFFFFFFF;
	unsigned int		max = 0;
	float		avg = 0.0;

	for (;;) {
		c = getopt(argc, argv, "hrp:d:");

		if (c < 0)
			break;

		switch (c) {
		default:
		case 'h':
			usage();
			break;
		case 'p':
			sscanf(optarg, "%d", &ipg);
			break;
		case 'd':
			sscanf(optarg, "%d", &obs_window);
			break;
		}
	}
	if (optind < argc)
		usage();

	ipg *= 1000; /* scale to nsec */

	if (ipg > 400000000) { printf("specified delay is too large \n"); return(-1); }

	err = connect();

	if (err) { printf("connect failed (%s)\n", strerror(errno)); return(errno); }

	err = igb_init(&igb_dev);

	if (err) { printf("init failed (%s)\n", strerror(errno)); return(errno); }

	err = igb_dma_malloc_page(&igb_dev, &a_page);

	if (err) { printf("malloc failed (%s)\n", strerror(errno)); return(errno); }

#define PKT_SZ	200
	memset(&a_packet, 0, sizeof(a_packet));

	a_packet.dmatime = a_packet.attime = a_packet.flags = 0;
	a_packet.map.paddr = a_page.dma_paddr;
	a_packet.map.mmap_size = a_page.mmap_size;

	a_packet.offset = 0; 
	a_packet.vaddr = a_page.dma_vaddr + a_packet.offset;
	a_packet.len = PKT_SZ;
	
	free_packets = NULL; 
	
	/* divide the dma page into buffers for packets */
	for (i = 1; i < ((a_page.mmap_size) / PKT_SZ); i++) {
		tmp_packet = malloc(sizeof(struct igb_packet));
		if (NULL == tmp_packet)	{ printf("malloc failed (%s)\n", strerror(errno)); return(errno); }
		*tmp_packet = a_packet;
		tmp_packet->offset = (i * PKT_SZ);
		tmp_packet->vaddr += tmp_packet->offset;
		tmp_packet->next = free_packets;
		memset(tmp_packet->vaddr, 0xffffffff, PKT_SZ); /* MAC header at least */
		free_packets = tmp_packet;
	}

	igb_set_class_bandwidth(&igb_dev, 0, 0, 0); /* disable Qav */
	igb_readreg(&igb_dev, 0x3570, &tqavctl);
	tqavctl &= 0xFFFF; /* zero-out the prefetch delay */
	igb_writereg(&igb_dev, 0x3570, tqavctl); 

	halt_tx = 0;
	signal(SIGINT, sigint_handler);


	igb_get_wallclock(&igb_dev, &last_time, &rdtsc0);

	obs_window_count = obs_window;

	while (!halt_tx) {
		tmp_packet = free_packets;

		if (NULL == tmp_packet) goto cleanup;

		free_packets = tmp_packet->next;

		igb_get_wallclock(&igb_dev, &last_time, &rdtsc0);

		tmp_packet->attime = last_time + ipg;
		*(u_int64_t *)(tmp_packet->vaddr + 32) = tmp_packet->attime;

		err = igb_xmit(&igb_dev, 0, tmp_packet);

		if (ENOSPC == err) {
			/* put back for now */
			tmp_packet->next = free_packets;
			free_packets = tmp_packet;
		}

		sleep(1); /* sleep 1 sec */

cleanup:
		igb_clean(&igb_dev, &cleaned_packets);
		i = 0;
		while (cleaned_packets) {
			i++;
			tmp_packet = cleaned_packets;
			cleaned_packets = cleaned_packets->next;
			/* remap attime to compare to dma time */
			while (tmp_packet->attime > 999999999) tmp_packet->attime -= 1000000000;
			if ((tmp_packet->attime > 999000000) && (tmp_packet->dmatime < 1000000))
				captured_latency = (unsigned int)(1000000000 - tmp_packet->attime + tmp_packet->dmatime);
			else
				captured_latency = (unsigned int)(tmp_packet->dmatime - tmp_packet->attime);

			if (captured_latency < min) 
				min = captured_latency;

			if (captured_latency > max)
				max = captured_latency;

			avg += ((float)captured_latency / (float)obs_window);

			tmp_packet->next = free_packets;
			free_packets = tmp_packet;
		} 
		obs_window_count--;
		if (0 == obs_window_count) {
			obs_window_count = obs_window;
			printf("min = %d max = %d avg = %f\n", min, max, avg);
			min = 0xFFFFFFFF;
			max = 0;
			avg = 0.0;
		}
		
	}

	igb_dma_free_page(&igb_dev, &a_page);
	err = igb_detach(&igb_dev);
	return(0);	
}

