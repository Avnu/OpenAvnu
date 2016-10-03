#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "maap_net.h"

#define NUM_BUFFERS 4

struct maap_net {
	char net_buffer[NUM_BUFFERS][MAAP_NET_BUFFER_SIZE];
	int buffer_available[NUM_BUFFERS];
	int index_to_send[NUM_BUFFERS];
};

Net *Net_newNet(void)
{
	Net *pNew;
	int i;

	pNew = malloc(sizeof(Net));
	if (!pNew) { return NULL; }
	memset(pNew, 0, sizeof(Net));
	for (i = 0; i < NUM_BUFFERS; ++i)
	{
		pNew->buffer_available[i] = 1;
		pNew->index_to_send[i] = -1;
	}
	return pNew;
}

void Net_delNet(Net *net)
{
	assert(net);
	free(net);
}

void *Net_getPacketBuffer(Net *net)
{
	int i;

	assert(net);
	for (i = 0; i < NUM_BUFFERS; ++i)
	{
		if (net->buffer_available[i])
		{
			net->buffer_available[i] = 0;
			printf("Allocated buffer %d\n", i);
			return (void *)(net->net_buffer[i]);
		}
	}
	return NULL;
}

int Net_queuePacket(Net *net, void *buffer)
{
	int buffer_index, queue_index;

	assert(net);
	/* @TODO: This could be done mathematically, rather than in a loop. */
	for (buffer_index = 0; buffer_index < NUM_BUFFERS; ++buffer_index)
	{
		if (net->net_buffer[buffer_index] == buffer)
		{
			/* We found the index of the buffer provided. */
			printf("Queuing buffer %d\n", buffer_index);
			break;
		}
	}
	if (buffer_index >= NUM_BUFFERS) { return -1; }

	for (queue_index = 0; queue_index < NUM_BUFFERS; ++queue_index)
	{
		assert(net->index_to_send[queue_index] != buffer_index);
		if (net->index_to_send[queue_index] < 0)
		{
			/* We can add the buffer to this spot in the queue. */
			net->index_to_send[queue_index] = buffer_index;
			printf("Buffer %d queued at index %d\n", buffer_index, queue_index);
			return 0;
		}
	}
	return -1;
}

void *Net_getNextQueuedPacket(Net *net)
{
	void *pBuffer = NULL;
	int i;

	assert(net);
	if (net->index_to_send[0] >= 0)
	{
		printf("Buffer %d pulled from queue\n", net->index_to_send[0]);
		pBuffer = net->net_buffer[net->index_to_send[0]];
		for (i = 0; i < NUM_BUFFERS - 1; ++i)
		{
			net->index_to_send[i] = net->index_to_send[i + 1];
		}
		net->index_to_send[NUM_BUFFERS - 1] = -1;
	}

	return pBuffer;
}

int Net_freeQueuedPacket(Net *net, void *buffer)
{
	int buffer_index;

	assert(net);
	/* @TODO: This could be done mathematically, rather than in a loop. */
	for (buffer_index = 0; buffer_index < NUM_BUFFERS; ++buffer_index)
	{
		if (net->net_buffer[buffer_index] == buffer)
		{
			/* We found the index of the buffer provided. */
			printf("Freed buffer %d\n", buffer_index);
			break;
		}
	}
	if (buffer_index >= NUM_BUFFERS) { return -1; }

	net->buffer_available[buffer_index] = 1;
	return 0;
}
