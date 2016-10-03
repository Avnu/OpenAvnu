#include "maap.h"
#include "maap_packet.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <assert.h>

/* Linux-specific header files */
#include <inttypes.h>
#include <linux/if_ether.h>

static int get_count(Maap_Client *mc, Range *range) {
  (void)mc;
  return range->interval->high - range->interval->low + 1;
}

static uint64_t get_start_address(Maap_Client *mc, Range *range) {
  return mc->address_base + range->interval->high;
}

static int send_packet(Maap_Client *mc, MAAP_Packet *p) {
  uint8_t *pbuf = NULL;
  int ret = 0;
  (void)mc;

  pbuf = Net_getPacketBuffer(mc->net);

  pack_maap(p, pbuf);

  ret = Net_queuePacket(mc->net, pbuf);
  return ret;
}

static int send_probe(Maap_Client *mc, Range *range) {
  MAAP_Packet p;

  init_packet(&p, mc->dest_mac, mc->src_mac);

  p.message_type = MAAP_PROBE;
  p.requested_start_address = get_start_address(mc, range);
  p.requested_count = get_count(mc, range);

  return send_packet(mc, &p);
}

static int send_announce(Maap_Client *mc, Range *range) {
  MAAP_Packet p;

  init_packet(&p, mc->dest_mac, mc->src_mac);

  p.message_type = MAAP_ANNOUNCE;
  p.requested_start_address = get_start_address(mc, range);
  p.requested_count = get_count(mc, range);

  return send_packet(mc, &p);
}

static int send_defend(Maap_Client *mc, Range *range, uint64_t start,
                       uint16_t count, uint64_t destination) {
  MAAP_Packet p;

  init_packet(&p, mc->dest_mac, mc->src_mac);

  p.DA = destination;
  p.message_type = MAAP_DEFEND;
  p.requested_start_address = start;
  p.requested_count = count;
  p.start_address = get_start_address(mc, range);
  p.count = get_count(mc, range);

  return send_packet(mc, &p);
}

static int inform_acquired(Maap_Client *mc, Range *range) {
  Maap_Notify note;
  printf("Address range acquired: %" PRIx64 " %d\n",
         get_start_address(mc, range),
         get_count(mc, range));

  note.kind = MAAP_ACQUIRED;
  note.id = range->id;
  note.start = get_start_address(mc, range);
  note.count = get_count(mc, range);

  add_notify(mc, &note);
  return 0;
}

static int inform_yielded(Maap_Client *mc, Range *range) {
  Maap_Notify note;
  printf("Address range yielded\n");

  note.kind = MAAP_YIELDED;
  note.id = range->id;
  note.start = get_start_address(mc, range);
  note.count = get_count(mc, range);

  add_notify(mc, &note);
  return 0;
}

static void start_timer(Maap_Client *mc) {

  if (mc->timer_queue) {
    Time_setTimer(mc->timer, &mc->timer_queue->next_act_time);
  }

  mc->timer_running = 1;
}

void add_notify(Maap_Client *mc, Maap_Notify *mn) {
  Maap_Notify_List *tmp, *li = calloc(1, sizeof (Maap_Notify_List));
  memcpy(&li->notify, mn, sizeof (Maap_Notify));

  if (mc->notifies == NULL) {
    mc->notifies = li;
  } else {
    tmp = mc->notifies;
    while (tmp->next) {
      tmp = tmp->next;
    }
    tmp->next = li;
  }
}

int get_notify(Maap_Client *mc, Maap_Notify *mn) {
  Maap_Notify_List *tmp;

  if (mc->notifies) {
    tmp = mc->notifies;
    memcpy(mn, tmp, sizeof (Maap_Notify));
    mc->notifies = tmp->next;
    free(tmp);
    return 1;
  }
  return 0;
}

int maap_init_client(Maap_Client *mc, uint64_t range_info) {
  if (mc->initialized) {
    printf("MAAP already initialized\n");
    return -1;
  }

  mc->timer = Time_newTimer();
  if (!mc->timer) {
    printf("Failed to create Timer\n");
    return -1;
  }

  mc->net = Net_newNet();
  if (!mc->net) {
    printf("Failed to create Net\n");
    Time_delTimer(mc->timer);
    return -1;
  }

  mc->address_base = range_info & MAAP_BASE_MASK;
  /* Consider 0 to be 0x10000, since 0 doesn't make sense and 0x10000 is
     the maximum number of values that can be stored in 16 bits */
  mc->range_len = (range_info & MAAP_RANGE_MASK) >> (ETH_ALEN * 8);
  if (mc->range_len == 0) { mc->range_len = 0x10000; }
  mc->ranges = NULL;
  mc->timer_queue = NULL;
  mc->maxid = 0;
  mc->timer_running = 0;
  mc->notifies = NULL;

  mc->initialized = 1;

  printf("MAAP initialized, start: %012llx, max: %08x\n",
         (unsigned long long)range_info & MAAP_BASE_MASK,
         (unsigned int)((range_info & MAAP_RANGE_MASK) >> (ETH_ALEN * 8)) - 1);
  return 0;
}

void maap_deinit_client(Maap_Client *mc) {
  assert(!mc->initialized);
  assert(!mc->timer_queue);

  Time_delTimer(mc->timer);
  Net_delNet(mc->net);

  mc->timer = NULL;
  mc->net = NULL;
  mc->initialized = 0;
}

int rand_ms(int variation) {
  return random() % variation;
}

int schedule_timer(Maap_Client *mc, Range *range) {
  Range *rp;
  uint64_t ns;
  Time ts;

  if (range->state == MAAP_STATE_PROBING) {
    ns = MAAP_PROBE_INTERVAL_BASE + rand_ms(MAAP_PROBE_INTERVAL_VARIATION);
    ns = ns * 1000000;
    printf("Scheduling probe timer for %" PRIu64 " ns from now\n", ns);
    Time_setFromNanos(&ts, ns);
    printf("Which is a timespec of:  ");
    Time_dump(&ts);
    Time_setFromMonotonicTimer(&range->next_act_time);
    printf("\nCurrent time is:  ");
    Time_dump(&range->next_act_time);
    Time_add(&range->next_act_time, &ts);
    printf("\nExpiration time is:  ");
    Time_dump(&range->next_act_time);
    printf("\n\n");
  } else if (range->state == MAAP_STATE_DEFENDING) {
    ns = MAAP_ANNOUNCE_INTERVAL_BASE + rand_ms(MAAP_ANNOUNCE_INTERVAL_VARIATION);
    ns = ns * 1000000;
    printf("Scheduling defend timer for %" PRIu64 " ns from now\n", ns);
    Time_setFromNanos(&ts, (uint64_t)ns);
    Time_setFromMonotonicTimer(&range->next_act_time);
    Time_add(&range->next_act_time, &ts);
    Time_dump(&range->next_act_time);
    printf("\n\n");
  }

  if (mc->timer_queue == NULL ||
      Time_cmp(&range->next_act_time, &mc->timer_queue->next_act_time) < 0) {
    range->next_timer = mc->timer_queue;
    mc->timer_queue = range;
  } else {
    rp = mc->timer_queue;
    while (rp->next_timer &&
           Time_cmp(&rp->next_timer->next_act_time, &range->next_act_time) <= 0) {
      rp = rp->next_timer;
    }
    range->next_timer = rp->next_timer;
    rp->next_timer = range;
  }

  return 0;
}

int assign_interval(Maap_Client *mc, Range *range, uint16_t len) {
  Interval *iv;
  int rv = INTERVAL_OVERLAP;
  uint32_t range_max;

  range_max = mc->range_len - 1;

  while (rv == INTERVAL_OVERLAP) {
    iv = alloc_interval(random() % mc->range_len, len);
    if (iv->high > range_max) {
      rv = INTERVAL_OVERLAP;
    } else {
      rv = insert_interval(&mc->ranges, iv);
    }
    if (rv == INTERVAL_OVERLAP) {
      free_interval(iv);
    }
  }

  iv->data = range;
  range->interval = iv;

  return 0;
}

int maap_reserve_range(Maap_Client *mc, uint16_t length) {
  int id;
  Range *range;

  range = malloc(sizeof (Range));
  if (range == NULL) {
    return -1;
  }

  id = mc->maxid++;
  range->id = id;
  range->state = MAAP_STATE_PROBING;
  range->counter = MAAP_PROBE_RETRANSMITS;
  Time_setFromMonotonicTimer(&range->next_act_time);
  range->interval = NULL;
  range->next_timer = NULL;

  assign_interval(mc, range, length);
  send_probe(mc, range);
  range->counter--;
  schedule_timer(mc, range);
  start_timer(mc);

  printf("Requested address range, id %d\n", id);

  return id;
}

int maap_release_range(Maap_Client *mc, int id) {
  Interval *iv;
  Range *range;
  int rv = -1;

  range = mc->timer_queue;
  while (range) {
    if (range->id == id) {
      iv = range->interval;
      iv = remove_interval(&mc->ranges, iv);
      free_interval(iv);
      /* memory for range will be freed the next time its timer elapses */
      range->state = MAAP_STATE_RELEASED;
      printf("Released range id %d\n", id);
      rv = 0;
    }
    range = range->next_timer;
  }

  return rv;
}

int maap_handle_packet(Maap_Client *mc, uint8_t *stream, int len) {
  MAAP_Packet p;
  Interval *iv;
  uint32_t start;
  Range *range;
  int rv;
  unsigned long long int a, b, c, d;

  printf("RECEIVED MAAP PACKET LEN %d\n", len);
  if (len < 42) {
    printf("Truncated MAAP packet received, discarding\n");
    return 0;
  }
  rv = unpack_maap(&p, stream);
  if (rv != 0) {
    return rv;
  }

  printf("Unpacked packet\n");

  a = p.requested_start_address + p.requested_count - 1;
  b = mc->address_base;
  c = mc->address_base + mc->range_len - 1;
  d = p.requested_start_address;
  if (a < b || c < d) {
    printf("Packet refers to a range outside of our concern\n");
    printf("%016llx < %016llx || %016llx < %016llx\n", a, b, c, d);
    return 0;
  }

  start = (uint64_t)p.requested_start_address - mc->address_base;
  iv = search_interval(mc->ranges, start, p.requested_count);
  if (iv != NULL) {
    range = iv->data;
    if (range->state == MAAP_STATE_PROBING) {
      printf("Found a conflicting preexisting range, look for a new one\n");
      /* Find an alternate interval, remove old interval,
         and restart probe counter */
      assign_interval(mc, range, iv->high - iv->low + 1);
      iv = remove_interval(&mc->ranges, iv);
      free_interval(iv);
      range->counter = MAAP_PROBE_RETRANSMITS;
    } else if (range->state == MAAP_STATE_DEFENDING) {
      printf("Someone is messing with our range!\n");
      if (p.message_type == MAAP_PROBE) {
        printf("DEFEND!\n");
        send_defend(mc, range, p.requested_start_address, p.requested_count,
                    p.SA);
      } else if (p.message_type == MAAP_ANNOUNCE) {
        /* We may only defend vs. an ANNOUNCE once */
        if (range->counter == 0) {
          printf("Defend vs. ANNOUNCE\n");
          send_defend(mc, range, p.requested_start_address, p.requested_count,
                      p.SA);
          range->counter = 1;
        } else {
          printf("Yield vs. ANNOUNCE\n");
          inform_yielded(mc, range);
          iv = remove_interval(&mc->ranges, iv);
          free_interval(iv);
          /* memory will be freed the next time its timer elapses */
          range->state = MAAP_STATE_RELEASED;
        }
      } else {
        printf("Got a DEFEND vs. a range we own\n");
        /* Don't know what to do with a DEFEND, so ignore it.  They'll
           send another ANNOUNCE anyway.  We could yield here if we wanted
           to be nice */
        ;
      }
    }
  }

  return 0;
}

int handle_probe_timer(Maap_Client *mc, Range *range) {
  if (range->counter == 0) {
    inform_acquired(mc, range);
    range->state = MAAP_STATE_DEFENDING;
    send_announce(mc, range);
    schedule_timer(mc, range);
  } else {
    send_probe(mc, range);
    range->counter--;
    schedule_timer(mc, range);
  }

  return 0;
}

int handle_defend_timer(Maap_Client *mc, Range *range) {
  send_announce(mc, range);
  schedule_timer(mc, range);

  return 0;
}

int maap_handle_timer(Maap_Client *mc) {
  Time currenttime;
  Range *range;

  /* Get the current time. */
  Time_setFromMonotonicTimer(&currenttime);
  printf("Current time is:  ");
  Time_dump(&currenttime);
  printf("\n");

  mc->timer_running = 0;

  while ((range = mc->timer_queue) && Time_passed(&currenttime, &range->next_act_time)) {
    printf("Due timer:  ");
    Time_dump(&range->next_act_time);
    mc->timer_queue = range->next_timer;
    range->next_timer = NULL;
    printf("\n");

    if (range->state == MAAP_STATE_PROBING) {
      printf("Handling probe timer\n");
      handle_probe_timer(mc, range);
    } else if (range->state == MAAP_STATE_DEFENDING) {
      printf("Handling defend timer\n");
      handle_defend_timer(mc, range);
    } else if (range->state == MAAP_STATE_RELEASED) {
      printf("Freeing released timer\n");
      free(range);
    }

  }

  start_timer(mc);

  return 0;
}

int64_t maap_get_delay_to_next_timer(Maap_Client *mc)
{
	int64_t timeRemaining;

	if (!(mc->timer) || !(mc->timer_queue))
	{
		/* There are no timers waiting, so wait for an hour.
		 * (No particular reason; it just sounded reasonable.) */
		timeRemaining = 60LL * 60LL * 1000000000LL;
	}
	else
	{
		/* Get the time remaining for the next timer. */
		timeRemaining = Time_remaining(mc->timer);
	}
	printf("Next delay:  %" PRId64 " ns\n\n", timeRemaining);
	return timeRemaining;
}
