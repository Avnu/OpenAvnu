#ifndef GUARD_MRP_DOUBLES_H
#define GUARD_MRP_DOUBLES_H

/*
 * Test doubles to stand in for implementations of functions in mrpd
 *
 * Prototypes are provided by mrp.h, see mrp_doubles.c for
 * implementation details.
 */

#define MRP_CPPUTEST 1

/* Set this to 1 to print the name of each test double function as it
   is called */
#define TRACE_MRPD_DOUBLE 0

#define MRPD_TIMER_COUNT 12
#define MRPD_DOUBLE_LOG_SIZE 1024

#define TIMER_UNDEF   -1
#define TIMER_STOPPED  0
#define TIMER_STARTED  1

#include "mrpd.h"

/**
 * Initialize or reset the test double state.
 *
 * This clears out the PDU buffers, timer information, etc.
 */
void mrpd_reset(void);

/**
 * Retrieve the number of PDUs sent by mrpd.
 */
unsigned int mrpd_send_packet_count(void);

/**
 * Structure to keep track of a timer test double.
 */
typedef struct {
	int state;
	unsigned long value;
	unsigned long interval;
} timer_double_t;


/*
 * TODO: At some point we may want to move some of the MSRP-specific
 * stuff to a different file.
 */
struct msrp_attribute;
/**
 * Callback function type that can be used to observe and validate
 * MSRP events
 */
typedef void (*msrp_observer_t)(int event, struct msrp_attribute *attrib);
/**
 * Print the contents of a msrp_attribute structure to stdout for debugging.
 */
void dump_msrp_attrib(struct msrp_attribute *attr);


/**
 * Structure that holds all test double state.
 *
 * Read and write to this to see how test double functions were called
 * and provide return values to be used by double functions.
 */
struct mrpd_test_state {
	/* Timer State */
	timer_double_t timers[MRPD_TIMER_COUNT];
	HTIMER periodic_timer_id;

	/* Control Message */
	char ctl_msg_data[MAX_MRPD_CMDSZ];
	int ctl_msg_length;

	/* Protocol Socket */
	uint16_t ethertype;
	unsigned char address[6];
	int sock_initialized;

	/* PDUs */
	unsigned char rx_PDU[MAX_FRAME_SIZE];
	unsigned char tx_PDU[MAX_FRAME_SIZE];
	unsigned int rx_PDU_len;
	int sent_count;

	/* MSRP Events */
	uint16_t msrp_event_counts[21];
	uint16_t msrp_event_counts_per_type[4][21];
	int forward_msrp_events;
	msrp_observer_t msrp_observe;

	/* Log Buffer */
	char mrpd_log[MRPD_DOUBLE_LOG_SIZE];
};
extern struct mrpd_test_state test_state;

/* Events are defined in mrp.h ranging sequentially from 100 to 2100 */
#define MSRP_EVENT_IDX(event) (event/MRP_EVENT_SPACING-1)
#define MSRP_EVENT_ID(idx) ((idx+1)*MRP_EVENT_SPACING)
#define MSRP_TYPE_IDX(atype) (atype-1)

#endif
