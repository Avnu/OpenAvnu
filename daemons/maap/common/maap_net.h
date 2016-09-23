#ifndef MAAP_NET_H
#define MAAP_NET_H

typedef struct maap_net Net;

Net *Net_newNet(void);

void Net_delNet(Net *net);

void *Net_getPacketBuffer(Net *net);

int Net_sendPacket(Net *net, void *buffer);

#endif
