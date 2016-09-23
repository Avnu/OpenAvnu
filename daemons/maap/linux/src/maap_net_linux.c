#include <stdlib.h>
#include <assert.h>

#include "maap_net.h"

struct maap_net {
  char net_buffer[64];
};

Net *Net_newNet(void)
{
  return malloc(sizeof (Net));
}

void Net_delNet(Net *net)
{
  assert(net);
  free(net);
}

void *Net_getPacketBuffer(Net *net)
{
  assert(net);
  return (void*)net->net_buffer;
}

int Net_sendPacket(Net *net, void *buffer)
{
  assert(net);
  assert(buffer == (void*)net->net_buffer);
  /* @TODO Send Packet Here */

  return 1;
}


