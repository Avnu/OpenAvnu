 /*
  * Copyright (c) <2013>, Intel Corporation.
  *
  * This program is free software; you can redistribute it and/or modify it
  * under the terms and conditions of the GNU Lesser General Public License,
  * version 2.1, as published by the Free Software Foundation.
  *
  * This program is distributed in the hope it will be useful, but WITHOUT
  * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
  * more details.
  *
  * You should have received a copy of the GNU Lesser General Public License along with
  * this program; if not, write to the Free Software Foundation, Inc.,
  * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  *
  */

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sched.h>

#include <arpa/inet.h>

#include <linux/if.h>

#include <netinet/in.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>

#include <pci/pci.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/un.h>
#include <sys/user.h>

#include "avb.h"
#include "mrpd.h"
#include "mrp.h"
#include "msrp.h"


/* global variables */
int control_socket = -1;
volatile int halt_tx = 0;
volatile int listeners = 0;
volatile int mrp_okay;
volatile int mrp_error = 0;;
volatile int domain_a_valid = 0;
volatile int domain_b_valid = 0;
int domain_class_b_id;
int domain_class_b_priority;
int domain_class_b_vid;
int g_start_feed_socket = 0;
device_t igb_dev;
pthread_t monitor_thread;
pthread_attr_t monitor_attr;
uint32_t payload_length;

unsigned char monitor_stream_id[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
unsigned char STATION_ADDR[] = { 0, 0, 0, 0, 0, 0 };
unsigned char STREAM_ID[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
/* IEEE 1722 reserved address */
unsigned char DEST_ADDR[] = { 0x91, 0xE0, 0xF0, 0x00, 0x0E, 0x80 };

#define STREAMID	0xABCDEF

/* (1) packet every 125 usec */
#define PACKET_IPG		125000

#define USE_MRPD 1

#ifdef USE_MRPD

int domain_class_a_id;
int domain_class_a_priority;
int domain_class_a_vid;

int send_mrp_msg(char *notify_data, int notify_len)
{
	struct sockaddr_in addr;
	socklen_t addr_len;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MRPD_PORT_DEFAULT);
	inet_aton("127.0.0.1", &addr.sin_addr);
	addr_len = sizeof(addr);
	if (control_socket != -1)
		return (sendto
			(control_socket, notify_data, notify_len, 0,
			 (struct sockaddr *)&addr, addr_len));

	return 0;
}

int mrp_connect()
{
	struct sockaddr_in addr;
	int sock_fd = -1;

	sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock_fd < 0)
		goto out;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MRPD_PORT_DEFAULT);
	inet_aton("127.0.0.1", &addr.sin_addr);
	memset(&addr, 0, sizeof(addr));
	control_socket = sock_fd;
	return 0;

 out:	if (sock_fd != -1)
		close(sock_fd);
	sock_fd = -1;
	return -1;
}

int mrp_disconnect()
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(64);
	if (NULL == msgbuf)
		return -1;

	memset(msgbuf, 0, 64);
	sprintf(msgbuf, "BYE");
	mrp_okay = 0;
	rc = send_mrp_msg(msgbuf, 1500);

	free(msgbuf);
	return rc;
}

int recv_mrp_okay()
{
	while ((mrp_okay == 0) && (mrp_error == 0))
		usleep(20000);

	return 0;
}

int mrp_register_domain(int *class_id, int *priority, u_int16_t * vid)
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(64);
	if (NULL == msgbuf)
		return -1;

	memset(msgbuf, 0, 64);
	sprintf(msgbuf, "S+D:C=%d,P=%d,V=%04x", *class_id, *priority, *vid);
	mrp_okay = 0;
	rc = send_mrp_msg(msgbuf, 1500);

	free(msgbuf);
	return rc;
}

int mrp_get_domain(int *class_a_id, int *a_priority, u_int16_t * a_vid,
		   int *class_b_id, int *b_priority, u_int16_t * b_vid)
{
	char *msgbuf;

	/* we may not get a notification if we are joining late,
	 * so query for what is already there ...
	 */
	msgbuf = malloc(64);
	if (NULL == msgbuf)
		return -1;

	memset(msgbuf, 0, 64);
	sprintf(msgbuf, "S??");
	send_mrp_msg(msgbuf, 64);
	free(msgbuf);

	while (!halt_tx && (domain_a_valid == 0) && (domain_b_valid == 0))
		usleep(20000);

	*class_a_id = 0;
	*a_priority = 0;
	*a_vid = 0;
	*class_b_id = 0;
	*b_priority = 0;
	*b_vid = 0;
	if (domain_a_valid) {
		*class_a_id = domain_class_a_id;
		*a_priority = domain_class_a_priority;
		*a_vid = domain_class_a_vid;
	}
	if (domain_b_valid) {
		*class_b_id = domain_class_b_id;
		*b_priority = domain_class_b_priority;
		*b_vid = domain_class_b_vid;
	}

	return 0;
}

int mrp_await_listener(unsigned char *streamid)
{
	char *msgbuf;

	memcpy(monitor_stream_id, streamid, sizeof(monitor_stream_id));
	msgbuf = malloc(64);
	if (NULL == msgbuf)
		return -1;

	memset(msgbuf, 0, 64);
	sprintf(msgbuf, "S??");
	send_mrp_msg(msgbuf, 64);
	free(msgbuf);

	/* either already there ... or need to wait ... */
	while (!halt_tx && (listeners == 0))
		usleep(20000);

	return 0;
}

int process_mrp_msg(char *buf, int buflen)
{

	/*
	 * 1st character indicates application
	 * [MVS] - MAC, VLAN or STREAM
	 */
	unsigned int id;
	unsigned int priority;
	unsigned int vid;
	int i, j, k;
	unsigned int substate;
	unsigned char recovered_streamid[8];

	k = 0;
next_line:if (k >= buflen)
		return 0;
	switch (buf[k]) {
	case 'E':
		fprintf(stderr, "%s from mrpd\n", buf);
		fflush(stdout);
		mrp_error = 1;
		break;
	case 'O':
		mrp_okay = 1;
		break;
	case 'M':
	case 'V':
		fprintf(stderr, "%s unhandled from mrpd\n", buf);
		fflush(stdout);

		/* unhandled for now */
		break;
	case 'L':

		/* parse a listener attribute - see if it matches our monitor_stream_id */
		i = k;
		while (buf[i] != 'D')
			i++;
		i += 2;		/* skip the ':' */
		sscanf(&(buf[i]), "%d", &substate);
		while (buf[i] != 'S')
			i++;
		i += 2;		/* skip the ':' */
		for (j = 0; j < 8; j++) {
			sscanf(&(buf[i + 2 * j]), "%02x", &id);
			recovered_streamid[j] = (unsigned char)id;
		} printf
		    ("FOUND STREAM ID=%02x%02x%02x%02x%02x%02x%02x%02x ",
		     recovered_streamid[0], recovered_streamid[1],
		     recovered_streamid[2], recovered_streamid[3],
		     recovered_streamid[4], recovered_streamid[5],
		     recovered_streamid[6], recovered_streamid[7]);
		switch (substate) {
		case 0:
			fprintf(stderr, "with state ignore\n");
			break;
		case 1:
			fprintf(stderr, "with state askfailed\n");
			break;
		case 2:
			fprintf(stderr, "with state ready\n");
			break;
		case 3:
			fprintf(stderr, "with state readyfail\n");
			break;
		default:
			fprintf(stderr, "with state UNKNOWN (%d)\n", substate);
			break;
		}
		if (substate > MSRP_LISTENER_ASKFAILED) {
			if (memcmp
			    (recovered_streamid, monitor_stream_id,
			     sizeof(recovered_streamid)) == 0) {
				listeners = 1;
				fprintf(stderr, "added listener\n");
			}
		}
		fflush(stdout);

		/* try to find a newline ... */
		while ((i < buflen) && (buf[i] != '\n') && (buf[i] != '\0'))
			i++;
		if (i == buflen)
			return 0;
		if (buf[i] == '\0')
			return 0;
		i++;
		k = i;
		goto next_line;
		break;
	case 'D':
		i = k + 4;

		/* save the domain attribute */
		sscanf(&(buf[i]), "%d", &id);
		while (buf[i] != 'P')
			i++;
		i += 2;		/* skip the ':' */
		sscanf(&(buf[i]), "%d", &priority);
		while (buf[i] != 'V')
			i++;
		i += 2;		/* skip the ':' */
		sscanf(&(buf[i]), "%x", &vid);
		if (id == 6) {
			domain_class_a_id = id;
			domain_class_a_priority = priority;
			domain_class_a_vid = vid;
			domain_a_valid = 1;
		} else {
			domain_class_b_id = id;
			domain_class_b_priority = priority;
			domain_class_b_vid = vid;
			domain_b_valid = 1;
		}
		while ((i < buflen) && (buf[i] != '\n') && (buf[i] != '\0'))
			i++;
		if ((i == buflen) || (buf[i] == '\0'))
			return 0;
		i++;
		k = i;
		goto next_line;
		break;
	case 'T':

		/* as simple_talker we don't care about other talkers */
		i = k;
		while ((i < buflen) && (buf[i] != '\n') && (buf[i] != '\0'))
			i++;
		if (i == buflen)
			return 0;
		if (buf[i] == '\0')
			return 0;
		i++;
		k = i;
		goto next_line;
		break;
	case 'S':

		/* handle the leave/join events */
		switch (buf[k + 4]) {
		case 'L':
			i = k + 5;
			while (buf[i] != 'D')
				i++;
			i += 2;	/* skip the ':' */
			sscanf(&(buf[i]), "%d", &substate);
			while (buf[i] != 'S')
				i++;
			i += 2;	/* skip the ':' */
			for (j = 0; j < 8; j++) {
				sscanf(&(buf[i + 2 * j]), "%02x", &id);
				recovered_streamid[j] = (unsigned char)id;
			} printf
			    ("EVENT on STREAM ID=%02x%02x%02x%02x%02x%02x%02x%02x ",
			     recovered_streamid[0], recovered_streamid[1],
			     recovered_streamid[2], recovered_streamid[3],
			     recovered_streamid[4], recovered_streamid[5],
			     recovered_streamid[6], recovered_streamid[7]);
			switch (substate) {
			case 0:
				fprintf(stderr, "with state ignore\n");
				break;
			case 1:
				fprintf(stderr, "with state askfailed\n");
				break;
			case 2:
				fprintf(stderr, "with state ready\n");
				break;
			case 3:
				fprintf(stderr, "with state readyfail\n");
				break;
			default:
				fprintf(stderr, "with state UNKNOWN (%d)\n", substate);
				break;
			}
			switch (buf[k + 1]) {
			case 'L':
				fprintf(stderr, "got a leave indication\n");
				if (memcmp
				    (recovered_streamid, monitor_stream_id,
				     sizeof(recovered_streamid)) == 0) {
					listeners = 0;
					fprintf(stderr, "listener left\n");
				}
				break;
			case 'J':
			case 'N':
				fprintf(stderr, "got a new/join indication\n");
				if (substate > MSRP_LISTENER_ASKFAILED) {
					if (memcmp
					    (recovered_streamid,
					     monitor_stream_id,
					     sizeof(recovered_streamid)) == 0)
						listeners = 1;
				}
				break;
			}

			/* only care about listeners ... */
		default:
			return 0;
			break;
		}
		break;
	case '\0':
		break;
	}
	return 0;
}

void *mrp_monitor_thread(void *arg)
{
	char *msgbuf;
	struct sockaddr_in client_addr;
	struct msghdr msg;
	struct iovec iov;
	int bytes = 0;
	struct pollfd fds;
	int rc;

	if (NULL == arg)
		rc = 0;
	else
		rc = 1;

	msgbuf = (char *)malloc(MAX_MRPD_CMDSZ);
	if (NULL == msgbuf)
		return NULL;

	while (!halt_tx) {
		fds.fd = control_socket;
		fds.events = POLLIN;
		fds.revents = 0;
		rc = poll(&fds, 1, 100);
		if (rc < 0) {
			free(msgbuf);
			pthread_exit(NULL);
		}
		if (rc == 0)
			continue;
		if ((fds.revents & POLLIN) == 0) {
			free(msgbuf);
			pthread_exit(NULL);
		}
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
		if (bytes < 0)
			continue;
		process_mrp_msg(msgbuf, bytes);
	}
	free(msgbuf);
	pthread_exit(NULL);
}

int mrp_monitor()
{
	pthread_attr_init(&monitor_attr);
	pthread_create(&monitor_thread, NULL, mrp_monitor_thread, NULL);

	return 0;
}

int mrp_join_listener(uint8_t * streamid)
{
	char *msgbuf;
	int rc;
	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S+L:S=%02X%02X%02X%02X%02X%02X%02X%02X"
		",D=2", streamid[0], streamid[1], streamid[2], streamid[3],
		streamid[4], streamid[5], streamid[6], streamid[7]);
	mrp_okay = 0;
	rc = send_mrp_msg(msgbuf, 1500);

	free(msgbuf);
	return rc;
}

int
mrp_advertise_stream(uint8_t * streamid,
		     uint8_t * destaddr,
		     u_int16_t vlan,
		     int pktsz, int interval, int priority, int latency)
{
	char *msgbuf;
	int rc;
	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S++:S=%02X%02X%02X%02X%02X%02X%02X%02X"
		",A=%02X%02X%02X%02X%02X%02X"
		",V=%04X"
		",Z=%d"
		",I=%d"
		",P=%d"
		",L=%d", streamid[0], streamid[1], streamid[2],
		streamid[3], streamid[4], streamid[5], streamid[6],
		streamid[7], destaddr[0], destaddr[1], destaddr[2],
		destaddr[3], destaddr[4], destaddr[5], vlan, pktsz,
		interval, priority << 5, latency);
	mrp_okay = 0;
	rc = send_mrp_msg(msgbuf, 1500);

	/* rc = recv_mrp_okay(); */
	free(msgbuf);
	return rc;
}

int
mrp_unadvertise_stream(uint8_t * streamid,
		       uint8_t * destaddr,
		       u_int16_t vlan,
		       int pktsz, int interval, int priority, int latency)
{
	char *msgbuf;
	int rc;
	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S--:S=%02X%02X%02X%02X%02X%02X%02X%02X"
		",A=%02X%02X%02X%02X%02X%02X"
		",V=%04X"
		",Z=%d"
		",I=%d"
		",P=%d"
		",L=%d", streamid[0], streamid[1], streamid[2],
		streamid[3], streamid[4], streamid[5], streamid[6],
		streamid[7], destaddr[0], destaddr[1], destaddr[2],
		destaddr[3], destaddr[4], destaddr[5], vlan, pktsz,
		interval, priority << 5, latency);
	mrp_okay = 0;
	rc = send_mrp_msg(msgbuf, 1500);

	/* rc = recv_mrp_okay(); */
	free(msgbuf);
	return rc;
}

#endif

uint64_t reverse_64(uint64_t val)
{
	uint32_t low, high;

	low = val & 0xffffffff;
	high = (val >> 32) & 0xffffffff;
	low = htonl(low);
	high = htonl(high);

	val = 0;
	val = val | low;
	val = (val << 32) | high;

	return val;
}

void sigint_handler(int signum)
{
	fprintf(stderr, "got SIGINT\n");
	halt_tx = signum;
}

int get_mac_addr(int8_t *iface)
{
	int lsock = socket(PF_PACKET, SOCK_RAW, htons(0x800));
	struct ifreq if_request;
	int rc;

	if (lsock < 0)
		return -1;

	memset(&if_request, 0, sizeof(if_request));
	strncpy(if_request.ifr_name, (const char *)iface, sizeof(if_request.ifr_name));

	rc = ioctl(lsock, SIOCGIFHWADDR, &if_request);
	if (rc < 0) {
		close(lsock);
		return -1;
	}
	memcpy(STATION_ADDR, if_request.ifr_hwaddr.sa_data,
			sizeof(STATION_ADDR));
	close(lsock);

	return 0;
}

int main(int argc, char *argv[])
{
	struct igb_dma_alloc a_page;
	struct igb_packet a_packet;
	struct igb_packet *tmp_packet;
	struct igb_packet *cleaned_packets;
	struct igb_packet *free_packets;
	six1883_header *h61883;
	seventeen22_header *h1722;
	unsigned i;
	int err, seq_number, frame_size;
	int8_t *iface = NULL;
#ifdef DOMAIN_QUERY
	int class_b_id = 0;
	int b_priority = 0;
	u_int16_t b_vid = 0;
#endif
#ifdef USE_MRPD
	int class_a_id = 0;
	int a_priority = 0;
	u_int16_t a_vid = 0;
#endif
	unsigned samples_count = 0;
	uint32_t packet_size;
	int32_t read_bytes;
	uint8_t *data_ptr;
	void *stream_packet;
	long long int frame_sequence = 0;
	struct sched_param sched;

	if (argc < 2) {
		fprintf(stderr,"%s <if_name> <payload>\n", argv[0]);
		return -1;
	}

	iface = (int8_t *)strdup(argv[1]);
	packet_size = atoi(argv[2]);;
	payload_length = atoi(argv[2]);;
	packet_size += sizeof(six1883_header) + sizeof(seventeen22_header) + sizeof(eth_header);

#ifdef USE_MRPD
	err = mrp_connect();
	if (err) {
		fprintf(stderr, "socket creation failed\n");
		return (errno);
	}
#endif

	err = pci_connect(&igb_dev);
	if (err) {
		fprintf(stderr, "connect failed (%s) - are you running as root?\n", strerror(errno));
		return (errno);
	}

	err = igb_init(&igb_dev);
	if (err) {
		fprintf(stderr, "init failed (%s) - is the driver really loaded?\n", strerror(errno));
		return (errno);
	}

	err = igb_dma_malloc_page(&igb_dev, &a_page);
	if (err) {
		fprintf(stderr, "malloc failed (%s) - out of memory?\n", strerror(errno));
		return (errno);
	}

	signal(SIGINT, sigint_handler);

	err = get_mac_addr(iface);
	if (err) {
		fprintf(stderr, "failed to open iface(%s)\n",iface);
		return -1;
	}

#ifdef USE_MRPD
	mrp_monitor();
	domain_a_valid = 1;
	class_a_id = MSRP_SR_CLASS_A;
	a_priority = MSRP_SR_CLASS_A_PRIO;
	a_vid = 2;
	fprintf(stderr, "detected domain Class A PRIO=%d VID=%04x...\n", a_priority,
	       a_vid);

        mrp_register_domain(&class_a_id, &a_priority, &a_vid);

	domain_a_valid = 1;
	a_vid = 2;
	fprintf(stderr, "detected domain Class A PRIO=%d VID=%04x...\n", a_priority, a_vid);
#endif

	igb_set_class_bandwidth(&igb_dev, PACKET_IPG / 125000, 0, packet_size - 22, 0);

	memset(STREAM_ID, 0, sizeof(STREAM_ID));
	memcpy(STREAM_ID, STATION_ADDR, sizeof(STATION_ADDR));

	a_packet.dmatime = a_packet.attime = a_packet.flags = 0;
	a_packet.map.paddr = a_page.dma_paddr;
	a_packet.map.mmap_size = a_page.mmap_size;
	a_packet.offset = 0;
	a_packet.vaddr = a_page.dma_vaddr + a_packet.offset;
	a_packet.len = packet_size;
	free_packets = NULL;
	seq_number = 0;

	frame_size = payload_length + sizeof(six1883_header) + sizeof(seventeen22_header) + sizeof(eth_header);

	stream_packet = avb_create_packet(payload_length);

	h1722 = (seventeen22_header *)((uint8_t*)stream_packet + sizeof(eth_header));
	h61883 = (six1883_header *)((uint8_t*)stream_packet + sizeof(eth_header) +
					sizeof(seventeen22_header));

	/*initalize h1722 header */
	avb_initialize_h1722_to_defaults(h1722);
	/* set the length */
	avb_set_1722_length(h1722, htons(payload_length + sizeof(six1883_header)));
	avb_set_1722_stream_id(h1722,reverse_64(STREAMID));
	avb_set_1722_sid_valid(h1722, 0x1);

	
	/*initalize h61883 header */
	avb_initialize_61883_to_defaults(h61883);
	avb_set_61883_format_tag(h61883, 0x1);
	avb_set_61883_packet_channel(h61883, 0x1f);
	avb_set_61883_packet_tcode(h61883, 0xa);
	avb_set_61883_source_id(h61883 , 0x3f);
	avb_set_61883_data_block_size(h61883, 0x1);
	avb_set_61883_eoh(h61883, 0x2);
	avb_set_61883_format_id(h61883, 0x10);
	avb_set_61883_format_dependent_field(h61883, 0x2);
	avb_set_61883_syt(h61883, 0xffff);

	/* initilaze the source & destination mac address */
	avb_eth_header_set_mac(stream_packet, DEST_ADDR, iface);

	/* set 1772 eth type */
	avb_1722_set_eth_type(stream_packet);

	/* divide the dma page into buffers for packets */
	for (i = 1; i < ((a_page.mmap_size) / packet_size); i++) {
		tmp_packet = malloc(sizeof(struct igb_packet));
		if (NULL == tmp_packet) {
			fprintf(stderr, "failed to allocate igb_packet memory!\n");
			return (errno);
		}
		*tmp_packet = a_packet;
		tmp_packet->offset = (i * packet_size);
		tmp_packet->vaddr += tmp_packet->offset;
		tmp_packet->next = free_packets;
		memset(tmp_packet->vaddr, 0, packet_size);	/* MAC header at least */
		memcpy(((char *)tmp_packet->vaddr), stream_packet, frame_size);
		tmp_packet->len = frame_size;
		free_packets = tmp_packet;
	}

#ifdef USE_MRPD
	/* 
	 * subtract 16 bytes for the MAC header/Q-tag - pktsz is limited to the 
	 * data payload of the ethernet frame .
	 *
	 * IPG is scaled to the Class (A) observation interval of packets per 125 usec
	 */
	fprintf(stderr, "advertising stream ...\n");
	mrp_advertise_stream(STREAM_ID, DEST_ADDR, a_vid, packet_size - 16,
		PACKET_IPG / 125000, a_priority, 3900);
	fprintf(stderr, "awaiting a listener ...\n");
	mrp_await_listener(STREAM_ID);
#endif

	memset(&sched, 0 , sizeof (sched));
	sched.sched_priority = 1;
	sched_setscheduler(0, SCHED_RR, &sched);

	while (listeners && !halt_tx)
	{
		tmp_packet = free_packets;
		if (NULL == tmp_packet)
			goto cleanup;

		stream_packet = ((char *)tmp_packet->vaddr);
		free_packets = tmp_packet->next;

		/* unfortuntely unless this thread is at rtprio
		 * you get pre-empted between fetching the time
		 * and programming the packet and get a late packet
		 */
		h1722 = (seventeen22_header *)((uint8_t*)stream_packet + sizeof(eth_header));
		avb_set_1722_seq_number(h1722, seq_number++);
		if (seq_number % 4 == 0)
			avb_set_1722_timestamp_valid(h1722, 0);
		else
			avb_set_1722_timestamp_valid(h1722, 1);

		data_ptr = (uint8_t *)((uint8_t*)stream_packet + sizeof(eth_header) + sizeof(seventeen22_header) 
					+ sizeof(six1883_header));
		
		read_bytes = read(0, (void *)data_ptr, payload_length);
		/* Error case while reading the input file */
		if (read_bytes < 0) {
			fprintf(stderr,"Failed to read from STDIN %s\n", argv[2]);
			continue;
		}
		samples_count += read_bytes;
		h61883 = (six1883_header *)((uint8_t*)stream_packet + sizeof(eth_header) + sizeof(seventeen22_header));
		avb_set_61883_data_block_continuity(h61883 , samples_count);

		err = igb_xmit(&igb_dev, 0, tmp_packet);
		if (!err) {
			fprintf(stderr,"frame sequence = %lld\n", frame_sequence++);
			continue;
		} else {
			fprintf(stderr,"Failed frame sequence = %lld !!!!\n", frame_sequence++);
		}

		if (ENOSPC == err) {
			/* put back for now */
			tmp_packet->next = free_packets;
			free_packets = tmp_packet;
		}
cleanup:
		igb_clean(&igb_dev, &cleaned_packets);
		while (cleaned_packets) {
			tmp_packet = cleaned_packets;
			cleaned_packets = cleaned_packets->next;
			tmp_packet->next = free_packets;
			free_packets = tmp_packet;
		}
	}

	if (halt_tx == 0)
		fprintf(stderr, "listener left ...\n");

	halt_tx = 1;
	sleep(1);
#ifdef USE_MRPD
	mrp_unadvertise_stream(STREAM_ID, DEST_ADDR, a_vid, packet_size - 16,
			       PACKET_IPG / 125000, a_priority, 3900);
#endif
	/* disable Qav */
	igb_set_class_bandwidth(&igb_dev, 0, 0, 0, 0);
#ifdef USE_MRPD
	err = mrp_disconnect();
#endif
	igb_dma_free_page(&igb_dev, &a_page);

	err = igb_detach(&igb_dev);

	pthread_exit(NULL);

	return 0;
}
