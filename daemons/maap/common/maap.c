#include "maap.h"
#include "maap_packet.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <assert.h>

/* Linux-specific header files */
#include <linux/if_ether.h>

/* Uncomment the DEBUG_TIMER_MSG define to display timer debug messages. */
/* #define DEBUG_TIMER_MSG */

/* Uncomment the DEBUG_NEGOTIATE_MSG define to display negotiation debug messages. */
/* #define DEBUG_NEGOTIATE_MSG */


static int get_count(Maap_Client *mc, Range *range) {
  (void)mc;
  return range->interval->high - range->interval->low + 1;
}

static unsigned long long int get_start_address(Maap_Client *mc, Range *range) {
  return mc->address_base + range->interval->low;
}

static unsigned long long int get_end_address(Maap_Client *mc, Range *range) {
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
  uint64_t conflict_start, conflict_end;

  init_packet(&p, mc->dest_mac, mc->src_mac);

  /* Determine the range of addresses where the conflict occurred
   * (the union of the requested and allocated ranges). */
  conflict_start = get_start_address(mc, range);
  if (conflict_start < start) { conflict_start = start; }
  conflict_end = get_end_address(mc, range);
  if (conflict_end > start + count - 1) { conflict_end = start + count - 1; }

  p.DA = destination;
  p.message_type = MAAP_DEFEND;
  p.requested_start_address = start;
  p.requested_count = count;
  p.conflict_start_address = conflict_start;
  p.conflict_count = (uint16_t)(conflict_end - conflict_start + 1);

  return send_packet(mc, &p);
}

static int inform_initialized(Maap_Client *mc, const void *sender, Maap_Notify_Error result) {
  Maap_Notify note;

  note.kind = MAAP_NOTIFY_INITIALIZED;
  note.id = -1; /* Not used */
  note.start = mc->address_base;
  note.count = mc->range_len;
  note.result = result;

  add_notify(mc, sender, &note);
  return 0;
}

static int inform_acquired(Maap_Client *mc, Range *range, Maap_Notify_Error result) {
  Maap_Notify note;

  note.kind = MAAP_NOTIFY_ACQUIRED;
  note.id = range->id;
  note.start = get_start_address(mc, range);
  note.count = get_count(mc, range);
  note.result = result;

  add_notify(mc, range->sender, &note);
  return 0;
}

static int inform_not_acquired(Maap_Client *mc, const void *sender, int range_size, Maap_Notify_Error result) {
  Maap_Notify note;

  note.kind = MAAP_NOTIFY_ACQUIRED;
  note.id = -1;
  note.start = 0;
  note.count = range_size;
  note.result = result;

  add_notify(mc, sender, &note);
  return 0;
}

static int inform_released(Maap_Client *mc, const void *sender, int id, Range *range, Maap_Notify_Error result) {
  Maap_Notify note;

  note.kind = MAAP_NOTIFY_RELEASED;
  note.id = id;
  note.start = (range ? get_start_address(mc, range) : 0);
  note.count = (range ? get_count(mc, range) : 0);
  note.result = result;

  add_notify(mc, sender, &note);
  return 0;
}

static int inform_status(Maap_Client *mc, const void *sender, int id, Range *range, Maap_Notify_Error result) {
  Maap_Notify note;

  note.kind = MAAP_NOTIFY_STATUS;
  note.id = id;
  note.start = (range ? get_start_address(mc, range) : 0);
  note.count = (range ? get_count(mc, range) : 0);
  note.result = result;

  add_notify(mc, sender, &note);
  return 0;
}

static int inform_yielded(Maap_Client *mc, Range *range, int result) {
  Maap_Notify note;

  note.kind = MAAP_NOTIFY_YIELDED;
  note.id = range->id;
  note.start = get_start_address(mc, range);
  note.count = get_count(mc, range);
  note.result = result;

  add_notify(mc, range->sender, &note);
  return 0;
}

static void start_timer(Maap_Client *mc) {

  if (mc->timer_queue) {
    Time_setTimer(mc->timer, &mc->timer_queue->next_act_time);
  }
}

void add_notify(Maap_Client *mc, const void *sender, const Maap_Notify *mn) {
  Maap_Notify_List *tmp, *li = calloc(1, sizeof (Maap_Notify_List));
  memcpy(&li->notify, mn, sizeof (Maap_Notify));
  li->sender = sender;

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

int get_notify(Maap_Client *mc, const void **sender, Maap_Notify *mn) {
  Maap_Notify_List *tmp;

  if (mc->notifies) {
    tmp = mc->notifies;
    if (mn) { memcpy(mn, &(tmp->notify), sizeof (Maap_Notify)); }
    if (sender) { *sender = tmp->sender; }
    mc->notifies = tmp->next;
    free(tmp);
    return 1;
  }
  return 0;
}

void print_notify(Maap_Notify *mn)
{
  assert(mn);

  switch (mn->result)
  {
  case MAAP_NOTIFY_ERROR_NONE:
    /* No error.  Don't display anything. */
    break;
  case MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION:
    printf("Error:  MAAP is not initialized, so the command cannot be performed.\n");
    break;
  case MAAP_NOTIFY_ERROR_ALREADY_INITIALIZED:
    printf("Error:  MAAP is already initialized, so the values cannot be changed.\n");
    break;
  case MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE:
    printf("Error:  The MAAP reservation is not available.  Try again with a smaller address block size.\n");
    break;
  case MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID:
    printf("Error:  The MAAP reservation ID is not valid, so cannot be released.\n");
    break;
  case MAAP_NOTIFY_ERROR_OUT_OF_MEMORY:
    printf("Error:  The MAAP application is out of memory.\n");
    break;
  case MAAP_NOTIFY_ERROR_INTERNAL:
    printf("Error:  The MAAP application experienced an internal error.\n");
    break;
  default:
    printf("Error:  The MAAP application returned an unknown error %d.\n", mn->result);
    break;
  }

  switch (mn->kind)
  {
  case MAAP_NOTIFY_INITIALIZED:
    if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
      printf("MAAP initialized:  0x%012llx-0x%012llx (Size: %d)\n",
             (unsigned long long) mn->start,
             (unsigned long long) mn->start + mn->count - 1,
             (unsigned int) mn->count);
    } else {
        printf("MAAP previously initialized:  0x%012llx-0x%012llx (Size: %d)\n",
               (unsigned long long) mn->start,
               (unsigned long long) mn->start + mn->count - 1,
               (unsigned int) mn->count);
    }
    break;
  case MAAP_NOTIFY_ACQUIRED:
    if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
      printf("Address range %d acquired:  0x%012llx-0x%012llx (Size %d)\n",
             mn->id,
             (unsigned long long) mn->start,
             (unsigned long long) mn->start + mn->count - 1,
             mn->count);
    } else {
      printf("Address range of size %d not acquired\n",
             mn->count);
    }
    break;
  case MAAP_NOTIFY_RELEASED:
    if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
      printf("Address range %d released:  0x%012llx-0x%012llx (Size %d)\n",
             mn->id,
             (unsigned long long) mn->start,
             (unsigned long long) mn->start + mn->count - 1,
             mn->count);
    } else {
      printf("Address range %d not released\n",
             mn->id);
    }
    break;
  case MAAP_NOTIFY_STATUS:
    if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
      printf("ID %d is address range 0x%012llx-0x%012llx (Size %d)\n",
             mn->id,
             (unsigned long long) mn->start,
             (unsigned long long) mn->start + mn->count - 1,
             mn->count);
    } else {
      printf("ID %d is not valid\n",
             mn->id);
    }
    break;
  case MAAP_NOTIFY_YIELDED:
    if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
      printf("Address range %d yielded:  0x%012llx-0x%012llx (Size %d)\n",
             mn->id,
             (unsigned long long) mn->start,
	         (unsigned long long) mn->start + mn->count - 1,
             mn->count);
    } else {
      printf("Unexpected yield error\n");
    }
    break;
  default:
    printf("Notification type %d not recognized\n", mn->kind);
    break;
  }
}


int maap_init_client(Maap_Client *mc, const void *sender, uint64_t range_address_base, uint32_t range_len) {

  if (mc->initialized) {
    /* If the desired values are the same as the initialized values, pretend the command succeeded.
     * Otherwise, let the sender know the range that was already specified and cannot change. */
    int matches = (range_address_base == mc->address_base && range_len == mc->range_len);
    inform_initialized(mc, sender, (matches ? MAAP_NOTIFY_ERROR_NONE : MAAP_NOTIFY_ERROR_ALREADY_INITIALIZED));

    return (matches ? 0 : -1);
  }

  mc->timer = Time_newTimer();
  if (!mc->timer) {
    fprintf(stderr, "Failed to create Timer\n");
    return -1;
  }

  mc->net = Net_newNet();
  if (!mc->net) {
    fprintf(stderr, "Failed to create Net\n");
    Time_delTimer(mc->timer);
    return -1;
  }

  mc->address_base = range_address_base;
  mc->range_len = range_len;
  mc->ranges = NULL;
  mc->timer_queue = NULL;
  mc->maxid = 0;
  mc->notifies = NULL;

  mc->initialized = 1;

  /* Let the sender know the range is now specified. */
  inform_initialized(mc, sender, MAAP_NOTIFY_ERROR_NONE);

  return 0;
}

void maap_deinit_client(Maap_Client *mc) {
  if (mc->initialized) {
    if (mc->ranges) {
      /** @todo Free reservation memory */
      mc->ranges = NULL;
    }
    mc->timer_queue = NULL;

    if (mc->timer) {
      Time_delTimer(mc->timer);
      mc->timer = NULL;
    }

    if (mc->net) {
      Net_delNet(mc->net);
      mc->net = NULL;
    }

    while (get_notify(mc, NULL, NULL)) { /* Do nothing with the result */ }

    mc->initialized = 0;
  }
}

int rand_ms(int variation) {
  return random() % variation;
}

int schedule_timer(Maap_Client *mc, Range *range) {
  Range *rp;
  unsigned long long int ns;
  Time ts;

  if (range->state == MAAP_STATE_PROBING) {
    ns = MAAP_PROBE_INTERVAL_BASE + rand_ms(MAAP_PROBE_INTERVAL_VARIATION);
    ns = ns * 1000000;
#ifdef DEBUG_TIMER_MSG
    printf("Scheduling probe timer for %llu ns from now\n", ns);
#endif
    Time_setFromNanos(&ts, ns);
#ifdef DEBUG_TIMER_MSG
    printf("Which is a timespec of:  ");
    Time_dump(&ts);
#endif
    Time_setFromMonotonicTimer(&range->next_act_time);
#ifdef DEBUG_TIMER_MSG
    printf("\nCurrent time is:  ");
    Time_dump(&range->next_act_time);
#endif
    Time_add(&range->next_act_time, &ts);
#ifdef DEBUG_TIMER_MSG
    printf("\nExpiration time is:  ");
    Time_dump(&range->next_act_time);
    printf("\n\n");
#endif
  } else if (range->state == MAAP_STATE_DEFENDING) {
    ns = MAAP_ANNOUNCE_INTERVAL_BASE + rand_ms(MAAP_ANNOUNCE_INTERVAL_VARIATION);
    ns = ns * 1000000;
#ifdef DEBUG_TIMER_MSG
    printf("Scheduling defend timer for %llu ns from now\n", ns);
#endif
    Time_setFromNanos(&ts, (uint64_t)ns);
    Time_setFromMonotonicTimer(&range->next_act_time);
    Time_add(&range->next_act_time, &ts);
#ifdef DEBUG_TIMER_MSG
    Time_dump(&range->next_act_time);
    printf("\n\n");
#endif
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

static int assign_interval(Maap_Client *mc, Range *range, uint16_t len) {
  Interval *iv;
  int i, rv = INTERVAL_OVERLAP;
  uint32_t range_max;

  if (len > mc->range_len) { return -1; }
  range_max = mc->range_len - 1;

  /** @todo Use the saved MAAP_ANNOUNCE message ranges to search for addresses likely to be available.
   *  Old announced ranges (e.g. older than 2 minutes) can be deleted if there are no ranges available.
   *  We can also select new address blocks adjacent to our existing address blocks, which will fill the available address space more efficiently.
   *  While this doesn't strictly adhere to the 1722 MAAP specification, it is defensible (as the initial block was random). */

  for (i = 0; i < 1000 && rv == INTERVAL_OVERLAP; ++i) {
    iv = alloc_interval(random() % (mc->range_len + 1 - len), len);
    assert(iv->high <= range_max);
    rv = insert_interval(&mc->ranges, iv);
    if (rv == INTERVAL_OVERLAP) {
      free_interval(iv);
    }
  }
  if (i >= 1000) {
    /* There don't appear to be any options! */
    return -1;
  }

  iv->data = range;
  range->interval = iv;

  return 0;
}

int maap_reserve_range(Maap_Client *mc, const void *sender, uint32_t length) {
  int id;
  Range *range;

  if (!mc->initialized) {
    printf("Reserve not allowed, as MAAP not initialized\n");
    inform_not_acquired(mc, sender, length, MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION);
    return -1;
  }

  if (length > 0xFFFF || length > mc->range_len) {
    /* Range size cannot be more than 16 bits in size, due to the MAAP packet format */
    inform_not_acquired(mc, sender, length, MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE);
    return -1;
  }

  range = malloc(sizeof (Range));
  if (range == NULL) {
    inform_not_acquired(mc, sender, length, MAAP_NOTIFY_ERROR_OUT_OF_MEMORY);
    return -1;
  }

  id = ++(mc->maxid);
  range->id = id;
  range->state = MAAP_STATE_PROBING;
  range->counter = MAAP_PROBE_RETRANSMITS;
  Time_setFromMonotonicTimer(&range->next_act_time);
  range->interval = NULL;
  range->sender = sender;
  range->next_timer = NULL;

  if (assign_interval(mc, range, length) < 0)
  {
    /* Cannot find any available intervals of the requested size. */
    inform_not_acquired(mc, sender, length, MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE);
    free(range);
    return -1;
  }

  send_probe(mc, range);
  schedule_timer(mc, range);
  start_timer(mc);

  printf("Requested address range, id %d\n", id);
#ifdef DEBUG_NEGOTIATE_MSG
  printf("Selected address range 0x%012llx-0x%012llx\n", get_start_address(mc, range), get_end_address(mc, range));
#endif

  return id;
}

int maap_release_range(Maap_Client *mc, const void *sender, int id) {
  Interval *iv;
  Range *range;

  if (!mc->initialized) {
    printf("Release not allowed, as MAAP not initialized\n");
    inform_released(mc, sender, id, NULL, MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION);
    return -1;
  }

  range = mc->timer_queue;
  while (range) {
    if (range->id == id && range->state == MAAP_STATE_DEFENDING) {
      inform_released(mc, sender, id, range, MAAP_NOTIFY_ERROR_NONE);
      if (sender != range->sender)
      {
        /* Also inform the sender that originally reserved this range. */
        inform_released(mc, range->sender, id, range, MAAP_NOTIFY_ERROR_NONE);
      }

      iv = range->interval;
      iv = remove_interval(&mc->ranges, iv);
      free_interval(iv);
      /* memory for range will be freed the next time its timer elapses */
      range->state = MAAP_STATE_RELEASED;

      return 0;
    }
    range = range->next_timer;
  }

  printf("Range id %d does not exist to release\n", id);
  inform_released(mc, sender, id, NULL, MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID);
  return -1;
}

void maap_range_status(Maap_Client *mc, const void *sender, int id)
{
  Range *range;

  if (!mc->initialized) {
    printf("Status not allowed, as MAAP not initialized\n");
    inform_status(mc, sender, id, NULL, MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION);
    return;
  }

  range = mc->timer_queue;
  while (range) {
    if (range->id == id && range->state != MAAP_STATE_RELEASED) {
      inform_status(mc, sender, id, range, MAAP_NOTIFY_ERROR_NONE);
      return;
    }
    range = range->next_timer;
  }

  printf("Range id %d does not exist\n", id);
  inform_status(mc, sender, id, NULL, MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID);
}


int maap_handle_packet(Maap_Client *mc, const uint8_t *stream, int len) {
  MAAP_Packet p;
  Interval *iv;
  uint32_t start;
  Range *range;
  int rv;
  unsigned long long int own_base, own_max, incoming_base, incoming_max;

  if (len < MAAP_PKT_SIZE) {
    fprintf(stderr, "Truncated MAAP packet of length %d received, discarding\n", len);
    return -1;
  }
  rv = unpack_maap(&p, stream);
  if (rv != 0) {
    fprintf(stderr, "Error unpacking the MAAP packet\n");
    return -1;
  }

  if (p.Ethertype != MAAP_TYPE ||
      p.CD != 1 || p.subtype != MAAP_SUBTYPE ||
      p.maap_data_length != 16 )
  {
    /* This is not a MAAP packet.  Ignore it. */
#ifdef DEBUG_NEGOTIATE_MSG
    printf("Ignoring non-MAAP packet of length %d\n", len);
#endif
    return -1;
  }

  if (p.version != 0) {
    fprintf(stderr, "AVTP version %u not supported\n", p.version);
    return -1;
  }

  if (p.message_type < MAAP_PROBE || p.message_type > MAAP_ANNOUNCE) {
    fprintf(stderr, "Maap packet message type %u not recognized\n", p.message_type);
    return -1;
  }

  own_base = mc->address_base;
  own_max = mc->address_base + mc->range_len - 1;
  incoming_base = p.requested_start_address;
  incoming_max = p.requested_start_address + p.requested_count - 1;

#ifdef DEBUG_NEGOTIATE_MSG
  if (p.message_type == MAAP_PROBE) { printf("Received PROBE for range 0x%012llx-0x%012llx (Size %u)\n", incoming_base, incoming_max, p.requested_count); }
  if (p.message_type == MAAP_DEFEND) {
    printf("Received DEFEND for range 0x%012llx-0x%012llx (Size %u),\n"
           "conflicting with range 0x%012llx-0x%012llx (Size %u)\n",
           incoming_base, incoming_max, p.requested_count,
           (unsigned long long) p.conflict_start_address,
           (unsigned long long) p.conflict_start_address + p.conflict_count - 1,
           p.conflict_count); }
  if (p.message_type == MAAP_ANNOUNCE) { printf("Received ANNOUNCE for range 0x%012llx-0x%012llx (Size %u)\n", incoming_base, incoming_max, p.requested_count); }
#endif

  if (incoming_max < own_base || own_max < incoming_base) {
#ifdef DEBUG_NEGOTIATE_MSG
    printf("Packet refers to a range outside of our concern\n");
    printf("\t0x%012llx < 0x%012llx || 0x%012llx < 0x%012llx\n", incoming_max, own_base, own_max, incoming_base);
#endif
    return 0;
  }

  /** @todo If this is a MAAP_ANNOUNCE message, save the announced range and time received for later reference. */

  start = (uint64_t)p.requested_start_address - mc->address_base;
  for (iv = search_interval(mc->ranges, start, p.requested_count); iv != NULL && interval_check_overlap(iv, start, p.requested_count); iv = next_interval(iv)) {
    range = iv->data;
    if (range->state == MAAP_STATE_PROBING) {

      if (p.message_type == MAAP_PROBE && compare_mac_addresses(mc->src_mac, p.SA)) {
        /* We won with the lower MAC Address.  Do nothing. */
#ifdef DEBUG_NEGOTIATE_MSG
        printf("Ignoring conflicting probe request\n");
#endif
      } else {
        /* Find an alternate interval, remove old interval,
           and restart probe counter */
        int range_size = iv->high - iv->low + 1;
        if (assign_interval(mc, range, range_size) < 0) {
          /* No interval is available, so stop probing and report an error. */
          printf("Unable to find an available address block to probe\n");
          inform_not_acquired(mc, range->sender, range_size, MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE);
          iv = remove_interval(&mc->ranges, iv);
          free_interval(iv);
          /* memory will be freed the next time its timer elapses */
          range->state = MAAP_STATE_RELEASED;
        }
#ifdef DEBUG_NEGOTIATE_MSG
        printf("Selected new address range 0x%012llx-0x%012llx\n",
               get_start_address(mc, range), get_end_address(mc, range));
#endif
        iv = remove_interval(&mc->ranges, iv);
        free_interval(iv);
        range->counter = MAAP_PROBE_RETRANSMITS;
        send_probe(mc, range);
      }

    } else if (range->state == MAAP_STATE_DEFENDING) {

      printf("Conflict detected with our range (id %d)!\n", range->id);
#ifdef DEBUG_NEGOTIATE_MSG
      printf("    Request of 0x%012llx-0x%012llx inside our\n",
             incoming_base, incoming_max);
      printf("    range of 0x%012llx-0x%012llx\n",
             get_start_address(mc, range), get_end_address(mc, range));
#endif

      if (p.message_type == MAAP_PROBE) {
        printf("DEFEND!\n");
        send_defend(mc, range, p.requested_start_address, p.requested_count, p.SA);
      } else if (compare_mac_addresses(mc->src_mac, p.SA)) {
        /* We won with the lower MAC Address.  Do nothing. */
        printf("IGNORE\n");
      } else {
        printf("YIELD\n");
        inform_yielded(mc, range, MAAP_NOTIFY_ERROR_NONE);
        iv = remove_interval(&mc->ranges, iv);
        free_interval(iv);
        /* memory will be freed the next time its timer elapses */
        range->state = MAAP_STATE_RELEASED;
      }

    }
  }

  return 0;
}

int handle_probe_timer(Maap_Client *mc, Range *range) {
  if (range->counter == 0) {
    inform_acquired(mc, range, MAAP_NOTIFY_ERROR_NONE);
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
#ifdef DEBUG_TIMER_MSG
  printf("Current time is:  ");
  Time_dump(&currenttime);
  printf("\n");
#endif

  while ((range = mc->timer_queue) && Time_passed(&currenttime, &range->next_act_time)) {
#ifdef DEBUG_TIMER_MSG
    printf("Due timer:  ");
    Time_dump(&range->next_act_time);
    printf("\n");
#endif
    mc->timer_queue = range->next_timer;
    range->next_timer = NULL;

    if (range->state == MAAP_STATE_PROBING) {
#ifdef DEBUG_TIMER_MSG
      printf("Handling probe timer\n");
#endif
      handle_probe_timer(mc, range);
    } else if (range->state == MAAP_STATE_DEFENDING) {
#ifdef DEBUG_TIMER_MSG
      printf("Handling defend timer\n");
#endif
      handle_defend_timer(mc, range);
    } else if (range->state == MAAP_STATE_RELEASED) {
#ifdef DEBUG_TIMER_MSG
      printf("Freeing released timer\n");
#endif
      free(range);
    }

  }

  start_timer(mc);

  return 0;
}

int64_t maap_get_delay_to_next_timer(Maap_Client *mc)
{
	long long int timeRemaining;

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
#ifdef DEBUG_TIMER_MSG
	printf("Next delay:  %lld ns\n\n", timeRemaining);
#endif
	return timeRemaining;
}
