#include <assert.h>
#include <string.h>
#include "maap.h"
#include "maap_packet.h"

int unpack_maap(MAAP_Packet *packet, uint8_t *stream) {
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
  packet->status = (tmp8 & 0xf8) >> 3;

  memcpy(&tmp16, stream, 2);
  packet->MAAP_data_length = BE16TOH(tmp16) & 0x07ff;
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
  packet->start_address = BE64TOH(tmp64) >> 16;
  stream += 6;

  memcpy(&tmp16, stream, 2);
  packet->count = BE16TOH(tmp16);

  return 0;
}

int pack_maap(MAAP_Packet *packet, uint8_t *stream) {
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

  tmp16 = HTOBE16(((packet->status & 0x001f) << 11) |
		  (packet->MAAP_data_length & 0x07ff));
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

  tmp64 = HTOBE64(packet->start_address << 16);
  memcpy(stream, &tmp64, 6);
  stream += 6;

  tmp16 = HTOBE16(packet->count);
  memcpy(stream, &tmp16, 2);

  return 0;
}

void init_packet(MAAP_Packet *packet) {
  assert(packet);

  packet->DA = MAAP_DEST_64;
  //  packet->DA = 0xFFFFFFFFFFFF;
  packet->SA = 0x0;
  packet->Ethertype = MAAP_TYPE;
  packet->CD = 1;
  packet->subtype = 0x7e;
  packet->SV = 0;
  packet->version = 1;
  packet->message_type = 0;
  packet->status = 0;
  packet->MAAP_data_length = 16;
  packet->stream_id = 0;
  packet->requested_start_address = 0;
  packet->requested_count = 0;
  packet->start_address = 0;
  packet->count = 0;
}
