#ifndef MAAP_H
#define MAAP_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "intervals.h"
#include "maap_iface.h"
#include "maap_timer.h"
#include "maap_net.h"

#define MAAP_PROBE_RETRANSMITS                  3

/* Times are in milliseconds */
#define MAAP_PROBE_INTERVAL_BASE                500
#define MAAP_PROBE_INTERVAL_VARIATION           100
#define MAAP_ANNOUNCE_INTERVAL_BASE             30000
#define MAAP_ANNOUNCE_INTERVAL_VARIATION        1000

#define MAAP_STATE_PROBING      0
#define MAAP_STATE_DEFENDING    1
#define MAAP_STATE_RELEASED     2

#define MAAP_CB_ACQUIRED 0
#define MAAP_CB_YIELDED  1

#define MAAP_DEST_MAC {0x91, 0xE0, 0xF0, 0x00, 0xFF, 0x00}
#define MAAP_DEST_64 0x000091E0F000FF00LL
#define MAAP_TYPE 0x22F0
#define MAAP_SUBTYPE 0xFE
#define MAAP_PKT_SIZE 42
#define MAAP_RANGE_MASK 0xFFFF000000000000LL
#define MAAP_BASE_MASK  0x0000FFFFFFFFFFFFLL

typedef struct notify_list Notify_List;
struct notify_list {
  Notify notify;
  Notify_List *next;
};

typedef struct range Range;
struct range {
  int id;
  int state;
  int counter;
  Time next_act_time;
  Interval *interval;
  Range *next_timer;
};

typedef struct {
  uint64_t address_base;
  uint32_t range_len;
  Interval *ranges;
  Range *timer_queue;
  Timer *timer;
  Net *net;
  int timer_running;
  int maxid;
  Notify_List *notifies;
  int initialized;
} Client;

int maap_init_client(Client *mc, uint64_t range_info);
int maap_reserve_range(Client *mc, uint16_t length);
int maap_release_range(Client *mc, int id);

int maap_handle_packet(Client *mc, uint8_t *stream, int len);
int maap_handle_timer(Client *mc, Time *time);
void add_notify(Client *mc, Notify *mn);
int get_notify(Client *mc, Notify *mn);

#endif
