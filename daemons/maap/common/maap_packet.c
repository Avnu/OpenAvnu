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

#include <assert.h>
#include <string.h>
#ifdef _WIN32
#include "Winsock2.h"
#endif
#include "maap.h"
#include "maap_packet.h"

#define MAAP_LOG_COMPONENT "Packet"
#include "maap_log.h"

int unpack_maap(MAAP_Packet *packet, const uint8_t *stream) {
	uint64_t tmp64;
	uint16_t tmp16;
	uint8_t tmp8;

	assert(packet);
	assert(stream);

	memcpy(&tmp64, stream, 6);
	packet->DA = BE64TOH(tmp64) >> 16;
	stream += 6;

	memcpy(&tmp64, stream, 6);
	packet->SA = BE64TOH(tmp64) >> 16;
	stream += 6;

	memcpy(&tmp16, stream, 2);
	packet->Ethertype = BE16TOH(tmp16);
	stream += 2;

	tmp8 = *stream;
	packet->subtype = tmp8;
	stream++;

	tmp8 = *stream;
	packet->SV = (tmp8 & 0x80) >> 7;
	packet->version = (tmp8 & 0x70) >> 4;
	packet->message_type = tmp8 & 0x0f;
	stream++;

	memcpy(&tmp8, stream, 1);
	packet->maap_version = (tmp8 & 0xf8) >> 3;

	memcpy(&tmp16, stream, 2);
	packet->control_data_length = BE16TOH(tmp16) & 0x07ff;
	stream += 2;

	memcpy(&tmp64, stream, 8);
	packet->stream_id = BE64TOH(tmp64);
	stream += 8;

	memcpy(&tmp64, stream, 6);
	packet->requested_start_address = BE64TOH(tmp64) >> 16;
	stream += 6;

	memcpy(&tmp16, stream, 2);
	packet->requested_count = BE16TOH(tmp16);
	stream += 2;

	memcpy(&tmp64, stream, 6);
	packet->conflict_start_address = BE64TOH(tmp64) >> 16;
	stream += 6;

	memcpy(&tmp16, stream, 2);
	packet->conflict_count = BE16TOH(tmp16);

	return 0;
 }

int pack_maap(const MAAP_Packet *packet, uint8_t *stream) {
	uint64_t tmp64;
	uint16_t tmp16;
	uint8_t tmp8;

	assert(packet);
	assert(stream);

	tmp64 = HTOBE64(packet->DA << 16);
	memcpy(stream, &tmp64, 6);
	stream += 6;

	tmp64 = HTOBE64(packet->SA << 16);
	memcpy(stream, &tmp64, 6);
	stream += 6;

	tmp16 = HTOBE16(packet->Ethertype);
	memcpy(stream, &tmp16, 2);
	stream += 2;

	tmp8 = (uint8_t) packet->subtype;
	*stream = tmp8;
	stream++;

	tmp8 = (packet->SV << 7) | ((packet->version & 0x07) << 4) |
		(packet->message_type & 0x0f);
	*stream = tmp8;
	stream++;

	tmp16 = HTOBE16(((packet->maap_version & 0x001f) << 11) |
					(packet->control_data_length & 0x07ff));
	memcpy(stream, &tmp16, 2);
	stream += 2;

	tmp64 = HTOBE64(packet->stream_id);
	memcpy(stream, &tmp64, 8);
	stream += 8;

	tmp64 = HTOBE64(packet->requested_start_address << 16);
	memcpy(stream, &tmp64, 6);
	stream += 6;

	tmp16 = HTOBE16(packet->requested_count);
	memcpy(stream, &tmp16, 2);
	stream += 2;

	tmp64 = HTOBE64(packet->conflict_start_address << 16);
	memcpy(stream, &tmp64, 6);
	stream += 6;

	tmp16 = HTOBE16(packet->conflict_count);
	memcpy(stream, &tmp16, 2);

	return 0;
}

void init_packet(MAAP_Packet *packet, uint64_t dest_mac, uint64_t src_mac) {
	assert(packet);
	assert(dest_mac != 0);
	assert(src_mac != 0);

	packet->DA = dest_mac;
	packet->SA = src_mac;
	packet->Ethertype = MAAP_TYPE;
	packet->subtype = MAAP_SUBTYPE;
	packet->SV = 0; /* SV is 0 - Defined in IEEE 1722-2016 Table F.23 */
	packet->version = 0;  /* AVTP version is 0 - Defined in IEEE 1722-2016 4.4.3.4 */
	packet->message_type = 0;
	packet->maap_version = 1; /* MAAP version is 1 - Defined in IEEE 1722-2016 B.2.3.1 */
	packet->control_data_length = 16; /* Control data length is 16 - Defined in IEEE 1722-2016 B.2.1 */
	packet->stream_id = 0; /* MAAP stream_id is 0 - Defined in IEEE 1722-2016 B.2.4 */
	packet->requested_start_address = 0;
	packet->requested_count = 0;
	packet->conflict_start_address = 0;
	packet->conflict_count = 0;
}

uint64_t convert_mac_address(const uint8_t macaddr[])
{
	uint64_t retVal;

	assert(macaddr);
	retVal = BE64TOH(*(uint64_t *)macaddr) >> 16;
	return retVal;
}

int compare_mac_addresses(uint64_t local_mac, uint64_t remote_mac)
{
	int i;
	uint8_t local_byte, remote_byte;

	for (i = 0; i < 6; ++i)
	{
		/* Test the next least-significant byte. */
		local_byte = (uint8_t)(local_mac & 0xFF);
		remote_byte = (uint8_t)(remote_mac & 0xFF);

		/* See if we have a difference. */
		if (local_byte < remote_byte) return 1;
		if (local_byte > remote_byte) return 0;

		/* Try the next lowest-byte in the next iteration. */
		local_mac = local_mac >> 8;
		remote_mac = remote_mac >> 8;
	}

	/* Assume that our own packet was somehow reflected back at us.
	 * Return 1 to ignore this packet. */
	return 1;
}
