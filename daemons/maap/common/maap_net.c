#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "maap_net.h"

/* Number of preallocated buffers to use. */
#define NUM_BUFFERS 4

/* Uncomment the DEBUG_NET_MSG define to display debug messages. */
/* #define DEBUG_NET_MSG */


typedef enum {
  BUFFER_FREE = 0,
  BUFFER_SUPPLIED,
  BUFFER_QUEUED,
  BUFFER_SENDING,
} Buffer_State;

struct maap_buffer {
	char buffer[MAAP_NET_BUFFER_SIZE];
	Buffer_State state;
	struct maap_buffer *pNext; /* Pointer to next item when used in a linked list */
};

struct maap_net {
	struct maap_buffer net_buffer[NUM_BUFFERS];
	struct maap_buffer *pFirst; /* Linked list of additional buffers, in case more than NUM_BUFFERS supplied. */
};

Net *Net_newNet(void)
{
	Net *pNew;

	pNew = malloc(sizeof(Net));
	if (!pNew) { return NULL; }
	memset(pNew, 0, sizeof(Net));
	return pNew;
}

void Net_delNet(Net *net)
{
	struct maap_buffer *pDel;

	assert(net);
	while (net->pFirst) {
		pDel = net->pFirst;
		net->pFirst = pDel->pNext;
		free(pDel);
	}
	free(net);
}

void *Net_getPacketBuffer(Net *net)
{
	struct maap_buffer *pNew, *pLast;
	int buffer_index;

	assert(net);

	/* See if one of the preallocated buffers is available. */
	for (buffer_index = 0; buffer_index < NUM_BUFFERS; ++buffer_index)
	{
		if (net->net_buffer[buffer_index].state == BUFFER_FREE)
		{
			net->net_buffer[buffer_index].state = BUFFER_SUPPLIED;
#ifdef DEBUG_NET_MSG
			printf("Allocated buffer %d\n", buffer_index);
#endif
			return (void *)(net->net_buffer[buffer_index].buffer);
		}
	}

	/* We need to create an overflow buffer. */
	pNew = malloc(sizeof(struct maap_buffer));
	if (pNew) {
		memset(pNew, 0, sizeof(struct maap_buffer));
		if (net->pFirst == NULL) {
			net->pFirst = pNew;
		} else {
			pLast = net->pFirst;
			while (pLast->pNext) { pLast = pLast->pNext; }
			pLast->pNext = pNew;
		}

		pNew->state = BUFFER_SUPPLIED;
#ifdef DEBUG_NET_MSG
		printf("Allocated buffer 0x%llx\n", (long long int)(uintptr_t) pNew);
#endif
		return (void *)(pNew->buffer);
	}

	assert(0);
	return NULL;
}

int Net_queuePacket(Net *net, void *buffer)
{
	struct maap_buffer *pTest;
	int buffer_index;

	assert(net);

	/* See if the supplied buffer is one of the preallocated buffers. */
	for (buffer_index = 0; buffer_index < NUM_BUFFERS; ++buffer_index)
	{
		if (net->net_buffer[buffer_index].buffer == buffer)
		{
			/* We found the buffer provided. */
			assert(net->net_buffer[buffer_index].state == BUFFER_SUPPLIED);
			net->net_buffer[buffer_index].state = BUFFER_QUEUED;
#ifdef DEBUG_NET_MSG
			printf("Queuing buffer %d\n", buffer_index);
#endif
			return 0;
		}
	}

	/* See if the supplied buffer is one of the allocated buffers. */
	for (pTest = net->pFirst; pTest != NULL; pTest = pTest->pNext)
	{
		if (pTest->buffer == buffer)
		{
			/* We found the the buffer provided. */
			assert(pTest->state == BUFFER_SUPPLIED);
			pTest->state = BUFFER_QUEUED;
#ifdef DEBUG_NET_MSG
			printf("Queuing buffer 0x%llx\n", (long long int)(uintptr_t) pTest);
#endif
			return 0;
		}
	}

	assert(0);
	return -1;
}

void *Net_getNextQueuedPacket(Net *net)
{
	struct maap_buffer *pTest;
	int buffer_index;

	assert(net);

	/* See if one of the preallocated buffers is available. */
	for (buffer_index = 0; buffer_index < NUM_BUFFERS; ++buffer_index)
	{
		if (net->net_buffer[buffer_index].state == BUFFER_QUEUED)
		{
			net->net_buffer[buffer_index].state = BUFFER_SENDING;
#ifdef DEBUG_NET_MSG
			printf("Buffer %d pulled from queue\n\n", buffer_index);
#endif
			return (void *)(net->net_buffer[buffer_index].buffer);
		}
	}

	/* See if one of the allocated buffers is available. */
	for (pTest = net->pFirst; pTest != NULL; pTest = pTest->pNext)
	{
		if (pTest->state == BUFFER_QUEUED)
		{
			pTest->state = BUFFER_SENDING;
#ifdef DEBUG_NET_MSG
			printf("Buffer 0x%llx pulled from queue\n\n", (long long int)(uintptr_t) pTest);
#endif
			return (void *)(pTest->buffer);
		}
	}

	/* No packets are currently in the queue. */
	return NULL;
}

int Net_freeQueuedPacket(Net *net, void *buffer)
{
	struct maap_buffer *pTest, *pPrevious;
	int buffer_index;

	assert(net);

	/* See if the supplied buffer is one of the preallocated buffers. */
	for (buffer_index = 0; buffer_index < NUM_BUFFERS; ++buffer_index)
	{
		if (net->net_buffer[buffer_index].buffer == buffer)
		{
			/* We found the buffer provided. */
			assert(net->net_buffer[buffer_index].state == BUFFER_SENDING);
			net->net_buffer[buffer_index].state = BUFFER_FREE;
#ifdef DEBUG_NET_MSG
			printf("Freed buffer %d\n", buffer_index);
#endif
			return 0;
		}
	}

	/* See if the supplied buffer is one of the allocated buffers. */
	for (pPrevious = NULL, pTest = net->pFirst; pTest != NULL; pPrevious = pTest, pTest = pTest->pNext)
	{
		if (pTest->buffer == buffer)
		{
			/* We found the the buffer provided. */
			assert(pTest->state == BUFFER_SENDING);
			pTest->state = BUFFER_FREE;
#ifdef DEBUG_NET_MSG
			printf("Freed buffer 0x%llx\n", (long long int)(uintptr_t) pTest);
#endif

			/* Free the buffer. */
			if (pPrevious) {
				/* Remove this item from the middle (or end) of the queue. */
				pPrevious->pNext = pTest->pNext;
			} else {
				/* Remove this item from the start of the queue. */
				net->pFirst = pTest->pNext;
			}
			free(pTest);

			return 0;
		}
	}

	assert(0);
	return -1;
}