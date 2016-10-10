#ifndef MAAP_H
#define MAAP_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "intervals.h"
#include "maap_iface.h"
#include "maap_timer.h"
#include "maap_net.h"

#define MAAP_PROBE_RETRANSMITS                  3 /**< Number of allowed probes - This value is defined in IEEE 1722-2011 Table B.3 */

/* Times are in milliseconds */
#define MAAP_PROBE_INTERVAL_BASE                500    /**< Probe interval minimum time in milliseconds - This value is defined in IEEE 1722-2011 Table B.3 */
#define MAAP_PROBE_INTERVAL_VARIATION           100    /**< Probe interval additional time in milliseconds - This value is defined in IEEE 1722-2011 Table B.3 */
#define MAAP_ANNOUNCE_INTERVAL_BASE             30000  /**< Announce interval minimum time in milliseconds - This value is defined in IEEE 1722-2011 Table B.3 */
#define MAAP_ANNOUNCE_INTERVAL_VARIATION        2000   /**< Announce interval additional time in milliseconds - This value is defined in IEEE 1722-2011 Table B.3 */

#define MAAP_STATE_PROBING      0
#define MAAP_STATE_DEFENDING    1
#define MAAP_STATE_RELEASED     2

#define MAAP_CB_ACQUIRED 0
#define MAAP_CB_YIELDED  1

#define MAAP_DEST_MAC {0x91, 0xE0, 0xF0, 0x00, 0xFF, 0x00} /**< MAAP multicast Address - Defined in IEEE 1722-2011 Table B.5 */

#define MAAP_DYNAMIC_POOL_BASE 0x91E0F0000000LL /**< MAAP dynamic allocation pool base address - Defined in IEEE 1722-2011 Table B.4 */
#define MAAP_DYNAMIC_POOL_SIZE 0xFE00 /**< MAAP dynamic allocation pool size - Defined in IEEE 1722-2011 Table B.4 */

#define MAAP_TYPE 0x22F0 /**< AVTP Ethertype - Defined in IEEE 1722-2011 Table 5.1 */
#define MAAP_SUBTYPE 0x7E /**< AVTP MAAP subtype - Defined in IEEE 1722-2011 Table 5.2 */
#define MAAP_PKT_SIZE 42

typedef struct maap_notify_list Maap_Notify_List;
struct maap_notify_list {
  Maap_Notify notify;
  const void *sender;
  Maap_Notify_List *next;
};

typedef struct range Range;
struct range {
  int id;
  int state;
  int counter;
  Time next_act_time;
  Interval *interval;
  const void *sender;
  Range *next_timer;
};

typedef struct {
  uint64_t dest_mac;
  uint64_t src_mac;
  uint64_t address_base;
  uint32_t range_len;
  Interval *ranges;
  Range *timer_queue;
  Timer *timer;
  Net *net;
  int timer_running;
  int maxid;
  Maap_Notify_List *notifies;
  int initialized;
} Maap_Client;

int maap_init_client(Maap_Client *mc, const void *sender, uint64_t range_address_base, uint32_t range_len);
void maap_deinit_client(Maap_Client *mc);

int maap_reserve_range(Maap_Client *mc, const void *sender, uint32_t length);
int maap_release_range(Maap_Client *mc, const void *sender, int id);
void maap_range_status(Maap_Client *mc, const void *sender, int id);

int maap_handle_packet(Maap_Client *mc, const uint8_t *stream, int len);
int maap_handle_timer(Maap_Client *mc);

/**
 * Get the number of nanoseconds until the next timer event.
 *
 * @param mc Pointer to the Maap_Client to use
 *
 * @return Number of nanoseconds until the next timer expires, or a very large value if there are no timers waiting.
 */
int64_t maap_get_delay_to_next_timer(Maap_Client *mc);


/**
 * Add a notification to the end of the notifications queue.
 *
 * @param mc Pointer to the Maap_Client to use
 * @param sender Sender information pointer used to track the entity requesting the command
 * @param mn Maap_Notify structure with the notification information to use
 */
void add_notify(Maap_Client *mc, const void *sender, const Maap_Notify *mn);

/**
 * Get the next notification from the notifications queue.
 *
 * @param mc Pointer to the Maap_Client to use
 * @param sender Pointer to empty sender information pointer to receive the entity requesting the command
 * @param mn Empty Maap_Notify structure to fill with the notification information
 *
 * @return 1 if a notification was returned, 0 if there are no more queued notifications.
 */
int get_notify(Maap_Client *mc, const void **sender, Maap_Notify *mn);

/**
 * Output the text equivalent of the notification information to stdout.
 *
 * @param mn Pointer to the notification information structure.
 */
void print_notify(Maap_Notify *mn);

#endif
