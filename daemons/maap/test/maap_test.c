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

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pcap.h>
#include <stdlib.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <endian.h>

/* Test function forward declarations */
void test1_setup(void);
void test2_setup(void);
void test3_setup(void);
void delay_setup(void);

/* MAAP packet encoding/decoding */
#define MAAP_RESERVED 0
#define MAAP_PROBE    1
#define MAAP_DEFEND   2
#define MAAP_ANNOUNCE 3

typedef struct maap_packet {
	uint64_t DA;
	uint64_t SA;
	uint16_t Ethertype;
	uint16_t subtype;
	uint8_t SV;
	uint8_t version;
	uint8_t message_type;
	uint8_t status;
	uint16_t control_data_length;
	uint64_t stream_id;
	uint64_t requested_start_address;
	uint16_t requested_count;
	uint64_t start_address;
	uint16_t count;
} maap_packet_t;

void dump_raw_stream(uint8_t *stream) {
	int i;
	for (i = 0; i < 42; i++) {
		printf("%02x ", stream[i]);
		if ((i+1) % 16 == 0) {
			printf("\n");
		}
	}
}

void dump_packed_packet(maap_packet_t *packet) {
	printf("\n\n");
	printf("DA: %012llx\n", (unsigned long long)packet->DA);
	printf("SA: %012llx\n", (unsigned long long)packet->SA);
	printf("Ethertype: 0x%04x\n", packet->Ethertype);
	printf("subtype: 0x%02x\n", packet->subtype);
	printf("SV: %d\n", packet->SV);
	printf("version: %d\n", packet->version);
	printf("message_type: %d\n", packet->message_type);
	printf("status: %d\n", packet->status);
	printf("control_data_length: %d\n", packet->control_data_length);
	printf("stream_id: %016llx\n", (unsigned long long)packet->stream_id);
	printf("requested_start_address: %012llx\n", (unsigned long long)packet->requested_start_address);
	printf("requested_count: %d\n", packet->requested_count);
	printf("start_address: %012llx\n", (unsigned long long)packet->start_address);
	printf("count: %d\n", packet->count);
}

int unpack_maap(maap_packet_t *packet, uint8_t *stream) {
	packet->DA = be64toh(*(uint64_t *)stream) >> 16;
	stream += 6;
	packet->SA = be64toh(*(uint64_t *)stream) >> 16;
	stream += 6;
	packet->Ethertype = be16toh(*(uint16_t *)stream);
	stream += 2;
	packet->subtype = *stream;
	stream++;
	packet->SV = (*stream & 0x80) >> 7;
	packet->version = (*stream & 0x70) >> 4;
	packet->message_type = *stream & 0x0f;
	stream++;
	packet->status = (*stream & 0xf8) >> 3;
	packet->control_data_length = be16toh(*(uint16_t *)stream) & 0x07ff;
	stream += 2;
	packet->stream_id = be64toh(*(uint64_t *)stream);
	stream += 8;
	packet->requested_start_address = be64toh(*(uint64_t *)stream) >> 16;
	stream += 6;
	packet->requested_count = be16toh(*(uint16_t *)stream);
	stream += 2;
	packet->start_address = be64toh(*(uint64_t *)stream) >> 16;
	stream += 6;
	packet->count = be16toh(*(uint16_t *)stream);
	return 0;
}

int pack_maap(maap_packet_t *packet, uint8_t *stream) {
	*(uint64_t *)stream = htobe64(packet->DA << 16);
	stream += 6;
	*(uint64_t *)stream = htobe64(packet->SA << 16);
	stream += 6;
	*(uint16_t *)stream = htobe16(packet->Ethertype);
	stream += 2;
	*stream = packet->subtype;
	stream++;
	*stream = (packet->SV << 7) | ((packet->version & 0x07) << 4) |
		(packet->message_type & 0x0f);
	stream++;
	*(uint16_t *)stream = htobe16(((packet->status & 0x001f) << 11) |
		(packet->control_data_length & 0x07ff));
	stream += 2;
	*(uint64_t *)stream = htobe64(packet->stream_id);
	stream += 8;
	*(uint64_t *)stream = htobe64(packet->requested_start_address << 16);
	stream += 6;
	*(uint16_t *)stream = htobe16(packet->requested_count);
	stream += 2;
	*(uint64_t *)stream = htobe64(packet->start_address << 16);
	stream += 6;
	*(uint16_t *)stream = htobe16(packet->count);
	return 0;
}

/* callback for pcap when packets arrive */
void parse_packet(u_char *args, const struct pcap_pkthdr *header,
				const u_char *packet);

struct maap_test_state {
	int (*pkt_handler)(maap_packet_t *);
	int state;
	int rawsock;
	uint64_t hwaddr;
};

struct maap_test_state test_state = {NULL, 0, 0, 0};

int get_raw_sock(int ethertype) {
	int rawsock;

	if((rawsock = socket(PF_PACKET, SOCK_RAW, htons(ethertype))) == -1) {
		perror("Error creating raw sock: ");
		exit(-1);
	}

	return rawsock;
}

int bind_sock(char *device, int rawsock, int protocol) {
	struct sockaddr_ll sll;
	struct ifreq ifr;

	bzero(&sll, sizeof(sll));
	bzero(&ifr, sizeof(ifr));

	/* First, get the interface index */
	strncpy((char *)ifr.ifr_name, device, IFNAMSIZ);
	if((ioctl(rawsock, SIOCGIFINDEX, &ifr)) == -1)    {
		printf("Error getting Interface index !\n");
		printf("Problem interface: %s\n", device);
		exit(-1);
	}

	/* Bind our raw socket to this interface */
	sll.sll_family = AF_PACKET;
	sll.sll_ifindex = ifr.ifr_ifindex;
	sll.sll_protocol = htons(protocol);

	/* Get the mac address */
	if ((ioctl(rawsock, SIOCGIFHWADDR, &ifr)) == -1) {
		printf("Error getting HW address\n");
		exit(-1);
	}
	bcopy(ifr.ifr_hwaddr.sa_data, &test_state.hwaddr, 6);
	test_state.hwaddr = be64toh(test_state.hwaddr) >> 16;
	printf("Our MAC is %012llx\n", (unsigned long long int)test_state.hwaddr);

	if((bind(rawsock, (struct sockaddr*)&sll, sizeof(sll))) == -1) {
		perror("Error binding raw socket to interface\n");
		exit(-1);
	}

	return 1;
}

int send_packet(int rawsock, maap_packet_t *mp) {
	int sent = 0;
	unsigned char pkt[60];

	mp->SA = test_state.hwaddr;

	pack_maap(mp, pkt);
	if((sent = write(rawsock, pkt, 60)) != 60) {
		return 0;
	}
	return 1;
}

int main(int argc, char *argv[]) {
	char *dev, errbuf[PCAP_ERRBUF_SIZE];
	pcap_t *handle;
	struct bpf_program fp;
	char filter_exp[] = "ether proto 0x22F0";
	bpf_u_int32 mask;
	bpf_u_int32 net;

	if (argc < 2) {
		dev = pcap_lookupdev(errbuf);
		if (dev == NULL) {
			fprintf(stderr, "Couldn't find default device: %s\n", errbuf);
			fprintf(stderr, "Try specifying the device you want: %s <device>\n", argv[0]);
			return 2;
		}
	} else {
		dev = argv[1];
	}

	if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) {
		fprintf(stderr, "Couldn't get netmask for device: %s\n", dev);
		net = 0;
		mask = 0;
	}

	handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);
	if (handle == NULL) {
		fprintf(stderr, "Couldn't open device: %s\n", dev);
		return 2;
	}

	printf("Listening on device: %s\n", dev);

	if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
		fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp,
				pcap_geterr(handle));
		return 2;
	}

	if (pcap_setfilter(handle, &fp) == -1) {
		fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp,
				pcap_geterr(handle));
		return 2;
	}

	test_state.rawsock = get_raw_sock(0x22F0);
	bind_sock(dev, test_state.rawsock, 0x22F0);

	test1_setup();

	pcap_loop(handle, -1, parse_packet, NULL);

	return 0;
}

void parse_packet(u_char *args, const struct pcap_pkthdr *header,
				const u_char *packet) {
	maap_packet_t mp;

	(void)args; (void)header;

	unpack_maap(&mp, (uint8_t *)packet);

	dump_packed_packet(&mp);

	/* Ignore messages from us */
	if (mp.SA == test_state.hwaddr) {
		return;
	}

	if (test_state.pkt_handler) {
		test_state.pkt_handler(&mp);
	}

	return;
}

/*************************************************************************
 * Test functions
 *************************************************************************/

/* Wait for a probe, send a defense response, see if next probe is for a
   different range */

#define WAITING_FOR_1ST_PROBE 0
#define WAITING_FOR_2ND_PROBE 1
#define WAITING_FOR_3RD_PROBE 2

int test1_handler(maap_packet_t *mp) {
	static uint64_t start;
	static uint16_t count;

	switch (test_state.state) {

	case WAITING_FOR_1ST_PROBE:

		/* Make sure we're getting PROBE packets */
		if (mp->message_type != MAAP_PROBE) {
			fprintf(stderr, "Test 1: Error: Received a non-probe MAAP packet.  Please start the test suite before starting the MAAP client to be tested.\n");
			exit(2);
		}
		printf("Received a probe from %012llx for %d addresses starting at %012llx\n",
			(long long unsigned int)mp->SA, mp->requested_count,
			(long long unsigned int)mp->requested_start_address);

		/* Store the range from the PROBE so we can conflict with it and compare
		   later */
		start = mp->requested_start_address;
		count = mp->requested_count;

		/* Send a defense of this range */
		mp->DA = mp->SA;
		mp->SA = 0;
		mp->message_type = MAAP_DEFEND;
		mp->start_address = start;
		mp->count = count;
		send_packet(test_state.rawsock, mp);
		printf("Sent defend packet\n");

		/* Update testing state */
		test_state.state = WAITING_FOR_2ND_PROBE;

		break;

	case WAITING_FOR_2ND_PROBE:

		/* Make sure we got a probe in response */
		if (mp->message_type != MAAP_PROBE) {
			fprintf(stderr, "Test 1: FAIL - Got a non-probe packet, type %d.\n",
					mp->message_type);
			exit(2);
		}

		printf("Received a probe from %012llx for %d addresses starting at %012llx\n",
			(long long unsigned int)mp->SA, mp->requested_count,
			(long long unsigned int)mp->requested_start_address);

		/* See if the address range has changed */
		if (mp->requested_start_address == start) {
			/* They may have sent before we did; give them one more try */
			mp->DA = mp->SA;
			mp->SA = 0;
			mp->message_type = MAAP_DEFEND;
			mp->start_address = start;
			mp->count = count;
			send_packet(test_state.rawsock, mp);
			printf("Sent another defend packet\n");

			/* Update testing state */
			test_state.state = WAITING_FOR_3RD_PROBE;

		} else {

			/* Success! */
			printf("Test 1: PASS\n");

			test2_setup();

		}

		break;

	case WAITING_FOR_3RD_PROBE:

		/* Make sure we got a probe in response */
		if (mp->message_type != MAAP_PROBE) {
			fprintf(stderr, "Test 1: FAIL - Got a non-probe packet, type %d.\n",
					mp->message_type);
			exit(2);
		}

		printf("Received a probe from %012llx for %d addresses starting at %012llx\n",
			(long long unsigned int)mp->SA, mp->requested_count,
			(long long unsigned int)mp->requested_start_address);

		/* Make sure the address range has changed */
		if (mp->requested_start_address == start) {
			fprintf(stderr, "Test 1: FAIL - Got a probe for the same range.\n");
			exit(2);
		}

		/* Success! */
		printf("Test 1: PASS\n");

		test2_setup();

		break;
	}

	return 0;
}

void test1_setup(void) {
	test_state.state = WAITING_FOR_1ST_PROBE;
	test_state.pkt_handler = test1_handler;
	printf("Starting test 1: responding to a defend packet while probing\n");
}

/*
 * Test 2
 */

/* Wait for an announce, send a probe for the same range, ensure we get a
 * defend for the range */

#define WAITING_FOR_ANNOUNCE 0
#define WAITING_FOR_DEFEND   1

int test2_handler(maap_packet_t *mp) {
	static uint64_t start;
	static uint16_t count;

	switch (test_state.state) {

	case WAITING_FOR_ANNOUNCE:
		if (mp->message_type != MAAP_ANNOUNCE) {
			/* Ignore non-announce packets */
			break;
		}
		printf("Received an announce from %012llx for %d addresses starting at %012llx\n",
			(long long unsigned int)mp->SA, mp->requested_count,
			(long long unsigned int)mp->requested_start_address);

		/* Store the range from the ANNOUNCE so we can check the defense of this
		   range */
		start = mp->requested_start_address;
		count = mp->requested_count;

		/* Send a probe for this range */
		mp->message_type = MAAP_PROBE;
		send_packet(test_state.rawsock, mp);
		printf("Sent probe packet\n");

		/* Update testing state */
		test_state.state = WAITING_FOR_DEFEND;

		break;

	case WAITING_FOR_DEFEND:
		if (mp->message_type != MAAP_DEFEND) {
			fprintf(stderr, "Test 2: FAIL - Got a non-defend packet, type %d.\n",
				mp->message_type);
			exit(2);
		}
		if (mp->requested_start_address != start) {
			fprintf(stderr, "Test 2: FAIL - Requested start address wasn't filled out with the value from the probe.\n");
			fprintf(stderr, "Req start address: %012llx  Probe address: %012llx\n",
				(unsigned long long)mp->requested_start_address,
				(unsigned long long)start);
			exit(2);
		}
		if (mp->requested_count != count) {
			fprintf(stderr, "Test 2: FAIL - Requested count wasn't filled out with the value from the probe.\n");
			fprintf(stderr, "Requested count: %d  Probe count: %d\n",
				mp->requested_count, count);
			exit(2);
		}
		if (mp->start_address != start) {
			fprintf(stderr, "Test 2: FAIL - Start address wasn't filled out with the first conflicting address from the probe.\n");
			fprintf(stderr, "Start address: %012llx  Conflicting address: %012llx\n",
				(unsigned long long)mp->start_address,
				(unsigned long long)start);
			exit(2);
		}
		if (mp->count != count) {
			fprintf(stderr, "Test 2: FAIL - Count wasn't filled out with the number of addresses conflicting with the probe.\n");
			fprintf(stderr, "Defend count: %d  Conflicting count: %d\n", mp->count, count);
			exit(2);
		}

		/* Success! */
		printf("Test 2: PASS\n");

		test3_setup();

		break;

	}

	return 0;
}

void test2_setup(void) {
	test_state.state = WAITING_FOR_ANNOUNCE;
	test_state.pkt_handler = test2_handler;
	printf("Starting test 2: defending a reservation from a probe\n");
	printf("Waiting for an announce packet...");
}

/*
 * Test 3
 */

#define WAITING_FOR_PROBE 2

int test3_handler(maap_packet_t *mp) {
	static uint64_t start;
	static uint16_t count;
	static maap_packet_t saved_announce;

	switch (test_state.state) {

	case WAITING_FOR_ANNOUNCE:
		if (mp->message_type != MAAP_ANNOUNCE) {
			/* Ignore non-announce packets */
			break;
		}
		printf("Received an announce from %012llx for %d addresses starting at %012llx\n",
			(long long unsigned int)mp->SA, mp->requested_count,
			(long long unsigned int)mp->requested_start_address);

		/* Store the range from the ANNOUNCE so we can check the defense of this
		   range */
		start = mp->requested_start_address;
		count = mp->requested_count;

		/* Send an announce for this range (send the same packet right back) */
		send_packet(test_state.rawsock, mp);
		printf("Sent announce packet\n");

		/* Save a copy so we can send it again */
		memcpy(&saved_announce, mp, sizeof (maap_packet_t));

		/* Update testing state */
		test_state.state = WAITING_FOR_DEFEND;

		break;

	case WAITING_FOR_DEFEND:
		if (mp->message_type != MAAP_DEFEND) {
			fprintf(stderr, "test 3: FAIL - Got a non-defend packet, type %d.\n",
				mp->message_type);
			exit(2);
		}
		if (mp->requested_start_address != start) {
			fprintf(stderr, "test 3: FAIL - Requested start address wasn't filled out with the value from the probe.\n");
			fprintf(stderr, "Req start address: %012llx  Probe address: %012llx\n",
				(unsigned long long)mp->requested_start_address,
				(unsigned long long)start);
			exit(2);
		}
		if (mp->requested_count != count) {
			fprintf(stderr, "test 3: FAIL - Requested count wasn't filled out with the value from the probe.\n");
			fprintf(stderr, "Requested count: %d  Probe count: %d\n",
				mp->requested_count, count);
			exit(2);
		}
		if (mp->start_address != start) {
			fprintf(stderr, "test 3: FAIL - Start address wasn't filled out with the first conflicting address from the probe.\n");
			fprintf(stderr, "Start address: %012llx  Conflicting address: %012llx\n",
				(unsigned long long)mp->start_address,
				(unsigned long long)start);
			exit(2);
		}
		if (mp->count != count) {
			fprintf(stderr, "test 3: FAIL - Count wasn't filled out with the number of addresses conflicting with the probe.\n");
			fprintf(stderr, "Defend count: %d  Conflicting count: %d\n", mp->count, count);
			exit(2);
		}

		/* Send out the same announce again */
		send_packet(test_state.rawsock, &saved_announce);
		printf("Sent second announce\n");

		/* Update testing state */
		test_state.state = WAITING_FOR_PROBE;

		break;

	case WAITING_FOR_PROBE:
		/* Make sure we got a probe in response */
		if (mp->message_type != MAAP_PROBE) {
			fprintf(stderr, "Test 3: FAIL - Got a non-probe packet, type %d.\n",
				mp->message_type);
			exit(2);
		}

		printf("Received a probe from %012llx for %d addresses starting at %012llx\n",
			(long long unsigned int)mp->SA, mp->requested_count,
			(long long unsigned int)mp->requested_start_address);

		/* Make sure the address range has changed */
		if (mp->requested_start_address == start) {
			fprintf(stderr, "Test 3: FAIL - Got a probe for the same range.\n");
			exit(2);
		}

		/* Success! */
		printf("Test 3: PASS\n");

		delay_setup();

		break;

	}

	return 0;
}

void test3_setup(void) {
	test_state.state = WAITING_FOR_ANNOUNCE;
	test_state.pkt_handler = test3_handler;
	printf("Starting Test 3: yielding a reservation after 2 announces\n");
	printf("Waiting for an announce packet...\n");
}

int delay_handler(maap_packet_t *mp) {
	if (mp->message_type == MAAP_ANNOUNCE) {
		test_state.state += 1;
		if (test_state.state == 10) {
			printf("Received 10 Announce messages.\n");
			test3_setup();
		}
	}
	return 0;
}

void delay_setup(void) {
	test_state.state = 0;
	test_state.pkt_handler = delay_handler;
	printf("Waiting for ~5min (10 Announces)\n");
}
