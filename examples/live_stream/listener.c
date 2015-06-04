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

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if.h>
#include <netpacket/packet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>

#include <pci/pci.h>

#include "avb.h"
#include "listener_mrp_client.h"

#define USE_MRPD 1

/* globals */

unsigned char glob_dest_addr[] = { 0x91, 0xE0, 0xF0, 0x00, 0x0E, 0x80 };

void sigint_handler(int signum)
{
	int ret;

	fprintf(stderr, "Received signal %d:leaving...\n", signum);
#if USE_MRPD
	if (0 != talker) {
		ret = send_leave();
		if (ret)
			printf("send_leave failed\n");
	}
#endif /* USE_MRPD */
	if (2 > control_socket)
	{
		close(control_socket);
	}
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[ ])
{
	device_t igb_dev;
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
	int rc;

	if (argc < 2) {
		fprintf(stderr, "Usage : %s <interface_name> <payload>\n",argv[0]);
		return EINVAL;
	}
	signal(SIGINT, sigint_handler);

#if USE_MRPD
	if (create_socket()) {
		fprintf(stderr, "Socket creation failed.\n");
		return errno;
	}

	rc = report_domain_status();
	if (rc) {
		printf("report_domain_status failed\n");
		return EXIT_FAILURE;
	}

	fprintf(stdout,"Waiting for talker...\n");
	await_talker();
	rc = send_ready();
	if (rc) {
		printf("send_ready failed\n");
		return EXIT_FAILURE;
	}

#endif /* USE_MRPD */
	iface = strdup(argv[1]);

	error = pci_connect(&igb_dev);
	if (error) {
		fprintf(stderr, "connect failed (%s) - are you running as root?\n", strerror(errno));
		return errno;
	}

	error = igb_init(&igb_dev);
	if (error) {
		fprintf(stderr, "init failed (%s) - is the driver really loaded?\n", strerror(errno));
		return errno;
	}

	socket_descriptor = socket(AF_PACKET, SOCK_RAW, htons(ETHER_TYPE_AVTP));
	if (socket_descriptor < 0) {
		fprintf(stderr, "failed to open socket: %s \n", strerror(errno));
		return EINVAL;
	}

	memset(&device, 0, sizeof(device));
	memcpy(device.ifr_name, iface, IFNAMSIZ);
	error = ioctl(socket_descriptor, SIOCGIFINDEX, &device);
	if (error < 0) {
		fprintf(stderr, "Failed to get index of iface %s: %s\n", iface, strerror(errno));
		return EINVAL;
	}

	ifindex = device.ifr_ifindex;
	memset(&ifsock_addr, 0, sizeof(ifsock_addr));
	ifsock_addr.sll_family = AF_PACKET;
	ifsock_addr.sll_ifindex = ifindex;
	ifsock_addr.sll_protocol = htons(ETHER_TYPE_AVTP);
	error = bind(socket_descriptor, (struct sockaddr *) & ifsock_addr, sizeof(ifsock_addr));
	if (error < 0) {
		fprintf(stderr, "Failed to bind: %s\n", strerror(errno));
		return EINVAL;
	}

	memset(&mreq, 0, sizeof(mreq));
	mreq.mr_ifindex = ifindex;
	mreq.mr_type = PACKET_MR_MULTICAST;
	mreq.mr_alen = 6;
	memcpy(mreq.mr_address, glob_dest_addr, mreq.mr_alen);
	error = setsockopt(socket_descriptor, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
	if (error < 0) {
		fprintf(stderr, "Failed to add multi-cast addresses to port: %u\n", ifindex);
		return EINVAL;
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

	return EXIT_SUCCESS;
}


