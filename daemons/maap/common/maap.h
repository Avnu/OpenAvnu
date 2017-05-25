/*************************************************************************************
Copyright (c) 2016-2017, Harman International Industries, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************************/

/**
 * @file
 *
 * @brief Main MAAP supporting functions
 *
 * Includes functions to perform the MAAP negotiation, and defines useful for MAAP support.
 */

#ifndef MAAP_H
#define MAAP_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "intervals.h"
#include "maap_iface.h"
#include "maap_timer.h"
#include "maap_net.h"

#define MAAP_PROBE_RETRANSMITS                  3 /**< Number of allowed probes - This value is defined in IEEE 1722-2016 Table B.8 */

/* Times are in milliseconds */
#define MAAP_PROBE_INTERVAL_BASE                500    /**< Probe interval minimum time in milliseconds - This value is defined in IEEE 1722-2016 Table B.8 */
#define MAAP_PROBE_INTERVAL_VARIATION           100    /**< Probe interval additional time in milliseconds - This value is defined in IEEE 1722-2016 Table B.8 */
#define MAAP_ANNOUNCE_INTERVAL_BASE             30000  /**< Announce interval minimum time in milliseconds - This value is defined in IEEE 1722-2016 Table B.8 */
#define MAAP_ANNOUNCE_INTERVAL_VARIATION        2000   /**< Announce interval additional time in milliseconds - This value is defined in IEEE 1722-2016 Table B.8 */

#define MAAP_DEST_MAC {0x91, 0xE0, 0xF0, 0x00, 0xFF, 0x00} /**< MAAP multicast Address - Defined in IEEE 1722-2016 Table B.10 */

#define MAAP_DYNAMIC_POOL_BASE 0x91E0F0000000LL /**< MAAP dynamic allocation pool base address - Defined in IEEE 1722-2016 Table B.9 */
#define MAAP_DYNAMIC_POOL_SIZE 0xFE00 /**< MAAP dynamic allocation pool size - Defined in IEEE 1722-2016 Table B.9 */

#define MAAP_TYPE 0x22F0  /**< AVTP Ethertype - Defined in IEEE 1722-2016 Table 5 */
#define MAAP_SUBTYPE 0xFE /**< AVTP MAAP subtype - Defined in IEEE 1722-2016 Table 6 */
#define MAAP_PKT_SIZE 42  /**< Number of bytes for a raw MAAP Ethernet packet */

 /** MAAP Range States */
typedef enum {
	MAAP_STATE_INVALID = 0,
	MAAP_STATE_PROBING,    /**< Probing to determine if the address interval is available */
	MAAP_STATE_DEFENDING,  /**< The address interval has been reserved, and defend if conflicts detected */
	MAAP_STATE_RELEASED,   /**< The address interval has been released, and is waiting to be freed */
 } Maap_State;


/** Wrapper for struct maap_notify */
typedef struct maap_notify_list Maap_Notify_List;

/** Structure for each queued notification */
struct maap_notify_list {
	Maap_Notify notify;     /**< Notification information to send */
	const void *sender;     /**< Sender information pointer for the entity that requested the original command */
	Maap_Notify_List *next; /**< Next notification in the queue */
};


/** Wrapper for struct range */
typedef struct range Range;

/** Structure for each range in use */
struct range {
	int id;             /**< Unique identifier for this range */
	Maap_State state;   /**< State of this range */
	int counter;        /**< Counter used to limit the number of probes for this range */
	int overlapping;    /**< Temporary flag used to keep track of ranges that require overlap processing */
	Time next_act_time; /**< Next time to perform an action for this range */
	Interval *interval; /**< Interval information for the range */
	const void *sender; /**< Sender information pointer for the entity that requested the range */
	Range *next_timer;  /**< Next range in the list */
};


/** MAAP Initialization Information */
typedef struct {
	uint64_t dest_mac;          /**< Multicast address for MAAP packets */
	uint64_t src_mac;           /**< Local adapter interface MAC Address */
	uint64_t address_base;      /**< Starting address of the recognized range of addresses (typically #MAAP_DYNAMIC_POOL_BASE) */
	uint32_t range_len;         /**< Number of recognized addresses (typically #MAAP_DYNAMIC_POOL_SIZE) */
	Interval *ranges;           /**< Pointer to the root of the #Interval tree, which contains all the Range structures */
	Range *timer_queue;         /**< Pointer to a linked list of ranges that need timer support,
								 * with the first timer to expire being first in the list */
	Timer *timer;               /**< Pointer to the platform-specific timing support (initialized by calling #Time_newTimer) */
	Net *net;                   /**< Pointer to the platform-specific networking support (initialized by calling #Net_newNet) */
	int maxid;                  /**< Identifier value of the latest reservation */
	Maap_Notify_List *notifies; /**< Pointer to a linked list of queued notification */
	int initialized;            /**< 1 if the structure has been initialized, 0 otherwise */
} Maap_Client;


/**
 * Initialize the MAAP support, in response to a MAAP_CMD_INIT command.
 *
 * @param mc Pointer to the Maap_Client structure to use
 * @param sender Sender information pointer used to track the entity requesting the command
 * @param range_address_base Starting address of the recognized range of addresses (typically #MAAP_DYNAMIC_POOL_BASE)
 * @param range_len Number of recognized addresses (typically #MAAP_DYNAMIC_POOL_SIZE)
 *
 * @return 0 if the initialization was successful, -1 otherwise.
 */
int maap_init_client(Maap_Client *mc, const void *sender, uint64_t range_address_base, uint32_t range_len);

/**
 * Deinitialize the MAAP support.
 *
 * @param mc Pointer to the Maap_Client structure to use
 */
void maap_deinit_client(Maap_Client *mc);


/**
 * Reserve a block of addresses, in support of a MAAP_CMD_RESERVE command.
 *
 * @note This call starts the reservation process.
 * One or more MAAP_NOTIFY_ACQUIRING notifications may be sent during the process.
 * A MAAP_NOTIFY_ACQUIRED notification will be sent when the process is complete
 * (either succeeded or failed).
 *
 * @param mc Pointer to the Maap_Client structure to use
 * @param sender Sender information pointer used to track the entity requesting the command
 * @param attempt_base The base address to be attempted first, or 0 if no preference.
 *        If the preferred address is not available, another random address will be attempted instead.
 * @param length Number of addresses in the block to reserve (1 to 65535)
 *
 * @return The identifier value if the request was started successfully, -1 otherwise.
 */
int maap_reserve_range(Maap_Client *mc, const void *sender, uint64_t attempt_base, uint32_t length);

/**
 * Release a reserved block of addresses, in support of a MAAP_CMD_RELEASE command.
 *
 * @note This call starts the release process.
 * A MAAP_NOTIFY_RELEASED notification will be sent when the process is complete.
 *
 * @param mc Pointer to the Maap_Client structure to use
 * @param sender Sender information pointer used to track the entity requesting the command
 * @param id Identifier for the address block to release
 *
 * @return 0 if the release was started successfully, -1 otherwise.
 */
int maap_release_range(Maap_Client *mc, const void *sender, int id);

/**
 * Get the start and length of a block of addresses, in support of a MAAP_CMD_STATUS command.
 *
 * @note This call starts the status process.
 * A MAAP_NOTIFY_STATUS notification will be sent when the process is complete.
 *
 * @param mc Pointer to the Maap_Client structure to use
 * @param sender Sender information pointer used to track the entity requesting the command
 * @param id Identifier for the address block to get the status of
 */
void maap_range_status(Maap_Client *mc, const void *sender, int id);

/**
 * Yield a reserved block of addresses, in support of a MAAP_CMD_YIELD command.
 *
 * @note This call starts the yield process, which is only useful for testing.
 * A MAAP_NOTIFY_YIELDED notification will be sent when the process is complete.
 *
 * @param mc Pointer to the Maap_Client structure to use
 * @param sender Sender information pointer used to track the entity requesting the command
 * @param id Identifier for the address block to yield
 *
 * @return 0 if the yield was started successfully, -1 otherwise.
 */
int maap_yield_range(Maap_Client *mc, const void *sender, int id);


/**
 * Processing for a received (incoming) networking packet
 *
 * @param mc Pointer to the Maap_Client structure to use
 * @param stream Binary data for the raw incoming packet
 * @param len Length (in bytes) for the raw incoming packet
 *
 * @return 0 if the packet is a MAAP packet, -1 otherwise.
 */
int maap_handle_packet(Maap_Client *mc, const uint8_t *stream, int len);

/**
 * Determine if the next timer has expired, and perform any relevant actions if it has.
 *
 * @param mc Pointer to the Maap_Client structure to use
 *
 * @return 0 if successful, -1 otherwise.
 */
int maap_handle_timer(Maap_Client *mc);

/**
 * Get the number of nanoseconds until the next timer event.
 *
 * @param mc Pointer to the Maap_Client structure to use
 *
 * @return Number of nanoseconds until the next timer expires, or a very large value if there are no timers waiting.
 */
int64_t maap_get_delay_to_next_timer(Maap_Client *mc);


/**
 * Add a notification to the end of the notifications queue.
 *
 * @param mc Pointer to the Maap_Client structure to use
 * @param sender Sender information pointer used to track the entity requesting the command
 * @param mn Maap_Notify structure with the notification information to use
 */
void add_notify(Maap_Client *mc, const void *sender, const Maap_Notify *mn);

/**
 * Get the next notification from the notifications queue.
 *
 * @param mc Pointer to the Maap_Client structure to use
 * @param sender Pointer to empty sender information pointer to receive the entity requesting the command
 * @param mn Empty Maap_Notify structure to fill with the notification information
 *
 * @return 1 if a notification was returned, 0 if there are no more queued notifications.
 */
int get_notify(Maap_Client *mc, const void **sender, Maap_Notify *mn);

/**
 * Output the text equivalent of the notification information to the callback function.
 *
 * @param mn Pointer to the notification information structure.
 * @param notify_callback Function of type #print_notify_callback_t that will handle printable results.
 * @param callback_data Data to return with the callback.
 */
void print_notify(Maap_Notify *mn, print_notify_callback_t notify_callback, void *callback_data);

#endif
