#include <stdio.h>
#include <string.h>

#include "CppUTest/TestHarness.h"

extern "C" {

#include "maap.h"
#include "maap_packet.h"

} TEST_GROUP(maap_group)
{
	void setup() {
		/* Try a variety of "random" values over subsequent tests. */
		srand(time(NULL));
	}

	void teardown() {
	}
};

TEST(maap_group, Init)
{
	const uint8_t dest_mac[6] = MAAP_DEST_MAC;
	const uint8_t src_mac[6] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
	const uint64_t range_base_addr = MAAP_DYNAMIC_POOL_BASE + 0x1000;
	const uint32_t range_size = MAAP_DYNAMIC_POOL_SIZE - 0x800;
	Maap_Client mc;
	Maap_Notify mn;
	const int sender1_in = 1, sender2_in = 2, sender3_in = 3;
	const void *sender_out;

	/* Initialize the Maap_Client structure */
	memset(&mc, 0, sizeof(Maap_Client));
	mc.dest_mac = convert_mac_address(dest_mac);
	mc.src_mac = convert_mac_address(src_mac);

	/* Test the maap_init_client() function */
	LONGS_EQUAL(0, maap_init_client(&mc, &sender1_in, range_base_addr, range_size));
	LONGS_EQUAL(range_base_addr, mc.address_base);
	LONGS_EQUAL(range_size, mc.range_len);
	CHECK(mc.ranges == NULL);
	CHECK(mc.timer_queue == NULL);
	CHECK(mc.timer != NULL);
	CHECK(mc.net != NULL);
	CHECK(mc.initialized);

	/* We should receive exactly one notification of the initialization. */
	sender_out = NULL;
	memset(&mn, 0, sizeof(Maap_Notify));
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender1_in);
	LONGS_EQUAL(MAAP_NOTIFY_INITIALIZED, mn.kind);
	LONGS_EQUAL(-1, mn.id);
	LONGS_EQUAL(range_base_addr, mn.start);
	LONGS_EQUAL(range_size, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));

	/* Test the maap_init_client() function again with the same information */
	LONGS_EQUAL(0, maap_init_client(&mc, &sender2_in, range_base_addr, range_size));
	LONGS_EQUAL(range_base_addr, mc.address_base);
	LONGS_EQUAL(range_size, mc.range_len);
	CHECK(mc.ranges == NULL);
	CHECK(mc.timer_queue == NULL);
	CHECK(mc.timer != NULL);
	CHECK(mc.net != NULL);
	CHECK(mc.initialized);

	/* We should receive exactly one notification of the initialization. */
	sender_out = NULL;
	memset(&mn, 0, sizeof(Maap_Notify));
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender2_in);
	LONGS_EQUAL(MAAP_NOTIFY_INITIALIZED, mn.kind);
	LONGS_EQUAL(-1, mn.id);
	LONGS_EQUAL(range_base_addr, mn.start);
	LONGS_EQUAL(range_size, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));

	/* Test the maap_init_client() function again with different information */
	LONGS_EQUAL(-1, maap_init_client(&mc, &sender3_in, range_base_addr + 1, range_size));
	LONGS_EQUAL(range_base_addr, mc.address_base);
	LONGS_EQUAL(range_size, mc.range_len);
	CHECK(mc.ranges == NULL);
	CHECK(mc.timer_queue == NULL);
	CHECK(mc.timer != NULL);
	CHECK(mc.net != NULL);
	CHECK(mc.initialized);

	/* We should receive exactly one notification of the initialization error. */
	sender_out = NULL;
	memset(&mn, 0, sizeof(Maap_Notify));
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender3_in);
	LONGS_EQUAL(MAAP_NOTIFY_INITIALIZED, mn.kind);
	LONGS_EQUAL(-1, mn.id);
	LONGS_EQUAL(range_base_addr, mn.start);
	LONGS_EQUAL(range_size, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_ALREADY_INITIALIZED, mn.result);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));

	/* We are done with the Maap_Client structure */
	maap_deinit_client(&mc);
}

TEST(maap_group, Reserve_Release)
{
	const uint8_t dest_mac[6] = MAAP_DEST_MAC;
	const uint8_t src_mac[6] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
	const uint64_t range_base_addr = MAAP_DYNAMIC_POOL_BASE;
	const uint32_t range_size = MAAP_DYNAMIC_POOL_SIZE;
	Maap_Client mc;
	Maap_Notify mn;
	const int sender1_in = 1, sender2_in = 2, sender3_in = 3;
	const void *sender_out;
	int id;
	int countdown;
	void *packet_data;
	MAAP_Packet packet_contents;
	int probe_packets_detected, announce_packets_detected;

	/* Initialize the Maap_Client structure */
	memset(&mc, 0, sizeof(Maap_Client));
	mc.dest_mac = convert_mac_address(dest_mac);
	mc.src_mac = convert_mac_address(src_mac);

	/* Initialize the range */
	/* We should receive exactly one notification of the initialization. */
	LONGS_EQUAL(0, maap_init_client(&mc, &sender1_in, range_base_addr, range_size));
	sender_out = NULL;
	memset(&mn, 0, sizeof(Maap_Notify));
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


	/* Reserve most of the range of addresses */
	id = maap_reserve_range(&mc, &sender2_in, range_size - 4);
	CHECK(id > 0);

	/* Handle any packets generated during the activity.
	 * Stop once we are notified of a result. */
	sender_out = NULL;
	memset(&mn, 0, sizeof(Maap_Notify));
	probe_packets_detected = 0, announce_packets_detected = 0;
	for (countdown = 100; countdown > 0 && !get_notify(&mc, &sender_out, &mn); --countdown)
	{
		/** @todo Verify that maap_get_delay_to_next_timer() returns an appropriate time delay. */
		LONGS_EQUAL(0, maap_handle_timer(&mc));

		if ((packet_data = Net_getNextQueuedPacket(mc.net)) != NULL)
		{
			/** @todo Add more verification that the packet to send is a proper packet. */
			LONGS_EQUAL(0, unpack_maap(&packet_contents, (const uint8_t *) packet_data));
			CHECK(packet_contents.message_type == MAAP_PROBE || packet_contents.message_type == MAAP_ANNOUNCE);
			if (packet_contents.message_type == MAAP_PROBE)
			{
				LONGS_EQUAL(0, announce_packets_detected);
				probe_packets_detected++;
			}
			else if (packet_contents.message_type == MAAP_ANNOUNCE)
			{
				LONGS_EQUAL(3, probe_packets_detected);
				LONGS_EQUAL(0, announce_packets_detected);
				announce_packets_detected++;
			}
			Net_freeQueuedPacket(mc.net, packet_data);
		}
	}
	CHECK(countdown > 0);
	LONGS_EQUAL(3, probe_packets_detected);
	LONGS_EQUAL(1, announce_packets_detected);

	/* Verify that the notification indicated a successful reservation. */
	CHECK(sender_out == &sender2_in);
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
	LONGS_EQUAL(id, mn.id);
	CHECK(mn.start >= range_base_addr);
	CHECK(mn.start + mn.count - 1 <= range_base_addr + range_size - 1);
	LONGS_EQUAL(range_size - 4, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


	/* Try another reasonable reservation.  It should fail, as there is not enough space available. */
	LONGS_EQUAL(-1, maap_reserve_range(&mc, &sender3_in, 10));

	/* We should receive exactly one notification of the reservation error. */
	sender_out = NULL;
	memset(&mn, 0, sizeof(Maap_Notify));
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender3_in);
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
	LONGS_EQUAL(-1, mn.id);
	LONGS_EQUAL(0, mn.start);
	LONGS_EQUAL(10, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE, mn.result);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


	/** @todo Verify the status of our existing reservation */


	/* Release the first reservation, and verify that we get one notification of the release. */
	maap_release_range(&mc, &sender2_in, id);
	sender_out = NULL;
	memset(&mn, 0, sizeof(Maap_Notify));
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender2_in);
	LONGS_EQUAL(MAAP_NOTIFY_RELEASED, mn.kind);
	LONGS_EQUAL(id, mn.id);
	CHECK(mn.start >= range_base_addr);
	CHECK(mn.start + mn.count - 1 <= range_base_addr + range_size - 1);
	LONGS_EQUAL(range_size - 4, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


	/* Try our reasonable reservation again. */
	id = maap_reserve_range(&mc, &sender3_in, 10);
	CHECK(id > 0);

	/* Handle any packets generated during the activity.
	 * Stop once we are notified of a result. */
	sender_out = NULL;
	memset(&mn, 0, sizeof(Maap_Notify));
	probe_packets_detected = 0, announce_packets_detected = 0;
	for (countdown = 100; countdown > 0 && !get_notify(&mc, &sender_out, &mn); --countdown)
	{
		/** @todo Verify that maap_get_delay_to_next_timer() returns an appropriate time delay. */
		LONGS_EQUAL(0, maap_handle_timer(&mc));

		if ((packet_data = Net_getNextQueuedPacket(mc.net)) != NULL)
		{
			/** @todo Add more verification that the packet to send is a proper packet. */
			LONGS_EQUAL(0, unpack_maap(&packet_contents, (const uint8_t *) packet_data));
			CHECK(packet_contents.message_type == MAAP_PROBE || packet_contents.message_type == MAAP_ANNOUNCE);
			if (packet_contents.message_type == MAAP_PROBE)
			{
				LONGS_EQUAL(0, announce_packets_detected);
				probe_packets_detected++;
			}
			else if (packet_contents.message_type == MAAP_ANNOUNCE)
			{
				LONGS_EQUAL(3, probe_packets_detected);
				LONGS_EQUAL(0, announce_packets_detected);
				announce_packets_detected++;
			}
			Net_freeQueuedPacket(mc.net, packet_data);
		}
	}
	CHECK(countdown > 0);
	LONGS_EQUAL(3, probe_packets_detected);
	LONGS_EQUAL(1, announce_packets_detected);

	/* Verify that the notification indicated a successful reservation. */
	CHECK(sender_out == &sender3_in);
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
	LONGS_EQUAL(id, mn.id);
	CHECK(mn.start >= range_base_addr);
	CHECK(mn.start + mn.count - 1 <= range_base_addr + range_size - 1);
	LONGS_EQUAL(10, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


	/* We are done with the Maap_Client structure */
	maap_deinit_client(&mc);
}
