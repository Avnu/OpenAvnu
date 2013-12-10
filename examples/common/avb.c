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

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <linux/if.h>

#include <netinet/in.h>

#include <pci/pci.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "avb.h"

int pci_connect(device_t * igb_dev)
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
		if (err) {
			printf("attach failed! (%s)\n", strerror(errno));
			continue;
		}
		goto out;
	}

	pci_cleanup(pacc);
	return ENXIO;

out:	pci_cleanup(pacc);

	return 0;
}

int gptpinit(int *shm_fd, char *memory_offset_buffer)
{
	*shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
	if (*shm_fd == -1) {
		perror("shm_open()");
		return false;
	}
	memory_offset_buffer =
	    (char *)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
			 *shm_fd, 0);
	if (memory_offset_buffer == (char *)-1) {
		perror("mmap()");
		memory_offset_buffer = NULL;
		shm_unlink(SHM_NAME);
		return false;
	}
	return true;
}

void gptpdeinit(int shm_fd, char *memory_offset_buffer)
{
	if (memory_offset_buffer != NULL) {
		munmap(memory_offset_buffer, SHM_SIZE);
	}
	if (shm_fd != -1) {
		close(shm_fd);
	}
}

int gptpscaling(gPtpTimeData * td, char *memory_offset_buffer)
{
	pthread_mutex_lock((pthread_mutex_t *) memory_offset_buffer);
	memcpy(td, memory_offset_buffer + sizeof(pthread_mutex_t), sizeof(*td));
	pthread_mutex_unlock((pthread_mutex_t *) memory_offset_buffer);

	fprintf(stderr, "ml_phoffset = %lld, ls_phoffset = %lld\n",
		td->ml_phoffset, td->ls_phoffset);
	fprintf(stderr, "ml_freqffset = %d, ls_freqoffset = %d\n",
		td->ml_freqoffset, td->ls_freqoffset);

	return true;
}

/* setters & getters for seventeen22_header */
void avb_set_1722_cd_indicator(seventeen22_header *h1722, uint64_t cd_indicator)
{
	h1722->cd_indicator = cd_indicator;
}

uint64_t avb_get_1722_cd_indicator(seventeen22_header *h1722)
{
	return h1722->cd_indicator;
}

void avb_set_1722_subtype(seventeen22_header *h1722, uint64_t subtype)
{
	h1722->subtype = subtype;
}

uint64_t avb_get_1722_subtype(seventeen22_header *h1722)
{
	return h1722->subtype;
}

void avb_set_1722_sid_valid(seventeen22_header *h1722, uint64_t sid_valid)
{
	h1722->sid_valid = sid_valid;
}

uint64_t avb_get_1722_sid_valid(seventeen22_header *h1722)
{
	return h1722->sid_valid;
}

void avb_set_1722_version(seventeen22_header *h1722, uint64_t version)
{
	h1722->version = version;
}

uint64_t avb_get_1722_version(seventeen22_header *h1722)
{
	return h1722->version;
}

void avb_set_1722_reset(seventeen22_header *h1722, uint64_t reset)
{
	h1722->reset = reset;
}

uint64_t avb_get_1722_reset(seventeen22_header *h1722)
{
	return h1722->reset;
}

void avb_set_1722_reserved0(seventeen22_header *h1722, uint64_t reserved0)
{
	h1722->reserved0 = reserved0;
}

uint64_t avb_get_1722_reserved0(seventeen22_header *h1722)
{
	return h1722->reserved0;
}

void avb_set_1722_gateway_valid(seventeen22_header *h1722, uint64_t gateway_valid)
{
	h1722->gateway_valid = gateway_valid;
}

uint64_t avb_get_1722_gateway_valid(seventeen22_header *h1722)
{
	return h1722->gateway_valid;
}

void avb_set_1722_timestamp_valid(seventeen22_header *h1722, uint64_t timestamp_valid)
{
	h1722->timestamp_valid = timestamp_valid;
}

uint64_t avb_get_1722_timestamp_valid(seventeen22_header *h1722)
{
	return h1722->timestamp_valid;
}

void avb_set_1722_reserved1(seventeen22_header *h1722, uint64_t reserved1)
{
	h1722->reserved1 = reserved1;
}

uint64_t avb_get_1722_reserved1(seventeen22_header *h1722)
{
	return h1722->reserved1;
}

void avb_set_1722_stream_id(seventeen22_header *h1722, uint64_t stream_id)
{
	h1722->stream_id = stream_id;
}

uint64_t avb_get_1722_stream_id(seventeen22_header *h1722)
{
	return h1722->stream_id;
}

void avb_set_1722_seq_number(seventeen22_header *h1722, uint64_t seq_number)
{
	h1722->seq_number = seq_number;
}

uint64_t avb_get_1722_seq_number(seventeen22_header *h1722)
{
	return h1722->seq_number;
}

void avb_set_1722_timestamp_uncertain(seventeen22_header *h1722, uint64_t timestamp_uncertain)
{
	h1722->timestamp_uncertain = timestamp_uncertain;
}

uint64_t avb_get_1722_timestamp_uncertain(seventeen22_header *h1722)
{
	return h1722->timestamp_uncertain;
}

void avb_set_1722_timestamp(seventeen22_header *h1722, uint64_t timestamp)
{
	h1722->timestamp = timestamp;
}

uint64_t avb_get_1722_timestamp(seventeen22_header *h1722)
{
	return h1722->timestamp;
}

void avb_set_1722_gateway_info(seventeen22_header *h1722, uint64_t gateway_info)
{
	h1722->gateway_info = gateway_info;
}

uint64_t avb_get_1722_gateway_info(seventeen22_header *h1722)
{
	return h1722->gateway_info;
}

void avb_set_1722_length(seventeen22_header *h1722, uint64_t length)
{
	h1722->length = length;
}

uint64_t avb_get_1722_length(seventeen22_header *h1722)
{
	return h1722->length;
}

/* setters & getters for six1883_header */

void avb_set_61883_packet_channel(six1883_header *h61883, uint16_t packet_channel)
{
	h61883->packet_channel = packet_channel;
}

uint16_t avb_get_61883_length(six1883_header *h61883)
{
	return h61883->packet_channel;
}

void avb_set_61883_format_tag(six1883_header *h61883, uint16_t format_tag)
{
	h61883->format_tag = format_tag;
}

uint16_t avb_get_61883_format_tag(six1883_header *h61883)
{
	return h61883->format_tag;
}

void avb_set_61883_app_control(six1883_header *h61883, uint16_t app_control)
{
	h61883->app_control = app_control;
}

uint16_t avb_get_61883_app_control(six1883_header *h61883)
{
	return h61883->app_control;
}

void avb_set_61883_packet_tcode(six1883_header *h61883, uint16_t packet_tcode)
{
	h61883->packet_tcode = packet_tcode;
}

uint16_t avb_get_61883_packet_tcode(six1883_header *h61883)
{
	return h61883->packet_tcode;
}

void avb_set_61883_source_id(six1883_header *h61883, uint16_t source_id)
{
	h61883->source_id = source_id;
}

uint16_t avb_get_61883_source_id(six1883_header *h61883)
{
	return h61883->source_id;
}

void avb_set_61883_reserved0(six1883_header *h61883, uint16_t reserved0)
{
	h61883->reserved0 = reserved0;
}

uint16_t avb_get_61883_reserved0(six1883_header *h61883)
{
	return h61883->reserved0;
}

void avb_set_61883_data_block_size(six1883_header *h61883, uint16_t data_block_size)
{
	h61883->data_block_size = data_block_size;
}

uint16_t avb_get_61883_data_block_size(six1883_header *h61883)
{
	return h61883->data_block_size;
}

void avb_set_61883_reserved1(six1883_header *h61883, uint16_t reserved1)
{
	h61883->reserved1 = reserved1;
}

uint16_t avb_get_61883_reserved1(six1883_header *h61883)
{
	return h61883->reserved1;
}

void avb_set_61883_source_packet_header(six1883_header *h61883, uint16_t source_packet_header)
{
	h61883->source_packet_header = source_packet_header;
}

uint16_t avb_get_61883_source_packet_header(six1883_header *h61883)
{
	return h61883->source_packet_header;
}

void avb_set_61883_quadlet_padding_count(six1883_header *h61883, uint16_t quadlet_padding_count)
{
	h61883->quadlet_padding_count = quadlet_padding_count;
}

uint16_t avb_get_61883_quadlet_padding_count(six1883_header *h61883)
{
	return h61883->quadlet_padding_count;
}

void avb_set_61883_fraction_number(six1883_header *h61883, uint16_t fraction_number)
{
	h61883->fraction_number = fraction_number;
}

uint16_t avb_get_61883_fraction_number(six1883_header *h61883)
{
	return h61883->fraction_number;
}

void avb_set_61883_data_block_continuity(six1883_header *h61883, uint16_t data_block_continuity)
{
	h61883->data_block_continuity = data_block_continuity;
}

uint16_t avb_get_61883_data_block_continuity(six1883_header *h61883)
{
	return h61883->data_block_continuity;
}

void avb_set_61883_format_id(six1883_header *h61883, uint16_t format_id)
{
	h61883->format_id = format_id;
}

uint16_t avb_get_61883_format_id(six1883_header *h61883)
{
	return h61883->format_id;
}

void avb_set_61883_eoh(six1883_header *h61883, uint16_t eoh)
{
	h61883->eoh = eoh;
}

uint16_t avb_get_61883_eoh(six1883_header *h61883)
{
	return h61883->eoh;
}

void avb_set_61883_format_dependent_field(six1883_header *h61883, uint16_t format_dependent_field)
{
	h61883->format_dependent_field = format_dependent_field;
}

uint16_t avb_get_61883_format_dependent_field(six1883_header *h61883)
{
	return h61883->format_dependent_field;
}

void avb_set_61883_syt(six1883_header *h61883, uint16_t syt)
{
	h61883->syt = syt;
}

uint16_t avb_get_61883_syt(six1883_header *h61883)
{
	return h61883->syt;
}

void * avb_create_packet(uint32_t payload_len)
{
	void *avb_packet = NULL;
	uint32_t size;

	size = sizeof(six1883_header);
	size += sizeof(eth_header) + sizeof(seventeen22_header) + payload_len;

	avb_packet = calloc(size, sizeof(uint8_t));
	if (!avb_packet)
		return NULL;

	return avb_packet;
}

void avb_initialize_h1722_to_defaults(seventeen22_header *h1722)
{
	avb_set_1722_subtype(h1722, 0x0);
	avb_set_1722_cd_indicator(h1722, 0x0);
	avb_set_1722_timestamp_valid(h1722, 0x0);
	avb_set_1722_gateway_valid(h1722, 0x0);
	avb_set_1722_reserved0(h1722, 0x0);
	avb_set_1722_sid_valid(h1722, 0x0);
	avb_set_1722_reset(h1722, 0x0);
	avb_set_1722_version(h1722, 0x0);
	avb_set_1722_sid_valid(h1722, 0x0);
	avb_set_1722_timestamp_uncertain(h1722, 0x0);
	avb_set_1722_reserved1(h1722, 0x0);
	avb_set_1722_timestamp(h1722, 0x0);
	avb_set_1722_gateway_info(h1722, 0x0);
	avb_set_1722_length(h1722, 0x0);
}

void avb_initialize_61883_to_defaults(six1883_header *h61883)
{
	avb_set_61883_packet_channel(h61883, 0x0);
	avb_set_61883_format_tag(h61883, 0x0);
	avb_set_61883_app_control(h61883, 0x0);
	avb_set_61883_packet_tcode(h61883, 0x0);
	avb_set_61883_source_id(h61883, 0x0);
	avb_set_61883_reserved0(h61883, 0x0);
	avb_set_61883_data_block_size(h61883, 0x0);
	avb_set_61883_reserved1(h61883, 0x0);
	avb_set_61883_source_packet_header(h61883, 0x0);
	avb_set_61883_quadlet_padding_count(h61883, 0x0);
	avb_set_61883_fraction_number(h61883, 0x0);
	avb_set_61883_data_block_continuity(h61883, 0x0);
	avb_set_61883_format_id(h61883, 0x0);
	avb_set_61883_eoh(h61883, 0x0);
	avb_set_61883_format_dependent_field(h61883, 0x0);
	avb_set_61883_syt(h61883, 0x0);
}

int32_t avb_get_iface_mac_address(int8_t *iface, uint8_t *addr)
{
	struct ifreq ifreq;
	int fd, ret;

	/* Create a socket */
	fd = socket(PF_PACKET, SOCK_RAW, htons(0x800));
	if (fd < 0)
		return -1;

	memset(&ifreq, 0, sizeof(ifreq));

	strncpy(ifreq.ifr_name, (const char*)iface, sizeof(ifreq.ifr_name));
	ret = ioctl(fd, SIOCGIFHWADDR, &ifreq);
	if (ret < 0) {
		close(fd);
		return -1;
	}

	memcpy(addr, ifreq.ifr_hwaddr.sa_data, ETH_ALEN);
	close(fd);

	return 0;
}

void avb_1722_set_eth_type(eth_header *eth_header) {

	eth_header->h_protocol[0] = 0x22;
	eth_header->h_protocol[1] = 0xf0;

	return;
}

int32_t
avb_eth_header_set_mac(eth_header *ethernet_header, uint8_t *addr, int8_t *iface)
{
	uint8_t source_mac[ETH_ALEN];

	if (!addr || !iface)
		return -EINVAL;

	if (avb_get_iface_mac_address(iface, source_mac))
		return -EINVAL;

	memcpy(ethernet_header->h_dest, addr, ETH_ALEN);
	memcpy(ethernet_header->h_source, source_mac, ETH_ALEN);

	return 0;
}