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

#include "maap.h"
#include "maap_packet.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#ifdef _WIN32
 /* Windows-specific header values */
#define random() rand()
#define srandom(s) srand(s)
#else
 /* Linux-specific header files */
#include <linux/if_ether.h>
#endif

#define MAAP_LOG_COMPONENT "Main"
#include "maap_log.h"

 /* Uncomment the DEBUG_TIMER_MSG define to display timer debug messages. */
#define DEBUG_TIMER_MSG

 /* Uncomment the DEBUG_NEGOTIATE_MSG define to display negotiation debug messages. */
#define DEBUG_NEGOTIATE_MSG


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

#ifdef DEBUG_NEGOTIATE_MSG
	if (MAAP_LOG_LEVEL_DEBUG <= MAAP_LOG_LEVEL) {
		Time t;
		Time_setFromMonotonicTimer(&t);
		MAAP_LOGF_DEBUG("Sending probe at %s", Time_dump(&t));
	}
#endif

	return send_packet(mc, &p);
}

static int send_announce(Maap_Client *mc, Range *range) {
	MAAP_Packet p;

	init_packet(&p, mc->dest_mac, mc->src_mac);

	p.message_type = MAAP_ANNOUNCE;
	p.requested_start_address = get_start_address(mc, range);
	p.requested_count = get_count(mc, range);

#ifdef DEBUG_NEGOTIATE_MSG
	if (MAAP_LOG_LEVEL_DEBUG <= MAAP_LOG_LEVEL) {
		Time t;
		Time_setFromMonotonicTimer(&t);
		MAAP_LOGF_DEBUG("Sending announce at %s", Time_dump(&t));
	}
#endif

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

#ifdef DEBUG_NEGOTIATE_MSG
	if (MAAP_LOG_LEVEL_DEBUG <= MAAP_LOG_LEVEL) {
		Time t;
		Time_setFromMonotonicTimer(&t);
		MAAP_LOGF_DEBUG("Sending defend at %s", Time_dump(&t));
	}
#endif

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

static int inform_acquiring(Maap_Client *mc, Range *range) {
	Maap_Notify note;

	note.kind = MAAP_NOTIFY_ACQUIRING;
	note.id = range->id;
	note.start = get_start_address(mc, range);
	note.count = get_count(mc, range);
	note.result = MAAP_NOTIFY_ERROR_NONE;

	add_notify(mc, range->sender, &note);
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

static int inform_not_acquired(Maap_Client *mc, const void *sender, int id, int range_size, Maap_Notify_Error result) {
	Maap_Notify note;

	note.kind = MAAP_NOTIFY_ACQUIRED;
	note.id = id;
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

static int inform_yielded(Maap_Client *mc, const void *sender, int id, Range *range, Maap_Notify_Error result) {
	Maap_Notify note;

	note.kind = MAAP_NOTIFY_YIELDED;
	note.id = id;
	note.start = (range ? get_start_address(mc, range) : 0);
	note.count = (range ? get_count(mc, range) : 0 );
	note.result = result;

	add_notify(mc, sender, &note);
	return 0;
}

static void start_timer(Maap_Client *mc) {

	if (mc->timer_queue) {
		Time_setTimer(mc->timer, &mc->timer_queue->next_act_time);
	}
}

static void remove_range_interval(Interval **root, Interval *node) {
	Range *old_range = node->data;
	Interval *free_inter, *test_inter;

	/* Remove and free the interval from the set of intervals.
	 * Note that the interval freed may not be the same one supplied. */
	assert(!old_range || old_range->interval == node);
	free_inter = remove_interval(root, node);
	assert(free_inter->data == old_range);
	free_interval(free_inter);

	/* Make sure the remaining ranges point to the intervals that hold them.
	 * This is necessary as the Range object may have moved to a different node. */
	for (test_inter = minimum_interval(*root); test_inter != NULL; test_inter = next_interval(test_inter)) {
		Range *range = test_inter->data;
		assert(range);
		assert(range != old_range);
		if (range->interval != test_inter) {
			range->interval = test_inter;
		}
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

void print_notify(Maap_Notify *mn, print_notify_callback_t notify_callback, void *callback_data)
{
	char szOutput[300];

	assert(mn);

	switch (mn->result)
	{
	case MAAP_NOTIFY_ERROR_NONE:
		/* No error.  Don't display anything. */
		break;
	case MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION:
		notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR,
			"MAAP is not initialized, so the command cannot be performed.");
		break;
	case MAAP_NOTIFY_ERROR_ALREADY_INITIALIZED:
		notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR,
			"MAAP is already initialized, so the values cannot be changed.");
		break;
	case MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE:
		notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR,
			"The MAAP reservation is not available, or yield cannot allocate a replacement block. "
			"Try again with a smaller address block size.");
		break;
	case MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID:
		notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR,
			"The MAAP reservation ID is not valid, so cannot be released or report its status.");
		break;
	case MAAP_NOTIFY_ERROR_OUT_OF_MEMORY:
		notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR,
			"The MAAP application is out of memory.");
		break;
	case MAAP_NOTIFY_ERROR_INTERNAL:
		notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR,
			"The MAAP application experienced an internal error.");
		break;
	default:
		sprintf(szOutput, "The MAAP application returned an unknown error %d.", mn->result);
		notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR, szOutput);
		break;
	}

	switch (mn->kind)
	{
	case MAAP_NOTIFY_INITIALIZED:
		if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
			sprintf(szOutput, "MAAP initialized:  0x%012llx-0x%012llx (Size: %d)",
				(unsigned long long) mn->start,
				(unsigned long long) mn->start + mn->count - 1,
				(unsigned int) mn->count);
			notify_callback(callback_data, MAAP_LOG_LEVEL_INFO, szOutput);
		} else {
			sprintf(szOutput, "MAAP previously initialized to 0x%012llx-0x%012llx (Size: %d)",
				(unsigned long long) mn->start,
				(unsigned long long) mn->start + mn->count - 1,
				(unsigned int) mn->count);
			notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR, szOutput);
		}
		break;
	case MAAP_NOTIFY_ACQUIRING:
		if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
			sprintf(szOutput, "Address range %d querying:  0x%012llx-0x%012llx (Size %d)",
				mn->id,
				(unsigned long long) mn->start,
				(unsigned long long) mn->start + mn->count - 1,
				mn->count);
			notify_callback(callback_data, MAAP_LOG_LEVEL_INFO, szOutput);
		} else {
			sprintf(szOutput, "Unknown address range %d acquisition error", mn->id);
			notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR, szOutput);
		}
		break;
	case MAAP_NOTIFY_ACQUIRED:
		if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
			sprintf(szOutput, "Address range %d acquired:  0x%012llx-0x%012llx (Size %d)",
				mn->id,
				(unsigned long long) mn->start,
				(unsigned long long) mn->start + mn->count - 1,
				mn->count);
			notify_callback(callback_data, MAAP_LOG_LEVEL_INFO, szOutput);
		} else if (mn->id != -1) {
			sprintf(szOutput, "Address range %d of size %d not acquired",
				mn->id, mn->count);
			notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR, szOutput);
		} else {
			sprintf(szOutput, "Address range of size %d not acquired",
				mn->count);
			notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR, szOutput);
		}
		break;
	case MAAP_NOTIFY_RELEASED:
		if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
			sprintf(szOutput, "Address range %d released:  0x%012llx-0x%012llx (Size %d)",
				mn->id,
				(unsigned long long) mn->start,
				(unsigned long long) mn->start + mn->count - 1,
				mn->count);
			notify_callback(callback_data, MAAP_LOG_LEVEL_INFO, szOutput);
		} else {
			sprintf(szOutput, "Address range %d not released",
				mn->id);
			notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR, szOutput);
		}
		break;
	case MAAP_NOTIFY_STATUS:
		if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
			sprintf(szOutput, "ID %d is address range 0x%012llx-0x%012llx (Size %d)",
				mn->id,
				(unsigned long long) mn->start,
				(unsigned long long) mn->start + mn->count - 1,
				mn->count);
			notify_callback(callback_data, MAAP_LOG_LEVEL_INFO, szOutput);
		} else {
			sprintf(szOutput, "ID %d is not valid",
				mn->id);
			notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR, szOutput);
		}
		break;
	case MAAP_NOTIFY_YIELDED:
		if (mn->result != MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION && mn->result != MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID) {
			sprintf(szOutput, "Address range %d yielded:  0x%012llx-0x%012llx (Size %d)",
				mn->id,
				(unsigned long long) mn->start,
				(unsigned long long) mn->start + mn->count - 1,
				mn->count);
			notify_callback(callback_data, MAAP_LOG_LEVEL_WARNING, szOutput);
			if (mn->result != MAAP_NOTIFY_ERROR_NONE) {
				notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR,
					"A new address range will not be allocated");
			}
		} else {
			sprintf(szOutput, "ID %d is not valid",
				mn->id);
			notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR, szOutput);
		}
		break;
	default:
		sprintf(szOutput, "Notification type %d not recognized", mn->kind);
		notify_callback(callback_data, MAAP_LOG_LEVEL_ERROR, szOutput);
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
		MAAP_LOG_ERROR("Failed to create Timer");
		return -1;
	}

	mc->net = Net_newNet();
	if (!mc->net) {
		MAAP_LOG_ERROR("Failed to create Net");
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
		while (mc->timer_queue) {
			Range * pDel = mc->timer_queue;
			mc->timer_queue = mc->timer_queue->next_timer;
			if (pDel->state == MAAP_STATE_RELEASED) { free(pDel); }
		}

		while (mc->ranges) {
			Range *range = mc->ranges->data;
			remove_range_interval(&mc->ranges, mc->ranges);
			if (range) { free(range); }
		}

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
	/* Return a value between 1 and variation-1, inclusive.
	 * This is to adhere to IEEE 1722-2016 B.3.4.1 and B.3.4.2. */
	return random() % (variation - 1) + 1;
}

int schedule_timer(Maap_Client *mc, Range *range) {
	Range *rp, *prev_rp;
	unsigned long long int ns;
	Time ts;

	assert(mc);
	assert(range);

#ifdef DEBUG_TIMER_MSG
	if (MAAP_LOG_LEVEL_DEBUG <= MAAP_LOG_LEVEL) {
		Time_setFromMonotonicTimer(&ts);
		MAAP_LOGF_DEBUG("schedule_timer called at:  %s", Time_dump(&ts));
	}
#endif

	if (range->state == MAAP_STATE_PROBING) {
		ns = MAAP_PROBE_INTERVAL_BASE + rand_ms(MAAP_PROBE_INTERVAL_VARIATION);
		ns = ns * 1000000;
#ifdef DEBUG_TIMER_MSG
		MAAP_LOGF_DEBUG("Scheduling probe timer for %llu ns from now", ns);
#endif
		Time_setFromNanos(&ts, ns);
		Time_setFromMonotonicTimer(&range->next_act_time);
		Time_add(&range->next_act_time, &ts);
#ifdef DEBUG_TIMER_MSG
		MAAP_LOGF_DEBUG("Expiration time is:  %s", Time_dump(&range->next_act_time));
#endif
	} else if (range->state == MAAP_STATE_DEFENDING) {
		ns = MAAP_ANNOUNCE_INTERVAL_BASE + rand_ms(MAAP_ANNOUNCE_INTERVAL_VARIATION);
		ns = ns * 1000000;
#ifdef DEBUG_TIMER_MSG
		MAAP_LOGF_DEBUG("Scheduling defend timer for %llu ns from now", ns);
#endif
		Time_setFromNanos(&ts, (uint64_t)ns);
		Time_setFromMonotonicTimer(&range->next_act_time);
		Time_add(&range->next_act_time, &ts);
#ifdef DEBUG_TIMER_MSG
		MAAP_LOGF_DEBUG("Expiration time is:  %s", Time_dump(&range->next_act_time));
#endif
	}

	/* Remove the range from the timer queue, if it is already in it. */
	if (mc->timer_queue == range) {
		/* Range was at the front of the queue. */
		mc->timer_queue = range->next_timer;
	} else if (mc->timer_queue) {
		/* Search the rest of the queue. */
		prev_rp = mc->timer_queue;
		rp = prev_rp->next_timer;
		while (rp && rp != range) {
			prev_rp = rp;
			rp = rp->next_timer;
		}
		if (rp) {
			/* Range was found.  Remove it. */
			prev_rp->next_timer = rp->next_timer;
			rp->next_timer = NULL;
		}
	}

	/* Add the range to the timer queue. */
	if (mc->timer_queue == NULL ||
		Time_cmp(&range->next_act_time, &mc->timer_queue->next_act_time) < 0) {
		range->next_timer = mc->timer_queue;
		mc->timer_queue = range;
	} else {
		rp = mc->timer_queue;
		while (rp->next_timer &&
			Time_cmp(&rp->next_timer->next_act_time, &range->next_act_time) <= 0)
		{
			rp = rp->next_timer;
		}
		range->next_timer = rp->next_timer;
		rp->next_timer = range;
	}

#ifdef DEBUG_TIMER_MSG
	/* Perform a sanity test on the timer queue. */
	{
		Range *test = mc->timer_queue;
		int i;
		for (i = 0; test && i < 100000; ++i) {
			assert(test->next_timer != test);
			assert(test->next_timer == NULL || Time_cmp(&test->next_act_time, &test->next_timer->next_act_time) <= 0);
			test = test->next_timer;
		}
		if (test) {
			MAAP_LOG_ERROR("Timer infinite loop detected!");
			assert(0);
		}
	}
#endif

	return 0;
}

static int assign_interval(Maap_Client *mc, Range *range, uint64_t attempt_base, uint16_t len) {
	Interval *iv;
	int i, rv = INTERVAL_OVERLAP;
	uint32_t range_max;

	if (len > mc->range_len) { return -1; }
	range_max = mc->range_len - 1;

	/* If we were supplied with a base address to attempt, try that first. */
	if (attempt_base >= mc->address_base &&
		attempt_base + len - 1 <= mc->address_base + mc->range_len - 1)
	{
		iv = alloc_interval((uint32_t) (attempt_base - mc->address_base), len);
		assert(iv->high <= range_max);
		rv = insert_interval(&mc->ranges, iv);
		if (rv == INTERVAL_OVERLAP) {
			free_interval(iv);
		}
	}

	/** @todo Use the saved MAAP_ANNOUNCE message ranges to search for addresses likely to be available.
	 *  Old announced ranges (e.g. older than 1.75 minutes) can be deleted if there are no ranges available. */

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

int maap_reserve_range(Maap_Client *mc, const void *sender, uint64_t attempt_base, uint32_t length) {
	int id;
	Range *range;

	if (!mc->initialized) {
		MAAP_LOG_DEBUG("Reserve not allowed, as MAAP not initialized");
		inform_not_acquired(mc, sender, -1, length, MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION);
		return -1;
	}

	if (length > 0xFFFF || length > mc->range_len) {
		/* Range size cannot be more than 16 bits in size, due to the MAAP packet format */
		inform_not_acquired(mc, sender, -1, length, MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE);
		return -1;
	}

	range = malloc(sizeof(Range));
	if (range == NULL) {
		inform_not_acquired(mc, sender, -1, length, MAAP_NOTIFY_ERROR_OUT_OF_MEMORY);
		return -1;
	}

	id = ++(mc->maxid);
	range->id = id;
	range->state = MAAP_STATE_PROBING;
	range->counter = MAAP_PROBE_RETRANSMITS;
	range->overlapping = 0;
	Time_setFromMonotonicTimer(&range->next_act_time);
	range->interval = NULL;
	range->sender = sender;
	range->next_timer = NULL;

	if (assign_interval(mc, range, attempt_base, length) < 0)
	{
		/* Cannot find any available intervals of the requested size. */
		inform_not_acquired(mc, sender, -1, length, MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE);
		free(range);
		return -1;
	}

#ifdef DEBUG_NEGOTIATE_MSG
	MAAP_LOGF_DEBUG("Requested address range, id %d", id);
	MAAP_LOGF_DEBUG("Selected address range 0x%012llx-0x%012llx", get_start_address(mc, range), get_end_address(mc, range));
#endif
	inform_acquiring(mc, range);

	schedule_timer(mc, range);
	start_timer(mc);
	send_probe(mc, range);

	return id;
}

int maap_release_range(Maap_Client *mc, const void *sender, int id) {
	Interval *iv;
	Range *range;

	if (!mc->initialized) {
		MAAP_LOG_DEBUG("Release not allowed, as MAAP not initialized");
		inform_released(mc, sender, id, NULL, MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION);
		return -1;
	}

	range = mc->timer_queue;
	while (range) {
		if (range->id == id && range->state != MAAP_STATE_RELEASED) {
			inform_released(mc, sender, id, range, MAAP_NOTIFY_ERROR_NONE);
			if (sender != range->sender)
			{
				/* Also inform the sender that originally reserved this range. */
				inform_released(mc, range->sender, id, range, MAAP_NOTIFY_ERROR_NONE);
			}

			iv = range->interval;
			remove_range_interval(&mc->ranges, iv);
			/* memory for range will be freed the next time its timer elapses */
			range->state = MAAP_STATE_RELEASED;

			return 0;
		}
		range = range->next_timer;
	}

	MAAP_LOGF_DEBUG("Range id %d does not exist to release", id);
	inform_released(mc, sender, id, NULL, MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID);
	return -1;
}

void maap_range_status(Maap_Client *mc, const void *sender, int id)
{
	Range *range;

	if (!mc->initialized) {
		MAAP_LOG_DEBUG("Status not allowed, as MAAP not initialized");
		inform_status(mc, sender, id, NULL, MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION);
		return;
	}

	range = mc->timer_queue;
	while (range) {
		if (range->id == id && range->state == MAAP_STATE_DEFENDING) {
			inform_status(mc, sender, id, range, MAAP_NOTIFY_ERROR_NONE);
			return;
		}
		range = range->next_timer;
	}

	MAAP_LOGF_DEBUG("Range id %d does not exist", id);
	inform_status(mc, sender, id, NULL, MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID);
}

int maap_yield_range(Maap_Client *mc, const void *sender, int id) {
	Range *range;
	MAAP_Packet announce_packet;
	uint8_t announce_buffer[MAAP_NET_BUFFER_SIZE];

	if (!mc->initialized) {
		MAAP_LOG_DEBUG("Yield not allowed, as MAAP not initialized");
		inform_yielded(mc, sender, id, NULL, MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION);
		return -1;
	}

	range = mc->timer_queue;
	while (range) {
		if (range->id == id && range->state == MAAP_STATE_DEFENDING) {
			// Create a conflicting packet for this range.
			// Use a source address which will always be less than our address, so we should always yield.
			init_packet(&announce_packet, 0x010000000000ull, 0x010000000000ull);
			announce_packet.message_type = MAAP_ANNOUNCE;
			announce_packet.requested_start_address = get_start_address(mc, range);
			announce_packet.requested_count = get_count(mc, range);
			pack_maap(&announce_packet, announce_buffer);
			maap_handle_packet(mc, announce_buffer, MAAP_NET_BUFFER_SIZE);

			return 0;
		}
		range = range->next_timer;
	}

	MAAP_LOGF_DEBUG("Range id %d does not exist", id);
	inform_yielded(mc, sender, id, NULL, MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID);
	return -1;
}

int maap_handle_packet(Maap_Client *mc, const uint8_t *stream, int len) {
	MAAP_Packet p;
	Interval *iv;
	uint32_t start;
	Range *range;
	int rv, num_overlaps;
	unsigned long long int own_base, own_max, incoming_base, incoming_max;

	if (len < MAAP_PKT_SIZE) {
		MAAP_LOGF_ERROR("Truncated MAAP packet of length %d received, discarding", len);
		return -1;
	}
	rv = unpack_maap(&p, stream);
	if (rv != 0) {
		MAAP_LOG_ERROR("Error unpacking the MAAP packet");
		return -1;
	}

	if (p.Ethertype != MAAP_TYPE ||
		p.subtype != MAAP_SUBTYPE ||
		p.control_data_length != 16 )
	{
		/* This is not a MAAP packet.  Ignore it. */
#ifdef DEBUG_NEGOTIATE_MSG
		MAAP_LOGF_DEBUG("Ignoring non-MAAP packet of length %d", len);
#endif
		return -1;
	}

	if (p.version != 0) {
		MAAP_LOGF_ERROR("AVTP version %u not supported", p.version);
		return -1;
	}

	if (p.message_type < MAAP_PROBE || p.message_type > MAAP_ANNOUNCE) {
		MAAP_LOGF_ERROR("MAAP packet message type %u not recognized", p.message_type);
		return -1;
	}

	own_base = mc->address_base;
	own_max = mc->address_base + mc->range_len - 1;
	incoming_base = p.requested_start_address;
	incoming_max = p.requested_start_address + p.requested_count - 1;

#ifdef DEBUG_NEGOTIATE_MSG
	if (p.message_type == MAAP_PROBE) {
		MAAP_LOGF_DEBUG("Received PROBE for range 0x%012llx-0x%012llx (Size %u)", incoming_base, incoming_max, p.requested_count);
	}
	if (p.message_type == MAAP_DEFEND) {
		MAAP_LOGF_DEBUG("Received DEFEND for range 0x%012llx-0x%012llx (Size %u),",
			incoming_base, incoming_max, p.requested_count);
		MAAP_LOGF_DEBUG("conflicting with range 0x%012llx-0x%012llx (Size %u)",
			(unsigned long long) p.conflict_start_address,
			(unsigned long long) p.conflict_start_address + p.conflict_count - 1,
			p.conflict_count);
	}
	if (p.message_type == MAAP_ANNOUNCE) {
		MAAP_LOGF_DEBUG("Received ANNOUNCE for range 0x%012llx-0x%012llx (Size %u)", incoming_base, incoming_max, p.requested_count);
	}
#endif

	if (incoming_max < own_base || own_max < incoming_base) {
#ifdef DEBUG_NEGOTIATE_MSG
		MAAP_LOG_DEBUG("Packet refers to a range outside of our concern:");
		MAAP_LOGF_DEBUG("    0x%012llx < 0x%012llx || 0x%012llx < 0x%012llx", incoming_max, own_base, own_max, incoming_base);
#endif
		return 0;
	}

	/** @todo If this is a MAAP_ANNOUNCE message, save the announced range and time received for later reference. */

	/* Flag all the range items that overlap with the incoming packet. */
	num_overlaps = 0;
	start = (uint32_t) (p.requested_start_address - mc->address_base);
	for (iv = search_interval(mc->ranges, start, p.requested_count); iv != NULL && interval_check_overlap(iv, start, p.requested_count); iv = next_interval(iv)) {
		range = iv->data;
		range->overlapping = 1;
		num_overlaps++;
	}

	while (num_overlaps-- > 0) {
		/* Find the first item that is still flagged. */
		for (iv = search_interval(mc->ranges, start, p.requested_count); iv != NULL; iv = next_interval(iv)) {
			range = iv->data;
			if (range->overlapping) { break; }
		}
		if (!iv) {
			/* We reached the end of the list. */
			assert(0); /* We should never get here! */
			break;
		}
		range->overlapping = 0;

		if (range->state == MAAP_STATE_PROBING) {

			if (p.message_type == MAAP_PROBE && compare_mac_addresses(mc->src_mac, p.SA)) {
				/* We won with the lower MAC Address.  Do nothing. */
#ifdef DEBUG_NEGOTIATE_MSG
				MAAP_LOG_DEBUG("Ignoring conflicting probe request");
#endif
			} else {
				/* Find an alternate interval, remove old interval,
				   and restart probe counter */
				int range_size = iv->high - iv->low + 1;
				iv->data = NULL; /* Range is moving to a new interval */
				if (assign_interval(mc, range, 0, range_size) < 0) {
					/* No interval is available, so stop probing and report an error. */
					MAAP_LOG_WARNING("Unable to find an available address block to probe");
					inform_not_acquired(mc, range->sender, range->id, range_size, MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE);
					remove_range_interval(&mc->ranges, iv);
					/* memory will be freed the next time its timer elapses */
					range->state = MAAP_STATE_RELEASED;
				} else {
#ifdef DEBUG_NEGOTIATE_MSG
					MAAP_LOGF_DEBUG("Selected new address range 0x%012llx-0x%012llx",
						get_start_address(mc, range), get_end_address(mc, range));
#endif
					inform_acquiring(mc, range);

					remove_range_interval(&mc->ranges, iv);
					range->counter = MAAP_PROBE_RETRANSMITS;

					schedule_timer(mc, range);
					send_probe(mc, range);
				}
			}

		} else if (range->state == MAAP_STATE_DEFENDING) {

			MAAP_LOGF_INFO("Conflict detected with our range (id %d)!", range->id);
#ifdef DEBUG_NEGOTIATE_MSG
			MAAP_LOGF_DEBUG("    Request of 0x%012llx-0x%012llx conflicts with our range of 0x%012llx-0x%012llx",
				incoming_base, incoming_max,
				get_start_address(mc, range), get_end_address(mc, range));
#endif

			if (p.message_type == MAAP_PROBE) {
				MAAP_LOG_INFO("DEFEND!");
				send_defend(mc, range, p.requested_start_address, p.requested_count, p.SA);
			} else if (compare_mac_addresses(mc->src_mac, p.SA)) {
				/* We won with the lower MAC Address.  Do nothing. */
				MAAP_LOG_INFO("IGNORE");
			} else {
				Range *new_range;
				int range_size = iv->high - iv->low + 1;

				MAAP_LOG_INFO("YIELD");

				/* Start a new reservation request for the owner of the yielded reservation.
				 * Use the same ID as the yielded range, so the owner can easily track it.
				 *
				 * Note:  Because our previous range is still in our range list,
				 * the new range selected will not overlap it.
				 */
				new_range = malloc(sizeof(Range));
				if (new_range == NULL) {
					inform_yielded(mc, range->sender, range->id, range, MAAP_NOTIFY_ERROR_OUT_OF_MEMORY);
				} else {
					new_range->id = range->id;
					new_range->state = MAAP_STATE_PROBING;
					new_range->counter = MAAP_PROBE_RETRANSMITS;
					new_range->overlapping = 0;
					Time_setFromMonotonicTimer(&new_range->next_act_time);
					new_range->interval = NULL;
					new_range->sender = range->sender;
					new_range->next_timer = NULL;
					if (assign_interval(mc, new_range, 0, range_size) < 0)
					{
						/* Cannot find any available intervals of the requested size. */
						inform_yielded(mc, range->sender, range->id, range, MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE);
						free(new_range);
					} else {
#ifdef DEBUG_NEGOTIATE_MSG
						MAAP_LOGF_DEBUG("Requested replacement address range, id %d", new_range->id);
						MAAP_LOGF_DEBUG("Selected replacement address range 0x%012llx-0x%012llx", get_start_address(mc, new_range), get_end_address(mc, new_range));
#endif

						/* Send a probe for the replacement address range to try. */
						schedule_timer(mc, new_range);
						send_probe(mc, new_range);

						inform_yielded(mc, range->sender, range->id, range, MAAP_NOTIFY_ERROR_NONE);
						inform_acquiring(mc, new_range);
					}
				}

				/* We are done with the old range. */
				remove_range_interval(&mc->ranges, iv);
				/* memory will be freed the next time its timer elapses */
				range->state = MAAP_STATE_RELEASED;
			}

		}
	}

	start_timer(mc);

	return 0;
}

int handle_probe_timer(Maap_Client *mc, Range *range) {
	if (range->counter == 0) {
		inform_acquired(mc, range, MAAP_NOTIFY_ERROR_NONE);
		range->state = MAAP_STATE_DEFENDING;
		schedule_timer(mc, range);
		send_announce(mc, range);
	} else {
		range->counter--;
		schedule_timer(mc, range);
		send_probe(mc, range);
	}

	return 0;
}

int handle_defend_timer(Maap_Client *mc, Range *range) {
	schedule_timer(mc, range);
	send_announce(mc, range);

	return 0;
}

int maap_handle_timer(Maap_Client *mc) {
	Time currenttime;
	Range *range;

	/* Get the current time. */
	Time_setFromMonotonicTimer(&currenttime);
#ifdef DEBUG_TIMER_MSG
	MAAP_LOGF_DEBUG("maap_handle_timer called at:  %s", Time_dump(&currenttime));
#endif

	while ((range = mc->timer_queue) && Time_passed(&currenttime, &range->next_act_time)) {
#ifdef DEBUG_TIMER_MSG
		MAAP_LOGF_DEBUG("Due timer:  %s", Time_dump(&range->next_act_time));
#endif
		mc->timer_queue = range->next_timer;
		range->next_timer = NULL;

		if (range->state == MAAP_STATE_PROBING) {
#ifdef DEBUG_TIMER_MSG
			MAAP_LOG_DEBUG("Handling probe timer");
#endif
			handle_probe_timer(mc, range);
		} else if (range->state == MAAP_STATE_DEFENDING) {
#ifdef DEBUG_TIMER_MSG
			MAAP_LOG_DEBUG("Handling defend timer");
#endif
			handle_defend_timer(mc, range);
		} else if (range->state == MAAP_STATE_RELEASED) {
#ifdef DEBUG_TIMER_MSG
			MAAP_LOG_DEBUG("Freeing released timer");
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
	MAAP_LOG_DEBUG(""); /* Blank line */
	MAAP_LOGF_DEBUG("Time_remaining:  %lld ns", timeRemaining);
	MAAP_LOG_DEBUG(""); /* Blank line */
#endif
	return timeRemaining;
}
