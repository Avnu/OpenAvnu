/*
 * Copyright (c) 2014 VAYAVYA LABS PVT LTD - http://vayavyalabs.com/
 * All rights reserved.
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

#ifndef __MAAP__PROTOCOL__

#define __MAAP__PROTOCOL__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#define MAAP_PROBE_RETRANSMITS			3
#define MAAP_PROBE_INTERVAL_BASE_MS		500 
#define MAAP_PROBE_INTERVAL_VARIATION_MS	100
#define MAAP_ANNOUNCE_INTERVAL_BASE_S		30
#define MAAP_ANNOUNCE_INTERVAL_VARIATION_S	2 

#define PROBE_TIMER_MS rand_frange(MAAP_PROBE_INTERVAL_BASE_MS,\
	MAAP_PROBE_INTERVAL_BASE_MS + MAAP_PROBE_INTERVAL_VARIATION_MS)
#define ANNOUNCE_TIMER_S rand_frange(MAAP_ANNOUNCE_INTERVAL_BASE_S,\
	MAAP_ANNOUNCE_INTERVAL_BASE_S + MAAP_ANNOUNCE_INTERVAL_VARIATION_S)

#define MAAP_SUBTYPE		0x7E
#define MAAP_VER_DATA_LEN	(((0x01 & 0x1F) << 11) | (0x010 & 0x7FF))
#define ETH_TYPE		0x22F0
#define ETH_PKT_LEN		sizeof(ethpkt_t) 
#define MAC_ADDR_LEN		6 
#define BLOCK			0 
#define NON_BLOCK		1

#define AGN_ADR(ADDR, val) for(i=0; i<MAC_ADDR_LEN; i++) {ADDR[i] = val;}

/* states */
typedef enum maap_state {
	INITIAL = 0,
	PROBE,
	DEFEND,
} state_t;    

/* message types */
typedef enum msg_type {
	MAAP_PROBE = 1,
	MAAP_DEFEND,
	MAAP_ANNOUNCE
} msg_type_t;

/* Ethernet header */
typedef struct ethhedr {
	uint8_t h_dest[6];
	uint8_t h_source[6];
	uint16_t eth_type;
} ethhedr_t; 

/* MAAP PDU */
typedef struct __attribute__ ((packed)) {
	uint64_t subtype:7;
	uint64_t cd:1;
	uint64_t message_type:4;
	uint64_t version:3;
	uint64_t sv:1;
	uint64_t maap_version_data_length:16;
	uint64_t stream_id;
	uint8_t requested_start_address[6];
	uint16_t requested_count;
	uint8_t conflict_start_address[6];
	uint16_t conflict_count;
} maap_pdu_t;

/* structure to store packet header and data */
typedef struct ethpacket {
	ethhedr_t header;
	maap_pdu_t data;
} ethpkt_t;  

/* MAAP Info private data structure */
typedef struct maap_info {
	ethpkt_t ethpkt;
	ethpkt_t ethpkt_rx;
	state_t state; 
	uint8_t src_mac[6];
	uint8_t requested_mac_add[6];
	uint16_t requested_mac_count;
	uint8_t acquired_mac_add[6];
	uint16_t acquired_mac_count;
	uint8_t conflict_mac_add[6];
	uint16_t conflict_mac_count;
} maap_info_t;

/* 
 * Function to initialize the pdu, header info and private data structure
 * maap_info
 */
void Init(maap_info_t *, uint8_t *);

/* Function to prepare ethernet packet */
void prepare_ethpkt(maap_info_t *, msg_type_t);

/* State transition */
int state_transit(maap_info_t *);

/* Get a random value within the range (min, max) */
int rand_range(int , int);
double rand_frange(double , double);

/* Generate a random address from the MAAP Dynamic allocation range */
void generate_address(maap_info_t *);

/* Compare the Mac address and return 1 if address are same else return 0 */
int compare_mac(uint8_t *, uint8_t *);

/* Send MAAP_ANNOUNCE periodically with a delay of ANNOUNCE_TIMER */
void *send_announce(void *);

/* Get Multicast MAAP MAC Address */ 
void get_multicast_mac_adr(uint8_t *);

/* Create thread and Destroy thread */
void create_thread(maap_info_t *, void *);
void destroy_thread();

/* Send and receive of ethernet packet */
void send_packet(ethpkt_t *); 
int recv_packet(ethpkt_t *, int);

/* Delay based on the timer type passed */ 
void delay(int , int ); 

/* Lock and release */
void Lock(); 
void UnLock();
uint16_t hton_s(uint16_t );

//#define DEBUG
#ifdef DEBUG
#define DBG(x...) fprintf(stderr, x)
#define DBG_ADDR(ADDR) for(i=0; i<MAC_ADDR_LEN; i++) { fprintf(stderr, "%#x " \
		                      ,ADDR[i]); }
#else
#define DBG(x...) do {} while(0)
#define DBG_ADDR(ADDR) for(i=0; i<MAC_ADDR_LEN; i++) { fprintf(stderr, "%#x " \
		                      ,ADDR[i]); }
//#define DBG_ADDR(ADDR) do {} while(0)
#endif

#endif
