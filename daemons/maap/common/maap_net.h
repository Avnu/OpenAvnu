#ifndef MAAP_NET_H
#define MAAP_NET_H

#define MAAP_NET_BUFFER_SIZE 64

typedef struct maap_net Net;

Net *Net_newNet(void);

void Net_delNet(Net *net);

void *Net_getPacketBuffer(Net *net);

int Net_queuePacket(Net *net, void *buffer);

void *Net_getNextQueuedPacket(Net *net);

int Net_freeQueuedPacket(Net *net, void *buffer);

#endif
