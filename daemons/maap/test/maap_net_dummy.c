#include <stdlib.h>
#include <assert.h>

#include <stdio.h>

#include "maap_net.h"

typedef enum {
  BUFFER_FREE = 0,
  BUFFER_SUPPLIED,
  BUFFER_QUEUED,
  BUFFER_SENDING,
} Buffer_State;

struct maap_net {
  char net_buffer[64];
  Buffer_State state;
};

/* Instance to use for testing. */
static Net s_net;

Net *Net_newNet(void)
{
  s_net.state = BUFFER_FREE;
  return &s_net;
}

void Net_delNet(Net *net)
{
  assert(net == &s_net);
  assert(net->state == BUFFER_FREE);
}

void *Net_getPacketBuffer(Net *net)
{
  assert(net == &s_net);
  assert(net->state == BUFFER_FREE);
  net->state = BUFFER_SUPPLIED;
  return (void*)net->net_buffer;
}

int Net_queuePacket(Net *net, void *buffer)
{
  assert(net == &s_net);
  assert(buffer == net->net_buffer);
  assert(net->state == BUFFER_SUPPLIED);
  net->state = BUFFER_QUEUED;
  return 0;
}

void *Net_getNextQueuedPacket(Net *net)
{
  assert(net == &s_net);
  if (net->state == BUFFER_QUEUED) {
    net->state = BUFFER_SENDING;
    return net->net_buffer;
  }
  assert(net->state == BUFFER_FREE);
  return NULL;
}

int Net_freeQueuedPacket(Net *net, void *buffer)
{
  assert(net == &s_net);
  assert(buffer == net->net_buffer);
  assert(net->state == BUFFER_SENDING);
  net->state = BUFFER_FREE;
  return 0;
}
