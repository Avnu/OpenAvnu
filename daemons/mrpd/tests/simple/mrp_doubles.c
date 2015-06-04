#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "mrp_doubles.h"
#include "mrp.h"

#if TRACE_MRPD_DOUBLE
#define TRACE printf("%s\n", __FUNCTION__);
#else
#define TRACE
#endif

struct mrpd_test_state test_state;

/*
 * Test Double Controls
 */

void mrpd_reset(void)
{
	int i;
TRACE
	test_state.periodic_timer_id = 0;
	test_state.timers[0].state = TIMER_STOPPED;

	for (i = 1; i < MRPD_TIMER_COUNT; i++) {
		test_state.timers[i].state = TIMER_UNDEF;
		test_state.timers[i].value = 0;
		test_state.timers[i].interval = 0;
	}

	memset(test_state.ctl_msg_data, 0, MAX_MRPD_CMDSZ);
	test_state.ctl_msg_length = 0;

	test_state.ethertype = 0;
	memset(test_state.address, 0, sizeof test_state.address);
	test_state.sock_initialized = 0;

	memset(test_state.rx_PDU, 0, MAX_FRAME_SIZE);
	memset(test_state.tx_PDU, 0, MAX_FRAME_SIZE);
	test_state.sent_count = 0;

	memset(test_state.msrp_event_counts, 0, sizeof test_state.msrp_event_counts);
	memset(test_state.msrp_event_counts_per_type, 0, sizeof test_state.msrp_event_counts_per_type);
	test_state.forward_msrp_events = 1;
	test_state.msrp_observe = NULL;

	memset(test_state.mrpd_log, 0, MRPD_DOUBLE_LOG_SIZE);
}

unsigned int mrpd_send_packet_count(void)
{
        return test_state.sent_count;
}


/*
 * Test Double Implementations
 */

unsigned char STATION_ADDR[] = { 0x00, 0x88, 0x77, 0x66, 0x55, 0x44 };

HTIMER mrpd_timer_create(void)
{
	int i;
	HTIMER out = MRPD_TIMER_COUNT; /* set to invalid value */
TRACE
	for (i = 0; i < MRPD_TIMER_COUNT; i++) {
		if (test_state.timers[i].state == TIMER_UNDEF) {
			test_state.timers[i].state = TIMER_STOPPED;
			out = (HTIMER)i;
			break;
		}
	}

	assert(out != MRPD_TIMER_COUNT && "Out of mrpd test double timers");
	return out;
}

void mrpd_timer_close(HTIMER t)
{
	int id = (int)t;
TRACE
	assert(id >= 0 && id < MRPD_TIMER_COUNT);
	assert(test_state.timers[id].state != TIMER_UNDEF);
	test_state.timers[id].state = TIMER_UNDEF;
	test_state.timers[id].value = 0;
	test_state.timers[id].interval = 0;
}

int mrpd_timer_start_interval(HTIMER timerfd,
			      unsigned long value_ms, unsigned long interval_ms)
{
	int id = (int)timerfd;
TRACE
	assert(id >= 0 && id < MRPD_TIMER_COUNT);
	assert(test_state.timers[id].state == TIMER_STOPPED);
	test_state.timers[id].state = TIMER_STARTED;
	test_state.timers[id].value = value_ms;
	test_state.timers[id].interval = interval_ms;

        return 0;
}

int mrpd_timer_start(HTIMER timerfd, unsigned long value_ms)
{
TRACE
        return mrpd_timer_start_interval(timerfd, value_ms, 0);
}

int mrpd_timer_stop(HTIMER timerfd)
{
	int id = (int)timerfd;
TRACE
	assert(id >= 0 && id < MRPD_TIMER_COUNT);
	test_state.timers[id].state = TIMER_STOPPED;
	test_state.timers[id].value = 0;
	test_state.timers[id].interval = 0;

        return 0;
}

int mrpd_init_timers(struct mrp_database *mrp_db)
{
TRACE
        mrp_db->join_timer = mrpd_timer_create();
        mrp_db->lv_timer = mrpd_timer_create();
        mrp_db->lva_timer = mrpd_timer_create();
        mrp_db->join_timer_running = 0;
        mrp_db->lv_timer_running = 0;
        mrp_db->lva_timer_running = 0;

        return 0;
};


int mrp_periodictimer_start()
{
	HTIMER id = test_state.periodic_timer_id;
TRACE
        return mrpd_timer_start_interval(id, 1000, 1000);
}

int mrp_periodictimer_stop()
{
	HTIMER id = test_state.periodic_timer_id;
TRACE
        return mrpd_timer_stop(id);
}

int mrpd_recvmsgbuf(SOCKET sock, char **buf)
{
TRACE
	(void)sock;
	*buf = malloc(MAX_FRAME_SIZE);
	memcpy(*buf, test_state.rx_PDU, test_state.rx_PDU_len);

        return test_state.rx_PDU_len;
}

int mrpd_send_ctl_msg(struct sockaddr_in *client_addr, char *notify_data,
		      int notify_len)
{
TRACE
	assert(notify_len <= MAX_MRPD_CMDSZ);

	(void)client_addr; /* unused */
	memcpy(test_state.ctl_msg_data, notify_data, notify_len);

        return notify_len;
}

size_t mrpd_send(SOCKET sockfd, const void *buf, size_t len, int flags)
{
TRACE
	(void)sockfd; /* unused */
	(void)flags;  /* unused */
	memcpy(test_state.tx_PDU, buf, len);
        test_state.sent_count++;
        return len;
}

int mrpd_close_socket(SOCKET sock)
{
TRACE
	(void)sock;  /* unused */
	test_state.sock_initialized = 0;

        return 0;
}

int mrpd_init_protocol_socket(uint16_t etype, SOCKET *sock,
			      unsigned char *multicast_addr)
{
TRACE
	(void)sock;
	test_state.ethertype = etype;
	memcpy(test_state.address, multicast_addr, sizeof test_state.address);
	test_state.sock_initialized = 1;

        return 0;
}

#include "msrp.h"
extern int msrp_event_orig(int event, struct msrp_attribute *rattrib);
void dump_msrp_attrib(struct msrp_attribute *attr)
{
	printf("prev: %sNULL\n", attr->prev ? "Not " : "");
	printf("next: %sNULL\n", attr->prev ? "Not " : "");
	printf("type: %d\n", attr->type);
	printf("attribute:\n");
	if (attr->type < 4) {
		printf("  StreamID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
		       attr->attribute.talk_listen.StreamID[0],
		       attr->attribute.talk_listen.StreamID[1],
		       attr->attribute.talk_listen.StreamID[2],
		       attr->attribute.talk_listen.StreamID[3],
		       attr->attribute.talk_listen.StreamID[4],
		       attr->attribute.talk_listen.StreamID[5],
		       attr->attribute.talk_listen.StreamID[6],
		       attr->attribute.talk_listen.StreamID[7]);
	}
	if (attr->type < 3) {
		printf("  Dest_Addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
		       attr->attribute.talk_listen.DataFrameParameters.Dest_Addr[0],
		       attr->attribute.talk_listen.DataFrameParameters.Dest_Addr[1],
		       attr->attribute.talk_listen.DataFrameParameters.Dest_Addr[2],
		       attr->attribute.talk_listen.DataFrameParameters.Dest_Addr[3],
		       attr->attribute.talk_listen.DataFrameParameters.Dest_Addr[4],
		       attr->attribute.talk_listen.DataFrameParameters.Dest_Addr[5]);
		printf("  Vlan_ID: 0x%04x\n",
		       attr->attribute.talk_listen.DataFrameParameters.Vlan_ID);
		printf("  MaxFrameSize: %d\n",
		       attr->attribute.talk_listen.TSpec.MaxFrameSize);
		printf("  MaxIntervalFrames: %d\n",
		       attr->attribute.talk_listen.TSpec.MaxIntervalFrames);
		printf("  PriorityAndRank: 0x%x\n",
		       attr->attribute.talk_listen.PriorityAndRank);
		printf("  AccumulatedLatency: %d\n",
		       attr->attribute.talk_listen.AccumulatedLatency);
	}
	if (attr->type == 2) {
		printf("  BridgeID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
		       attr->attribute.talk_listen.FailureInformation.BridgeID[0],
		       attr->attribute.talk_listen.FailureInformation.BridgeID[1],
		       attr->attribute.talk_listen.FailureInformation.BridgeID[2],
		       attr->attribute.talk_listen.FailureInformation.BridgeID[3],
		       attr->attribute.talk_listen.FailureInformation.BridgeID[4],
		       attr->attribute.talk_listen.FailureInformation.BridgeID[5],
		       attr->attribute.talk_listen.FailureInformation.BridgeID[6],
		       attr->attribute.talk_listen.FailureInformation.BridgeID[7]);
		printf("  FailureCode: %d\n",
		       attr->attribute.talk_listen.FailureInformation.FailureCode);
	}
	if (attr->type == 4) {
		printf("  SRclassID: %d\n",
		       attr->attribute.domain.SRclassID);
		printf("  SRclassPriority: %d\n",
		       attr->attribute.domain.SRclassPriority);
		printf("  neighborSRclassPriority: %d\n",
		       attr->attribute.domain.neighborSRclassPriority);
		printf("  SRclassVID: %d\n",
		       attr->attribute.domain.SRclassVID);
	}
	printf("substate: %u\n", attr->substate);
	printf("direction: %u\n", attr->direction);
	printf("applicant:\n");
	printf("  mrp_state: %d\n", attr->applicant.mrp_state);
	printf("  tx: %d\n", attr->applicant.tx);
	printf("  sndmsg: %d\n", attr->applicant.sndmsg);
	printf("  encode: %d\n", attr->applicant.encode);
	printf("  mrp_previous_state: %d\n", attr->applicant.mrp_previous_state);
	printf("registrar:\n");
	printf("  mrp_state: %d\n", attr->registrar.mrp_state);
	printf("  notify: %d\n", attr->registrar.notify);
	printf("  rsvd: %d\n", attr->registrar.rsvd);
	printf("  macaddr: %02x:%02x:%02x:%02x:%02x:%02x\n",
	       attr->registrar.macaddr[0],
	       attr->registrar.macaddr[1],
	       attr->registrar.macaddr[2],
	       attr->registrar.macaddr[3],
	       attr->registrar.macaddr[4],
	       attr->registrar.macaddr[5]);
}

int msrp_event(int event, struct msrp_attribute *rattrib) {
	test_state.msrp_event_counts[MSRP_EVENT_IDX(event)]++;
	if(rattrib)
		test_state.msrp_event_counts_per_type[MSRP_TYPE_IDX(rattrib->type)][MSRP_EVENT_IDX(event)]++;
	if (test_state.msrp_observe) {
		test_state.msrp_observe(event, rattrib);
	}
	if (test_state.forward_msrp_events) {
		msrp_event_orig(event, rattrib);
	}
	return 0;
}
