/*
  Copyright (c) 2019 Egor Pomozov <Egor.Pomozov@aquantia.com>
*/
#include <stdint.h>

#ifndef _ASYNC_PCAP_STORING_H_
#define _ASYNC_PCAP_STORING_H_

int async_pcap_store_packet(void *context, void *buf, uint32_t size, uint64_t ts);
void async_pcap_release_context(void *context);
int async_pcap_initialize_context(char *file_name, uint32_t packet_count, uint32_t packet_size, void **context);

#endif //_ASYNC_PCAP_STORING_H_
