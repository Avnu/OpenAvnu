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

/* MAC Address Acquisition Protocol- dynamically allocate multicast MAC address */
#include "maap_protocol.h"

extern uint8_t *maap_shm_mem;

/* generate pseudo random mac_address in the MAAP range */
void generate_address(maap_info_t *maap_info)
{
	uint16_t val;
	uint8_t maap_address_llimit[6] = {0x91,0xE0,0xF0,0x00,0x00,0x00};

	memcpy(maap_info->requested_mac_add, maap_address_llimit, MAC_ADDR_LEN);
	val = rand_range(0x0000, 0xFDFF);
	maap_info->requested_mac_add[4] = (val & 0xff00) >> 8; 
	maap_info->requested_mac_add[5] = val & 0x00ff;
}

int state_transit(maap_info_t *maap_info) 
{  
	state_t current_state = maap_info->state;
	maap_pdu_t *maap_pdu = &maap_info->ethpkt.data;
	ethpkt_t *pkt_tx = &maap_info->ethpkt; 
	ethpkt_t *pkt_rx = &maap_info->ethpkt_rx;
	msg_type_t msg_type = 0;
	uint8_t *s = NULL;
	int i= 0;
	int num_bytes = 0;
	int maap_probe_count;

	switch (current_state) {
	case INITIAL:
		DBG("state: INITIAL\n");
		generate_address(maap_info);
		maap_probe_count = MAAP_PROBE_RETRANSMITS; 
		maap_info->state = PROBE;
		break;

	case PROBE: 
		DBG("state: PROBE\n");
		prepare_ethpkt(maap_info, MAAP_PROBE);

		while (maap_probe_count) {
			/* send MAAP_PROBE */
			send_packet(pkt_tx);
			printf("\nSENT MAAP_PROBE \n");

			delay(0, PROBE_TIMER_MS);

			memset(pkt_rx, 0x00, ETH_PKT_LEN);
			/* Listen for MAAP_DEFEND */
			num_bytes = recv_packet(pkt_rx, NON_BLOCK);
			if (num_bytes > 0) {
				msg_type = pkt_rx->data.message_type; 
				DBG("msg_type %x \n",msg_type);

				if (msg_type == MAAP_DEFEND || 
				msg_type ==  MAAP_ANNOUNCE ||
				msg_type == MAAP_PROBE) {
					if (compare_mac(pkt_rx->data.requested_start_address,
					maap_pdu->requested_start_address)) {
						DBG("\nReceived\
						MAAP_DEFEND frame from \
						Mac Address: ");
						DBG_ADDR(pkt_rx->header.h_source);
						DBG("\nRequested\
						Address of recv frame: ");
						DBG_ADDR(pkt_rx->data.requested_start_address);
						DBG("\nRequested Address: ");
						DBG_ADDR(maap_pdu->requested_start_address);

						maap_info->state = INITIAL;
						return 1;
					}
			} else {
				printf("\n Invalid MAAP packet\n");
			}  
			} else {
				maap_probe_count -=1;
			}  
		}

		maap_info->acquired_mac_count =
			maap_info->requested_mac_count;
		memcpy(maap_info->acquired_mac_add,
			maap_info->requested_mac_add, MAC_ADDR_LEN);

		prepare_ethpkt(maap_info, MAAP_ANNOUNCE);
		send_packet(pkt_tx);  

		create_thread(maap_info, send_announce);

		printf("\nSENT MAAP_ANNOUNCE \n\nANNOUNCED ADDRESS:\n");
			DBG_ADDR(maap_info->acquired_mac_add);

		s = maap_shm_mem;
		/* writing allocated mac-address to shared memory */
		for (i = 0; i < MAC_ADDR_LEN ; i++) {
			*s++ = maap_info->acquired_mac_add[i];
		}

		maap_info->state = DEFEND;
		printf("\n\nSTATE change to DEFEND:\n");
		break;

	case DEFEND:
		memset(pkt_rx, 0x00, ETH_PKT_LEN);
		/* Listen MAAP_PROBE */
		num_bytes = recv_packet(pkt_rx, BLOCK);

		if (num_bytes > 0) {
			msg_type = pkt_rx->data.message_type;
			if (compare_mac(pkt_rx->data.requested_start_address,
				maap_info->acquired_mac_add)) {
				if (msg_type == MAAP_PROBE) {
					memcpy(maap_info->conflict_mac_add,
					pkt_rx->data.requested_start_address,
					MAC_ADDR_LEN);
					maap_info->conflict_mac_count =
					pkt_rx->data.requested_count;

					DBG("\n Received frame\
						MAAP_PROBE \n Source address of\
						the received frame:"); 
					DBG_ADDR(pkt_rx->header.h_source);
					DBG("\n Requested start address\
						of received frame :");
					DBG_ADDR(pkt_rx->data.requested_start_address);
					DBG("\n Announced Address :");
					DBG_ADDR(maap_info->acquired_mac_add);

					Lock();
					prepare_ethpkt(maap_info,
						MAAP_DEFEND);
					send_packet(pkt_tx);
					UnLock();
					DBG(" SENT DEFEND \n"); 
				} else if (msg_type == MAAP_ANNOUNCE ||
					msg_type == MAAP_DEFEND) {
					DBG("\n Received \
					msg_type-MAAP_ANNOUNCE \
					Source Address: ");
					DBG_ADDR(pkt_rx->header.h_source);
					DBG("\n Destination Address: ");
					DBG_ADDR(pkt_rx->header.h_dest);

					destroy_thread(); 
					maap_info->state = INITIAL;
					DBG("\nstate change to INITIAL");
				}
			}
		} 
		break;

	default:
		printf("Error: Undefined State\n");
		break;
	}

	return 0;
}


void Init(maap_info_t *maap_info, uint8_t src_mac_adr[6])
{
	maap_pdu_t *maap_pkt = &maap_info->ethpkt.data;
	uint8_t dest_mac[6];
	int i;

	DBG("Init\n");
	maap_info->state = INITIAL;
	maap_pkt->subtype          = MAAP_SUBTYPE; 
	maap_pkt->cd               = 0x1; 
	maap_pkt->message_type     = 0x0;
	maap_pkt->version          = 0x0;
	maap_pkt->sv               = 0x0;
	maap_pkt->maap_version_data_length = hton_s(MAAP_VER_DATA_LEN);
	maap_pkt->stream_id        = 0x00;
	AGN_ADR(maap_pkt->requested_start_address, 0x00); 
	AGN_ADR(maap_pkt->conflict_start_address, 0x00);
	maap_pkt->requested_count = 0x00; 
	maap_pkt->conflict_count  = 0x00;

	get_multicast_mac_adr(dest_mac);
	memcpy(maap_info->src_mac, src_mac_adr, MAC_ADDR_LEN);

	memcpy(maap_info->ethpkt.header.h_dest, dest_mac, MAC_ADDR_LEN);
	memcpy(maap_info->ethpkt.header.h_source, src_mac_adr, MAC_ADDR_LEN);
	maap_info->ethpkt.header.eth_type = hton_s(ETH_TYPE);

	maap_info->requested_mac_count = hton_s(0x0001);
}

void prepare_ethpkt(maap_info_t *maap_info, msg_type_t msg_type)
{
	ethpkt_t *pkt_tx = &maap_info->ethpkt;
	ethpkt_t *pkt_rx = &maap_info->ethpkt_rx;
	maap_pdu_t *maap_pkt = &maap_info->ethpkt.data;
	uint8_t src_mac[6];
	uint8_t dest_mac[6];
	int i;

	maap_pkt->message_type = msg_type;
	memcpy(src_mac, maap_info->src_mac, MAC_ADDR_LEN);
	get_multicast_mac_adr(dest_mac);

	switch(msg_type)
	{
		case MAAP_PROBE:
			memcpy(maap_pkt->requested_start_address,
			       maap_info->requested_mac_add, MAC_ADDR_LEN);
			AGN_ADR(maap_pkt->conflict_start_address, 0x00);
			maap_pkt->requested_count =
						maap_info->requested_mac_count;
			maap_pkt->conflict_count  = 0x00;
			memcpy(pkt_tx->header.h_dest, dest_mac, MAC_ADDR_LEN);
			memcpy(pkt_tx->header.h_source, src_mac, MAC_ADDR_LEN);
			break;

		case MAAP_DEFEND:
			memcpy(maap_pkt->requested_start_address,
				maap_info->requested_mac_add, MAC_ADDR_LEN);
			memcpy(maap_pkt->conflict_start_address,
				maap_info->conflict_mac_add, MAC_ADDR_LEN);
			maap_pkt->requested_count =
				maap_info->requested_mac_count;
			maap_pkt->conflict_count =
				maap_info->conflict_mac_count;
                        /* 
                         * DEST_MAC set to source mac address received in
                         * MAAP_PROBE
                         */
			memcpy(pkt_tx->header.h_dest, pkt_rx->header.h_source,
			       MAC_ADDR_LEN); 
			memcpy(pkt_tx->header.h_source, src_mac, MAC_ADDR_LEN);
			break;

		case MAAP_ANNOUNCE:
			memcpy(maap_pkt->requested_start_address,
				maap_info->acquired_mac_add, MAC_ADDR_LEN);
			AGN_ADR(maap_pkt->conflict_start_address, 0x00);
			maap_pkt->requested_count =
						maap_info->acquired_mac_count;
			maap_pkt->conflict_count  = 0x00;
			memcpy(pkt_tx->header.h_dest, dest_mac, MAC_ADDR_LEN);
			memcpy(pkt_tx->header.h_source, src_mac, MAC_ADDR_LEN);
			break;

		default:
			printf("Error: Message type not set\n");
			break;
	}
}

/*
 * compare MAC address from received MAAP PDU and receiving station MAC address
 * return TRUE if they are same(different from what given in the protocol which is 
 * to return TRUE if MAC address of receiving station is lower than received MAAP PDU)
 */
int compare_mac(uint8_t rcv_add[6], uint8_t req_addr[6])
{
	int i, flag = 0;

	for (i=0; i<6; i++) {
		if (rcv_add[i] == req_addr[i])
		{
			flag = 1; 
		} else {
			flag = 0;
			break;
		} 
	}
	return flag;
}

void *send_announce(void *maap_Info)
{
	maap_info_t *maap_info; 
	maap_info = (maap_info_t *)maap_Info;

	ethpkt_t *pkt_tx = &maap_info->ethpkt;

	while(1)
	{
		delay(ANNOUNCE_TIMER_S, 0);
		Lock();
		prepare_ethpkt(maap_info, MAAP_ANNOUNCE);
		send_packet(pkt_tx); 
		UnLock();
		printf("\nSENT MAAP_ANNOUNCE");
	}
}

/* Multicast MAAP MAC address */
void get_multicast_mac_adr(uint8_t *dest_mac) 
{
	dest_mac[0] = 0x91; 
	dest_mac[1] = 0xE0; 
	dest_mac[2] = 0xF0; 
	dest_mac[3] = 0x00; 
	dest_mac[4] = 0xFF; 
	dest_mac[5] = 0x00; 
}
