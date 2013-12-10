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

#ifndef __AVBTP_H__
#define __AVBTP_H__

#include <inttypes.h>

#include "igb.h"

#define VALID		1
#define INVALID		0

#define MAC_ADDR_LEN	6

#define IGB_BIND_NAMESZ		24

#define SHM_SIZE 4*8 + sizeof(pthread_mutex_t) /* 3 - 64 bit and 2 - 32 bits */
#define SHM_NAME  "/ptp"

#define MAX_SAMPLE_VALUE ((1U << ((sizeof(int32_t)*8)-1))-1)

#define IEEE_61883_IIDC_SUBTYPE 0x0

#define MRPD_PORT_DEFAULT 7500

#define STREAM_ID_SIZE		8

#define ETHER_TYPE_AVTP		0x22f0

typedef struct __attribute__ ((packed)) {
	uint64_t subtype:7;
	uint64_t cd_indicator:1;
	uint64_t timestamp_valid:1;
	uint64_t gateway_valid:1;
	uint64_t reserved0:1;
	uint64_t reset:1;
	uint64_t version:3;
	uint64_t sid_valid:1;
	uint64_t seq_number:8;
	uint64_t timestamp_uncertain:1;
	uint64_t reserved1:7;
	uint64_t stream_id;
	uint64_t timestamp:32;
	uint64_t gateway_info:32;
	uint64_t length:16;

} seventeen22_header;

/* 61883 CIP with SYT Field */
typedef struct {
	uint16_t packet_channel:6;
	uint16_t format_tag:2;
	uint16_t app_control:4;
	uint16_t packet_tcode:4;
	uint16_t source_id:6;
	uint16_t reserved0:2;
	uint16_t data_block_size:8;
	uint16_t reserved1:2;
	uint16_t source_packet_header:1;
	uint16_t quadlet_padding_count:3;
	uint16_t fraction_number:2;
	uint16_t data_block_continuity:8;
	uint16_t format_id:6;
	uint16_t eoh:2;
	uint16_t format_dependent_field:8;
	uint16_t syt;
} six1883_header;

typedef struct {
	uint8_t label;
	uint8_t value[3];
} six1883_sample;

#define ETH_ALEN   6 /* Size of Ethernet address */

typedef struct __attribute__ ((packed)) {
	/* Destination MAC address. */
	uint8_t h_dest [ETH_ALEN];
	/* Destination MAC address. */
	uint8_t h_source [ETH_ALEN];
	/* Protocol ID. */
	uint8_t h_protocol[2];
} eth_header;

typedef struct { 
	int64_t ml_phoffset;
	int64_t ls_phoffset;
	int32_t ml_freqoffset;
	int32_t ls_freqoffset;
	int64_t local_time;
} gPtpTimeData;

typedef enum { false = 0, true = 1 } bool;

int pci_connect(device_t * igb_dev);

int gptpscaling(gPtpTimeData * td, char *memory_offset_buffer);

void gptpdeinit(int shm_fd, char *memory_offset_buffer);

int gptpinit(int *shm_fd, char *memory_offset_buffer);

void avb_set_1722_cd_indicator(seventeen22_header *h1722, uint64_t cd_indicator);
uint64_t avb_get_1722_cd_indicator(seventeen22_header *h1722);
void avb_set_1722_subtype(seventeen22_header *h1722, uint64_t subtype);
uint64_t avb_get_1722_subtype(seventeen22_header *h1722);
void avb_set_1722_sid_valid(seventeen22_header *h1722, uint64_t sid_valid);
uint64_t avb_get_1722_sid_valid(seventeen22_header *h1722);
void avb_set_1722_version(seventeen22_header *h1722, uint64_t version);
uint64_t avb_get_1722_version(seventeen22_header *h1722);
void avb_set_1722_reset(seventeen22_header *h1722, uint64_t reset);
uint64_t avb_get_1722_reset(seventeen22_header *h1722);
void avb_set_1722_reserved0(seventeen22_header *h1722, uint64_t reserved0);
uint64_t avb_get_1722_reserved0(seventeen22_header *h1722);
void avb_set_1722_reserved1(seventeen22_header *h1722, uint64_t reserved1);
uint64_t avb_get_1722_reserved1(seventeen22_header *h1722);
void avb_set_1722_timestamp_uncertain(seventeen22_header *h1722, uint64_t timestamp_uncertain);
uint64_t avb_get_1722_timestamp_uncertain(seventeen22_header *h1722);
void avb_set_1722_timestamp(seventeen22_header *h1722, uint64_t timestamp);
uint64_t avb_get_1722_reset(seventeen22_header *h1722);
void avb_set_1722_reserved0(seventeen22_header *h1722, uint64_t reserved0);
uint64_t avb_get_1722_reserved0(seventeen22_header *h1722);
void avb_set_1722_gateway_valid(seventeen22_header *h1722, uint64_t gateway_valid);
uint64_t avb_get_1722_gateway_valid(seventeen22_header *h1722);
void avb_set_1722_timestamp_valid(seventeen22_header *h1722, uint64_t timestamp_valid);
uint64_t avb_get_1722_timestamp_valid(seventeen22_header *h1722);
void avb_set_1722_reserved1(seventeen22_header *h1722, uint64_t reserved1);
uint64_t avb_get_1722_reserved1(seventeen22_header *h1722);
void avb_set_1722_timestamp_uncertain(seventeen22_header *h1722, uint64_t timestamp_uncertain);
uint64_t avb_get_1722_timestamp_uncertain(seventeen22_header *h1722);
void avb_set_1722_timestamp(seventeen22_header *h1722, uint64_t timestamp);
uint64_t avb_get_1722_timestamp(seventeen22_header *h1722);
void avb_set_1722_gateway_info(seventeen22_header *h1722, uint64_t gateway_info);
uint64_t avb_get_1722_gateway_info(seventeen22_header *h1722);
void avb_set_1722_length(seventeen22_header *h1722, uint64_t length);
uint64_t avb_get_1722_length(seventeen22_header *h1722);
void avb_set_1722_stream_id(seventeen22_header *h1722, uint64_t stream_id);
uint64_t avb_get_1722_stream_id(seventeen22_header *h1722);
void avb_set_1722_seq_number(seventeen22_header *h1722, uint64_t seq_number);
uint64_t avb_get_1722_seq_number(seventeen22_header *h1722);

void avb_set_61883_packet_channel(six1883_header *h61883, uint16_t packet_channel);
uint16_t avb_get_61883_length(six1883_header *h61883);
void avb_set_61883_format_tag(six1883_header *h61883, uint16_t format_tag);
uint16_t avb_get_61883_format_tag(six1883_header *h61883);
void avb_set_61883_app_control(six1883_header *h61883, uint16_t app_control);
uint16_t avb_get_61883_app_control(six1883_header *h61883);
void avb_set_61883_packet_tcode(six1883_header *h61883, uint16_t packet_tcode);
uint16_t avb_get_61883_packet_tcode(six1883_header *h61883);
void avb_set_61883_source_id(six1883_header *h61883, uint16_t source_id);
uint16_t avb_get_61883_source_id(six1883_header *h61883);
void avb_set_61883_reserved0(six1883_header *h61883, uint16_t reserved0);
uint16_t avb_get_61883_reserved0(six1883_header *h61883);
void avb_set_61883_data_block_size(six1883_header *h61883, uint16_t data_block_size);
uint16_t avb_get_61883_data_block_size(six1883_header *h61883);
void avb_set_61883_reserved1(six1883_header *h61883, uint16_t reserved1);
uint16_t avb_get_61883_reserved1(six1883_header *h61883);
void avb_set_61883_source_packet_header(six1883_header *h61883, uint16_t source_packet_header);
uint16_t avb_get_61883_source_packet_header(six1883_header *h61883);
void avb_set_61883_quadlet_padding_count(six1883_header *h61883, uint16_t quadlet_padding_count);
uint16_t avb_get_61883_quadlet_padding_count(six1883_header *h61883);
void avb_set_61883_fraction_number(six1883_header *h61883, uint16_t fraction_number);
uint16_t avb_get_61883_fraction_number(six1883_header *h61883);
void avb_set_61883_data_block_continuity(six1883_header *h61883, uint16_t data_block_continuity);
uint16_t avb_get_61883_data_block_continuity(six1883_header *h61883);
void avb_set_61883_format_id(six1883_header *h61883, uint16_t format_id);
uint16_t avb_get_61883_format_id(six1883_header *h61883);
void avb_set_61883_eoh(six1883_header *h61883, uint16_t eoh);
uint16_t avb_get_61883_eoh(six1883_header *h61883);
void avb_set_61883_format_dependent_field(six1883_header *h61883, uint16_t format_dependent_field);
uint16_t avb_get_61883_format_dependent_field(six1883_header *h61883);
void avb_set_61883_syt(six1883_header *h61883, uint16_t syt);
uint16_t avb_get_61883_syt(six1883_header *h61883);

void * avb_create_packet(uint32_t payload_len);

void avb_initialize_h1722_to_defaults(seventeen22_header *h1722);

void avb_initialize_61883_to_defaults(six1883_header *h61883);

int32_t avb_get_iface_mac_address(int8_t *iface, uint8_t *addr);

int32_t
avb_eth_header_set_mac(eth_header *ethernet_header, uint8_t *addr, int8_t *iface);

void avb_1722_set_eth_type(eth_header *eth_header);

#endif		/*  __AVBTP_H__ */
