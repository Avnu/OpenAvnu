#include <stdio.h>
#include <string.h>

#include "CppUTest/TestHarness.h"

extern "C" {

#include "maap_net.h"

}

/* Number of items to test in a group.
 * This should be greater than NUM_BUFFERS in maap_net.c */
#define GROUP_TEST_SIZE 100

TEST_GROUP(maap_net_group)
{
	void setup() {
	}

	void teardown() {
	}
};

TEST(maap_net_group, Single_Item)
{
	Net * net;
	int *buffer;

	net = Net_newNet();
	CHECK(net != NULL);

	/* Make sure the queue is empty. */
	CHECK(Net_getNextQueuedPacket(net) == NULL);

	/* Put a buffer in the queue. */
	buffer = (int*) Net_getPacketBuffer(net);
	CHECK(buffer != NULL);
	*buffer = 0x1234;
	LONGS_EQUAL(0, Net_queuePacket(net, buffer));

	/* Get the buffer from the queue. */
	/* Note that we don't assume this is the same buffer pointer that we queued. */
	buffer = (int*) Net_getNextQueuedPacket(net);
	CHECK(buffer != NULL);
	LONGS_EQUAL(0x1234, *buffer);
	LONGS_EQUAL(0, Net_freeQueuedPacket(net, buffer));

	/* Make sure the queue is empty. */
	CHECK(Net_getNextQueuedPacket(net) == NULL);

	Net_delNet(net);
}

TEST(maap_net_group, Queue_Singly)
{
	Net * net;
	int *buffer;
	int i;

	net = Net_newNet();
	CHECK(net != NULL);

	/* Make sure the queue is empty. */
	CHECK(Net_getNextQueuedPacket(net) == NULL);

	/* Put some buffers in the queue. */
	/* Queue each item before going to the next item. */
	for (i = 0; i < GROUP_TEST_SIZE; i++)
	{
		buffer = (int*) Net_getPacketBuffer(net);
		CHECK(buffer != NULL);
		*buffer = i;
		LONGS_EQUAL(0, Net_queuePacket(net, buffer));
	}

	/* Get each buffer from the queue. */
	/* Free each item before going to the next item. */
	for (i = 0; i < GROUP_TEST_SIZE; i++)
	{
		buffer = (int*) Net_getNextQueuedPacket(net);
		CHECK(buffer != NULL);
		LONGS_EQUAL(i, *buffer);
		LONGS_EQUAL(0, Net_freeQueuedPacket(net, buffer));
	}

	/* Make sure the queue is empty. */
	CHECK(Net_getNextQueuedPacket(net) == NULL);

	Net_delNet(net);
}

TEST(maap_net_group, Queue_Block)
{
	Net * net;
	int *buffer[GROUP_TEST_SIZE];
	int i;

	net = Net_newNet();
	CHECK(net != NULL);

	/* Make sure the queue is empty. */
	CHECK(Net_getNextQueuedPacket(net) == NULL);

	/* Put some buffers in the queue. */
	/* Get all the buffers before queuing the buffers. */
	for (i = 0; i < GROUP_TEST_SIZE; i++)
	{
		buffer[i] = (int*) Net_getPacketBuffer(net);
		CHECK(buffer[i] != NULL);
		*(buffer[i]) = i;
	}
	for (i = 0; i < GROUP_TEST_SIZE; i++)
	{
		LONGS_EQUAL(0, Net_queuePacket(net, buffer[i]));
	}

	/* Get each buffer from the queue. */
	/* Dequeue all the buffers before freeing the buffers. */
	for (i = 0; i < GROUP_TEST_SIZE; i++)
	{
		buffer[i] = (int*) Net_getNextQueuedPacket(net);
		CHECK(buffer[i] != NULL);
		LONGS_EQUAL(i, *(buffer[i]));
	}
	for (i = 0; i < GROUP_TEST_SIZE; i++)
	{
		LONGS_EQUAL(0, Net_freeQueuedPacket(net, buffer[i]));
	}

	/* Make sure the queue is empty. */
	CHECK(Net_getNextQueuedPacket(net) == NULL);

	Net_delNet(net);
}
