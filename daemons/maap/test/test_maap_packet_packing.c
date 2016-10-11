#include "maap_packet.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static uint8_t test_stream[] = {
  0x01, 0x02, 0x03, 0x04,
  0x05, 0x06, 0x06, 0x05,
  0x04, 0x03, 0x02, 0x01,
  0x12, 0x34,
  0xfe, 0x11, 0x00, 0x10,
  0x01, 0x23, 0x45, 0x67,
  0x89, 0xab, 0xcd, 0xef,
  0xcb, 0xfe, 0x12, 0x34,
  0x00, 0x00, 0x00, 0xff,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

static MAAP_Packet test_packet = {
  0x010203040506,
  0x060504030201,
  0x1234,
  1,
  0x7e,
  0,
  1,
  1,
  0,
  16,
  0x0123456789abcdef,
  0xcbfe12340000,
  255,
  0x000000000000,
  0
};

void dump_stream(uint8_t *stream) {
  int i;
  for (i = 0; i < 42; i++) {
    if (i % 4 == 0) {
      printf("\n");
    }
    printf("%02x ", stream[i]);
  }
  printf("\n");
}

void dump_maap_packet(MAAP_Packet *packet) {
  printf("DA: %012llx\n", (unsigned long long int)packet->DA);
  printf("SA: %012llx\n", (unsigned long long int)packet->SA);
  printf("Ethertype: %04x\n", packet->Ethertype);
  printf("CD: %d\n", packet->CD);
  printf("subtype: %d\n", packet->subtype);
  printf("SV: %d\n", packet->SV);
  printf("version: %d\n", packet->version);
  printf("message_type: %d\n", packet->message_type);
  printf("maap_version: %d\n", packet->maap_version);
  printf("maap_data_length: %d\n", packet->maap_data_length);
  printf("stream_id: 0x%016llx\n", 
         (unsigned long long int)packet->stream_id);
  printf("requested_start_address: 0x%012llx\n", 
         (unsigned long long int)packet->requested_start_address);
  printf("requested_count: %d\n", packet->requested_count);
  printf("conflict_start_address: 0x%012llx\n",
         (unsigned long long int)packet->conflict_start_address);
  printf("conflict_count: %d\n", packet->conflict_count);
}

int cmp_maap_packets(MAAP_Packet *a, MAAP_Packet *b) {
  return (a->DA == b->DA &&
	  a->SA == b->SA &&
	  a->Ethertype == b->Ethertype &&
	  a->CD == b->CD &&
          a->subtype == b->subtype &&
          a->SV == b->SV &&
          a->version == b->version &&
          a->message_type == b->message_type &&
          a->maap_version == b->maap_version &&
          a->maap_data_length && b->maap_data_length &&
          a->stream_id == b->stream_id &&
          a->requested_start_address == b->requested_start_address &&
          a->requested_count == b->requested_count &&
          a->conflict_start_address == b->conflict_start_address &&
          a->conflict_count == b->conflict_count);
}
 
int main(void) {
  uint8_t buffer[42] = {0};
  MAAP_Packet result;

  unpack_maap(&result, test_stream);
  if (cmp_maap_packets(&result, &test_packet)) {
    printf("pack_maap success\n");
  } else {
    printf("pack_maap failure\n");
    printf("Expected:\n");
    dump_maap_packet(&test_packet);
    printf("Got:\n");
    dump_maap_packet(&result);
    printf("\n\n");
  }

  pack_maap(&test_packet, buffer);
  if (memcmp(test_stream, buffer, 42) == 0) {
    printf("unpack_maap success\n");
  } else {
    printf("unpack_maap failure\n");
    printf("Expected:\n");
    dump_stream(test_stream);
    printf("Got:\n");
    dump_stream(buffer);
  }
  return 0;
}
