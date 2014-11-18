/****************************************************************************
  Copyright (c) 2012, Intel Corporation
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
 * an MRP (MMRP, MVRP, MSRP) endpoint implementation of 802.1Q-2011
 */
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/user.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <netpacket/packet.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <sys/un.h>
#include <sys/eventfd.h>

#include "mrpd.h"
#include "mrp.h"
#include "mvrp.h"
#include "msrp.h"
#include "mmrp.h"

static void mrpd_log_timer_event(char *src, int event);

/* global mgmt parameters */
int daemonize;
int mmrp_enable;
int mvrp_enable;
int msrp_enable;
int logging_enable;
int mrpd_port;

char *interface;
int interface_fd;

/* state machine controls */
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
    "mrpd v" VERSION_STR "\n" "Copyright (c) 2012, Intel Corporation\n";

unsigned char STATION_ADDR[] = { 0x00, 0x88, 0x77, 0x66, 0x55, 0x44 };

/* global variables */
SOCKET control_socket;
extern SOCKET mmrp_socket;
extern SOCKET mvrp_socket;
extern SOCKET msrp_socket;

int periodic_timer;
int gc_timer;
unsigned int gc_ctl_msg_count = 0;
static struct mrp_periodictimer_state mrp_periodic_state;

extern struct mmrp_database *MMRP_db;
extern struct mvrp_database *MVRP_db;
extern struct msrp_database *MSRP_db;

int mrpd_timer_create(void)
{
	int t = timerfd_create(CLOCK_MONOTONIC, 0);
	if (-1 != t)
		fcntl(t, F_SETFL, O_NONBLOCK);
	return t;
}

void mrpd_timer_close(int t)
{
	if (-1 != t)
		close(t);
}

int mrpd_timer_start_interval(int timerfd,
			      unsigned long value_ms, unsigned long interval_ms)
{
	int rc;
	struct itimerspec itimerspec_new;
	struct itimerspec itimerspec_old;
	unsigned long ns_per_ms = 1000000;

	memset(&itimerspec_new, 0, sizeof(itimerspec_new));
	memset(&itimerspec_old, 0, sizeof(itimerspec_old));

	if (interval_ms) {
		itimerspec_new.it_interval.tv_sec = interval_ms / 1000;
		itimerspec_new.it_interval.tv_nsec =
		    (interval_ms % 1000) * ns_per_ms;
	}

	itimerspec_new.it_value.tv_sec = value_ms / 1000;
	itimerspec_new.it_value.tv_nsec = (value_ms % 1000) * ns_per_ms;

	rc = timerfd_settime(timerfd, 0, &itimerspec_new, &itimerspec_old);

	return (rc);
}

int mrpd_timer_start(int timerfd, unsigned long value_ms)
{
	return mrpd_timer_start_interval(timerfd, value_ms, 0);
}

int mrpd_timer_stop(int timerfd)
{
	int rc;
	struct itimerspec itimerspec_new;
	struct itimerspec itimerspec_old;

	memset(&itimerspec_new, 0, sizeof(itimerspec_new));
	memset(&itimerspec_old, 0, sizeof(itimerspec_old));

	rc = timerfd_settime(timerfd, 0, &itimerspec_new, &itimerspec_old);

	return (rc);
}

int gctimer_start()
{
	/* reclaim memory every 30 seconds */
	return mrpd_timer_start(gc_timer, 30 * 1000);
}

int mrp_periodictimer_start()
{
	/* Single periodic timer and state machine per port
	 * periodictimer has expired. (10.7.5.23)
	 * PeriodicTransmission state machine generates periodic events
	 * period is one-per-sec
	 */
	return mrpd_timer_start_interval(periodic_timer, 1000, 1000);
}

int mrp_periodictimer_stop()
{
	/* Single periodic timer and state machine per port
	 * periodictimer has expired. (10.7.5.23)
	 * PeriodicTransmission state machine generates periodic events
	 * period is one-per-sec
	 */
	return mrpd_timer_stop(periodic_timer);
}

int init_local_ctl(void)
{
	struct sockaddr_in addr;
	socklen_t addr_len;
	int sock_fd = -1;
	int rc;

	sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock_fd < 0)
		goto out;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(mrpd_port);
	inet_aton("127.0.0.1", (struct in_addr *)&addr.sin_addr.s_addr);
	addr_len = sizeof(addr);

	rc = bind(sock_fd, (struct sockaddr *)&addr, addr_len);

	if (rc < 0) {
#if LOG_ERRORS
		fprintf(stderr, "%s - Error on bind %s", __FUNCTION__, strerror(errno));
#endif
		goto out;
	}

	control_socket = sock_fd;

	return 0;
 out:
	if (sock_fd != -1)
		close(sock_fd);

	return -1;
}

int
mrpd_send_ctl_msg(struct sockaddr_in *client_addr, char *notify_data,
		  int notify_len)
{

	int rc;

	if (-1 == control_socket)
		return 0;

#if LOG_CLIENT_SEND
	if (logging_enable) {
		mrpd_log_printf("[%03d] CLT MSG %05d:%s",
				gc_ctl_msg_count, client_addr->sin_port,
				notify_data);
		gc_ctl_msg_count = (gc_ctl_msg_count + 1) % 1000;
	}
#endif
	rc = sendto(control_socket, notify_data, notify_len,
		    0, (struct sockaddr *)client_addr, sizeof(struct sockaddr));
	return rc;
}

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
		mrpd_log_printf("CMD:%s from CLNT %d\n", buf, client->sin_port);
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
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC MRP parse %s", buf);
		mrpd_send_ctl_msg(client, respbuf, sizeof(respbuf));
		return -1;
		break;
	}

	return 0;
}

int recv_ctl_msg()
{
	char *msgbuf;
	struct sockaddr_in client_addr;
	struct msghdr msg;
	struct iovec iov;
	int bytes = 0;

	msgbuf = (char *)malloc(MAX_MRPD_CMDSZ);
	if (NULL == msgbuf)
		return -1;

	memset(&msg, 0, sizeof(msg));
	memset(&client_addr, 0, sizeof(client_addr));
	memset(msgbuf, 0, MAX_MRPD_CMDSZ);

	iov.iov_len = MAX_MRPD_CMDSZ;
	iov.iov_base = msgbuf;
	msg.msg_name = &client_addr;
	msg.msg_namelen = sizeof(client_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	bytes = recvmsg(control_socket, &msg, 0);
	if (bytes <= 0)
		goto out;

	process_ctl_msg(msgbuf, bytes, &client_addr);
 out:
	free(msgbuf);

	return -1;
}

int mrpd_recvmsgbuf(int sock, char **buf)
{
	struct sockaddr_ll client_addr;
	struct msghdr msg;
	struct iovec iov;
	int bytes = 0;

	*buf = (char *)malloc(MAX_FRAME_SIZE);
	if (NULL == *buf)
		return -1;

	memset(&msg, 0, sizeof(msg));
	memset(&client_addr, 0, sizeof(client_addr));
	memset(*buf, 0, MAX_FRAME_SIZE);

	iov.iov_len = MAX_FRAME_SIZE;
	iov.iov_base = *buf;
	msg.msg_name = &client_addr;
	msg.msg_namelen = sizeof(client_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	/* Non-blocking sockets. */
	bytes = recvmsg(sock, &msg, MSG_DONTWAIT);

	if ( (bytes < 0) && (errno != EAGAIN) ) {
#if LOG_ERRORS
		fprintf(stderr, "recvmsg sock %d: %s\r\n", sock, strerror(errno));
#endif
		return(-1);
	}
	else if (bytes == 0) {
#if LOG_ERRORS
		fprintf(stderr, "Closed socket!\r\n");
#endif
		return(-1);
	}

	return bytes;
}

int
mrpd_init_protocol_socket(u_int16_t etype, int *sock,
			  unsigned char *multicast_addr)
{
	struct sockaddr_ll addr;
	struct ifreq if_request;
	int lsock;
	int rc;
	struct packet_mreq multicast_req;

	if (NULL == sock)
		return -1;
	if (NULL == multicast_addr)
		return -1;

	memset(&multicast_req, 0, sizeof(multicast_req));
	*sock = -1;

	lsock = socket(PF_PACKET, SOCK_RAW, htons(etype));
	if (lsock < 0)
		return -1;

	memset(&if_request, 0, sizeof(if_request));

	strncpy(if_request.ifr_name, interface, sizeof(if_request.ifr_name) - 1);

	rc = ioctl(lsock, SIOCGIFHWADDR, &if_request);
	if (rc < 0) {
		close(lsock);
		return -1;
	}

	memcpy(STATION_ADDR, if_request.ifr_hwaddr.sa_data,
	       sizeof(STATION_ADDR));

	memset(&if_request, 0, sizeof(if_request));

	strncpy(if_request.ifr_name, interface, sizeof(if_request.ifr_name)-1);

	rc = ioctl(lsock, SIOCGIFINDEX, &if_request);
	if (rc < 0) {
		close(lsock);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sll_ifindex = if_request.ifr_ifindex;
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(etype);

	rc = bind(lsock, (struct sockaddr *)&addr, sizeof(addr));
	if (0 != rc) {
#if LOG_ERRORS
		fprintf(stderr, "%s - Error on bind %s", __FUNCTION__, strerror(errno));
#endif
		close(lsock);
		return -1;
	}

	rc = setsockopt(lsock, SOL_SOCKET, SO_BINDTODEVICE, interface,
			strlen(interface));
	if (0 != rc) {
		close(lsock);
		return -1;
	}

	multicast_req.mr_ifindex = if_request.ifr_ifindex;
	multicast_req.mr_type = PACKET_MR_MULTICAST;
	multicast_req.mr_alen = 6;
	memcpy(multicast_req.mr_address, multicast_addr, 6);

	rc = setsockopt(lsock, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
			&multicast_req, sizeof(multicast_req));
	if (0 != rc) {
		close(lsock);
		return -1;
	}

	*sock = lsock;

	return 0;
}

int mrpd_close_socket(SOCKET sock)
{
	return close(sock);
}

int mrpd_init_timers(struct mrp_database *mrp_db)
{
	mrp_db->join_timer = mrpd_timer_create();
	mrp_db->lv_timer = mrpd_timer_create();
	mrp_db->lva_timer = mrpd_timer_create();
	mrp_db->join_timer_running = 0;
	mrp_db->lv_timer_running = 0;
	mrp_db->lva_timer_running = 0;

	if (-1 == mrp_db->join_timer)
		goto out;
	if (-1 == mrp_db->lv_timer)
		goto out;
	if (-1 == mrp_db->lva_timer)
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

	if (-1 == periodic_timer)
		goto out;
	if (-1 == gc_timer)
		goto out;

	gctimer_start();

	return 0;
 out:
	return -1;
}

int mrp_register_timers(struct mrp_database *mrp_db, fd_set * fds)
{
	int max_fd;

	FD_SET(mrp_db->join_timer, fds);
	FD_SET(mrp_db->lv_timer, fds);
	FD_SET(mrp_db->lva_timer, fds);

	max_fd = mrp_db->join_timer;
	if (mrp_db->lv_timer > max_fd)
		max_fd = mrp_db->lv_timer;
	if (mrp_db->lva_timer > max_fd)
		max_fd = mrp_db->lva_timer;

	return max_fd;
}

int mrpd_reclaim()
{

	/*
	 * if the local applications have neither registered interest
	 * by joining, and the remote node has quit advertising the attribute
	 * and allowing it to go into the MT state, delete the attribute 
	 */

	mmrp_reclaim();
	mvrp_reclaim();
	msrp_reclaim();

	gctimer_start();

	return 0;

}

void process_events(void)
{

	fd_set fds, sel_fds;
	int rc;
	int max_fd;

	/* wait for events, demux the received packets, process packets */

	FD_ZERO(&fds);
	FD_SET(control_socket, &fds);

	max_fd = control_socket;

	if (mmrp_enable) {
		FD_SET(mmrp_socket, &fds);
		if (mmrp_socket > max_fd)
			max_fd = mmrp_socket;

		if (NULL == MMRP_db)
			return;

		rc = mrp_register_timers(&(MMRP_db->mrp_db), &fds);
		if (rc > max_fd)
			max_fd = rc;
	}
	if (mvrp_enable) {
		FD_SET(mvrp_socket, &fds);
		if (mvrp_socket > max_fd)
			max_fd = mvrp_socket;

		if (NULL == MVRP_db)
			return;
		rc = mrp_register_timers(&(MVRP_db->mrp_db), &fds);
		if (rc > max_fd)
			max_fd = rc;

	}
	if (msrp_enable) {
		FD_SET(msrp_socket, &fds);
		if (msrp_socket > max_fd)
			max_fd = msrp_socket;

		if (NULL == MSRP_db)
			return;
		rc = mrp_register_timers(&(MSRP_db->mrp_db), &fds);
		if (rc > max_fd)
			max_fd = rc;

	}

	FD_SET(periodic_timer, &fds);
	if (periodic_timer > max_fd)
		max_fd = periodic_timer;

	rc = mrp_periodictimer_fsm(&mrp_periodic_state, MRP_EVENT_BEGIN);
	if (rc)
		return;

	FD_SET(gc_timer, &fds);
	if (gc_timer > max_fd)
		max_fd = gc_timer;

	do {

		sel_fds = fds;
		rc = select(max_fd + 1, &sel_fds, NULL, NULL, NULL);

		if (-1 == rc) {
#if LOG_ERRORS
			fprintf(stderr, "Error on select %s\r\n", strerror(errno));
#endif
			return;	/* exit on error */
		}
		else {
			if (FD_ISSET(control_socket, &sel_fds)) {
#if LOG_POLL_EVENTS
				mrpd_log_printf("== EVENT recv_ctl_msg ==\n");
#endif
				recv_ctl_msg();
			}
			if (mmrp_enable) {
				if FD_ISSET
					(mmrp_socket, &sel_fds) {
#if LOG_POLL_EVENTS
					mrpd_log_printf("== EVENT mmrp_recv_msg ==\n");
#endif
					mmrp_recv_msg();
					}
				if FD_ISSET
					(MMRP_db->mrp_db.lva_timer, &sel_fds) {
					mrpd_log_timer_event("MMRP",
							     MRP_EVENT_LVATIMER);
					mmrp_event(MRP_EVENT_LVATIMER, NULL);
					}
				if FD_ISSET
					(MMRP_db->mrp_db.lv_timer, &sel_fds) {
					mrpd_log_timer_event("MMRP",
							     MRP_EVENT_LVTIMER);
					mmrp_event(MRP_EVENT_LVTIMER, NULL);
					}
				if FD_ISSET
					(MMRP_db->mrp_db.join_timer, &sel_fds) {
					mrpd_log_timer_event("MMRP",
							     MRP_EVENT_TX);
					mmrp_event(MRP_EVENT_TX, NULL);
					}
			}
			if (mvrp_enable) {
				if FD_ISSET
					(mvrp_socket, &sel_fds) {
#if LOG_POLL_EVENTS
					mrpd_log_printf("== EVENT mvrp_recv_msg ==\n");
#endif
					mvrp_recv_msg();
					}
				if FD_ISSET
					(MVRP_db->mrp_db.lva_timer, &sel_fds) {
					mrpd_log_timer_event("MVRP",
							     MRP_EVENT_LVATIMER);
					mvrp_event(MRP_EVENT_LVATIMER, NULL);
					}
				if FD_ISSET
					(MVRP_db->mrp_db.lv_timer, &sel_fds) {
					mrpd_log_timer_event("MVRP",
							     MRP_EVENT_LVTIMER);
					mvrp_event(MRP_EVENT_LVTIMER, NULL);
					}
				if FD_ISSET
					(MVRP_db->mrp_db.join_timer, &sel_fds) {
					mrpd_log_timer_event("MVRP",
							     MRP_EVENT_TX);
					mvrp_event(MRP_EVENT_TX, NULL);
					}
			}
			if (msrp_enable) {
				if FD_ISSET
					(msrp_socket, &sel_fds) {
#if LOG_POLL_EVENTS
					mrpd_log_printf("== EVENT msrp_recv_msg ==\n");
#endif
					msrp_recv_msg();
				}
				if FD_ISSET
					(MSRP_db->mrp_db.lva_timer, &sel_fds) {
					mrpd_log_timer_event("MSRP",
							     MRP_EVENT_LVATIMER);
					msrp_event(MRP_EVENT_LVATIMER, NULL);
					}
				if FD_ISSET
					(MSRP_db->mrp_db.lv_timer, &sel_fds) {
					mrpd_log_timer_event("MSRP",
							     MRP_EVENT_LVTIMER);
					msrp_event(MRP_EVENT_LVTIMER, NULL);
					}
				if FD_ISSET
					(MSRP_db->mrp_db.join_timer, &sel_fds) {
					mrpd_log_timer_event("MSRP",
							     MRP_EVENT_TX);
					msrp_event(MRP_EVENT_TX, NULL);
					}
			}
			if (FD_ISSET(periodic_timer, &sel_fds)) {
#if LOG_POLL_EVENTS && LOG_TIMERS
				mrpd_log_printf("== EVENT periodic_timer ==\n");
#endif
				
				mrp_periodictimer_fsm(&mrp_periodic_state, MRP_EVENT_PERIODIC);
				if (mmrp_enable) {
					mmrp_event(MRP_EVENT_PERIODIC, NULL);
				}
				if (mvrp_enable) {
					mvrp_event(MRP_EVENT_PERIODIC, NULL);
				}
				if (msrp_enable) {
					msrp_event(MRP_EVENT_PERIODIC, NULL);
				}
			}
			if (FD_ISSET(gc_timer, &sel_fds)) {
				mrpd_reclaim();
			}
#if LOG_POLL_EVENTS
		mrpd_log_printf("== EVENT DONE ==\n");
#endif
		}
	} while (1);
}

void usage(void)
{
	fprintf(stderr,
		"\n"
		"usage: mrpd [-hdlmvsp] -i interface-name"
		"\n"
		"options:\n"
		"    -h  show this message\n"
		"    -d  run daemon in the background\n"
		"    -l  enable logging (ignored in daemon mode)\n"
		"    -m  enable MMRP Registrar and Participant\n"
		"    -v  enable MVRP Registrar and Participant\n"
		"    -s  enable MSRP Registrar and Participant\n"
		"    -i  specify interface to monitor\n"
		"\n" "%s" "\n", version_str);
	exit(1);
}

int main(int argc, char *argv[])
{
	int c;
	int rc = 0;

	daemonize = 0;
	mmrp_enable = 0;
	mvrp_enable = 0;
	msrp_enable = 0;
	logging_enable = 0;
	mrpd_port = MRPD_PORT_DEFAULT;
	interface = NULL;
	interface_fd = -1;
	registration = MRP_REGISTRAR_CTL_NORMAL;	/* default */
	participant = MRP_APPLICANT_CTL_NORMAL;	/* default */
	control_socket = INVALID_SOCKET;
	mmrp_socket = INVALID_SOCKET;
	mvrp_socket = INVALID_SOCKET;
	msrp_socket = INVALID_SOCKET;
	mrp_periodic_state.state = -1;
	periodic_timer = -1;
	gc_timer = -1;

	for (;;) {
		c = getopt(argc, argv, "hdlmvspi:");

		if (c < 0)
			break;

		switch (c) {
		case 'm':
			mmrp_enable = 1;
			break;
		case 'v':
			mvrp_enable = 1;
			break;
		case 's':
			msrp_enable = 1;
			break;
		case 'l':
			logging_enable = 1;
			break;
		case 'd':
			daemonize = 1;
			break;
		case 'i':
			if (interface) {
				printf
				    ("only one interface per daemon is supported\n");
				usage();
			}
			interface = strdup(optarg);
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}
	if (optind < argc)
		usage();

	if (NULL == interface)
		usage();

	if (!mmrp_enable && !mvrp_enable && !msrp_enable)
		usage();

	/* daemonize before we start creating file descriptors */

	if (daemonize) {
		rc = daemon(1, 0);
		if (rc)
			goto out;
	}
	rc = mrp_init();
	if (rc)
		goto out;

	rc = init_local_ctl();
	if (rc)
		goto out;

	rc = mmrp_init(mmrp_enable);
	if (rc) {
		printf("mmrp_enable failed\n");
		goto out;
	}

	rc = mvrp_init(mvrp_enable);
	if (rc) {
		printf("mvrp_enable failed\n");
		goto out;
	}

	rc = msrp_init(msrp_enable);
	if (rc) {
		printf("msrp_enable failed\n");
		goto out;
	}

	rc = init_timers();
	if (rc) {
		printf("init_timers failed\n");
		goto out;
	}

	printf("process_events()\n");
	process_events();
 out:
	if (rc)
		printf("Error starting. Run as sudo?\n");

	return rc;

}

static void mrpd_log_timer_event(char *src, int event)
{
#if LOG_POLL_EVENTS && LOG_TIMERS
	if (event == MRP_EVENT_LVATIMER) {
		mrpd_log_printf("== EVENT %s leaveAll timer expires ==\n", src);
	} else if (event == MRP_EVENT_LVTIMER) {
		mrpd_log_printf("== EVENT %s leave timer expires ==\n", src);
	} else if (event == MRP_EVENT_TX) {
		mrpd_log_printf("== EVENT %s join timer expires ==\n", src);
	}
#else
	(void)src;
	(void)event;
#endif
}

void mrpd_log_printf(const char *fmt, ...)
{
	struct timeval tv;
	char sz[512];

	if (logging_enable) {
		gettimeofday(&tv, NULL);
		va_list arglist;
		va_start(arglist, fmt);
		vsnprintf(sz, 512, fmt, arglist);
		printf("MRPD %03d.%06d %s",
		       (int)(tv.tv_sec % 1000), (int)tv.tv_usec, sz);
		if (strstr(sz, ":ERP"))
			printf("\n");
		va_end(arglist);
	}
}
