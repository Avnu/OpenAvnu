/*************************************************************************
  Copyright (c) 2015 VAYAVYA LABS PVT LTD - http://vayavyalabs.com/
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Vayavya labs nor the names of its
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

****************************************************************************/

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
