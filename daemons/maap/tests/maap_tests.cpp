#include <stdio.h>
#include <string.h>

#include "CppUTest/TestHarness.h"

extern "C" {

#include "maap.h"
#include "maap_packet.h"

#define TEST_DEST_ADDR 0x91E0F000FF00
#define TEST_SRC_ADDR  0x123456789abc

#define TEST_REMOTE_ADDR_LOWER  0x777777777777 /* Remote address that we should defer to */
#define TEST_REMOTE_ADDR_HIGHER 0x1111111111ee /* Remote address that we should ignore */


static void verify_sent_packets(Maap_Client *p_mc, Maap_Notify *p_mn,
	const void **p_sender_out,
	int *p_probe_packets_detected, int *p_announce_packets_detected,
	int send_defend /* = -1 */);


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
	const uint64_t range_base_addr = MAAP_DYNAMIC_POOL_BASE + 0x1000;
	const uint32_t range_size = MAAP_DYNAMIC_POOL_SIZE - 0x800;
	Maap_Client mc;
	Maap_Notify mn;
	const int sender1_in = 1, sender2_in = 2, sender3_in = 3;
	const void *sender_out;

	/* Initialize the Maap_Client structure */
	memset(&mc, 0, sizeof(Maap_Client));
	mc.dest_mac = TEST_DEST_ADDR;
	mc.src_mac = TEST_SRC_ADDR;

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
	const uint64_t range_base_addr = MAAP_DYNAMIC_POOL_BASE;
	const uint32_t range_size = MAAP_DYNAMIC_POOL_SIZE;
	Maap_Client mc;
	Maap_Notify mn;
	const int sender1_in = 1, sender2_in = 2, sender3_in = 3;
	const void *sender_out;
	int id;
	int probe_packets_detected, announce_packets_detected;

	/* Initialize the Maap_Client structure */
	memset(&mc, 0, sizeof(Maap_Client));
	mc.dest_mac = TEST_DEST_ADDR;
	mc.src_mac = TEST_SRC_ADDR;

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
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1);
	LONGS_EQUAL(4, probe_packets_detected);
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
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1);
	LONGS_EQUAL(4, probe_packets_detected);
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

TEST(maap_group, Retry_On_Defend)
{
	const uint64_t range_base_addr = MAAP_DYNAMIC_POOL_BASE;
	const uint32_t range_size = MAAP_DYNAMIC_POOL_SIZE;
	Maap_Client mc;
	Maap_Notify mn;
	const int sender1_in = 1, sender2_in = 2;
	const void *sender_out;
	int id;
	int probe_packets_detected, announce_packets_detected;

	/* Initialize the Maap_Client structure */
	memset(&mc, 0, sizeof(Maap_Client));
	mc.dest_mac = TEST_DEST_ADDR;
	mc.src_mac = TEST_SRC_ADDR;

	/* Initialize the range */
	/* We should receive exactly one notification of the initialization. */
	LONGS_EQUAL(0, maap_init_client(&mc, &sender1_in, range_base_addr, range_size));
	sender_out = NULL;
	memset(&mn, 0, sizeof(Maap_Notify));
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


	/* Try to reserve a block of addresses. */
	id = maap_reserve_range(&mc, &sender2_in, 10);
	CHECK(id > 0);

	/* Fake a Defend packet after the first probe. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, 1);
	LONGS_EQUAL(1, probe_packets_detected);
	LONGS_EQUAL(0, announce_packets_detected);

	/* We should not have a notification yet. */
	CHECK(sender_out == NULL);

	/* Fake a Defend packet after the third probe. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, 3);
	LONGS_EQUAL(3, probe_packets_detected);
	LONGS_EQUAL(0, announce_packets_detected);

	/* We should not have a notification yet. */
	CHECK(sender_out == NULL);

	/* Allow this attempt to succeed. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1);
	LONGS_EQUAL(4, probe_packets_detected);
	LONGS_EQUAL(1, announce_packets_detected);

	/* Verify that the notification indicated a successful reservation. */
	CHECK(sender_out == &sender2_in);
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

TEST(maap_group, Defend)
{
	const uint64_t range_base_addr = MAAP_DYNAMIC_POOL_BASE;
	const uint32_t range_size = MAAP_DYNAMIC_POOL_SIZE;
	Maap_Client mc;
	Maap_Notify mn;
	const int sender1_in = 1, sender2_in = 2;
	const void *sender_out;
	int id;
	uint64_t range_reserved_start;
	uint32_t range_reserved_count;
	int probe_packets_detected, announce_packets_detected;
	int countdown;
	void *packet_data = NULL;
	MAAP_Packet packet_contents;
	MAAP_Packet probe_packet;
	uint8_t probe_buffer[MAAP_NET_BUFFER_SIZE];

	/* Initialize the Maap_Client structure */
	memset(&mc, 0, sizeof(Maap_Client));
	mc.dest_mac = TEST_DEST_ADDR;
	mc.src_mac = TEST_SRC_ADDR;

	/* Initialize the range */
	/* We should receive exactly one notification of the initialization. */
	LONGS_EQUAL(0, maap_init_client(&mc, &sender1_in, range_base_addr, range_size));
	sender_out = NULL;
	memset(&mn, 0, sizeof(Maap_Notify));
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));

	/* Reserve a block of addresses. */
	id = maap_reserve_range(&mc, &sender2_in, 10);
	CHECK(id > 0);
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1);
	LONGS_EQUAL(4, probe_packets_detected);
	LONGS_EQUAL(1, announce_packets_detected);
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(id, mn.id);

	/* Save the results for use later. */
	range_reserved_start = mn.start;
	range_reserved_count = mn.count;


	/* Wait a while to see if an Announce is sent. */
	for (countdown = 10000; countdown > 0 && (packet_data = Net_getNextQueuedPacket(mc.net)) == NULL; --countdown)
	{
		/* Let some time pass.... */
		LONGS_EQUAL(0, maap_handle_timer(&mc));

		/* Verify that we haven't received a notification. */
		LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
	}
	CHECK(countdown > 0);
	LONGS_EQUAL(0, unpack_maap(&packet_contents, (const uint8_t *) packet_data));
	CHECK(packet_contents.message_type == MAAP_ANNOUNCE);
	Net_freeQueuedPacket(mc.net, packet_data);


	/* More time passes.... */
	for (countdown = 10; countdown > 0 && (packet_data = Net_getNextQueuedPacket(mc.net)) == NULL; --countdown)
	{
		/* Let some time pass.... */
		LONGS_EQUAL(0, maap_handle_timer(&mc));

		/* Verify that we haven't received a notification. */
		LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
	}
	LONGS_EQUAL(0, countdown);
	CHECK(packet_data == NULL);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


	/* Fake a probe request that conflicts with the start of our range. */
	init_packet(&probe_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_HIGHER);
	probe_packet.message_type = MAAP_PROBE;
	probe_packet.requested_start_address = range_reserved_start - 4; /* Use the start of our range. */
	probe_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&probe_packet, probe_buffer));
	maap_handle_packet(&mc, probe_buffer, MAAP_NET_BUFFER_SIZE);

	/* Verify that we defended our range. */
	LONGS_EQUAL(0, maap_handle_timer(&mc));
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
	CHECK((packet_data = Net_getNextQueuedPacket(mc.net)) != NULL);
	LONGS_EQUAL(0, unpack_maap(&packet_contents, (const uint8_t *) packet_data));
	CHECK(packet_contents.message_type == MAAP_DEFEND);
	CHECK(packet_contents.requested_start_address == probe_packet.requested_start_address);
	CHECK(packet_contents.requested_count == probe_packet.requested_count);
	CHECK(packet_contents.conflict_start_address == range_reserved_start);
	CHECK(packet_contents.conflict_count == 1); /* Only one address conflicts */
	Net_freeQueuedPacket(mc.net, packet_data);


	/* Fake a probe request that conflicts with the end of our range. */
	probe_packet.requested_start_address = range_reserved_start + range_reserved_count - 1; /* Use the end of our range. */
	probe_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&probe_packet, probe_buffer));
	maap_handle_packet(&mc, probe_buffer, MAAP_NET_BUFFER_SIZE);

	/* Verify that we defended our range. */
	LONGS_EQUAL(0, maap_handle_timer(&mc));
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
	CHECK((packet_data = Net_getNextQueuedPacket(mc.net)) != NULL);
	LONGS_EQUAL(0, unpack_maap(&packet_contents, (const uint8_t *) packet_data));
	CHECK(packet_contents.message_type == MAAP_DEFEND);
	CHECK(packet_contents.requested_start_address == probe_packet.requested_start_address);
	CHECK(packet_contents.requested_count == probe_packet.requested_count);
	CHECK(packet_contents.conflict_start_address == probe_packet.requested_start_address);
	CHECK(packet_contents.conflict_count == 1); /* Only one address conflicts */
	Net_freeQueuedPacket(mc.net, packet_data);


	/* Try some probe requests adjacent to, but not overlapping, our range. */
	probe_packet.requested_start_address = range_reserved_start - 5; /* Before the start of our range. */
	probe_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&probe_packet, probe_buffer));
	maap_handle_packet(&mc, probe_buffer, MAAP_NET_BUFFER_SIZE);
	probe_packet.requested_start_address = range_reserved_start + range_reserved_count; /* After the end of our range. */
	probe_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&probe_packet, probe_buffer));
	maap_handle_packet(&mc, probe_buffer, MAAP_NET_BUFFER_SIZE);

	/* Verify that we didn't defended our range. */
	LONGS_EQUAL(0, maap_handle_timer(&mc));
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
	CHECK((packet_data = Net_getNextQueuedPacket(mc.net)) == NULL);


	/* Fake an announce request that conflicts with the start of our range. */
	init_packet(&probe_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_LOWER);
	probe_packet.message_type = MAAP_ANNOUNCE;
	probe_packet.requested_start_address = range_reserved_start - 2; /* Overlap the start of our range. */
	probe_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&probe_packet, probe_buffer));
	maap_handle_packet(&mc, probe_buffer, MAAP_NET_BUFFER_SIZE);

	/* Verify that we yielded our range, and did not defend it. */
	LONGS_EQUAL(0, maap_handle_timer(&mc));
	CHECK((packet_data = Net_getNextQueuedPacket(mc.net)) == NULL);
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	LONGS_EQUAL(MAAP_NOTIFY_YIELDED, mn.kind);
	LONGS_EQUAL(id, mn.id);
	LONGS_EQUAL(range_reserved_start, mn.start);
	LONGS_EQUAL(range_reserved_count, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);


	/* We are done with the Maap_Client structure */
	maap_deinit_client(&mc);
}


static void verify_sent_packets(Maap_Client *p_mc, Maap_Notify *p_mn,
	const void **p_sender_out,
	int *p_probe_packets_detected, int *p_announce_packets_detected,
	int send_defend)
{
	int countdown;
	void *packet_data;
	MAAP_Packet packet_contents;

	/* Handle any packets generated during the activity.
	 * Stop once we are notified of a result. */
	*p_sender_out = NULL;
	memset(p_mn, 0, sizeof(Maap_Notify));
	*p_probe_packets_detected = 0;
	*p_announce_packets_detected = 0;
	for (countdown = 1000; countdown > 0 && !get_notify(p_mc, p_sender_out, p_mn); --countdown)
	{
		/** @todo Verify that maap_get_delay_to_next_timer() returns an appropriate time delay. */
		LONGS_EQUAL(0, maap_handle_timer(p_mc));

		if ((packet_data = Net_getNextQueuedPacket(p_mc->net)) != NULL)
		{
			/** @todo Add more verification that the packet to send is a proper packet. */
			LONGS_EQUAL(0, unpack_maap(&packet_contents, (const uint8_t *) packet_data));
			Net_freeQueuedPacket(p_mc->net, packet_data);
			CHECK(packet_contents.message_type >= MAAP_PROBE && packet_contents.message_type <= MAAP_ANNOUNCE);
			if (packet_contents.message_type == MAAP_PROBE)
			{
				CHECK(*p_probe_packets_detected < 4);
				LONGS_EQUAL(0, *p_announce_packets_detected);
				(*p_probe_packets_detected)++;
				/* printf("Probe packet sent (%d)\n", *p_probe_packets_detected); */

				if (*p_probe_packets_detected == send_defend)
				{
					MAAP_Packet defend_packet;
					uint8_t defend_buffer[MAAP_NET_BUFFER_SIZE];

					/* Send a defend packet in response to the probe packet. */
					init_packet(&defend_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_HIGHER);
					defend_packet.message_type = MAAP_DEFEND;
					defend_packet.requested_start_address = packet_contents.requested_start_address;
					defend_packet.requested_count = packet_contents.requested_count;
					defend_packet.conflict_start_address = packet_contents.requested_start_address; /* Just use the same address */
					defend_packet.conflict_count = 1;
					LONGS_EQUAL(0, pack_maap(&defend_packet, defend_buffer));
					maap_handle_packet(p_mc, defend_buffer, MAAP_NET_BUFFER_SIZE);
					/* printf("Defend packet received\n"); */

					/* Stop the loop at this point so the caller can see what happens next. */
					break;
				}
			}
			else if (packet_contents.message_type == MAAP_ANNOUNCE)
			{
				LONGS_EQUAL(4, *p_probe_packets_detected);
				LONGS_EQUAL(0, *p_announce_packets_detected);
				(*p_announce_packets_detected)++;
				/* printf("Announce packet sent (%d)\n", *p_announce_packets_detected); */
			}
			else
			{
				/* Unexpected MAAP_DEFEND packet. */
				CHECK(0);
			}
		}
	}
	CHECK(countdown > 0);
}