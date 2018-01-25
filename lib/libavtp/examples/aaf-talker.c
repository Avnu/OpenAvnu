/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of Intel Corporation nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* AAF Talker example.
 *
 * This example implements a very simple AAF talker application which reads
 * a PCM stream from stdin, creates AAF packets and transmit them via the
 * network.
 *
 * For simplicity, the example supports only one set of PCM parameters:
 *    - Sample format: 16-bit little endian
 *    - Sample rate: 48 kHz
 *    - Number of channels: 2 (stereo)
 *
 * TSN stream parameters (e.g. destination mac address, traffic priority) are
 * passed via command-line arguments. Run 'aaf-talker --help' for more
 * information.
 *
 * In order to have this example working properly, make sure you have
 * configured FQTSS feature from your NIC according (for further information
 * see tc-cbs(8)). Also, this example relies on system clock to set the AVTP
 * timestamp so make sure it is synchronized with the PTP Hardware Clock (PHC)
 * from your NIC and that the PHC is synchronized with the network clock. For
 * further information see ptp4l(8) and phc2sys(8).
 *
 * The easiest way to use this example is combining it with 'arecord' tool
 * provided by alsa-utils. 'arecord' reads the PCM stream from a capture ALSA
 * device (e.g. your microphone) and writes it to stdout. So to stream Audio
 * captured from your mic to a TSN network you should do something like this:
 *
 * $ arecord -f dat -t raw -D <capture-device> | aaf-talker <args>
 */

#include <alloca.h>
#include <argp.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "avtp.h"
#include "avtp_aaf.h"

#define STREAM_ID		0xAABBCCDDEEFF0001
#define SAMPLE_SIZE		2 /* Sample size in bytes. */
#define NUM_CHANNELS		2
#define DATA_LEN		(SAMPLE_SIZE * NUM_CHANNELS)
#define PDU_SIZE		(sizeof(struct avtp_stream_pdu) + DATA_LEN)
#define NSEC_PER_SEC		1000000000ULL
#define NSEC_PER_MSEC		1000000ULL

static char ifname[IFNAMSIZ];
static uint8_t macaddr[ETH_ALEN];
static int priority = -1;
static int max_transit_time;

static struct argp_option options[] = {
	{"dst-addr", 'd', "MACADDR", 0, "Stream Destination MAC address" },
	{"ifname", 'i', "IFNAME", 0, "Network Interface" },
	{"max-transit-time", 'm', "MSEC", 0, "Maximum Transit Time in ms" },
	{"prio", 'p', "NUM", 0, "SO_PRIORITY to be set in socket" },
	{ 0 }
};

static error_t parser(int key, char *arg, struct argp_state *state)
{
	int res;

	switch (key) {
	case 'd':
		res = sscanf(arg, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
					&macaddr[0], &macaddr[1], &macaddr[2],
					&macaddr[3], &macaddr[4], &macaddr[5]);
		if (res != 6) {
			fprintf(stderr, "Invalid address\n");
			exit(EXIT_FAILURE);
		}

		break;
	case 'i':
		strncpy(ifname, arg, sizeof(ifname) - 1);
		break;
	case 'm':
		max_transit_time = atoi(arg);
		break;
	case 'p':
		priority = atoi(arg);
		break;
	}

	return 0;
}

static struct argp argp = { options, parser };

static int setup_socket(void)
{
	int fd, res;

	fd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_TSN));
	if (fd < 0) {
		perror("Failed to open socket");
		return -1;
	}

	if (priority != -1) {
		res = setsockopt(fd, SOL_SOCKET, SO_PRIORITY, &priority,
							sizeof(priority));
		if (res < 0) {
			perror("Failed to set priority");
			goto err;
		}

	}

	return fd;

err:
	close(fd);
	return -1;
}

static int calculate_avtp_time(uint32_t *avtp_time)
{
	int res;
	struct timespec tspec;
	uint64_t ptime;

	res = clock_gettime(CLOCK_REALTIME, &tspec);
	if (res < 0) {
		perror("Failed to get time");
		return -1;
	}

	ptime = (tspec.tv_sec * NSEC_PER_SEC) +
			(max_transit_time * NSEC_PER_MSEC) + tspec.tv_nsec;

	*avtp_time = ptime % (1ULL << 32);

	return 0;
}

static int init_pdu(struct avtp_stream_pdu *pdu)
{
	int res;

	res = avtp_aaf_pdu_init(pdu);
	if (res < 0)
		return -1;

	res = avtp_aaf_pdu_set(pdu, AVTP_AAF_FIELD_TV, 1);
	if (res < 0)
		return -1;

	res = avtp_aaf_pdu_set(pdu, AVTP_AAF_FIELD_STREAM_ID, STREAM_ID);
	if (res < 0)
		return -1;

	res = avtp_aaf_pdu_set(pdu, AVTP_AAF_FIELD_FORMAT,
						AVTP_AAF_FORMAT_INT_16BIT);
	if (res < 0)
		return -1;

	res = avtp_aaf_pdu_set(pdu, AVTP_AAF_FIELD_NSR,
						AVTP_AAF_PCM_NSR_48KHZ);
	if (res < 0)
		return -1;

	res = avtp_aaf_pdu_set(pdu, AVTP_AAF_FIELD_CHAN_PER_FRAME,
								NUM_CHANNELS);
	if (res < 0)
		return -1;

	res = avtp_aaf_pdu_set(pdu, AVTP_AAF_FIELD_BIT_DEPTH, 16);
	if (res < 0)
		return -1;

	res = avtp_aaf_pdu_set(pdu, AVTP_AAF_FIELD_STREAM_DATA_LEN, DATA_LEN);
	if (res < 0)
		return -1;

	res = avtp_aaf_pdu_set(pdu, AVTP_AAF_FIELD_SP, AVTP_AAF_PCM_SP_NORMAL);
	if (res < 0)
		return -1;

	return 0;
}

int main(int argc, char *argv[])
{
	int fd, res;
	struct ifreq req;
	struct sockaddr_ll sk_addr;
	struct avtp_stream_pdu *pdu = alloca(PDU_SIZE);
	uint8_t seq_num = 0;

	argp_parse(&argp, argc, argv, 0, NULL, NULL);

	fd = setup_socket();
	if (fd < 0)
		return 1;

	snprintf(req.ifr_name, sizeof(req.ifr_name), "%s", ifname);
	res = ioctl(fd, SIOCGIFINDEX, &req);
	if (res < 0) {
		perror("Failed to get interface index");
		goto err;
	}

	sk_addr.sll_family = AF_PACKET;
	sk_addr.sll_protocol = htons(ETH_P_TSN);
	sk_addr.sll_halen = ETH_ALEN;
	sk_addr.sll_ifindex = req.ifr_ifindex;
	memcpy(&sk_addr.sll_addr, macaddr, ETH_ALEN);

	res = init_pdu(pdu);
	if (res < 0)
		goto err;

	while (1) {
		ssize_t n;
		uint32_t avtp_time;

		memset(pdu->avtp_payload, 0, DATA_LEN);

		n = read(STDIN_FILENO, pdu->avtp_payload, DATA_LEN);
		if (n == 0)
			break;

		if (n != DATA_LEN) {
			fprintf(stderr, "read %zd bytes, expected %d\n",
								n, DATA_LEN);
		}

		res = calculate_avtp_time(&avtp_time);
		if (res < 0) {
			fprintf(stderr, "Failed to calculate avtp time\n");
			goto err;
		}

		res = avtp_aaf_pdu_set(pdu, AVTP_AAF_FIELD_TIMESTAMP,
								avtp_time);
		if (res < 0)
			goto err;

		res = avtp_aaf_pdu_set(pdu, AVTP_AAF_FIELD_SEQ_NUM, seq_num++);
		if (res < 0)
			goto err;

		n = sendto(fd, pdu, PDU_SIZE, 0,
				(struct sockaddr *) &sk_addr, sizeof(sk_addr));
		if (n < 0) {
			perror("Failed to send data");
			goto err;
		}

		if (n != PDU_SIZE) {
			fprintf(stderr, "wrote %zd bytes, expected %zd\n",
								n, PDU_SIZE);
		}
	}

	close(fd);
	return 0;

err:
	close(fd);
	return 1;
}
