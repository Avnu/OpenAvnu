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

/* global macros */
#define MAX_FRAME_SIZE		1500

device_t igb_dev;

unsigned char DEST_ADDR[] = { 0x91, 0xE0, 0xF0, 0x00, 0x0E, 0x80 };

unsigned char stream_id[8];

int control_socket = 0;
volatile int talker = 0;

#define USE_MRPD 1

#ifdef USE_MRPD
int msg_process(char *buf, int buflen)
{
	uint32_t id;
        int j ;
	fprintf(stderr, "Msg: %s\n", buf);
 	int l = 0;
	if ('S' == buf[l++] && 'N' == buf[l++] && 'E' == buf[l++] && 'T' == buf[++l]) {
		while ('S' != buf[l++]);
		l++;
		for(j = 0; j < 8 ; l+=2, j++)
		{
			sscanf(&buf[l],"%02x",&id);
			stream_id[j] = (unsigned char)id;
		}
		talker = 1;
	}
	return 0;
}

int recv_msg()
{
	char *databuf;
	int bytes = 0;

	databuf = (char *)malloc(2000);
	if (NULL == databuf)
		return -1;

	memset(databuf, 0, 2000);
	bytes = recv(control_socket, databuf, 2000, 0);
	if (bytes <= -1) {
		free(databuf);
		return -1;
	}
	return msg_process(databuf, bytes);

}

int await_talker()
{
	while (0 == talker)	
		recv_msg();
	return 0;
}

int send_msg(char *data, int data_len)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MRPD_PORT_DEFAULT);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	inet_aton("127.0.0.1", &addr.sin_addr);
	if (-1 != control_socket)
		return (sendto(control_socket, data, data_len, 0, (struct sockaddr*)&addr, (socklen_t)sizeof(addr)));
	else 
		return 0;
}

int mrp_disconnect()
{
	int rc;
	char *msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "BYE");
	rc = send_msg(msgbuf, 1500);

	free(msgbuf);
	return rc;
}
	
int report_domain_status()
{
	int rc;
	char* msgbuf = malloc(1500);

	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S+D:C=6,P=3,V=0002");
	
	rc = send_msg(msgbuf, 1500);

	free(msgbuf);
	return rc;
}

int send_ready()
{
	char *databuf;
	int rc;
	databuf = malloc(1500);
	if (NULL == databuf)
		return -1;
	memset(databuf, 0, 1500);
	sprintf(databuf, "S+L:L=%02x%02x%02x%02x%02x%02x%02x%02x, D=2",
		     stream_id[0], stream_id[1],
		     stream_id[2], stream_id[3],
		     stream_id[4], stream_id[5],
		     stream_id[6], stream_id[7]);
	rc = send_msg(databuf, 1500);

#ifdef DEBUG
	fprintf(stdout,"Ready-Msg: %s\n", databuf);
#endif 

	free(databuf);
	return rc;
}

int send_leave()
{
	char *databuf;
	int rc;
	databuf = malloc(1500);
	if (NULL == databuf)
		return -1;
	memset(databuf, 0, 1500);
	sprintf(databuf, "S-L:L=%02x%02x%02x%02x%02x%02x%02x%02x, D=3",
		     stream_id[0], stream_id[1],
		     stream_id[2], stream_id[3],
		     stream_id[4], stream_id[5],
		     stream_id[6], stream_id[7]);
	rc = send_msg(databuf, 1500);
	free(databuf);
	return rc;
}

int create_socket()
{
	struct sockaddr_in addr;
	control_socket = socket(AF_INET, SOCK_DGRAM, 0);
		
	/** in POSIX fd 0,1,2 are reserved */
	if (2 > control_socket) {
		if (-1 > control_socket)
			close(control_socket);
		return -EINVAL;
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(0);
	
	if(0 > (bind(control_socket, (struct sockaddr*)&addr, sizeof(addr)))) {
		fprintf(stderr, "Could not bind socket.\n");
		close(control_socket);
		return -EINVAL;
	}
	return 0;
}

#endif

void sigint_handler(int signum)
{
	fprintf(stderr, "got SIGINT\n");
#ifdef USE_MRPD
	if (0 != talker)
		send_leave();
#endif
	if (2 > control_socket)
	{
		close(control_socket);
	}
	exit(0);
}

int main (int argc, char *argv[ ])
{
	struct ifreq device;
	int error;
	struct sockaddr_ll ifsock_addr;
	struct packet_mreq mreq;
	int ifindex;
	int socket_descriptor;
	char *iface;
	seventeen22_header *h1722;
	long long int frame_sequence = 0;
	unsigned char frame[MAX_FRAME_SIZE];
	int size, length;
	struct sched_param sched;

	if (argc < 2) {
		fprintf(stderr, "Usage : %s <interface_name> <payload>\n",argv[0]);
		return -EINVAL;
	}
	signal(SIGINT, sigint_handler);

#ifdef USE_MRPD
	if (create_socket()) {
		fprintf(stderr, "Socket creation failed.\n");
		return (errno);
	}

	report_domain_status();
	fprintf(stdout,"Waiting for talker...\n");
	await_talker();
	send_ready();
#endif
	iface = strdup(argv[1]);

	error = pci_connect(&igb_dev);
	if (error) {
		fprintf(stderr, "connect failed (%s) - are you running as root?\n", strerror(errno));
		return (errno);
	}

	error = igb_init(&igb_dev);
	if (error) {
		fprintf(stderr, "init failed (%s) - is the driver really loaded?\n", strerror(errno));
		return (errno);
	}

	socket_descriptor = socket(AF_PACKET, SOCK_RAW, htons(ETHER_TYPE_AVTP));
	if (socket_descriptor < 0) {
		fprintf(stderr, "failed to open socket: %s \n", strerror(errno));
		return -EINVAL;
	}

	memset(&device, 0, sizeof(device));
	memcpy(device.ifr_name, iface, IFNAMSIZ);
	error = ioctl(socket_descriptor, SIOCGIFINDEX, &device);
	if (error < 0) {
		fprintf(stderr, "Failed to get index of iface %s: %s\n", iface, strerror(errno));
		return -EINVAL;
	}

	ifindex = device.ifr_ifindex;
	memset(&ifsock_addr, 0, sizeof(ifsock_addr));
	ifsock_addr.sll_family = AF_PACKET;
	ifsock_addr.sll_ifindex = ifindex;
	ifsock_addr.sll_protocol = htons(ETHER_TYPE_AVTP);
	error = bind(socket_descriptor, (struct sockaddr *) & ifsock_addr, sizeof(ifsock_addr));
	if (error < 0) {
		fprintf(stderr, "Failed to bind: %s\n", strerror(errno));
		return -EINVAL;
	}

	memset(&mreq, 0, sizeof(mreq));
	mreq.mr_ifindex = ifindex;
	mreq.mr_type = PACKET_MR_MULTICAST;
	mreq.mr_alen = 6;
	memcpy(mreq.mr_address, DEST_ADDR, mreq.mr_alen);
	error = setsockopt(socket_descriptor, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
	if (error < 0) {
		fprintf(stderr, "Failed to add multi-cast addresses to port: %u\n", ifindex);
		return -EINVAL;
	}

	size = sizeof(ifsock_addr);
	frame_sequence = 0;
	memset(frame, 0, sizeof(frame));

	memset(&sched, 0, sizeof sched);
	sched.sched_priority = 1;
	error = sched_setscheduler(0, SCHED_RR, &sched);
	if (error < 0)
		fprintf(stderr, "Failed to select RR scheduler: %s (%d)\n",
			strerror(errno), errno);

	while (1) {
		error = recvfrom(socket_descriptor, frame, MAX_FRAME_SIZE, 0, (struct sockaddr *) &ifsock_addr, (socklen_t *)&size);
		if (error > 0) {
			fprintf(stderr,"frame sequence = %lld\n", frame_sequence++);
			h1722 = (seventeen22_header *)((uint8_t*)frame + sizeof(eth_header));
			length = ntohs(h1722->length) - sizeof(six1883_header);
			write(1, (uint8_t *)((uint8_t*)frame + sizeof(eth_header) + sizeof(seventeen22_header) +
				sizeof(six1883_header)), length);
		} else {
			fprintf(stderr,"recvfrom() error for frame sequence = %lld\n", frame_sequence++);
		}
	}

	usleep(100);
	close(socket_descriptor);

	return 0;
}


