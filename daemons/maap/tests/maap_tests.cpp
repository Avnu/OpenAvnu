#include <stdio.h>
#include <string.h>

#include "CppUTest/TestHarness.h"

extern "C" {

#include "maap.h"
#include "maap_packet.h"
#include "../test/maap_log_dummy.h"
#include "../test/maap_timer_dummy.h"

#ifdef _WIN32
	/* Windows-specific header values */
#define random() rand()
#define srandom(s) srand(s)
#endif

#define TEST_DEST_ADDR 0x91E0F000FF00
#define TEST_SRC_ADDR  0x123456789abc

#define TEST_REMOTE_ADDR_LOWER  0x777777777777 /* Remote address that we should defer to */
#define TEST_REMOTE_ADDR_HIGHER 0x1111111111ee /* Remote address that we should ignore */


static void verify_sent_packets(Maap_Client *p_mc, Maap_Notify *p_mn,
	const void **p_sender_out,
	int *p_probe_packets_detected, int *p_announce_packets_detected,
	int send_probe /* = -1 */, int send_announce /* = -1 */, int send_defend /* = -1 */,
	uint64_t remote_addr, int stop_after_probe);


} TEST_GROUP(maap_group)
{
	void setup() {
		/* Try a variety of "random" values over subsequent tests. */
		srandom((unsigned int) time(NULL));
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
	uint64_t range_reserved_start;
	uint32_t range_reserved_count;
	int probe_packets_detected, announce_packets_detected;
	int64_t next_delay;
	void *packet_data = NULL;
	MAAP_Packet packet_contents;

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
	id = maap_reserve_range(&mc, &sender2_in, 0, range_size - 4);
	CHECK(id > 0);

	/* Verify that we get an acquiring notification. */
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender2_in);
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRING, mn.kind);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);

	/* Verify that the reservation does not yet have a status */
	maap_range_status(&mc, &sender1_in, id);
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender1_in);
	LONGS_EQUAL(MAAP_NOTIFY_STATUS, mn.kind);
	LONGS_EQUAL(id, mn.id);
	LONGS_EQUAL(0, mn.start);
	LONGS_EQUAL(0, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID, mn.result);

	/* Handle any packets generated during the activity.
	 * Stop once we are notified of a result. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 0);
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

	/* Save the results for use later. */
	range_reserved_start = mn.start;
	range_reserved_count = mn.count;


	/* Wait a while to see if an Announce is sent. */
	/* Verify that the wait is 30-32 seconds. */
	next_delay = maap_get_delay_to_next_timer(&mc);
	CHECK(next_delay > MAAP_ANNOUNCE_INTERVAL_BASE * 1000000LL);
	CHECK(next_delay < (MAAP_ANNOUNCE_INTERVAL_BASE + MAAP_ANNOUNCE_INTERVAL_VARIATION) * 1000000LL);
	Time_increaseNanos(next_delay);
	LONGS_EQUAL(0, maap_handle_timer(&mc));
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
	packet_data = Net_getNextQueuedPacket(mc.net);
	CHECK(packet_data != NULL);
	LONGS_EQUAL(0, unpack_maap(&packet_contents, (const uint8_t *) packet_data));
	CHECK(packet_contents.message_type == MAAP_ANNOUNCE);
	Net_freeQueuedPacket(mc.net, packet_data);

	/* More time passes.... */
	CHECK(maap_get_delay_to_next_timer(&mc) > MAAP_ANNOUNCE_INTERVAL_BASE * 1000000LL);
	Time_increaseNanos(MAAP_ANNOUNCE_INTERVAL_BASE / 2 * 1000000LL);
	LONGS_EQUAL(0, maap_handle_timer(&mc));
	CHECK(Net_getNextQueuedPacket(mc.net) == NULL);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


	/* Try another reasonable reservation.  It should fail, as there is not enough space available. */
	LONGS_EQUAL(-1, maap_reserve_range(&mc, &sender3_in, 0, 10));

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


	/* Verify the status of our existing reservation */
	maap_range_status(&mc, &sender1_in, id);
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender1_in);
	LONGS_EQUAL(MAAP_NOTIFY_STATUS, mn.kind);
	LONGS_EQUAL(id, mn.id);
	LONGS_EQUAL(range_reserved_start, mn.start);
	LONGS_EQUAL(range_reserved_count, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


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


	/* Verify that the status of the reservation is invalid. */
	maap_range_status(&mc, &sender1_in, id);
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender1_in);
	LONGS_EQUAL(MAAP_NOTIFY_STATUS, mn.kind);
	LONGS_EQUAL(id, mn.id);
	LONGS_EQUAL(0, mn.start);
	LONGS_EQUAL(0, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID, mn.result);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


	/* Try our reasonable reservation again.  Supply a preferred address. */
	id = maap_reserve_range(&mc, &sender3_in, range_base_addr + 100, 10);
	CHECK(id > 0);

	/* Handle any packets generated during the activity.
	 * Stop once we are notified of a result. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 0);
	LONGS_EQUAL(4, probe_packets_detected);
	LONGS_EQUAL(1, announce_packets_detected);

	/* Verify that the notification indicated a successful reservation at the preferred address. */
	CHECK(sender_out == &sender3_in);
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
	LONGS_EQUAL(id, mn.id);
	LONGS_EQUAL(range_base_addr + 100, mn.start);
	LONGS_EQUAL(10, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));

	/* Save the results for use later. */
	range_reserved_start = mn.start;
	range_reserved_count = mn.count;


	/* Release the reservation. */
	maap_release_range(&mc, &sender3_in, id);
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender3_in);
	LONGS_EQUAL(MAAP_NOTIFY_RELEASED, mn.kind);
	LONGS_EQUAL(id, mn.id);
	LONGS_EQUAL(range_reserved_start, mn.start);
	LONGS_EQUAL(range_reserved_count, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
	CHECK(Net_getNextQueuedPacket(mc.net) == NULL);

	/* Verify that no additional announce packets are sent. */
	Time_increaseNanos((MAAP_ANNOUNCE_INTERVAL_BASE + MAAP_ANNOUNCE_INTERVAL_VARIATION) * 1000000ull);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
	CHECK(Net_getNextQueuedPacket(mc.net) == NULL);

	/* We are done with the Maap_Client structure */
	maap_deinit_client(&mc);
}

TEST(maap_group, Probing_vs_Probes)
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
	id = maap_reserve_range(&mc, &sender2_in, 0, 10);
	CHECK(id > 0);

	/* Fake a Probe packet we must defer to after the first probe. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, 1, -1, -1, TEST_REMOTE_ADDR_LOWER, 1);
	LONGS_EQUAL(1, probe_packets_detected);
	LONGS_EQUAL(0, announce_packets_detected);

	/* We should not have a notification yet. */
	CHECK(sender_out == NULL);

	/* Handle the Acquiring message. */
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRING, mn.kind);

	/* Fake a Probe packet we must defer to after the third probe. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, 3, -1, -1, TEST_REMOTE_ADDR_LOWER, 3);
	LONGS_EQUAL(3, probe_packets_detected);
	LONGS_EQUAL(0, announce_packets_detected);

	/* We should not have a notification yet. */
	CHECK(sender_out == NULL);

	/* Handle the Acquiring message. */
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRING, mn.kind);

	/* Fake a Probe packet we should ignore after the second probe. */
	/* This attempt should succeed. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, 2, -1, -1, TEST_REMOTE_ADDR_HIGHER, 0);
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

TEST(maap_group, Probing_vs_Announces)
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
	id = maap_reserve_range(&mc, &sender2_in, 0, 10);
	CHECK(id > 0);

	/* Fake an Announce packet after the first probe. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, 1, -1, TEST_REMOTE_ADDR_HIGHER, 1);
	LONGS_EQUAL(1, probe_packets_detected);
	LONGS_EQUAL(0, announce_packets_detected);

	/* We should not have a notification yet. */
	CHECK(sender_out == NULL);

	/* Handle the Acquiring message. */
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRING, mn.kind);

	/* Fake an Announce packet after the third probe. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, 3, -1, TEST_REMOTE_ADDR_LOWER, 3);
	LONGS_EQUAL(3, probe_packets_detected);
	LONGS_EQUAL(0, announce_packets_detected);

	/* We should not have a notification yet. */
	CHECK(sender_out == NULL);

	/* Handle the Acquiring message. */
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRING, mn.kind);

	/* Allow this attempt to succeed. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 0);
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

TEST(maap_group, Probing_vs_Defends)
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
	id = maap_reserve_range(&mc, &sender2_in, 0, 10);
	CHECK(id > 0);

	/* Fake a Defend packet after the first probe. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, 1, TEST_REMOTE_ADDR_LOWER, 1);
	LONGS_EQUAL(1, probe_packets_detected);
	LONGS_EQUAL(0, announce_packets_detected);

	/* We should not have a notification yet. */
	CHECK(sender_out == NULL);

	/* Handle the Acquiring message. */
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRING, mn.kind);

	/* Fake a Defend packet after the third probe. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, 3, TEST_REMOTE_ADDR_HIGHER, 3);
	LONGS_EQUAL(3, probe_packets_detected);
	LONGS_EQUAL(0, announce_packets_detected);

	/* We should not have a notification yet. */
	CHECK(sender_out == NULL);

	/* Handle the Acquiring message. */
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRING, mn.kind);

	/* Allow this attempt to succeed. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 0);
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

TEST(maap_group, Defending_vs_Probes)
{
	const uint64_t range_base_addr = MAAP_DYNAMIC_POOL_BASE;
	const uint32_t range_size = MAAP_DYNAMIC_POOL_SIZE;
	Maap_Client mc;
	Maap_Notify mn;
	const int sender1_in = 1, sender2_in = 2, sender3_in = 3;
	const void *sender_out;
	int id;
	uint64_t range_reserved_start;
	uint32_t range_reserved_count;
	int probe_packets_detected, announce_packets_detected;
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
	id = maap_reserve_range(&mc, &sender2_in, 0, 10);
	CHECK(id > 0);
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 0);
	LONGS_EQUAL(4, probe_packets_detected);
	LONGS_EQUAL(1, announce_packets_detected);
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(id, mn.id);

	/* Save the results for use later. */
	range_reserved_start = mn.start;
	range_reserved_count = mn.count;


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

	/* Verify the logging. */
	CHECK(strcmp(Logging_getLastTag(), "INFO") == 0);
	CHECK(strcmp(Logging_getLastMessage(), "DEFEND!") == 0);


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

	/* Verify the logging. */
	CHECK(strcmp(Logging_getLastTag(), "INFO") == 0);
	CHECK(strcmp(Logging_getLastMessage(), "DEFEND!") == 0);


	/* Try some probe requests adjacent to, but not overlapping, our range. */
	probe_packet.requested_start_address = range_reserved_start - 5; /* Before the start of our range. */
	probe_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&probe_packet, probe_buffer));
	maap_handle_packet(&mc, probe_buffer, MAAP_NET_BUFFER_SIZE);
	probe_packet.requested_start_address = range_reserved_start + range_reserved_count; /* After the end of our range. */
	probe_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&probe_packet, probe_buffer));
	maap_handle_packet(&mc, probe_buffer, MAAP_NET_BUFFER_SIZE);

	/* Verify that we didn't defend our range. */
	LONGS_EQUAL(0, maap_handle_timer(&mc));
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
	CHECK((packet_data = Net_getNextQueuedPacket(mc.net)) == NULL);


	/* Verify that the status of the range is valid. */
	maap_range_status(&mc, &sender3_in, id);
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender3_in);
	LONGS_EQUAL(MAAP_NOTIFY_STATUS, mn.kind);
	LONGS_EQUAL(id, mn.id);
	LONGS_EQUAL(range_reserved_start, mn.start);
	LONGS_EQUAL(range_reserved_count, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);


	/* We are done with the Maap_Client structure */
	maap_deinit_client(&mc);
}

TEST(maap_group, Defending_vs_Announces)
{
	const uint64_t range_base_addr = MAAP_DYNAMIC_POOL_BASE;
	const uint32_t range_size = MAAP_DYNAMIC_POOL_SIZE;
	Maap_Client mc;
	Maap_Notify mn;
	const int sender1_in = 1, sender2_in = 2, sender3_in = 3;
	const void *sender_out;
	int id;
	uint64_t range_reserved_start;
	uint32_t range_reserved_count;
	int probe_packets_detected, announce_packets_detected;
	void *packet_data = NULL;
	MAAP_Packet announce_packet;
	uint8_t announce_buffer[MAAP_NET_BUFFER_SIZE];

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
	id = maap_reserve_range(&mc, &sender2_in, 0, 10);
	CHECK(id > 0);
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 0);
	LONGS_EQUAL(4, probe_packets_detected);
	LONGS_EQUAL(1, announce_packets_detected);
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(id, mn.id);

	/* Save the results for use later. */
	range_reserved_start = mn.start;
	range_reserved_count = mn.count;


	/* Try some announcements adjacent to, but not overlapping, our range. */
	init_packet(&announce_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_LOWER);
	announce_packet.message_type = MAAP_ANNOUNCE;
	announce_packet.requested_start_address = range_reserved_start - 5; /* Before the start of our range. */
	announce_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&announce_packet, announce_buffer));
	maap_handle_packet(&mc, announce_buffer, MAAP_NET_BUFFER_SIZE);
	announce_packet.requested_start_address = range_reserved_start + range_reserved_count; /* After the end of our range. */
	announce_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&announce_packet, announce_buffer));
	maap_handle_packet(&mc, announce_buffer, MAAP_NET_BUFFER_SIZE);

	/* Try an announcement that overlaps our range, but from an address we should ignore. */
	init_packet(&announce_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_HIGHER);
	announce_packet.message_type = MAAP_ANNOUNCE;
	announce_packet.requested_start_address = range_reserved_start - 2; /* Overlap the start of our range. */
	announce_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&announce_packet, announce_buffer));
	maap_handle_packet(&mc, announce_buffer, MAAP_NET_BUFFER_SIZE);

	/* Verify that we didn't react. */
	LONGS_EQUAL(0, maap_handle_timer(&mc));
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
	CHECK((packet_data = Net_getNextQueuedPacket(mc.net)) == NULL);

	/* Verify the logging. */
	CHECK(strcmp(Logging_getLastTag(), "INFO") == 0);
	CHECK(strcmp(Logging_getLastMessage(), "IGNORE") == 0);

	/* Verify that the status of the range is still valid. */
	maap_range_status(&mc, &sender3_in, id);
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender3_in);
	LONGS_EQUAL(MAAP_NOTIFY_STATUS, mn.kind);
	LONGS_EQUAL(id, mn.id);
	LONGS_EQUAL(range_reserved_start, mn.start);
	LONGS_EQUAL(range_reserved_count, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);


	/* Fake an announcement that conflicts with the start of our range. */
	init_packet(&announce_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_LOWER);
	announce_packet.message_type = MAAP_ANNOUNCE;
	announce_packet.requested_start_address = range_reserved_start - 2; /* Overlap the start of our range. */
	announce_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&announce_packet, announce_buffer));
	maap_handle_packet(&mc, announce_buffer, MAAP_NET_BUFFER_SIZE);

	/* Verify that we yielded our range, and did not defend it. */
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender2_in);
	if (mn.kind == MAAP_NOTIFY_ACQUIRING) {
		/* Skip over the acquiring notification. */
		LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
		CHECK(sender_out == &sender2_in);
	}
	LONGS_EQUAL(MAAP_NOTIFY_YIELDED, mn.kind);
	LONGS_EQUAL(id, mn.id);
	LONGS_EQUAL(range_reserved_start, mn.start);
	LONGS_EQUAL(range_reserved_count, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);

	/* Verify the logging. */
	CHECK(strcmp(Logging_getLastTag(), "INFO") == 0);
	CHECK(strcmp(Logging_getLastMessage(), "YIELD") == 0);

	/* Verify that we requested a different range to replace the yielded one. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 0);
	LONGS_EQUAL(4, probe_packets_detected);
	LONGS_EQUAL(1, announce_packets_detected);
	CHECK(sender_out == &sender2_in);
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(id, mn.id); /* The ID should not have changed */

	/* Verify that the new range doesn't overlap the old range. */
	CHECK(mn.start + mn.count - 1 < range_reserved_start || range_reserved_start + range_reserved_count - 1 < mn.start);

	/* We are done with the Maap_Client structure */
	maap_deinit_client(&mc);
}

TEST(maap_group, Defending_vs_Defends)
{
	const uint64_t range_base_addr = MAAP_DYNAMIC_POOL_BASE;
	const uint32_t range_size = MAAP_DYNAMIC_POOL_SIZE;
	Maap_Client mc;
	Maap_Notify mn;
	const int sender1_in = 1, sender2_in = 2, sender3_in = 3;
	const void *sender_out;
	int id;
	uint64_t range_reserved_start;
	uint32_t range_reserved_count;
	int probe_packets_detected, announce_packets_detected;
	void *packet_data = NULL;
	MAAP_Packet defend_packet;
	uint8_t defend_buffer[MAAP_NET_BUFFER_SIZE];

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
	id = maap_reserve_range(&mc, &sender2_in, 0, 10);
	CHECK(id > 0);
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 0);
	LONGS_EQUAL(4, probe_packets_detected);
	LONGS_EQUAL(1, announce_packets_detected);
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(id, mn.id);

	/* Save the results for use later. */
	range_reserved_start = mn.start;
	range_reserved_count = mn.count;


	/* Try some defends adjacent to, but not overlapping, our range. */
	init_packet(&defend_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_LOWER);
	defend_packet.message_type = MAAP_DEFEND;
	defend_packet.requested_start_address = range_reserved_start - 5; /* Before the start of our range. */
	defend_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&defend_packet, defend_buffer));
	maap_handle_packet(&mc, defend_buffer, MAAP_NET_BUFFER_SIZE);
	defend_packet.requested_start_address = range_reserved_start + range_reserved_count; /* After the end of our range. */
	defend_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&defend_packet, defend_buffer));
	maap_handle_packet(&mc, defend_buffer, MAAP_NET_BUFFER_SIZE);

	/* Try a defend that overlaps our range, but from an address we should ignore. */
	init_packet(&defend_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_HIGHER);
	defend_packet.message_type = MAAP_DEFEND;
	defend_packet.requested_start_address = range_reserved_start - 2; /* Overlap the start of our range. */
	defend_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&defend_packet, defend_buffer));
	maap_handle_packet(&mc, defend_buffer, MAAP_NET_BUFFER_SIZE);

	/* Verify that we didn't react. */
	LONGS_EQUAL(0, maap_handle_timer(&mc));
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
	CHECK((packet_data = Net_getNextQueuedPacket(mc.net)) == NULL);

	/* Verify the logging. */
	CHECK(strcmp(Logging_getLastTag(), "INFO") == 0);
	CHECK(strcmp(Logging_getLastMessage(), "IGNORE") == 0);

	/* Verify that the status of the range is still valid. */
	maap_range_status(&mc, &sender3_in, id);
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender3_in);
	LONGS_EQUAL(MAAP_NOTIFY_STATUS, mn.kind);
	LONGS_EQUAL(id, mn.id);
	LONGS_EQUAL(range_reserved_start, mn.start);
	LONGS_EQUAL(range_reserved_count, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);


	/* Fake a defend that conflicts with the start of our range. */
	init_packet(&defend_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_LOWER);
	defend_packet.message_type = MAAP_DEFEND;
	defend_packet.requested_start_address = range_reserved_start - 2; /* Overlap the start of our range. */
	defend_packet.requested_count = 5;
	LONGS_EQUAL(0, pack_maap(&defend_packet, defend_buffer));
	maap_handle_packet(&mc, defend_buffer, MAAP_NET_BUFFER_SIZE);

	/* Verify that we yielded our range, and did not defend it. */
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender2_in);
	if (mn.kind == MAAP_NOTIFY_ACQUIRING) {
		/* Skip over the acquiring notification. */
		LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
		CHECK(sender_out == &sender2_in);
	}
	LONGS_EQUAL(MAAP_NOTIFY_YIELDED, mn.kind);
	LONGS_EQUAL(id, mn.id);
	LONGS_EQUAL(range_reserved_start, mn.start);
	LONGS_EQUAL(range_reserved_count, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);

	/* Verify the logging. */
	CHECK(strcmp(Logging_getLastTag(), "INFO") == 0);
	CHECK(strcmp(Logging_getLastMessage(), "YIELD") == 0);

	/* Verify that we requested a different range to replace the yielded one. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 0);
	LONGS_EQUAL(4, probe_packets_detected);
	LONGS_EQUAL(1, announce_packets_detected);
	CHECK(sender_out == &sender2_in);
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(id, mn.id); /* The ID should not have changed */

	/* Verify that the new range doesn't overlap the old range. */
	CHECK(mn.start + mn.count - 1 < range_reserved_start || range_reserved_start + range_reserved_count - 1 < mn.start);

	/* We are done with the Maap_Client structure */
	maap_deinit_client(&mc);
}

TEST(maap_group, Verify_Timing)
{
	const uint64_t range_base_addr = MAAP_DYNAMIC_POOL_BASE;
	const uint32_t range_size = MAAP_DYNAMIC_POOL_SIZE;
	Maap_Client mc;
	Maap_Notify mn;
	const int sender_in = 1;
	const void *sender_out;
	int id;
	int probe_packets_detected, announce_packets_detected;
	MAAP_Packet announce_packet;
	uint8_t announce_buffer[MAAP_NET_BUFFER_SIZE];
	void *packet_data;
	MAAP_Packet packet_contents;
	Time previous_time, current_time;

	/* Initialize the Maap_Client structure */
	memset(&mc, 0, sizeof(Maap_Client));
	mc.dest_mac = TEST_DEST_ADDR;
	mc.src_mac = TEST_SRC_ADDR;

	/* Initialize the range */
	/* We should receive exactly one notification of the initialization. */
	LONGS_EQUAL(0, maap_init_client(&mc, &sender_in, range_base_addr, range_size));
	sender_out = NULL;
	memset(&mn, 0, sizeof(Maap_Notify));
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


	/* Reserve a block of addresses. */
	id = maap_reserve_range(&mc, &sender_in, 0, 10);
	CHECK(id > 0);

	/* Verify the timing between Probes when interrupted. */
	for (int i = 0; i < 100; ++i)
	{
		/* Wait for three probes to be sent.
		 * Probe timing will be tested by verify_sent_packets. */
		verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 3);
		LONGS_EQUAL(3, probe_packets_detected);
		LONGS_EQUAL(0, announce_packets_detected);
		LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));

		/* Wait up to 500 ms */
		Time_increaseNanos((random() % MAAP_PROBE_INTERVAL_BASE) * 1000000LL);

		/* Send an announce that conflicts with the current reservation. */
		init_packet(&announce_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_LOWER);
		announce_packet.message_type = MAAP_ANNOUNCE;
		announce_packet.requested_start_address = mn.start;
		announce_packet.requested_count = mn.count;
		LONGS_EQUAL(0, pack_maap(&announce_packet, announce_buffer));
		maap_handle_packet(&mc, announce_buffer, MAAP_NET_BUFFER_SIZE);
	}


	/* Verify the timing between Probes when reservation is successful. */
	for (int i = 0; i < 100; ++i)
	{
		/* Wait for the previous attempt to succeed.
		 * Probe timing will be tested by verify_sent_packets. */
		verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 0);
		LONGS_EQUAL(4, probe_packets_detected);
		LONGS_EQUAL(1, announce_packets_detected);

		/* Verify that the notification indicated a successful reservation. */
		LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
		LONGS_EQUAL(id, mn.id);
		CHECK(mn.start >= range_base_addr);
		CHECK(mn.start + mn.count - 1 <= range_base_addr + range_size - 1);
		LONGS_EQUAL(10, mn.count);
		LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
		LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));

		/* Wait up to 30000 ms */
		Time_increaseNanos((random() % MAAP_ANNOUNCE_INTERVAL_BASE) * 1000000LL);

		/* Send an announce that conflicts with the current reservation. */
		init_packet(&announce_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_LOWER);
		announce_packet.message_type = MAAP_ANNOUNCE;
		announce_packet.requested_start_address = mn.start;
		announce_packet.requested_count = mn.count;
		LONGS_EQUAL(0, pack_maap(&announce_packet, announce_buffer));
		maap_handle_packet(&mc, announce_buffer, MAAP_NET_BUFFER_SIZE);

		/* Verify that we yielded our range, and did not defend it. */
		LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
		if (mn.kind == MAAP_NOTIFY_ACQUIRING) {
			/* Skip over the acquiring notification. */
			LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
		}
		LONGS_EQUAL(MAAP_NOTIFY_YIELDED, mn.kind);
		LONGS_EQUAL(id, mn.id);
		LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);

		/* Verify the logging. */
		CHECK(strcmp(Logging_getLastTag(), "INFO") == 0);
		CHECK(strcmp(Logging_getLastMessage(), "YIELD") == 0);
	}


	/* Wait for the reservation to succeed. */
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 0);
	LONGS_EQUAL(4, probe_packets_detected);
	LONGS_EQUAL(1, announce_packets_detected);
	LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));

	/* Record the time of the announcement. */
	Time_setFromMonotonicTimer(&previous_time);

	/* Verify the timing between Announcements */
	for (int i = 0; i < 100; ++i)
	{
		/* Wait for the next announcement. */
		Time_increaseNanos(maap_get_delay_to_next_timer(&mc));
		LONGS_EQUAL(0, maap_handle_timer(&mc));
		LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
		packet_data = Net_getNextQueuedPacket(mc.net);
		for (int j = 0; j < 100 && packet_data == NULL; ++j) {
			/* Timer expiration could be to free a reservation.  Keep waiting. */
			Time_increaseNanos(maap_get_delay_to_next_timer(&mc));
			LONGS_EQUAL(0, maap_handle_timer(&mc));
			LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
			packet_data = Net_getNextQueuedPacket(mc.net);
		}
		CHECK(packet_data != NULL);
		LONGS_EQUAL(0, unpack_maap(&packet_contents, (const uint8_t *) packet_data));
		Net_freeQueuedPacket(mc.net, packet_data);
		CHECK(packet_contents.message_type == MAAP_ANNOUNCE);

		/* Get the current time. */
		Time_setFromMonotonicTimer(&current_time);

		/* Determine the minimum time from the last announce. */
		Time time_min;
		Time_setFromNanos(&time_min, MAAP_ANNOUNCE_INTERVAL_BASE * 1000000LL);
		Time_add(&time_min, &previous_time);
		CHECK(Time_cmp(&time_min, &current_time) < 0);

		/* Determine the maximum time from the last announce. */
		Time time_max;
		Time_setFromNanos(&time_max, (MAAP_ANNOUNCE_INTERVAL_BASE + MAAP_ANNOUNCE_INTERVAL_VARIATION) * 1000000LL);
		Time_add(&time_max, &previous_time);
		CHECK(Time_cmp(&current_time, &time_max) < 0);

		/* Save the current time for the next comparison. */
		Time_setFromMonotonicTimer(&previous_time);
	}

	/* We are done with the Maap_Client structure */
	maap_deinit_client(&mc);
}

TEST(maap_group, Ignore_Versioning)
{
	const uint64_t range_base_addr = MAAP_DYNAMIC_POOL_BASE;
	const uint32_t range_size = MAAP_DYNAMIC_POOL_SIZE;
	Maap_Client mc;
	Maap_Notify mn;
	const int sender1_in = 1, sender2_in = 2, sender3_in = 3;
	const void *sender_out;
	int id;
	uint64_t range_reserved_start;
	uint32_t range_reserved_count;
	int probe_packets_detected, announce_packets_detected;
	MAAP_Packet custom_packet;
	uint8_t custom_buffer[MAAP_NET_BUFFER_SIZE];

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
	id = maap_reserve_range(&mc, &sender2_in, 0, 10);
	CHECK(id > 0);
	verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 0);
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

	/* Save the results for use later. */
	range_reserved_start = mn.start;
	range_reserved_count = mn.count;


	/* Send an unknown PDU with a greater maap_version value that overlaps our reservation. */
	init_packet(&custom_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_LOWER);
	custom_packet.maap_version++;
	custom_packet.message_type = 4; /* Not a valid message type. */
	custom_packet.requested_start_address = range_base_addr;
	custom_packet.requested_count = range_size;
	LONGS_EQUAL(0, pack_maap(&custom_packet, custom_buffer));
	maap_handle_packet(&mc, custom_buffer, MAAP_NET_BUFFER_SIZE);

	/* Verify that nothing happened. */
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
	CHECK(Net_getNextQueuedPacket(mc.net) == NULL);

	/* Verify the logging. */
	CHECK(strcmp(Logging_getLastTag(), "ERROR") == 0);
	CHECK(strcmp(Logging_getLastMessage(), "MAAP packet message type 4 not recognized") == 0);

	/* Verify that the logging is cleared after testing. */
	CHECK(strcmp(Logging_getLastTag(), "") == 0);
	CHECK(strcmp(Logging_getLastMessage(), "") == 0);

	/* Verify the status of our existing reservation */
	maap_range_status(&mc, &sender3_in, id);
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	CHECK(sender_out == &sender3_in);
	LONGS_EQUAL(MAAP_NOTIFY_STATUS, mn.kind);
	LONGS_EQUAL(id, mn.id);
	LONGS_EQUAL(range_reserved_start, mn.start);
	LONGS_EQUAL(range_reserved_count, mn.count);
	LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


	/* We are done with the Maap_Client structure */
	maap_deinit_client(&mc);
}

TEST(maap_group, Multiple_Conflicts_Defend)
{
	#define num_reservations 10

	const uint64_t range_base_addr = MAAP_DYNAMIC_POOL_BASE;
	const uint32_t range_size = MAAP_DYNAMIC_POOL_SIZE;
	Maap_Client mc;
	Maap_Notify mn;
	const int sender_in[num_reservations] = {0};
	const void *sender_out;
	int i;
	int id[num_reservations];
	int probe_packets_detected, announce_packets_detected;
	MAAP_Packet defend_packet;
	uint8_t defend_buffer[MAAP_NET_BUFFER_SIZE];
	int countdown;
	void *packet_data;
	MAAP_Packet packet_contents;

	/* Initialize the Maap_Client structure */
	memset(&mc, 0, sizeof(Maap_Client));
	mc.dest_mac = TEST_DEST_ADDR;
	mc.src_mac = TEST_SRC_ADDR;

	/* Initialize the range */
	/* We should receive exactly one notification of the initialization. */
	LONGS_EQUAL(0, maap_init_client(&mc, &sender_in[0], range_base_addr, range_size));
	sender_out = NULL;
	memset(&mn, 0, sizeof(Maap_Notify));
	LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


	/*
	 * Reserve some address ranges
	 */

	for (i = 0; i < num_reservations; ++i) {
		/* Reserve an address range */
		id[i] = maap_reserve_range(&mc, &sender_in[i], 0, 1);
		CHECK(id[i] > 0);
		verify_sent_packets(&mc, &mn, &sender_out, &probe_packets_detected, &announce_packets_detected, -1, -1, -1, 0, 0);
		LONGS_EQUAL(4, probe_packets_detected);
		LONGS_EQUAL(1, announce_packets_detected);
		CHECK(sender_out == &sender_in[i]);
		LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
		LONGS_EQUAL(id[i], mn.id);
		CHECK(mn.start >= range_base_addr);
		CHECK(mn.start + mn.count - 1 <= range_base_addr + range_size - 1);
		LONGS_EQUAL(1, mn.count);
		LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
		LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));
	}


	/*
	 * Verify that a Defend conflicting with all the reservations from a higher address is ignored.
	 */

	/* Send a Defend packet that conflicts with all the ranges. */
	init_packet(&defend_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_HIGHER);
	defend_packet.message_type = MAAP_DEFEND;
	defend_packet.requested_start_address = range_base_addr;
	defend_packet.requested_count = range_size;
	defend_packet.conflict_start_address = range_base_addr;
	defend_packet.conflict_count = range_size;
	LONGS_EQUAL(0, pack_maap(&defend_packet, defend_buffer));
	maap_handle_packet(&mc, defend_buffer, MAAP_NET_BUFFER_SIZE);

	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));

	/* Verify that the status of each range is valid. */
	for (i = 1; i < num_reservations; ++i) {
		maap_range_status(&mc, &sender_in[i], id[i]);
		LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
		CHECK(sender_out == &sender_in[i]);
		LONGS_EQUAL(MAAP_NOTIFY_STATUS, mn.kind);
		LONGS_EQUAL(id[i], mn.id);
		LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	}


	/*
	 * Verify that a Defend conflicting with all the reservations from a lower address results in new addresses.
	 *
	 * The test uses Defend instead of Announce so that the app can use the defended range
	 * for selecting a new reservation, which it can't do if tracking annoucements.
	 */

	/* Send a Defend packet that conflicts with all the ranges. */
	init_packet(&defend_packet, TEST_DEST_ADDR, TEST_REMOTE_ADDR_LOWER);
	defend_packet.message_type = MAAP_DEFEND;
	defend_packet.requested_start_address = range_base_addr;
	defend_packet.requested_count = range_size;
	defend_packet.conflict_start_address = range_base_addr;
	defend_packet.conflict_count = range_size;
	LONGS_EQUAL(0, pack_maap(&defend_packet, defend_buffer));
	maap_handle_packet(&mc, defend_buffer, MAAP_NET_BUFFER_SIZE);

	/* Wait for the right number of probes and announcements. */
	probe_packets_detected = 0;
	announce_packets_detected = 0;
	for (countdown = 1000; countdown > 0 && announce_packets_detected < num_reservations; --countdown)
	{
		Time_increaseNanos(maap_get_delay_to_next_timer(&mc));
		LONGS_EQUAL(0, maap_handle_timer(&mc));

		while ((packet_data = Net_getNextQueuedPacket(mc.net)) != NULL)
		{
			LONGS_EQUAL(0, unpack_maap(&packet_contents, (const uint8_t *) packet_data));
			Net_freeQueuedPacket(mc.net, packet_data);
			CHECK(packet_contents.message_type >= MAAP_PROBE && packet_contents.message_type <= MAAP_ANNOUNCE);
			if (packet_contents.message_type == MAAP_PROBE)
			{
				(probe_packets_detected)++;
				CHECK(probe_packets_detected <= 4 * num_reservations);
				CHECK(announce_packets_detected < probe_packets_detected / 4 + 1);
			}
			else if (packet_contents.message_type == MAAP_ANNOUNCE)
			{
				(announce_packets_detected)++;
				CHECK(probe_packets_detected > 3 * num_reservations);
				CHECK(announce_packets_detected <= num_reservations);
			}
			else
			{
				/* Unexpected MAAP_DEFEND packet. */
				CHECK(0);
			}
		}
	}
	CHECK(countdown > 0);
	CHECK(NULL == Net_getNextQueuedPacket(mc.net));

	/* We should have a yield notification and announce notification for each reservation. */
	for (i = 0; i < num_reservations; ++i) {
		LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
		while (mn.kind == MAAP_NOTIFY_ACQUIRING) {
			/* Ignore any acquiring messages. */
			LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
		}
		LONGS_EQUAL(MAAP_NOTIFY_YIELDED, mn.kind);
		LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	}
	for (i = 0; i < num_reservations; ++i) {
		LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
		while (mn.kind == MAAP_NOTIFY_ACQUIRING) {
			/* Ignore any acquiring messages. */
			LONGS_EQUAL(1, get_notify(&mc, &sender_out, &mn));
		}
		LONGS_EQUAL(MAAP_NOTIFY_ACQUIRED, mn.kind);
		LONGS_EQUAL(MAAP_NOTIFY_ERROR_NONE, mn.result);
	}
	LONGS_EQUAL(0, get_notify(&mc, &sender_out, &mn));


	/* We are done with the Maap_Client structure */
	maap_deinit_client(&mc);
}


static void verify_sent_packets(Maap_Client *p_mc, Maap_Notify *p_mn,
	const void **p_sender_out,
	int *p_probe_packets_detected, int *p_announce_packets_detected,
	int send_probe /* = -1 */, int send_announce /* = -1 */, int send_defend /* = -1 */,
	uint64_t remote_addr, int stop_after_probe)
{
	int countdown;
	void *packet_data;
	MAAP_Packet packet_contents;
	Time probe_time[4], announce_time;
	int acquiring_notification = 0;

	/* Handle any packets generated during the activity.
	 * Stop once we are notified of a result. */
	*p_sender_out = NULL;
	memset(p_mn, 0, sizeof(Maap_Notify));
	*p_probe_packets_detected = 0;
	*p_announce_packets_detected = 0;
	for (countdown = 1000; countdown > 0; --countdown)
	{
		while ((packet_data = Net_getNextQueuedPacket(p_mc->net)) != NULL)
		{
			/** @todo Add more verification that the packet to send is a proper packet. */
			LONGS_EQUAL(0, unpack_maap(&packet_contents, (const uint8_t *) packet_data));
			Net_freeQueuedPacket(p_mc->net, packet_data);
			CHECK(packet_contents.message_type >= MAAP_PROBE && packet_contents.message_type <= MAAP_ANNOUNCE);
			if (packet_contents.message_type == MAAP_PROBE)
			{
				CHECK(*p_probe_packets_detected < 4);
				LONGS_EQUAL(0, *p_announce_packets_detected);

				/* Save and test the timing between probes. */
				Time_setFromMonotonicTimer(&(probe_time[*p_probe_packets_detected]));
				if (*p_probe_packets_detected > 0) {

					/* Determine the minimum time from the last probe. */
					Time time_min;
					Time_setFromNanos(&time_min, MAAP_PROBE_INTERVAL_BASE * 1000000LL);
					Time_add(&time_min, &probe_time[*p_probe_packets_detected - 1]);
					CHECK(Time_cmp(&time_min, &(probe_time[*p_probe_packets_detected])) < 0);

					/* Determine the maximum time from the last probe. */
					Time time_max;
					Time_setFromNanos(&time_max, (MAAP_PROBE_INTERVAL_BASE + MAAP_PROBE_INTERVAL_VARIATION) * 1000000LL);
					Time_add(&time_max, &probe_time[*p_probe_packets_detected - 1]);
					CHECK(Time_cmp(&(probe_time[*p_probe_packets_detected]), &time_max) < 0);
				}

				(*p_probe_packets_detected)++;
				/* printf("Probe packet sent (%d)\n", *p_probe_packets_detected); */

				if (*p_probe_packets_detected == send_probe)
				{
					MAAP_Packet probe_packet;
					uint8_t probe_buffer[MAAP_NET_BUFFER_SIZE];

					/* Send a probe packet in response to the probe packet. */
					init_packet(&probe_packet, TEST_DEST_ADDR, remote_addr);
					probe_packet.message_type = MAAP_PROBE;
					probe_packet.requested_start_address = packet_contents.requested_start_address;
					probe_packet.requested_count = 1;
					LONGS_EQUAL(0, pack_maap(&probe_packet, probe_buffer));
					maap_handle_packet(p_mc, probe_buffer, MAAP_NET_BUFFER_SIZE);
					/* printf("Probe packet received\n"); */
				}

				if (*p_probe_packets_detected == send_announce)
				{
					MAAP_Packet announce_packet;
					uint8_t announce_buffer[MAAP_NET_BUFFER_SIZE];

					/* Send an announce packet in response to the probe packet. */
					init_packet(&announce_packet, TEST_DEST_ADDR, remote_addr);
					announce_packet.message_type = MAAP_ANNOUNCE;
					announce_packet.requested_start_address = packet_contents.requested_start_address;
					announce_packet.requested_count = 1;
					LONGS_EQUAL(0, pack_maap(&announce_packet, announce_buffer));
					maap_handle_packet(p_mc, announce_buffer, MAAP_NET_BUFFER_SIZE);
					/* printf("Announce packet received\n"); */
				}

				if (*p_probe_packets_detected == send_defend)
				{
					MAAP_Packet defend_packet;
					uint8_t defend_buffer[MAAP_NET_BUFFER_SIZE];

					/* Send a defend packet in response to the probe packet. */
					init_packet(&defend_packet, TEST_DEST_ADDR, remote_addr);
					defend_packet.message_type = MAAP_DEFEND;
					defend_packet.requested_start_address = packet_contents.requested_start_address;
					defend_packet.requested_count = packet_contents.requested_count;
					defend_packet.conflict_start_address = packet_contents.requested_start_address; /* Just use the same address */
					defend_packet.conflict_count = 1;
					LONGS_EQUAL(0, pack_maap(&defend_packet, defend_buffer));
					maap_handle_packet(p_mc, defend_buffer, MAAP_NET_BUFFER_SIZE);
					/* printf("Defend packet received\n"); */
				}

				/* If requested, stop the processing at this point so the caller can see what happens next. */
				if (*p_probe_packets_detected == stop_after_probe) { return; }
			}
			else if (packet_contents.message_type == MAAP_ANNOUNCE)
			{
				LONGS_EQUAL(4, *p_probe_packets_detected);
				LONGS_EQUAL(0, *p_announce_packets_detected);
				(*p_announce_packets_detected)++;
				/* printf("Announce packet sent (%d)\n", *p_announce_packets_detected); */

				/* Save and test the timing between the last probe and the announce. */
				Time_setFromMonotonicTimer(&announce_time);

				/* Determine the minimum time from the last probe. */
				Time time_min;
				Time_setFromNanos(&time_min, MAAP_PROBE_INTERVAL_BASE * 1000000LL);
				Time_add(&time_min, &probe_time[*p_probe_packets_detected - 1]);
				CHECK(Time_cmp(&time_min, &announce_time) < 0);

				/* Determine the maximum time from the last probe. */
				Time time_max;
				Time_setFromNanos(&time_max, (MAAP_PROBE_INTERVAL_BASE + MAAP_PROBE_INTERVAL_VARIATION) * 1000000LL);
				Time_add(&time_max, &probe_time[*p_probe_packets_detected - 1]);
				CHECK(Time_cmp(&announce_time, &time_max) < 0);
			}
			else
			{
				/* Unexpected MAAP_DEFEND packet. */
				CHECK(0);
			}
		}

		if (get_notify(p_mc, p_sender_out, p_mn)) {
			if (p_mn->kind == MAAP_NOTIFY_ACQUIRING) {
				/* This is a MAAP_NOTIFY_ACQUIRING notification.  We can ignore this once early on. */
				CHECK(!acquiring_notification);
				CHECK(*p_probe_packets_detected <= 1);
				acquiring_notification++;
				*p_sender_out = NULL;
			} else {
				/* We received a notification.  Stop processing so it can be evaluated by the caller. */
				break;
			}
		}

		/* Wait for the next event. */
		Time_increaseNanos(maap_get_delay_to_next_timer(p_mc));
		LONGS_EQUAL(0, maap_handle_timer(p_mc));
	}
	CHECK(countdown > 0);
}
