#include <stdio.h>
#include <string.h>

#include "CppUTest/TestHarness.h"

extern "C" {

#include "maap.h"
#include "maap_packet.h"

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
	0xfe,
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

static MAAP_Packet initialized_packet = {
	0x020304050607,
	0x070605040302,
	0x22F0, /* Ethertype */
	0xFE, /* subtype */
	0, /* SV */
	0, /* version */
	0, /* message_type */
	1, /* maap_version */
	16, /* control_data_length */
	0, /* stream_id */
	0, /* requested_start_address */
	0, /* requested_count */
	0, /* conflict_start_address */
	0 /* conflict_count */
};

static void dump_stream(uint8_t *stream) {
	int i;
	for (i = 0; i < 42; i++) {
		if (i % 4 == 0) {
			printf("\n");
		}
		printf("%02x ", stream[i]);
	}
	printf("\n");
}

static void cmp_maap_packets(MAAP_Packet *a, MAAP_Packet *b) {
	CHECK(a->DA == b->DA);
	CHECK(a->SA == b->SA);
	CHECK(a->Ethertype == b->Ethertype);
	CHECK(a->subtype == b->subtype);
	CHECK(a->SV == b->SV);
	CHECK(a->version == b->version);
	CHECK(a->message_type == b->message_type);
	CHECK(a->maap_version == b->maap_version);
	CHECK(a->control_data_length && b->control_data_length);
	CHECK(a->stream_id == b->stream_id);
	CHECK(a->requested_start_address == b->requested_start_address);
	CHECK(a->requested_count == b->requested_count);
	CHECK(a->conflict_start_address == b->conflict_start_address);
	CHECK(a->conflict_count == b->conflict_count);
}

static void dump_maap_packet(MAAP_Packet *packet) {
	printf("DA: %012llx\n", (unsigned long long int)packet->DA);
	printf("SA: %012llx\n", (unsigned long long int)packet->SA);
	printf("Ethertype: %04x\n", packet->Ethertype);
	printf("subtype: %d\n", packet->subtype);
	printf("SV: %d\n", packet->SV);
	printf("version: %d\n", packet->version);
	printf("message_type: %d\n", packet->message_type);
	printf("maap_version: %d\n", packet->maap_version);
	printf("control_data_length: %d\n", packet->control_data_length);
	printf("stream_id: 0x%016llx\n",
		(unsigned long long int)packet->stream_id);
	printf("requested_start_address: 0x%012llx\n",
		(unsigned long long int)packet->requested_start_address);
	printf("requested_count: %d\n", packet->requested_count);
	printf("conflict_start_address: 0x%012llx\n",
		(unsigned long long int)packet->conflict_start_address);
	printf("conflict_count: %d\n", packet->conflict_count);
}


} TEST_GROUP(maap_packet_group)
{
	void setup() {
	}

	void teardown() {
	}
};

TEST(maap_packet_group, Init)
{
	MAAP_Packet result;

	init_packet(&result, 0x020304050607, 0x070605040302);
	cmp_maap_packets(&result, &initialized_packet);
}

TEST(maap_packet_group, Unpack)
{
	uint8_t buffer[42] = {0};
	MAAP_Packet result;

	unpack_maap(&result, test_stream);
	cmp_maap_packets(&result, &test_packet);
}

TEST(maap_packet_group, Pack)
{
	uint8_t buffer[42] = {0};

	pack_maap(&test_packet, buffer);
	LONGS_EQUAL(0, memcmp(test_stream, buffer, 42));
}

TEST(maap_packet_group, Convert)
{
	uint8_t testaddr[6] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
	unsigned long long result = convert_mac_address(testaddr);
	CHECK(result == 0x123456789abcLL);
}

TEST(maap_packet_group, Compare_MAC)
{
	uint64_t local_mac, remote_mac;

	local_mac =  0xffffffffff01;
	remote_mac = 0x000000000002;
	LONGS_EQUAL(1, compare_mac_addresses(local_mac, remote_mac));

	local_mac =  0x000000000002;
	remote_mac = 0xffffffffff01;
	LONGS_EQUAL(0, compare_mac_addresses(local_mac, remote_mac));

	local_mac =  0x000000000000;
	remote_mac = 0xffffffffffff;
	LONGS_EQUAL(1, compare_mac_addresses(local_mac, remote_mac));

	local_mac =  0xffffffffffff;
	remote_mac = 0x000000000000;
	LONGS_EQUAL(0, compare_mac_addresses(local_mac, remote_mac));

	local_mac =  0x112233445566;
	remote_mac = 0x112277445566;
	LONGS_EQUAL(1, compare_mac_addresses(local_mac, remote_mac));

	local_mac =  0x112233445566;
	remote_mac = 0x112200445566;
	LONGS_EQUAL(0, compare_mac_addresses(local_mac, remote_mac));

	local_mac =  0x102233445566;
	remote_mac = 0x112233445566;
	LONGS_EQUAL(1, compare_mac_addresses(local_mac, remote_mac));

	local_mac =  0x112233445566;
	remote_mac = 0x102233445566;
	LONGS_EQUAL(0, compare_mac_addresses(local_mac, remote_mac));
}
