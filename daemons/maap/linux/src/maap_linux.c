/*************************************************************************
  Copyright (c) 2015 VAYAVYA LABS PVT LTD - http://vayavyalabs.com/
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in the
	  documentation and/or other materials provided with the distribution.

   3. Neither the name of the Vayavya labs nor the names of its
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

****************************************************************************/

#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/if.h>

#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "maap.h"
#include "maap_packet.h"
#include "maap_parse.h"


#define VERSION_STR	"0.1"

static const char *version_str =
	"maap_daemon v" VERSION_STR "\n"
	"Copyright (c) 2014-2015, VAYAVYA LABS PVT LTD\n"
	"Copyright (c) 2016, Harman International Industries Inc.\n";

void usage(void)
{
	fprintf(stderr,
		"\n"
		"usage: maap_daemon [-d] -i interface-name"
		"\n"
		"options:\n"
		"	-d  run daemon in the background\n"
		"	-i  specify interface to monitor\n"
		"\n" "%s" "\n", version_str);
	exit(1);
}

int main(int argc, char *argv[])
{
	int c;
	int daemonize = 0;
	int ret;
	char *iface = NULL;
	int socketfd;
	struct ifreq ifbuffer;
	uint8_t dest_mac[ETH_ALEN] = MAAP_DEST_MAC;
	uint8_t src_mac[ETH_ALEN];
	int ifindex;
	struct sockaddr_ll sockaddr;
	struct packet_mreq mreq;
	Maap_Client mc;
	void *packet_data;
	int64_t waittime;
	struct timeval tv;
	fd_set read_fds;
	int fdmax;
	char recvbuffer[1600];
	int recvbytes;
	Maap_Cmd recvcmd;

	/*
	 *  Parse the arguments
	 */

	while ((c = getopt(argc, argv, "hdi:")) >= 0)
	{
		switch (c)
		{
		case 'd':
			daemonize = 1;
			break;

		case 'i':
			if (iface)
			{
				printf("only one interface per daemon is supported\n");
				usage();
			}
			iface = strdup(optarg);
			break;

		case 'h':
		default:
			usage();
			break;
		}
	}
	if (optind < argc)
	{
		usage();
	}

	if (iface == NULL)
	{
		printf("A network interface is required\n");
		usage();
	}

	if (daemonize) {
		ret = daemon(1, 0);
		if (ret) {
			printf("Error: Failed to daemonize\n");
			return -1;
		}
	}


	/*
	 * Initialize the networking support.
	 */

	if ((socketfd = socket(PF_PACKET, SOCK_RAW, htons(MAAP_TYPE))) < 0 )
	{
		printf("Error: could not open socket %d\n",socketfd);
		return -1;
	}

	if (fcntl(socketfd, F_SETFL, O_NONBLOCK) < 0)
	{
		printf("Error: could not set the socket to non-blocking\n");
		return -1;
	}

	memset(&ifbuffer, 0x00, sizeof(ifbuffer));
	strncpy(ifbuffer.ifr_name, iface, IFNAMSIZ);
	if (ioctl(socketfd, SIOCGIFINDEX, &ifbuffer) < 0)
	{
		printf("Error: could not get interface index\n");
		close(socketfd);
		return -1;
	}
	free(iface);
	iface = NULL;

	ifindex = ifbuffer.ifr_ifindex;
	if (ioctl(socketfd, SIOCGIFHWADDR, &ifbuffer) < 0) {
		printf("Error: could not get interface address\n");
		close(socketfd);
		return -1;
	}

	memcpy(src_mac, ifbuffer.ifr_hwaddr.sa_data, ETH_ALEN);

	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sll_family = AF_PACKET;
	sockaddr.sll_ifindex = ifindex;
	sockaddr.sll_halen = ETH_ALEN;
	memcpy(sockaddr.sll_addr, dest_mac, ETH_ALEN);

	if (bind(socketfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr))) {
		printf("Error: could not bind datagram socket\n");
		return -1;
	}

	/* filter multicast address */
	memset(&mreq, 0, sizeof(mreq));
	mreq.mr_ifindex = ifindex;
	mreq.mr_type = PACKET_MR_MULTICAST;
	mreq.mr_alen = 6;
	memcpy(mreq.mr_address, dest_mac, mreq.mr_alen);

	if (setsockopt(socketfd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq,
		sizeof(mreq)) < 0) {
		printf("setsockopt PACKET_ADD_MEMBERSHIP failed\n");
		return -1;
	}


	/*
	 * Initialize the Maap_Client data structure.
	 */
	memset(&mc, 0, sizeof(mc));
	mc.dest_mac = convert_mac_address(dest_mac);
	mc.src_mac = convert_mac_address(src_mac);


	/*
	 * Seed the random number generator.
	 */

	srand((unsigned int)mc.src_mac ^ (unsigned int)time(NULL));



	/*
	 * Main event loop
	 */

	while (1)
	{
		/* Send any queued packets. */
		while (mc.net != NULL && (packet_data = Net_getNextQueuedPacket(mc.net)) != NULL)
		{
			if (send(socketfd, packet_data, MAAP_NET_BUFFER_SIZE, 0) < 0)
			{
				/* Something went wrong.  Abort! */
				printf("Error %d writing to network socket (%s)\n", errno, strerror(errno));
				break;
			}
			Net_freeQueuedPacket(mc.net, packet_data);
		}

		/* Determine how long to wait. */
		waittime = maap_get_delay_to_next_timer(&mc);
		if (waittime > 0)
		{
			tv.tv_sec = waittime / 1000000000;
			tv.tv_usec = (waittime % 1000000000) / 1000;
		}
		else
		{
			/* Act immediately. */
			tv.tv_sec = 0;
			tv.tv_usec = 0;
		}

		/* Wait for something to happen. */
		FD_ZERO(&read_fds);
		FD_SET(STDIN_FILENO, &read_fds);
		FD_SET(socketfd, &read_fds);
		fdmax = socketfd;
		ret = select(fdmax+1, &read_fds, NULL, NULL, &tv);
		if (ret < 0)
		{
			printf("select() error %d (%s)\n", errno, strerror(errno));
			break;
		}
		if (ret == 0)
		{
			/* The timer timed out.  Handle the timer. */
			maap_handle_timer(&mc);
			continue;
		}

		/* Handle any packets received. */
		if (FD_ISSET(socketfd, &read_fds))
		{
			struct sockaddr_ll ll_addr;
			socklen_t addr_len;

			while ((recvbytes = recvfrom(socketfd, recvbuffer, sizeof(recvbuffer), MSG_DONTWAIT, (struct sockaddr*)&ll_addr, &addr_len)) > 0)
			{
				maap_handle_packet(&mc, (uint8_t *)recvbuffer, recvbytes);
			}
			if (recvbytes < 0 && errno != EWOULDBLOCK)
			{
				/* Something went wrong.  Abort! */
				printf("Error %d reading from network socket (%s)\n", errno, strerror(errno));
				break;
			}
		}

		/* Handle any commands received via stdin. */
		if (FD_ISSET(STDIN_FILENO, &read_fds))
		{
			recvbytes = read(STDIN_FILENO, recvbuffer, sizeof(recvbuffer) - 1);
			if (recvbytes < 0)
			{
				printf("Error %d reading from stdin (%s)\n", errno, strerror(errno));
			}
			else
			{
				recvbuffer[recvbytes] = '\0';

				/* Process the data (may be binary or text.) */
				memset(&recvcmd, 0, sizeof(recvcmd));
				if (parse_write(&mc, recvbuffer)) {
					/* Received a command to exit. */
					break;
				}
			}
		}

	}

	close(socketfd);

	maap_deinit_client(&mc);

	return 0;
}
