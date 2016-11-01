#include <assert.h>
#include <string.h>
#include "maap.h"
#include "maap_packet.h"

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

  memcpy(&tmp8, stream, 1);
  packet->CD = (tmp8 & 0x80) >> 7;
  packet->subtype = tmp8 & 0x7f;
  stream++;

  memcpy(&tmp8, stream, 1);
  packet->SV = (tmp8 & 0x80) >> 7;
  packet->version = (tmp8 & 0x70) >> 4;
  packet->message_type = tmp8 & 0x0f;
  stream++;

  memcpy(&tmp8, stream, 1);
  packet->maap_version = (tmp8 & 0xf8) >> 3;

  memcpy(&tmp16, stream, 2);
  packet->maap_data_length = BE16TOH(tmp16) & 0x07ff;
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

  tmp8 = (packet->CD << 7) | (packet->subtype & 0x7f);
  memcpy(stream, &tmp8, 1);
  stream++;

  tmp8 = (packet->SV << 7) | ((packet->version & 0x07) << 4) |
    (packet->message_type & 0x0f);
  memcpy(stream, &tmp8, 1);
  stream++;

  tmp16 = HTOBE16(((packet->maap_version & 0x001f) << 11) |
		  (packet->maap_data_length & 0x07ff));
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
  packet->CD = 1; /* CD is 1 - Defined in IEEE 1722-2011 B.2.1 */
  packet->subtype = MAAP_SUBTYPE;
  packet->SV = 0; /* SV is 0 - Defined in IEEE 1722-2011 B.2.3 */
  packet->version = 0;  /* AVTP version is 0 - Defined in IEEE 1722-2011 5.2.4 */
  packet->message_type = 0;
  packet->maap_version = 1; /* MAAP version is 1 - Defined in IEEE 1722-2011 B.2.6 */
  packet->maap_data_length = 16; /* MAAP data length is 16 - Defined in IEEE 1722-2011 B.2.7 */
  packet->stream_id = 0; /* MAAP stream_id is 0 - Defined in IEEE 1722-2011 B.2.8 */
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
