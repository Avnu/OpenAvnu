#ifndef MAAP_PACKET_H
#define MAAP_PACKET_H

#include <stdint.h>

#include "platform.h"

#define MAAP_RESERVED   0
#define MAAP_PROBE      1
#define MAAP_DEFEND     2
#define MAAP_ANNOUNCE   3

typedef struct maap_packet {
  uint64_t DA;
  uint64_t SA;
  uint16_t Ethertype;
  uint8_t CD;
  uint16_t subtype;
  uint8_t SV;
  uint8_t version;
  uint8_t message_type;
  uint8_t status;
  uint16_t MAAP_data_length;
  uint64_t stream_id;
  uint64_t requested_start_address;
  uint16_t requested_count;
  uint64_t start_address;
  uint16_t count;
} MAAP_Packet;

void init_packet(MAAP_Packet *packet, uint64_t dest_mac, uint64_t src_mac);
int pack_maap(MAAP_Packet *packet, uint8_t *stream);
int unpack_maap(MAAP_Packet *packet, uint8_t *stream);

uint64_t convert_mac_address(uint8_t macaddr[]);

#endif
