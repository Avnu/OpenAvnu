/*************************************************************************************
Copyright (c) 2016-2017, Harman International Industries, Incorporated
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
*************************************************************************************/

/**
 * @file
 *
 * @brief Support for packing and unpacking MAAP packets
 *
 * These functions convert a binary stream of bytes into a #MAAP_Packet structure, or vice versa.
 */

#ifndef MAAP_PACKET_H
#define MAAP_PACKET_H

#include <stdint.h>

#include "platform.h"

#define MAAP_PROBE      1 /**< MAAP Probe MAC address(es) PDU - Defined in IEEE 1722-2016 Table B.1 */
#define MAAP_DEFEND     2 /**< MAAP Defend address(es) response PDU - Defined in IEEE 1722-2016 Table B.1 */
#define MAAP_ANNOUNCE   3 /**< MAAP Announce MAC address(es) acquired PDU - Defined in IEEE 1722-2016 Table B.1 */

/** MAAP Packet contents - Defined in IEEE 1722-2016 Figure B.1 */
typedef struct {
	uint64_t DA;                      /**< Destination Address */
	uint64_t SA;                      /**< Source Address */
	uint16_t Ethertype;               /**< AVTB Ethertype (i.e. @p MAAP_TYPE) */
	uint16_t subtype;                 /**< AVTP Subtype (i.e. @p MAAP_SUBTYPE) */
	uint8_t SV;                       /**< 1 if stream_id is valid, 0 otherwise.  Always 0 for MAAP. */
	uint8_t version;                  /**< AVTP Version.  Always 0 for MAAP */
	uint8_t message_type;             /**< MAAP message type (MAAP_PROBE, MAAP_DEFEND, or MAAP_ANNOUNCE) */
	uint8_t maap_version;             /**< MAAP Version.  Currently 1 for MAAP. */
	uint16_t control_data_length;     /**< Control Data Length in Bytes.  Always 16 for MAAP. */
	uint64_t stream_id;               /**< MAAP stream_id.  Always 0 for MAAP. */
	uint64_t requested_start_address; /**< Starting address for a MAAP_PROBE or MAAP_ANNOUNCE.
                                       * For a MAAP_DEFEND, the same address as the MAAP_PROBE or MAAP_ANNOUNCE that initiated the defend. */
	uint16_t requested_count;         /**< Number of addresses for a MAAP_PROBE or MAAP_ANNOUNCE.
                                       * For a MAAP_DEFEND, the same number of addresses as the MAAP_PROBE or MAAP_ANNOUNCE that initiated the defend. */
	uint64_t conflict_start_address;  /**< For a MAAP_DEFEND, the starting address of the block that conflicts with the MAAP_PROBE or MAAP_ANNOUNCE. */
	uint16_t conflict_count;          /**< For a MAAP_DEFEND, the number of addresses in the block that conflicts with the MAAP_PROBE or MAAP_ANNOUNCE. */
} MAAP_Packet;

/**
 * Initialize a #MAAP_Packet structure to prepare for sending a MAAP packet.
 *
 * @param packet Pointer to an empty #MAAP_Packet structure to fill
 * @param dest_mac Destination MAC Address for the packet
 * @param src_mac Source MAC Address for the packet
 */
void init_packet(MAAP_Packet *packet, uint64_t dest_mac, uint64_t src_mac);

/**
 * Convert (pack) a #MAAP_Packet structure into a binary stream of bytes.
 *
 * @param packet #MAAP_Packet structure to convert
 * @param stream Pointer to the buffer to fill with the binary stream
 *
 * @return 0 if successful, -1 if an error occurred.
 */
int pack_maap(const MAAP_Packet *packet, uint8_t *stream);

/**
 * Convert (unpack) a binary stream of bytes into a #MAAP_Packet structure
 *
 * @param packet #MAAP_Packet structure to fill with the packet contents
 * @param stream Pointer to the buffer containing the binary stream
 *
 * @return 0 if successful, -1 if an error occurred.
 */
int unpack_maap(MAAP_Packet *packet, const uint8_t *stream);

/**
 * Convert a byte-order MAC Address into a 64-bit number.
 *
 * The 64-bit number format is used to fill #MAAP_Packet#DA and #MAAP_Packet#SA in the #MAAP_Packet structure.
 *
 * @param macaddr Pointer to the byte-order MAC Address to convert
 *
 * @return A 64-bit number equivalent to the supplied MAC Address
 */
uint64_t convert_mac_address(const uint8_t macaddr[]);

/**
 * Compare two MAC addresses, starting with the least-significant bytes, to determine if the local address is numerically lower.
 *
 * @note This is equivalent to the compare_MAC function in IEEE 1722-2016 B3.6.4
 *
 * @param local_mac The MAC Address for the local network interface of the computer running this application
 * @param remote_mac The source MAC Address in a MAAP packet that was received
 *
 * @return 1 if the local_mac is less than then remote_mac, 0 otherwise
 */
int compare_mac_addresses(uint64_t local_mac, uint64_t remote_mac);

#endif
