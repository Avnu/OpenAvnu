/******************************************************************************

  Copyright (c) 2012, Intel Corporation 
  Copyright (c) 2012, AudioScience, Inc
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
/*
 * Main code for Windows implementation of MRP/MMRP/MVRP/MSRP
 */

#include <stdio.h>
#include <stdlib.h>

#include <pcap.h>
#include <winsock2.h>
#include <iphlpapi.h>

#include "mrpd.h"
#include "mrp.h"
#include "mvrp.h"
#include "msrp.h"
#include "mmrp.h"
#include "mrpw.h"

#include "que.h"

#define TIME_PERIOD_25_MILLISECONDS 25
#define NETIF_TIMEOUT (-2)

/* local structs */
struct wtimer {
	int running;
	int elapsed;
	unsigned int count;
	unsigned int interval;
	unsigned int start_time;
};

struct netif {
	//pcap version
	pcap_if_t *alldevs;
	pcap_if_t *d;
	uint8_t mac[6];
	int inum;
	int i;
	int error;
	pcap_t *pcap_interface;
	char errbuf[PCAP_ERRBUF_SIZE];
	struct bpf_program fcode;
	struct pcap_pkthdr *header;
	const u_char *ethernet_frame;
	uint8_t tx_frame[1500];	// ethernet frame used to send packets
};

enum {
	pkt_event_wpcap,
	pkt_event_localhost,
	pkt_event_wpcap_timeout,
	pkt_event_localhost_timeout,
	app_event_kill_all,
	loop_time_tick
};

/* global mgmt parameters */
int daemonize;
int mmrp_enable;
int mvrp_enable;
int msrp_enable;
int logging_enable;
int mrpd_port;
HANDLE pkt_events[6];
HANDLE sem_kill_wpcap_thread;
HANDLE sem_kill_localhost_thread;
struct que_def *que_wpcap;
struct que_def *que_localhost;

//char *net_interface;
int interface_fd;

/* state machine controls */
int periodic_enable;
int registration;

/* if registration is FIXED or FORBIDDEN
 * updates from MRP are discarded, and
 * only IN and JOININ messages are sent
 */

int participant;

/* if participant role is 'SILENT' (or non-participant)
 * applicant doesn't send any messages - configured per-attribute
 */

#define VERSION_STR	"0.0"

static const char *version_str =
    "mrpw v" VERSION_STR "\n" "Copyright (c) 2012, AudioScience Inc\n";

unsigned char STATION_ADDR[] = { 0x00, 0x88, 0x77, 0x66, 0x55, 0x44 };

/* global variables */
SOCKET control_socket;
extern SOCKET mmrp_socket;
extern SOCKET mvrp_socket;
extern SOCKET msrp_socket;
struct netif *net_if;

HTIMER periodic_timer;
HTIMER gc_timer;
HTIMER timer_check_tick;

extern struct mmrp_database *MMRP_db;
extern struct mvrp_database *MVRP_db;
extern struct msrp_database *MSRP_db;

struct netif *netif_open(int timeout_ms)
{
	int inum = 0;
	int i = 0, total_interfaces = 0;
	struct netif *netif;
	IP_ADAPTER_INFO *AdapterInfo;
	IP_ADAPTER_INFO *Current;
	ULONG AIS;
	DWORD status;

	netif = (struct netif *)calloc(1, sizeof(struct netif));

	/* Retrieve the device list on the local machine */
	if (pcap_findalldevs(&netif->alldevs, netif->errbuf) == -1) {
		printf("Error finding interfaces\n");
		goto error_return;
	}

	/* Print the list */
	for (netif->d = netif->alldevs; netif->d; netif->d = netif->d->next) {
		/*printf("%d. %s", ++i, d->name); */
		printf("%d", ++i);
		if (netif->d->description)
			printf(" (%-70s)\n", netif->d->description);
		else
			printf(" (No description available)\n");
	}
	total_interfaces = i;

	if (i == 0) {
		printf
		    ("\nNo interfaces found! Make sure WinPcap is installed.\n");
		goto error_return;
	}

	printf("Enter the interface number (1-%d):", i);
	scanf_s("%d", &inum);

	if (inum < 1 || inum > i) {
		printf("\nInterface number out of range.\n");
		/* Free the device list */
		pcap_freealldevs(netif->alldevs);
		goto error_return;
	}

	/* Jump to the selected adapter */
	for (netif->d = netif->alldevs, i = 0; i < inum - 1;
	     netif->d = netif->d->next, i++) ;

	/* Open the device */
	if ((netif->pcap_interface = pcap_open_live(netif->d->name,	// name of the device
						    65536,	// portion of the packet to capture
						    // 65536 guarantees that the whole packet will be captured on all the link layers
						    1,	// promiscuous mode
						    timeout_ms,	// read timeout in ms
						    netif->errbuf	// error buffer
	     )) == NULL) {
		fprintf(stderr,
			"\nUnable to open the adapter. %s is not supported by WinPcap\n",
			netif->d->name);
		/* Free the device list */
		pcap_freealldevs(netif->alldevs);
		goto error_return;
	}

	/* lookup ip address */
	AdapterInfo =
	    (IP_ADAPTER_INFO *) calloc(total_interfaces,
				       sizeof(IP_ADAPTER_INFO));
	AIS = sizeof(IP_ADAPTER_INFO) * total_interfaces;
	status = GetAdaptersInfo(AdapterInfo, &AIS);
	if (status != ERROR_SUCCESS) {
		fprintf(stderr,
			"\nError, GetAdaptersInfo() call in netif_win32_pcap.c failed\n");
		free(AdapterInfo);
		goto error_return;
	}

	for (Current = AdapterInfo; Current != NULL; Current = Current->Next) {
		if (strstr(netif->d->name, Current->AdapterName) != 0) {
			uint32_t my_ip;
			ULONG len;
			uint8_t tmp[16];

			my_ip =
			    inet_addr(Current->IpAddressList.IpAddress.String);
			len = sizeof(tmp);
			SendARP(my_ip, INADDR_ANY, tmp, &len);
			memcpy(netif->mac, &tmp[0], sizeof(netif->mac));
		}
	}
	free(AdapterInfo);
	return netif;

 error_return:
	free(netif);
	return NULL;
}

int netif_close(struct netif *net_if)
{
	/* At this point, we don't need any more the device list. Free it */
	pcap_freealldevs(net_if->alldevs);
	pcap_close(net_if->pcap_interface);
	free(net_if);
	return (0);
}

int netif_set_capture_ethertype(struct netif *net_if, uint16_t * ethertype,
				uint32_t count)
{
	struct bpf_program fcode;
	char ethertype_string[512];
	char ethertype_single[64];
	unsigned int i;

	ethertype_string[0] = 0;
	for (i = 0; i < count; i++) {
		sprintf(ethertype_single, "ether proto 0x%04x", ethertype[i]);
		strcat(ethertype_string, ethertype_single);
		if ((i + 1) < count)
			strcat(ethertype_string, " or ");
	}

	// compile a filter
	if (pcap_compile(net_if->pcap_interface, &fcode, ethertype_string, 1, 0)
	    < 0) {
		fprintf(stderr,
			"\nUnable to compile the packet filter. Check the syntax.\n");
		/* Free the device list */
		pcap_freealldevs(net_if->alldevs);
		return -1;
	}
	//set the filter
	if (pcap_setfilter(net_if->pcap_interface, &fcode) < 0) {
		fprintf(stderr, "\nError setting the filter.\n");
		/* Free the device list */
		pcap_freealldevs(net_if->alldevs);
		return -1;
	}

	return (0);
}

int netif_capture_frame(struct netif *net_if, uint8_t ** frame,
			uint16_t * length)
{
	struct pcap_pkthdr *header;
	int error = 0;

	*length = 0;
	error = pcap_next_ex(net_if->pcap_interface, &header, frame);
	if (error > 0) {
		*length = (uint16_t) header->len;
		return (1);
	} else {
		return (NETIF_TIMEOUT);
	}
}

int netif_send_frame(struct netif *net_if, uint8_t * frame, uint16_t length)
{
	//printf("TX frame: %d bytes\n", length);
	if (pcap_sendpacket(net_if->pcap_interface, frame, length) != 0) {
		fprintf(stderr, "\nError sending the packet: \n",
			pcap_geterr(net_if->pcap_interface));
		return -1;
	}
	return (0);
}

struct netif_thread_data {
	uint8_t *frame;
	uint16_t length;
};

DWORD WINAPI netif_thread(LPVOID lpParam)
{
	int status;
	struct netif_thread_data d;

	while (WaitForSingleObject(sem_kill_wpcap_thread, 0)) {
		status = netif_capture_frame(net_if, &d.frame, &d.length);
		if (status > 0) {
			que_push(que_wpcap, &d);
		} else {
			if (!SetEvent(pkt_events[pkt_event_wpcap_timeout])) {
				printf
				    ("SetEvent pkt_event_wpcap_timeout failed (%d)\n",
				     GetLastError());
				return 1;
			}
		}
	}
	return 0;
}

static int clock_monotonic_in_ms_init = 0;
static LARGE_INTEGER clock_monotonic_freq;

unsigned int clock_monotonic_in_ms(void)
{
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	if (!clock_monotonic_in_ms_init) {
		QueryPerformanceFrequency(&clock_monotonic_freq);
		clock_monotonic_in_ms_init = 1;
	}
	return (unsigned int)((double)count.QuadPart * 1000.0 /
			      (double)clock_monotonic_freq.QuadPart);
}

HTIMER mrpd_timer_create(void)
{
	return (struct wtimer *)calloc(1, sizeof(struct wtimer));
}

void mrpd_timer_close(HTIMER t)
{
	if (t)
		free(t);
}

int mrpd_timer_start_interval(HTIMER timerfd,
			      unsigned long value_ms, unsigned long interval_ms)
{
	struct wtimer *t = (struct wtimer *)timerfd;
	t->running = TRUE;
	t->elapsed = FALSE;
	t->count = value_ms;
	t->interval = interval_ms;
	t->start_time = clock_monotonic_in_ms();
	return 0;
}

int mrpd_timer_start(HTIMER timerfd, unsigned long value_ms)
{
	return mrpd_timer_start_interval(timerfd, value_ms, 0);
}

int mrpd_timer_restart(HTIMER timerfd)
{
	struct wtimer *t = (struct wtimer *)timerfd;
	if (t->interval) {
		t->start_time = t->start_time + t->interval;
		t->running = TRUE;
		t->elapsed = FALSE;
	}
	return 0;
}

int mrpd_timer_stop(HTIMER timerfd)
{
	struct wtimer *t = (struct wtimer *)timerfd;
	t->running = FALSE;
	t->elapsed = FALSE;
	return 0;
}

int mrpd_timer_timeout(HTIMER timerfd)
{
	struct wtimer *t = (struct wtimer *)timerfd;
	if (t->running && !t->elapsed) {
		unsigned int elapsed_ms;
		unsigned int current_time = clock_monotonic_in_ms();
		elapsed_ms = current_time - t->start_time;
		if (elapsed_ms > t->count) {
			t->elapsed = TRUE;
		}
	}
	return t->elapsed;
}

int gctimer_start()
{
	/* reclaim memory every 30 minutes */
	return mrpd_timer_start(gc_timer, 30 * 60 * 1000);
}

int periodictimer_start()
{
	/* periodictimer has expired. (10.7.5.23)
	 * PeriodicTransmission state machine generates periodic events
	 * period is one-per-sec
	 */
	return mrpd_timer_start_interval(periodic_timer, 1000, 1000);
}

int periodictimer_stop()
{
	/* periodictimer has expired. (10.7.5.23)
	 * PeriodicTransmission state machine generates periodic events
	 * period is one-per-sec
	 */
	return mrpd_timer_stop(periodic_timer);
}

int mrpd_init_timers(struct mrp_database *mrp_db)
{
	mrp_db->join_timer = mrpd_timer_create();
	mrp_db->lv_timer = mrpd_timer_create();
	mrp_db->lva_timer = mrpd_timer_create();

	if (NULL == mrp_db->join_timer)
		goto out;
	if (NULL == mrp_db->lv_timer)
		goto out;
	if (NULL == mrp_db->lva_timer)
		goto out;
	return 0;
 out:
	mrpd_timer_close(mrp_db->join_timer);
	mrpd_timer_close(mrp_db->lv_timer);
	mrpd_timer_close(mrp_db->lva_timer);

	return -1;
}

int init_timers(void)
{
	/*
	 * primarily whether to schedule the periodic timer as the
	 * rest are self-scheduling as a side-effect of state transitions
	 * of the various attributes
	 */

	periodic_timer = mrpd_timer_create();
	gc_timer = mrpd_timer_create();

	if (NULL == periodic_timer)
		goto out;
	if (NULL == gc_timer)
		goto out;

	gctimer_start();

	if (periodic_enable)
		periodictimer_start();

	return 0;
 out:
	return -1;
}

//sockaddr

int process_ctl_msg(char *buf, int buflen, struct sockaddr_in *client)
{

	char respbuf[8];
	/*
	 * Inbound/output commands from/to a client:
	 *
	 * When sent from a client, indicates an operation on the
	 * internal attribute databases. When sent by the daemon to
	 * a client, indicates an event such as a new attribute arrival,
	 * or a leaveall timer to force clients to re-advertise continued
	 * interest in an attribute.
	 *
	 * BYE   Client detaches from daemon
	 *
	 * M+? - JOIN_MT a MAC address or service declaration
	 * M++   JOIN_IN a MAC Address (XXX: MMRP doesn't use 'New' though?)
	 * M-- - LV a MAC address or service declaration
	 * V+? - JOIN_MT a VID (VLAN ID)
	 * V++ - JOIN_IN a VID (VLAN ID)
	 * V-- - LV a VID (VLAN ID)
	 * S+? - JOIN_MT a Stream
	 * S++ - JOIN_IN a Stream
	 * S-- - LV a Stream
	 *
	 * Outbound messages
	 * ERC - error, unrecognized command
	 * ERP - error, unrecognized parameter
	 * ERI - error, internal
	 * OK+ - success
	 *
	 * Registrar Commands
	 *
	 * M?? - query MMRP Registrar MAC Address database
	 * V?? - query MVRP Registrar VID database
	 * S?? - query MSRP Registrar database
	 *
	 * Registrar Responses (to ?? commands)
	 *
	 * MIN - Registered
	 * MMT - Registered, Empty
	 * MLV - Registered, Leaving
	 * MNE - New attribute notification
	 * MJO - JOIN attribute notification
	 * MLV - LEAVE attribute notification
	 * VIN - Registered
	 * VMT - Registered, Empty
	 * VLV - Registered, Leaving
	 * SIN - Registered
	 * SMT - Registered, Empty
	 * SLV - Registered, Leaving
	 *
	 */

	memset(respbuf, 0, sizeof(respbuf));

#if LOG_CLIENT_RECV
	if (logging_enable)
		printf("CMD:%s from CLNT %d\n", buf, client->sin_port);
#endif

	if (buflen < 3) {
		printf("buflen = %d!\b", buflen);

		return -1;
	}

	switch (buf[0]) {
	case 'M':
		return mmrp_recv_cmd(buf, buflen, client);
		break;
	case 'V':
		return mvrp_recv_cmd(buf, buflen, client);
		break;
	case 'S':
		return msrp_recv_cmd(buf, buflen, client);
		break;
	case 'B':
		mmrp_bye(client);
		mvrp_bye(client);
		msrp_bye(client);
		break;
	default:
		printf("unrecognized command %s\n", buf);
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC %s", buf);
		mrpd_send_ctl_msg(client, respbuf, sizeof(respbuf));
		return -1;
		break;
	}

	return 0;
}

static uint8_t *last_pdu_buffer;
static int last_pdu_buffer_size;

int mrpd_recvmsgbuf(SOCKET sock, char **buf)
{

	*buf = (char *)malloc(MAX_FRAME_SIZE);
	if (NULL == *buf)
		return -1;

	memcpy(*buf, last_pdu_buffer, last_pdu_buffer_size);
	return last_pdu_buffer_size;
}

int mrpd_send_ctl_msg(struct sockaddr_in *client_addr,
		      char *notify_data, int notify_len)
{

	int rc;

	if (INVALID_SOCKET == control_socket)
		return 0;

#if LOG_CLIENT_SEND
	printf("CTL MSG:%s to CLNT %d\n", notify_data, client_addr->sin_port);
#endif
	rc = sendto(control_socket, notify_data, notify_len,
		    0, (struct sockaddr *)client_addr, sizeof(struct sockaddr));
	return rc;
}

int mrpd_close_socket(SOCKET sock)
{
	return closesocket(sock);
}

size_t mrpd_send(SOCKET sockfd, const void *buf, size_t len, int flags)
{
	if (pcap_sendpacket(net_if->pcap_interface, buf, len) != 0) {
		fprintf(stderr, "\nError sending the packet: \n",
			pcap_geterr(net_if->pcap_interface));
		return -1;
	}
	return len;
}

int mrpd_init_protocol_socket(uint16_t etype, SOCKET * sock,
			      unsigned char *multicast_addr)
{
	*sock = 0x1234;
	return 0;
}

int init_local_ctl(void)
{
	struct sockaddr_in addr;
	SOCKET sock_fd = INVALID_SOCKET;
	int rc;
	u_long iMode = 1;
	DWORD iResult;
	int timeout = 100;	// ms

	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock_fd == INVALID_SOCKET)
		goto out;

	iResult = setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO,
			     (void *)&timeout, sizeof(timeout));
	if (iResult != NO_ERROR)
		goto out;
	iResult = setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO,
			     (void *)&timeout, sizeof(timeout));
	if (iResult != NO_ERROR)
		goto out;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(mrpd_port);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	rc = bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr));

	if (rc == SOCKET_ERROR)
		goto out;

	control_socket = sock_fd;

	return 0;
 out:
	if (sock_fd != INVALID_SOCKET)
		closesocket(sock_fd);

	return -1;
}

struct ctl_thread_params {
	char *msgbuf;
	struct sockaddr client_addr;
	int bytes;
};

DWORD WINAPI ctl_thread(LPVOID lpParam)
{
	struct ctl_thread_params s;
	int client_len;

	while (WaitForSingleObject(sem_kill_localhost_thread, 0)) {
		client_len = sizeof(struct sockaddr);
		s.msgbuf = (char *)malloc(MAX_MRPD_CMDSZ);
		s.bytes =
		    recvfrom(control_socket, s.msgbuf, MAX_MRPD_CMDSZ, 0,
			     &s.client_addr, &client_len);
		if (s.bytes > 0) {
			que_push(que_localhost, &s);
		} else {
			free(s.msgbuf);
			if (!SetEvent(pkt_events[pkt_event_localhost_timeout])) {
				printf
				    ("SetEvent pkt_event_localhost_timeout failed (%d)\n",
				     GetLastError());
				return 1;
			}
		}
	}
	return 0;
}

int mrpd_reclaim()
{

	/*
	 * if the local applications have neither registered interest
	 * by joining, and the remote node has quit advertising the attribute
	 * and allowing it to go into the MT state, delete the attribute 
	 */

	if (mmrp_enable)
		mmrp_reclaim();
	if (mvrp_enable)
		mvrp_reclaim();
	if (msrp_enable)
		msrp_reclaim();

	gctimer_start();

	return 0;

}

HANDLE kill_packet_capture;

int mrpw_init_threads(void)
{
	HANDLE hThread1, hThread2;
	DWORD dwThreadID1, dwThreadID2;
	struct avb_avtpdu *avtpdu = NULL;
	uint64_t src_mac_address = 0;
	int i;

	timer_check_tick = mrpd_timer_create();
	mrpd_timer_start_interval(timer_check_tick, 100, 100);

	que_wpcap = que_new(256, sizeof(struct netif_thread_data));
	que_localhost = que_new(256, sizeof(struct ctl_thread_params));

	sem_kill_wpcap_thread = CreateSemaphore(NULL, 0, 32767, NULL);
	sem_kill_localhost_thread = CreateSemaphore(NULL, 0, 32767, NULL);
	for (i = pkt_event_wpcap_timeout; i <= loop_time_tick; i++) {
		pkt_events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (pkt_events[i] == NULL) {
			fprintf(stderr, "CreateEvent error: %d\n",
				GetLastError());
			ExitProcess(0);
		}
	}
	pkt_events[pkt_event_wpcap] = que_data_available_object(que_wpcap);
	pkt_events[pkt_event_localhost] =
	    que_data_available_object(que_localhost);

	// Create threads
	hThread1 = CreateThread(NULL,	// default security attributes
				0,	// default stack size
				(LPTHREAD_START_ROUTINE) netif_thread, NULL,	// no thread function arguments
				0,	// default creation flags
				&dwThreadID1);	// receive thread identifier

	if (hThread1 == NULL) {
		fprintf(stderr, "CreateThread error: %d\n", GetLastError());
		return -1;
	}

	hThread2 = CreateThread(NULL,	// default security attributes
				0,	// default stack size
				(LPTHREAD_START_ROUTINE) ctl_thread, NULL,	// no thread function arguments
				0,	// default creation flags
				&dwThreadID2);	// receive thread identifier

	if (hThread2 == NULL) {
		fprintf(stderr, "CreateThread error: %d\n", GetLastError());
		return -1;
	}
	return 0;
}

int mrpw_run_once(void)
{
	struct netif_thread_data wpcap_pkt;
	struct ctl_thread_params localhost_pkt;
	uint16_t length = 0;
	uint8_t *payload;
	uint16_t protocol;
	uint8_t *proto;

	DWORD dwEvent =
	    WaitForMultipleObjects(sizeof(pkt_events) / sizeof(HANDLE),
				   pkt_events,
				   FALSE,
				   100);	/* 100ms wait */

	/* special exit case */
	if (WAIT_OBJECT_0 + app_event_kill_all == dwEvent)
		return -1;

	switch (dwEvent) {
	case WAIT_TIMEOUT:
	case WAIT_OBJECT_0 + loop_time_tick:
		/* timeout - run protocols */
		if (mmrp_enable) {
			if (mrpd_timer_timeout(MMRP_db->mrp_db.lva_timer))
				mmrp_event(MRP_EVENT_LVATIMER, NULL);
			if (mrpd_timer_timeout(MMRP_db->mrp_db.lv_timer))
				mmrp_event(MRP_EVENT_LVTIMER, NULL);
			if (mrpd_timer_timeout(MMRP_db->mrp_db.join_timer))
				mmrp_event(MRP_EVENT_TX, NULL);
		}

		if (mvrp_enable) {
			if (mrpd_timer_timeout(MVRP_db->mrp_db.lva_timer))
				mvrp_event(MRP_EVENT_LVATIMER, NULL);
			if (mrpd_timer_timeout(MVRP_db->mrp_db.lv_timer))
				mvrp_event(MRP_EVENT_LVTIMER, NULL);
			if (mrpd_timer_timeout(MVRP_db->mrp_db.join_timer))
				mvrp_event(MRP_EVENT_TX, NULL);
		}

		if (msrp_enable) {
			if (mrpd_timer_timeout(MSRP_db->mrp_db.lva_timer))
				msrp_event(MRP_EVENT_LVATIMER, NULL);
			if (mrpd_timer_timeout(MSRP_db->mrp_db.lv_timer))
				msrp_event(MRP_EVENT_LVTIMER, NULL);
			if (mrpd_timer_timeout(MSRP_db->mrp_db.join_timer))
				msrp_event(MRP_EVENT_TX, NULL);
		}

		if (mrpd_timer_timeout(periodic_timer)) {
			//printf("mrpd_timer_timeout(periodic_timer)\n");
			if (mmrp_enable)
				mmrp_event(MRP_EVENT_PERIODIC, NULL);
			if (mvrp_enable)
				mvrp_event(MRP_EVENT_PERIODIC, NULL);
			if (msrp_enable)
				msrp_event(MRP_EVENT_PERIODIC, NULL);
			mrpd_timer_restart(periodic_timer);
		}
		if (mrpd_timer_timeout(gc_timer)) {
			mrpd_reclaim();
		}
		mrpd_timer_restart(timer_check_tick);
		break;

	case WAIT_OBJECT_0 + pkt_event_wpcap:
		que_pop_nowait(que_wpcap, &wpcap_pkt);
		proto = &wpcap_pkt.frame[12];
		protocol = (uint16_t) proto[0] << 8 | (uint16_t) proto[1];
		payload = proto + 2;

		last_pdu_buffer = wpcap_pkt.frame;
		last_pdu_buffer_size = wpcap_pkt.length;

		switch (protocol) {
		case MVRP_ETYPE:
			if (mvrp_enable)
				mvrp_recv_msg();
			break;

		case MMRP_ETYPE:
			if (mmrp_enable)
				mmrp_recv_msg();
			break;

		case MSRP_ETYPE:
			if (msrp_enable)
				msrp_recv_msg();
			break;
		}
		if (mrpd_timer_timeout(&timer_check_tick)) {
			if (!SetEvent(pkt_events[loop_time_tick])) {
				printf("SetEvent loop_time_tick failed (%d)\n",
				       GetLastError());
				exit(-1);
			}
		}
		break;

	case WAIT_OBJECT_0 + pkt_event_wpcap_timeout:
		//printf("pkt_event_wpcap_timeout\n");
		break;

	case WAIT_OBJECT_0 + pkt_event_localhost:
		que_pop_nowait(que_localhost, &localhost_pkt);
		process_ctl_msg(localhost_pkt.msgbuf,
				localhost_pkt.bytes, (struct sockaddr_in *)
				&localhost_pkt.client_addr);
		if (mrpd_timer_timeout(&timer_check_tick)) {
			if (!SetEvent(pkt_events[loop_time_tick])) {
				printf("SetEvent loop_time_tick failed (%d)\n",
				       GetLastError());
				exit(-1);
			}
		}
		break;

	case WAIT_OBJECT_0 + pkt_event_localhost_timeout:
		//printf("pkt_event_localhost_timeout\n");
		break;

	default:
		printf("Unknown event %d\n", dwEvent);
	}
	return 0;
}

int mrpw_init_protocols(void)
{
	uint16_t ether_types[4];
	WSADATA wsa_data;
	int rc;

	mrpd_port = MRPD_PORT_DEFAULT;
	mmrp_enable = 1;
	mvrp_enable = 1;
	msrp_enable = 1;
	periodic_enable = 1;
	logging_enable = 1;

	WSAStartup(MAKEWORD(1, 1), &wsa_data);

	/* open our network interface and set the capture ethertype to MRP types */
	net_if = netif_open(TIME_PERIOD_25_MILLISECONDS);	// time out is 25ms
	if (!net_if) {
		fprintf(stderr, "ERROR - opening network interface\n");
		return -1;
	}

	ether_types[0] = MVRP_ETYPE;
	ether_types[1] = MMRP_ETYPE;
	ether_types[2] = MSRP_ETYPE;
	if (netif_set_capture_ethertype(net_if, ether_types, 3)) {
		fprintf(stderr, "ERROR - setting netif capture ethertype\n");
		return -1;
	}

	rc = mrp_init();
	if (rc)
		goto out;

	rc = init_local_ctl();
	if (rc)
		goto out;

	if (mmrp_enable) {
		rc = mmrp_init(mmrp_enable);
		if (rc)
			goto out;
	}

	if (mvrp_enable) {
		rc = mvrp_init(mvrp_enable);
		if (rc)
			goto out;
	}

	if (msrp_enable) {
		rc = msrp_init(msrp_enable);
		if (rc)
			goto out;
	}

	rc = init_timers();
	if (rc)
		goto out;

	return 0;

 out:
	return -1;

}

void mrpw_cleanup(void)
{
	WSACleanup();
}

void process_events(void)
{
	int status;
	while (1) {
		status = mrpw_run_once();
		if (status < 0)
			break;
	}
}

int main(int argc, char *argv[])
{
	int status;

	status = mrpw_init_protocols();

	if (status < 0) {
		mrpw_cleanup();
		return status;
	}

	status = mrpw_init_threads();
	if (status < 0) {
		mrpw_cleanup();
		return status;
	}

	process_events();

	mrpw_cleanup();
	return status;

}

void mrpd_log_printf(const char *fmt, ...)
{
	LARGE_INTEGER count;
	LARGE_INTEGER freq;
	unsigned int ms;
	va_list arglist;

	/* get time stamp in ms */
	QueryPerformanceCounter(&count);
	QueryPerformanceFrequency(&freq);
	ms = (unsigned int)((count.QuadPart * 1000 / freq.QuadPart) &
			    0xfffffff);

	printf("MRPD %03d.%03d ", ms / 1000, ms % 1000);

	va_start(arglist, fmt);
	vprintf(fmt, arglist);
	va_end(arglist);

}
