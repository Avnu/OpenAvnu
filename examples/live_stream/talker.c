 /*
  * Copyright (c) <2013>, Intel Corporation.
  *
  * This program is free software; you can redistribute it and/or modify it
  * under the terms and conditions of the GNU Lesser General Public License,
  * version 2.1, as published by the Free Software Foundation.
  *
  * This program is distributed in the hope it will be useful, but WITHOUT
  * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
  * more details.
  *
  * You should have received a copy of the GNU Lesser General Public License along with
  * this program; if not, write to the Free Software Foundation, Inc.,
  * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  *
  */

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if.h>
#include <signal.h>
#include <sys/ioctl.h>

#include <pci/pci.h>

#include "avb.h"
#include "talker_mrp_client.h"

#define USE_MRPD 1

#define STREAMID (0xABCDEF)
#define PACKET_IPG (125000) /* 1 packet every 125 usec */

/* globals */

uint32_t glob_payload_length;
unsigned char glob_station_addr[] = { 0, 0, 0, 0, 0, 0 };
unsigned char glob_stream_id[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
/* IEEE 1722 reserved address */
unsigned char glob_dest_addr[] = { 0x91, 0xE0, 0xF0, 0x00, 0x0E, 0x80 };

uint64_t reverse_64(uint64_t val)
{
	uint32_t low, high;

	low = val & 0xffffffff;
	high = (val >> 32) & 0xffffffff;
	low = htonl(low);
	high = htonl(high);

	val = 0;
	val = val | low;
	val = (val << 32) | high;

	return val;
}

void sigint_handler(int signum)
{
	fprintf(stderr, "got SIGINT\n");
	halt_tx = signum;
}

int get_mac_addr(int8_t *iface)
{
	int lsock = socket(PF_PACKET, SOCK_RAW, htons(0x800));
	struct ifreq if_request;
	int rc;

	if (lsock < 0)
		return -1;

	memset(&if_request, 0, sizeof(if_request));
	strncpy(if_request.ifr_name, (const char *)iface, sizeof(if_request.ifr_name));

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

int main(int argc, char *argv[])
{
	device_t igb_dev;
	struct igb_dma_alloc a_page;
	struct igb_packet a_packet;
	struct igb_packet *tmp_packet;
	struct igb_packet *cleaned_packets;
	struct igb_packet *free_packets;
	six1883_header *h61883;
	seventeen22_header *h1722;
	unsigned i;
	int err, seq_number, frame_size;
	int8_t *iface = NULL;
	unsigned samples_count = 0;
	uint32_t packet_size;
	int32_t read_bytes;
	uint8_t *data_ptr;
	void *stream_packet;
	long long int frame_sequence = 0;
	struct sched_param sched;

	if (argc < 2) {
		fprintf(stderr,"%s <if_name> <payload>\n", argv[0]);
		return EXIT_FAILURE;
	}

	iface = (int8_t *)strdup(argv[1]);
	packet_size = atoi(argv[2]);;
	glob_payload_length = atoi(argv[2]);;
	packet_size += sizeof(six1883_header) + sizeof(seventeen22_header) + sizeof(eth_header);

#ifdef USE_MRPD
	err = mrp_connect();
	if (err) {
		fprintf(stderr, "socket creation failed\n");
		return errno;
	}
#endif

	err = pci_connect(&igb_dev);
	if (err) {
		fprintf(stderr, "connect failed (%s) - are you running as root?\n", strerror(errno));
		return errno;
	}

	err = igb_init(&igb_dev);
	if (err) {
		fprintf(stderr, "init failed (%s) - is the driver really loaded?\n", strerror(errno));
		return errno;
	}

	err = igb_dma_malloc_page(&igb_dev, &a_page);
	if (err) {
		fprintf(stderr, "malloc failed (%s) - out of memory?\n", strerror(errno));
		return errno;
	}

	signal(SIGINT, sigint_handler);

	err = get_mac_addr(iface);
	if (err) {
		fprintf(stderr, "failed to open iface(%s)\n",iface);
		return EXIT_FAILURE;
	}

#ifdef USE_MRPD
	err = mrp_monitor();
	if (err) {
		printf("failed creating MRP monitor thread\n");
		return EXIT_FAILURE;
	}

	domain_a_valid = 1;
	domain_class_a_id = MSRP_SR_CLASS_A;
	domain_class_a_priority = MSRP_SR_CLASS_A_PRIO;
	domain_class_a_vid = 2;
	fprintf(stderr, "detected domain Class A PRIO=%d VID=%04x...\n", domain_class_a_priority,
	       domain_class_a_vid);

	err = mrp_register_domain(&domain_class_a_id, &domain_class_a_priority, &domain_class_a_vid);
	if (err) {
		printf("mrp_register_domain failed\n");
		return EXIT_FAILURE;
	}

	domain_a_valid = 1;
	domain_class_a_vid = 2;
	fprintf(stderr, "detected domain Class A PRIO=%d VID=%04x...\n", domain_class_a_priority, domain_class_a_vid);
#endif

	igb_set_class_bandwidth(&igb_dev, PACKET_IPG / 125000, 0, packet_size - 22, 0);

	memset(glob_stream_id, 0, sizeof(glob_stream_id));
	memcpy(glob_stream_id, glob_station_addr, sizeof(glob_station_addr));

	a_packet.dmatime = a_packet.attime = a_packet.flags = 0;
	a_packet.map.paddr = a_page.dma_paddr;
	a_packet.map.mmap_size = a_page.mmap_size;
	a_packet.offset = 0;
	a_packet.vaddr = a_page.dma_vaddr + a_packet.offset;
	a_packet.len = packet_size;
	free_packets = NULL;
	seq_number = 0;

	frame_size = glob_payload_length + sizeof(six1883_header) + sizeof(seventeen22_header) + sizeof(eth_header);

	stream_packet = avb_create_packet(glob_payload_length);

	h1722 = (seventeen22_header *)((uint8_t*)stream_packet + sizeof(eth_header));
	h61883 = (six1883_header *)((uint8_t*)stream_packet + sizeof(eth_header) +
					sizeof(seventeen22_header));

	/*initalize h1722 header */
	avb_initialize_h1722_to_defaults(h1722);
	/* set the length */
	avb_set_1722_length(h1722, htons(glob_payload_length + sizeof(six1883_header)));
	avb_set_1722_stream_id(h1722,reverse_64(STREAMID));
	avb_set_1722_sid_valid(h1722, 0x1);

	
	/*initalize h61883 header */
	avb_initialize_61883_to_defaults(h61883);
	avb_set_61883_format_tag(h61883, 0x1);
	avb_set_61883_packet_channel(h61883, 0x1f);
	avb_set_61883_packet_tcode(h61883, 0xa);
	avb_set_61883_source_id(h61883 , 0x3f);
	avb_set_61883_data_block_size(h61883, 0x1);
	avb_set_61883_eoh(h61883, 0x2);
	avb_set_61883_format_id(h61883, 0x10);
	avb_set_61883_format_dependent_field(h61883, 0x2);
	avb_set_61883_syt(h61883, 0xffff);

	/* initilaze the source & destination mac address */
	avb_eth_header_set_mac(stream_packet, glob_dest_addr, iface);

	/* set 1772 eth type */
	avb_1722_set_eth_type(stream_packet);

	/* divide the dma page into buffers for packets */
	for (i = 1; i < ((a_page.mmap_size) / packet_size); i++) {
		tmp_packet = malloc(sizeof(struct igb_packet));
		if (NULL == tmp_packet) {
			fprintf(stderr, "failed to allocate igb_packet memory!\n");
			return errno;
		}
		*tmp_packet = a_packet;
		tmp_packet->offset = (i * packet_size);
		tmp_packet->vaddr += tmp_packet->offset;
		tmp_packet->next = free_packets;
		memset(tmp_packet->vaddr, 0, packet_size);	/* MAC header at least */
		memcpy(((char *)tmp_packet->vaddr), stream_packet, frame_size);
		tmp_packet->len = frame_size;
		free_packets = tmp_packet;
	}

#ifdef USE_MRPD
	/* 
	 * subtract 16 bytes for the MAC header/Q-tag - pktsz is limited to the 
	 * data payload of the ethernet frame .
	 *
	 * IPG is scaled to the Class (A) observation interval of packets per 125 usec
	 */
	fprintf(stderr, "advertising stream ...\n");
	err = mrp_advertise_stream(glob_stream_id, glob_dest_addr, domain_class_a_vid, packet_size - 16,
				PACKET_IPG / 125000, domain_class_a_priority, 3900);
	if (err) {
		printf("mrp_advertise_stream failed\n");
		return EXIT_FAILURE;
	}

	fprintf(stderr, "awaiting a listener ...\n");
	err = mrp_await_listener(glob_stream_id);
	if (err) {
		printf("mrp_await_listener failed\n");
		return EXIT_FAILURE;
	}

#endif

	memset(&sched, 0 , sizeof (sched));
	sched.sched_priority = 1;
	sched_setscheduler(0, SCHED_RR, &sched);

	while (listeners && !halt_tx)
	{
		tmp_packet = free_packets;
		if (NULL == tmp_packet)
			goto cleanup;

		stream_packet = ((char *)tmp_packet->vaddr);
		free_packets = tmp_packet->next;

		/* unfortuntely unless this thread is at rtprio
		 * you get pre-empted between fetching the time
		 * and programming the packet and get a late packet
		 */
		h1722 = (seventeen22_header *)((uint8_t*)stream_packet + sizeof(eth_header));
		avb_set_1722_seq_number(h1722, seq_number++);
		if (seq_number % 4 == 0)
			avb_set_1722_timestamp_valid(h1722, 0);
		else
			avb_set_1722_timestamp_valid(h1722, 1);

		data_ptr = (uint8_t *)((uint8_t*)stream_packet + sizeof(eth_header) + sizeof(seventeen22_header) 
					+ sizeof(six1883_header));
		
		read_bytes = read(0, (void *)data_ptr, glob_payload_length);
		/* Error case while reading the input file */
		if (read_bytes < 0) {
			fprintf(stderr,"Failed to read from STDIN %s\n", argv[2]);
			continue;
		}
		samples_count += read_bytes;
		h61883 = (six1883_header *)((uint8_t*)stream_packet + sizeof(eth_header) + sizeof(seventeen22_header));
		avb_set_61883_data_block_continuity(h61883 , samples_count);

		err = igb_xmit(&igb_dev, 0, tmp_packet);
		if (!err) {
			fprintf(stderr,"frame sequence = %lld\n", frame_sequence++);
			continue;
		} else {
			fprintf(stderr,"Failed frame sequence = %lld !!!!\n", frame_sequence++);
		}

		if (ENOSPC == err) {
			/* put back for now */
			tmp_packet->next = free_packets;
			free_packets = tmp_packet;
		}
cleanup:
		igb_clean(&igb_dev, &cleaned_packets);
		while (cleaned_packets) {
			tmp_packet = cleaned_packets;
			cleaned_packets = cleaned_packets->next;
			tmp_packet->next = free_packets;
			free_packets = tmp_packet;
		}
	}

	if (halt_tx == 0)
		fprintf(stderr, "listener left ...\n");

	halt_tx = 1;
	sleep(1);
#ifdef USE_MRPD
	err = mrp_unadvertise_stream(glob_stream_id, glob_dest_addr, domain_class_a_vid, packet_size - 16,
				PACKET_IPG / 125000, domain_class_a_priority, 3900);
	if (err)
		printf("mrp_unadvertise_stream failed\n");
#endif
	/* disable Qav */
	igb_set_class_bandwidth(&igb_dev, 0, 0, 0, 0);
#ifdef USE_MRPD
	err = mrp_disconnect();
	if (err)
		printf("mrp_disconnect failed\n");
#endif
	igb_dma_free_page(&igb_dev, &a_page);

	err = igb_detach(&igb_dev);

	pthread_exit(NULL);

	return EXIT_SUCCESS;
}
