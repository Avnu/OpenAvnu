/******************************************************************************

  Copyright (c) 2019, Aquantia Corporation
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

#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <linux/if.h>
#include "avb_atl.h"

#include <pci/pci.h>

#include "talker_mrp_client.h"
#include "avb.h"

#define VERSION_STR "1.0"

#define SRC_CHANNELS (2)
#define GAIN (0.5)
#define L16_PAYLOAD_TYPE (96) /* for layer 4 transport - should be negotiated via RTSP */
#define ID_B_HDR_EXT_ID (0) /* for layer 4 transport - should be negotiated via RTSP */
#define L2_SAMPLES_PER_FRAME (6)
#define L4_SAMPLES_PER_FRAME (60)
#define L4_SAMPLE_SIZE (2)
#define CHANNELS (2)
#define RTP_SUBNS_SCALE_NUM (20000000)
#define RTP_SUBNS_SCALE_DEN (4656613)
#define XMIT_DELAY (200000000) /* us */
#define RENDER_DELAY (XMIT_DELAY+2000000)	/* us */
#define L2_PACKET_IPG (125000) /* (1) packet every 125 usec */
#define L4_PACKET_IPG (1250000)	/* (1) packet every 1.25 millisec */
#define L4_PORT ((uint16_t)5004)
#define PKT_SZ (100)

typedef long double FrequencyRatio;
volatile int *halt_tx_sig;//Global variable for signal handler

typedef struct __attribute__ ((packed)) {
	uint8_t version_length;
	uint8_t DSCP_ECN;
	uint16_t ip_length;
	uint16_t id;
	uint16_t fragmentation;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t hdr_cksum;
	uint32_t src;
	uint32_t dest;

	uint16_t source_port;
	uint16_t dest_port;
	uint16_t udp_length;
	uint16_t cksum;

	uint8_t version_cc;
	uint8_t mark_payload;
	uint16_t sequence;
	uint32_t timestamp;
	uint32_t ssrc;

	uint8_t tag[2];
	uint16_t total_length;
	uint8_t tag_length;
	uint8_t seconds[3];
	uint32_t nanoseconds;
} IP_RTP_Header;

typedef struct __attribute__ ((packed)) {
	uint32_t source;
	uint32_t dest;
	uint8_t zero;
	uint8_t protocol;
	uint16_t length;
} IP_PseudoHeader;

/* globals */

static const char *version_str = "simple_talker v" VERSION_STR "\n"
    "Copyright (c) 2012, Intel Corporation\n";

unsigned char glob_station_addr[] = { 0, 0, 0, 0, 0, 0 };
unsigned char glob_stream_id[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
/* IEEE 1722 reserved address */
unsigned char glob_l2_dest_addr[] = { 0x91, 0xE0, 0xF0, 0x00, 0x0e, 0x80 };
unsigned char glob_l3_dest_addr[] = { 224, 0, 0, 115 };

uint16_t inet_checksum(uint8_t *ip, int len){
	uint32_t sum = 0;  /* assume 32 bit long, 16 bit short */

	while(len > 1){
		sum += *(( uint16_t *) ip); ip += 2;
		if(sum & 0x80000000)   /* if high order bit set, fold */
			sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}

	if(len)       /* take care of left over byte */
		sum += (uint16_t) *(uint8_t *)ip;

	while(sum>>16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~sum;
}

#if 0
 else {
			if(sum & 0x80000000)   /* if high order bit set, fold */
				sum = (sum & 0xFFFF) + (sum >> 16);
			sum += *(( uint16_t *) buf_iov->iov_base); buf_iov->iov_base += 2;
			buf_iov->iov_len -= 2;
		}
#endif

uint16_t inet_checksum_sg( struct iovec *buf_iov, size_t buf_iovlen ){
	size_t i;
	uint32_t sum = 0;  /* assume 32 bit long, 16 bit short */
	uint8_t residual;
	int has_residual = 0;

	for( i = 0; i < buf_iovlen; ++i,++buf_iov ) {
		if( has_residual ) {
			if( buf_iov->iov_len > 0 ) {
				if(sum & 0x80000000)   /* if high order bit set, fold */
					sum = (sum & 0xFFFF) + (sum >> 16);
				sum += residual | (*(( uint8_t *) buf_iov->iov_base) << 8);
				buf_iov->iov_base += 1;
				buf_iov->iov_len -= 1;
			} else {
				if(sum & 0x80000000)   /* if high order bit set, fold */
					sum = (sum & 0xFFFF) + (sum >> 16);
				sum += (uint16_t) residual;
			}
			has_residual = 0;

		}
		while(buf_iov->iov_len > 1){
			if(sum & 0x80000000)   /* if high order bit set, fold */
				sum = (sum & 0xFFFF) + (sum >> 16);
			sum += *(( uint16_t *) buf_iov->iov_base); buf_iov->iov_base += 2;
			buf_iov->iov_len -= 2;
		}
		if( buf_iov->iov_len ) {
			residual = *(( uint8_t *) buf_iov->iov_base);
			has_residual = 1;
		}
	}
	if( has_residual ) {
		sum += (uint16_t) residual;
	}

	while(sum>>16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~sum;
}

static inline uint64_t ST_rdtsc(void)
{
	uint64_t ret;
	unsigned c, d;
	asm volatile ("rdtsc":"=a" (c), "=d"(d));
	ret = d;
	ret <<= 32;
	ret |= c;
	return ret;
}

void gensine32(int32_t * buf, unsigned count)
{
	long double interval = (2 * ((long double)M_PI)) / count;
	unsigned i;
	for (i = 0; i < count; ++i) {
		buf[i] =
		    (int32_t) (MAX_SAMPLE_VALUE * sinl(i * interval) * GAIN);
	}
}

int get_samples(unsigned count, int32_t * buffer)
{
	static int init = 0;
	static int32_t samples_onechannel[100];
	static unsigned index = 0;

	if (init == 0) {
		gensine32(samples_onechannel, 100);
		init = 1;
	}

	while (count > 0) {
		int i;
		for (i = 0; i < SRC_CHANNELS; ++i) {
			*(buffer++) = samples_onechannel[index];
		}
		index = (index + 1) % 100;
		--count;
	}

	return 0;
}

void sigint_handler(int signum)
{
	printf("got SIGINT\n");
	*halt_tx_sig = signum;
}

void l3_to_l2_multicast( unsigned char *l2, unsigned char *l3 ) {
	 l2[0]  = 0x1;
	 l2[1]  = 0x0;
	 l2[2]  = 0x5e;
	 l2[3]  = l3[1] & 0x7F;
	 l2[4]  = l3[2];
	 l2[5]  = l3[3];
}

int get_mac_address(char *interface)
{
	struct ifreq if_request;
	int lsock;
	int rc;

	lsock = socket(PF_PACKET, SOCK_RAW, htons(0x800));
	if (lsock < 0)
		return -1;

	memset(&if_request, 0, sizeof(if_request));
	strncpy(if_request.ifr_name, interface, sizeof(if_request.ifr_name) - 1);
	rc = ioctl(lsock, SIOCGIFHWADDR, &if_request);
	if (rc < 0) {
		close(lsock);
		return -1;
	}

	memcpy(glob_station_addr, if_request.ifr_hwaddr.sa_data,
	       sizeof(glob_station_addr));
	close(lsock);
	return 0;
}

static void usage(void)
{
	fprintf(stderr, "\n"
		"usage: simple_talker [-h] -i interface-name"
		"\n"
		"options:\n"
		"    -h  show this message\n"
		"    -i  specify interface for AVB connection\n"
		"    -t  transport equal to 2 for 1722 or 3 for RTP\n"
		"\n" "%s" "\n", version_str);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	unsigned i;
	int err;
	device_t atl_dev;
	int atl_shm_fd = -1;
	char *atl_mmap = NULL;
	struct atl_dma_alloc a_page;
	struct atl_packet a_packet;
	struct atl_packet *tmp_packet;
	struct atl_packet *cleaned_packets;
	struct atl_packet *free_packets;
	struct mrp_talker_ctx *ctx = malloc(sizeof(struct mrp_talker_ctx));
	int c;
	u_int64_t last_time;
	int rc = 0;
	char *interface = NULL;
	int transport = -1;
	uint16_t seqnum;
	uint32_t rtp_timestamp;
	uint64_t time_stamp;
	unsigned total_samples = 0;
	gPtpTimeData td;
	int32_t sample_buffer[L4_SAMPLES_PER_FRAME * SRC_CHANNELS];

	seventeen22_header *l2_header0;
	six1883_header *l2_header1;
	six1883_sample *sample;

	IP_RTP_Header *l4_headers;
	IP_PseudoHeader pseudo_hdr;
	unsigned l4_local_address = 0;
	int sd;
	struct sockaddr_in local;
	struct ifreq if_request;

	uint64_t now_local, now_8021as;
	uint64_t update_8021as;
	unsigned delta_8021as, delta_local;
	uint8_t dest_addr[6];
	size_t packet_size;
	struct mrp_domain_attr *class_a = malloc(sizeof(struct mrp_domain_attr));
	struct mrp_domain_attr *class_b = malloc(sizeof(struct mrp_domain_attr));

	for (;;) {
		c = getopt(argc, argv, "hi:t:");
		if (c < 0)
			break;
		switch (c) {
		case 'h':
			usage();
			break;
		case 'i':
			if (interface) {
			printf("only one interface per daemon is supported\n");
				usage();
			}
			interface = strdup(optarg);
			atl_dev.ifname = strdup(optarg);
			break;
		case 't':
			transport = strtoul( optarg, NULL, 10 );
		}
	}
	if (optind < argc)
		usage();
	if (NULL == interface) {
		usage();
	}
	if( transport < 2 || transport > 4 ) {
		fprintf( stderr, "Must specify valid transport\n" );
		usage();
	}

	rc = mrp_talker_client_init(ctx);
	if (rc) {
		printf("MRP talker client initialization failed\n");
		return errno;
	}

	halt_tx_sig = &ctx->halt_tx;

	rc = mrp_connect(ctx);
	if (rc) {
		printf("socket creation failed\n");
		return errno;
	}
	err = pci_connect(&atl_dev);
	if (err) {
		printf("connect failed (%s) - are you running as root?\n",
		       strerror(errno));
		return errno;
	}
	err = atl_init(&atl_dev);
	if (err) {
		printf("init failed (%s) - is the driver really loaded?\n",
		       strerror(errno));
		return errno;
	}
	err = atl_dma_malloc_page(&atl_dev, &a_page);
	if (err) {
		printf("malloc failed (%s) - out of memory?\n",
		       strerror(errno));
		return errno;
	}
	signal(SIGINT, sigint_handler);
	rc = get_mac_address(interface);
	if (rc) {
		printf("failed to open interface\n");
		usage();
	}
	if( transport == 2 ) {
		memcpy( dest_addr, glob_l2_dest_addr, sizeof(dest_addr));
	} else {
		memset( &local, 0, sizeof( local ));
		local.sin_family = PF_INET;
		local.sin_addr.s_addr = htonl( INADDR_ANY );
		local.sin_port = htons(L4_PORT);
		l3_to_l2_multicast( dest_addr, glob_l3_dest_addr );
		memset( &if_request, 0, sizeof( if_request ));
		strncpy(if_request.ifr_name, interface, sizeof(if_request.ifr_name)-1);
		sd = socket( AF_INET, SOCK_DGRAM, 0 );
		if( sd == -1 ) {
			printf( "Failed to open socket: %s\n", strerror( errno ));
			return errno;
		}
		if( bind( sd, (struct sockaddr *) &local, sizeof( local )) != 0 ) {
			printf( "Failed to bind on socket: %s\n", strerror( errno ));
			return errno;
		}
		if( ioctl( sd, SIOCGIFADDR, &if_request ) != 0 ) {
			printf
				( "Failed to get interface address (ioctl) on socket: %s\n",
				  strerror( errno ));
			return errno;
		}
		memcpy
			( &l4_local_address,
			  &(( struct sockaddr_in *)&if_request.ifr_addr)->sin_addr,
			  sizeof( l4_local_address ));

	}

	rc = mrp_get_domain(ctx, class_a, class_b);
	if (rc) {
		printf("failed calling msp_get_domain()\n");
		return EXIT_FAILURE;
	}
	printf("detected domain Class A PRIO=%d VID=%04x...\n",class_a->priority,
	       class_a->vid);

	rc = mrp_register_domain(class_a, ctx);
	if (rc) {
		printf("mrp_register_domain failed\n");
		return EXIT_FAILURE;
	}

	rc = mrp_join_vlan(class_a, ctx);
	if (rc) {
		printf("mrp_join_vlan failed\n");
		return EXIT_FAILURE;
	}

	if( transport == 2 ) {
		atl_set_class_bandwidth
			(&atl_dev, (PKT_SZ + 24) * 8000, 0); // 24 - IPG+PREAMBLE+FCS
	} else {
		atl_set_class_bandwidth
			(&atl_dev, 8000*(sizeof(*l4_headers)+L4_SAMPLES_PER_FRAME*CHANNELS*2 + 24), 0);
	}

	memset(glob_stream_id, 0, sizeof(glob_stream_id));
	memcpy(glob_stream_id, glob_station_addr, sizeof(glob_station_addr));

	if( transport == 2 ) {
		packet_size = PKT_SZ;
	} else {
		packet_size = 18 + sizeof(*l4_headers) +
				(L4_SAMPLES_PER_FRAME * CHANNELS * L4_SAMPLE_SIZE );
 	}

	a_packet.attime = a_packet.flags = 0;
	a_packet.map.paddr = a_page.dma_paddr;
	a_packet.map.mmap_size = a_page.mmap_size;
	a_packet.offset = 0;
	a_packet.vaddr = a_page.dma_vaddr + a_packet.offset;
	a_packet.len = packet_size;
	a_packet.next = NULL;
	free_packets = NULL;
	seqnum = 0;
	rtp_timestamp = 0; /* Should be random start */

	/* divide the dma page into buffers for packets */
	for (i = 1; i < ((a_page.mmap_size) / packet_size); i++) {
		tmp_packet = malloc(sizeof(struct atl_packet));
		if (NULL == tmp_packet) {
			printf("failed to allocate atl_packet memory!\n");
			return errno;
		}
		*tmp_packet = a_packet;
		tmp_packet->offset = (i * packet_size);
		tmp_packet->vaddr += tmp_packet->offset;
		tmp_packet->next = free_packets;
		memset(tmp_packet->vaddr, 0, packet_size);	/* MAC header at least */
		memcpy(tmp_packet->vaddr, dest_addr, sizeof(dest_addr));
		memcpy(tmp_packet->vaddr + 6, glob_station_addr,
		       sizeof(glob_station_addr));

		/* Q-tag */
		((char *)tmp_packet->vaddr)[12] = 0x81;
		((char *)tmp_packet->vaddr)[13] = 0x00;
		((char *)tmp_packet->vaddr)[14] =
		    ((ctx->domain_class_a_priority << 13 | ctx->domain_class_a_vid)) >> 8;
		((char *)tmp_packet->vaddr)[15] =
		    ((ctx->domain_class_a_priority << 13 | ctx->domain_class_a_vid)) & 0xFF;
		if( transport == 2 ) {
			((char *)tmp_packet->vaddr)[16] = 0x22;	/* 1722 eth type */
			((char *)tmp_packet->vaddr)[17] = 0xF0;
		} else {
			((char *)tmp_packet->vaddr)[16] = 0x08;	/* IP eth type */
			((char *)tmp_packet->vaddr)[17] = 0x00;
		}

		if( transport == 2 ) {
			/* 1722 header update + payload */
			l2_header0 =
				(seventeen22_header *) (((char *)tmp_packet->vaddr) + 18);
			l2_header0->cd_indicator = 0;
			l2_header0->subtype = 0;
			l2_header0->sid_valid = 1;
			l2_header0->version = 0;
			l2_header0->reset = 0;
			l2_header0->reserved0 = 0;
			l2_header0->gateway_valid = 0;
			l2_header0->reserved1 = 0;
			l2_header0->timestamp_uncertain = 0;
			memset(&(l2_header0->stream_id), 0, sizeof(l2_header0->stream_id));
			memcpy(&(l2_header0->stream_id), glob_station_addr,
				   sizeof(glob_station_addr));
			l2_header0->length = htons(32);
			l2_header1 = (six1883_header *) (l2_header0 + 1);
			l2_header1->format_tag = 1;
			l2_header1->packet_channel = 0x1F;
			l2_header1->packet_tcode = 0xA;
			l2_header1->app_control = 0x0;
			l2_header1->reserved0 = 0;
			l2_header1->source_id = 0x3F;
			l2_header1->data_block_size = 1;
			l2_header1->fraction_number = 0;
			l2_header1->quadlet_padding_count = 0;
			l2_header1->source_packet_header = 0;
			l2_header1->reserved1 = 0;
			l2_header1->eoh = 0x2;
			l2_header1->format_id = 0x10;
			l2_header1->format_dependent_field = 0x02;
			l2_header1->syt = 0xFFFF;
			tmp_packet->len =
				18 + sizeof(seventeen22_header) + sizeof(six1883_header) +
				(L2_SAMPLES_PER_FRAME * CHANNELS * sizeof(six1883_sample));
		} else {
			pseudo_hdr.source = l4_local_address;
			memcpy
				( &pseudo_hdr.dest, glob_l3_dest_addr, sizeof( pseudo_hdr.dest ));
			pseudo_hdr.zero = 0;
			pseudo_hdr.protocol = 0x11;
			pseudo_hdr.length = htons(packet_size-18-20);

			l4_headers =
				(IP_RTP_Header *) (((char *)tmp_packet->vaddr) + 18);
			l4_headers->version_length = 0x45;
			l4_headers->DSCP_ECN = 0x20;
			l4_headers->ip_length = htons(packet_size-18);
			l4_headers->id = 0;
			l4_headers->fragmentation = 0;
			l4_headers->ttl = 64;
			l4_headers->protocol = 0x11;
			l4_headers->hdr_cksum = 0;
			l4_headers->src = l4_local_address;
			memcpy
				( &l4_headers->dest, glob_l3_dest_addr, sizeof( l4_headers->dest ));
			{
				struct iovec iv0;
				iv0.iov_base = l4_headers;
				iv0.iov_len = 20;
				l4_headers->hdr_cksum =
					inet_checksum_sg( &iv0, 1 );
			}

			l4_headers->source_port = htons(L4_PORT);
			l4_headers->dest_port = htons(L4_PORT);
			l4_headers->udp_length = htons(packet_size-18-20);

			l4_headers->version_cc = 2;
			l4_headers->mark_payload = L16_PAYLOAD_TYPE;
			l4_headers->sequence = 0;
			l4_headers->timestamp = 0;
			l4_headers->ssrc = 0;

			l4_headers->tag[0] = 0xBE;
			l4_headers->tag[1] = 0xDE;
			l4_headers->total_length = htons(2);
			l4_headers->tag_length = (6 << 4) | ID_B_HDR_EXT_ID;

			tmp_packet->len =
				18 + sizeof(*l4_headers) +
				(L4_SAMPLES_PER_FRAME * CHANNELS * L4_SAMPLE_SIZE );

		}
		free_packets = tmp_packet;
	}

	/*
	 * subtract 16 bytes for the MAC header/Q-tag - pktsz is limited to the
	 * data payload of the ethernet frame.
	 *
	 * IPG is scaled to the Class (A) observation interval of packets per 125 usec.
	 */
	fprintf(stderr, "advertising stream ...\n");
	if( transport == 2 ) {
		rc = mrp_advertise_stream(glob_stream_id, dest_addr,
					PKT_SZ - 16,
					L2_PACKET_IPG / 125000,
					3900,ctx);
	} else {
		/*
		 * 1 is the wrong number for frame rate, but fractional values
		 * not allowed, not sure the significance of the value 6, but
		 * using it consistently
		 */
		rc = mrp_advertise_stream(glob_stream_id, dest_addr,
					sizeof(*l4_headers) + L4_SAMPLES_PER_FRAME * CHANNELS * 2 + 6,
					1,
					3900, ctx);
	}
	if (rc) {
		printf("mrp_advertise_stream failed\n");
		return EXIT_FAILURE;
	}

	fprintf(stderr, "awaiting a listener ...\n");
	rc = mrp_await_listener(glob_stream_id, ctx);
	if (rc) {
		printf("mrp_await_listener failed\n");
		return EXIT_FAILURE;
	}
	ctx->listeners = 1;
	printf("got a listener ...\n");
	ctx->halt_tx = 0;

	if(-1 == gptpinit(&atl_shm_fd, &atl_mmap)) {
		fprintf(stderr, "GPTP init failed.\n");
		return EXIT_FAILURE;
	}

	if (-1 == gptpscaling(atl_mmap, &td)) {
		fprintf(stderr, "GPTP scaling failed.\n");
		return EXIT_FAILURE;
	}

	if(atl_get_wallclock( &atl_dev, &now_local, NULL ) > 0) {
		fprintf( stderr, "Failed to get wallclock time\n" );
		return EXIT_FAILURE;
	}
	update_8021as = td.local_time - td.ml_phoffset;
	delta_local = (unsigned)(now_local - td.local_time);
	delta_8021as = (unsigned)(td.ml_freqoffset * delta_local);
	now_8021as = update_8021as + delta_8021as;

	last_time = now_local + XMIT_DELAY;
	time_stamp = now_8021as + RENDER_DELAY;

	rc = nice(-20);

    struct atl_packet *send_packets = NULL, *last_attached = NULL;

    tmp_packet = free_packets;
	while (ctx->listeners && !ctx->halt_tx) {
		if (NULL == tmp_packet)
			goto cleanup;

		free_packets = tmp_packet->next;
		tmp_packet->next = NULL;

		if( transport == 2 ) {
			uint32_t timestamp_l;
			get_samples( L2_SAMPLES_PER_FRAME, sample_buffer );
			l2_header0 =
				(seventeen22_header *) (((char *)tmp_packet->vaddr) + 18);
			l2_header1 = (six1883_header *) (l2_header0 + 1);

			/* unfortuntely unless this thread is at rtprio
			 * you get pre-empted between fetching the time
			 * and programming the packet and get a late packet
			 */
			tmp_packet->attime = last_time + L2_PACKET_IPG;
			last_time += L2_PACKET_IPG;

			l2_header0->seq_number = seqnum++;
			if (seqnum % 4 == 0)
				l2_header0->timestamp_valid = 0;

			else
				l2_header0->timestamp_valid = 1;

			timestamp_l = time_stamp;
			l2_header0->timestamp = htonl(timestamp_l);
			time_stamp += L2_PACKET_IPG;
			l2_header1->data_block_continuity = total_samples;
			total_samples += L2_SAMPLES_PER_FRAME*CHANNELS;
			sample =
				(six1883_sample *) (((char *)tmp_packet->vaddr) +
									(18 + sizeof(seventeen22_header) +
									 sizeof(six1883_header)));

			for (i = 0; i < L2_SAMPLES_PER_FRAME * CHANNELS; ++i) {
				uint32_t tmp = htonl(sample_buffer[i]);
				sample[i].label = 0x40;
				memcpy(&(sample[i].value), &(tmp),
					   sizeof(sample[i].value));
			}
		} else {
			uint16_t *l16_sample;
			uint8_t *tmp;
			get_samples( L4_SAMPLES_PER_FRAME, sample_buffer );

			l4_headers =
				(IP_RTP_Header *) (((char *)tmp_packet->vaddr) + 18);

			l4_headers->sequence =  seqnum++;
			l4_headers->timestamp = rtp_timestamp;

			tmp_packet->attime = last_time + L4_PACKET_IPG;
			last_time += L4_PACKET_IPG;

			l4_headers->nanoseconds = time_stamp/1000000000;
			tmp = (uint8_t *) &l4_headers->nanoseconds;
			l4_headers->seconds[0] = tmp[2];
			l4_headers->seconds[1] = tmp[1];
			l4_headers->seconds[2] = tmp[0];
			{
				uint64_t tmp;
				tmp  = time_stamp % 1000000000;
				tmp *= RTP_SUBNS_SCALE_NUM;
				tmp /= RTP_SUBNS_SCALE_DEN;
				l4_headers->nanoseconds = (uint32_t) tmp;
			}
			l4_headers->nanoseconds = htons(l4_headers->nanoseconds);


			time_stamp += L4_PACKET_IPG;

			l16_sample = (uint16_t *) (l4_headers+1);

			for (i = 0; i < L4_SAMPLES_PER_FRAME * CHANNELS; ++i) {
				uint16_t tmp = sample_buffer[i]/65536;
				l16_sample[i] = htons(tmp);
			}
			l4_headers->cksum = 0;
			{
				struct iovec iv[2];
				iv[0].iov_base = &pseudo_hdr;
				iv[0].iov_len = sizeof(pseudo_hdr);
				iv[1].iov_base = ((uint8_t *)l4_headers) + 20;
				iv[1].iov_len = packet_size-18-20;
				l4_headers->cksum =
					inet_checksum_sg( iv, 2 );
			}
		}

        if( send_packets == NULL ) {
			send_packets = tmp_packet;
		}

		if( last_attached ) {
			last_attached->next = tmp_packet;
		}

		last_attached = tmp_packet;
		tmp_packet = free_packets;
	cleanup:
	    err = atl_xmit(&atl_dev, 0, &send_packets);
		if (ENOSPC == err) {
			/* put back for now */
			tmp_packet->next = free_packets;
			free_packets = tmp_packet;
		}

	    cleaned_packets = NULL;
		atl_clean(&atl_dev, &cleaned_packets);

		if( free_packets ) {
			tmp_packet = free_packets;
			while (tmp_packet->next) {
				tmp_packet = tmp_packet->next;
			}
			tmp_packet->next = cleaned_packets;
		}
		else {
			free_packets = cleaned_packets;
		}

		send_packets = NULL;
		last_attached = NULL;
	}
	rc = nice(0);
	atl_stop_tx(&atl_dev, 0, &cleaned_packets);
	if (ctx->halt_tx == 0)
		printf("listener left ...\n");
	ctx->halt_tx = 1;
	if( transport == 2 ) {
		rc = mrp_unadvertise_stream
			(glob_stream_id, dest_addr, PKT_SZ - 16, L2_PACKET_IPG / 125000,
			 3900, ctx);
	} else {
		rc = mrp_unadvertise_stream
			(glob_stream_id, dest_addr,
			 sizeof(*l4_headers)+L4_SAMPLES_PER_FRAME*CHANNELS*2 + 6, 1,
			 3900, ctx);
	}
	if (rc)
		printf("mrp_unadvertise_stream failed\n");

	atl_set_class_bandwidth(&atl_dev, 0, 0);	/* disable Qav */

	rc = mrp_disconnect(ctx);
	if (rc)
		printf("mrp_disconnect failed\n");
	free(ctx);
	free(class_a);
	free(class_b);
	atl_dma_free_page(&atl_dev, &a_page);
	rc = gptpdeinit(&atl_shm_fd, &atl_mmap);
	err = atl_detach(&atl_dev);

	pthread_exit(NULL);

	return EXIT_SUCCESS;
}
