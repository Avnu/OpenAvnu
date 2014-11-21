/******************************************************************************

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

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <pci/pci.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <netpacket/packet.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <sys/un.h>
#include <pthread.h>
#include <poll.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "igb.h"
#include "talker_mrp_client.h"
#include <math.h>
#include <endian.h>
#include <stdint.h>

#include <jack/weakmacros.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <jack/thread.h>
#include "jack.h"
#include "defines.h"

typedef struct { 
  int64_t ml_phoffset;
  int64_t ls_phoffset;
  long double ml_freqoffset;
  long double ls_freqoffset;
  int64_t local_time;
} gPtpTimeData;


#define SHM_SIZE 4*8 + sizeof(pthread_mutex_t) /* 3 - 64 bit and 2 - 32 bits */
#define SHM_NAME  "/ptp"

#define MAX_SAMPLE_VALUE ((1U << ((sizeof(int32_t)*8)-1))-1)

#define SRC_CHANNELS (2)
#define SAMPLES_PER_SECOND (48000)
#define FREQUENCY (480)
#define SAMPLES_PER_CYCLE (SAMPLES_PER_SECOND/FREQUENCY)
#define GAIN (.5)

//1722 header
#define ETH_TYPE 0x22F0

#define CD_SUBTYPE 0x02		/* for simple audio format */
#define SV_VER_MR_RS_GV_TV 0x81
#define RS_TU	0x00
#define SAMPLE_FORMAT_NON_INTR_FLOAT 0x02
#define NOMINAL_SAMPLE_RATE 0x09
#define LINEAR_SAMPLE_MSB 0x20
unsigned char GATEWAY_INFO[] =
    { SAMPLE_FORMAT_NON_INTR_FLOAT, 0, NOMINAL_SAMPLE_RATE, LINEAR_SAMPLE_MSB };

#define PAYLOAD_SIZE SAMPLE_SIZE*SAMPLES_PER_FRAME*CHANNELS	/* 6*4 * 2 channels  = 48 bytes */

#define IGB_BIND_NAMESZ 24

#define XMIT_DELAY (200000000)	/* us */
#define RENDER_DELAY (XMIT_DELAY+2000000)	/* us */
typedef enum { false = 0, true = 1 } bool;
typedef struct __attribute__ ((packed)) {
	uint64_t subtype:7;
	uint64_t cd_indicator:1;
	uint64_t timestamp_valid:1;
	uint64_t gateway_valid:1;
	uint64_t reserved0:1;
	uint64_t reset:1;
	uint64_t version:3;
	uint64_t sid_valid:1;
	uint64_t seq_number:8;
	uint64_t timestamp_uncertain:1;
	uint64_t reserved1:7;
	uint64_t stream_id;
	uint64_t timestamp:32;
	uint64_t gateway_info:32;
	uint64_t length:16;
} seventeen22_header;

/* 61883 CIP with SYT Field */
typedef struct {
	uint16_t packet_channel:6;
	uint16_t format_tag:2;
	uint16_t app_control:4;
	uint16_t packet_tcode:4;
	uint16_t source_id:6;
	uint16_t reserved0:2;
	uint16_t data_block_size:8;
	uint16_t reserved1:2;
	uint16_t source_packet_header:1;
	uint16_t quadlet_padding_count:3;
	uint16_t fraction_number:2;
	uint16_t data_block_continuity:8;
	uint16_t format_id:6;
	uint16_t eoh:2;
	uint16_t format_dependent_field:8;
	uint16_t syt;
} six1883_header;

typedef struct {
	uint8_t label;
	uint8_t value[3];
} six1883_sample;

/* global variables */
device_t igb_dev;
volatile int unleash_jack = 0;
pthread_t packetizer_id;

/* evil global variables
 * FIXME: move to thread argument struct */
u_int64_t last_time;
int seqnum;
uint32_t time_stamp;
seventeen22_header *header0;
six1883_header *header1;
struct igb_packet *tmp_packet;
struct igb_packet *free_packets;


#define VERSION_STR	"1.0"
static const char *version_str = "simple_talker v" VERSION_STR "\n"
    "Copyright (c) 2012, Intel Corporation\n";

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

static int shm_fd = -1;
static char *memory_offset_buffer = NULL;
int gptpinit(void)
{
	shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
	if (shm_fd == -1) {
		perror("shm_open()");
		return false;
	}
	memory_offset_buffer =
	    (char *)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
			 shm_fd, 0);
	if (memory_offset_buffer == (char *)-1) {
		perror("mmap()");
		memory_offset_buffer = NULL;
		shm_unlink(SHM_NAME);
		return false;
	}
	return true;
}

void gptpdeinit(void)
{
	if (memory_offset_buffer != NULL) {
		munmap(memory_offset_buffer, SHM_SIZE);
	}
	if (shm_fd != -1) {
		close(shm_fd);
	}
}

int gptpscaling(gPtpTimeData * td)
{
	pthread_mutex_lock((pthread_mutex_t *) memory_offset_buffer);
	memcpy(td, memory_offset_buffer + sizeof(pthread_mutex_t), sizeof(*td));
	pthread_mutex_unlock((pthread_mutex_t *) memory_offset_buffer);

	fprintf(stderr, "ml_phoffset = %lld, ls_phoffset = %lld\n",
		td->ml_phoffset, td->ls_phoffset);
	fprintf(stderr, "ml_freqffset = %Lf, ls_freqoffset = %Lf\n",
		td->ml_freqoffset, td->ls_freqoffset);

	return true;
}

void sigint_handler(int signum)
{
	printf("got SIGINT\n");
	halt_tx = signum;
	unleash_jack = 0;
}

int pci_connect()
{
	struct pci_access *pacc;
	struct pci_dev *dev;
	int err;
	char devpath[IGB_BIND_NAMESZ];
	memset(&igb_dev, 0, sizeof(device_t));
	pacc = pci_alloc();
	pci_init(pacc);
	pci_scan_bus(pacc);
	for (dev = pacc->devices; dev; dev = dev->next) {
		pci_fill_info(dev,
			      PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);
		igb_dev.pci_vendor_id = dev->vendor_id;
		igb_dev.pci_device_id = dev->device_id;
		igb_dev.domain = dev->domain;
		igb_dev.bus = dev->bus;
		igb_dev.dev = dev->dev;
		igb_dev.func = dev->func;
		snprintf(devpath, IGB_BIND_NAMESZ, "%04x:%02x:%02x.%d",
			 dev->domain, dev->bus, dev->dev, dev->func);
		err = igb_probe(&igb_dev);
		if (err) {
			continue;
		}
		printf("attaching to %s\n", devpath);
		err = igb_attach(devpath, &igb_dev);
		if (err) {
			printf("attach failed! (%s)\n", strerror(errno));
			continue;
		}
		goto out;
	}
	pci_cleanup(pacc);
	return ENXIO;
 out:	pci_cleanup(pacc);
	return 0;
}

unsigned char STATION_ADDR[] = { 0, 0, 0, 0, 0, 0 };
unsigned char STREAM_ID[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* IEEE 1722 reserved address */
unsigned char DEST_ADDR[] = { 0x91, 0xE0, 0xF0, 0x00, 0x0e, 0x80 };

int get_mac_address(char *interface)
{
	struct ifreq if_request;
	int lsock;
	int rc;
	lsock = socket(PF_PACKET, SOCK_RAW, htons(0x800));
	if (lsock < 0)
		return -1;
	memset(&if_request, 0, sizeof(if_request));
	strncpy(if_request.ifr_name, interface, sizeof(if_request.ifr_name));
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

static void usage(void)
{
	fprintf(stderr, "\n"
		"usage: simple_talker [-h] -i interface-name"
		"\n"
		"options:\n"
		"    -h  show this message\n"
		"    -i  specify interface for AVB connection\n"
		"\n" "%s" "\n", version_str);
	exit(1);
}

#define PACKET_IPG	(125000)	/* (1) packet every 125 usec */



static void* packetizer_thread(void *arg) {
	struct igb_packet *cleaned_packets;
	six1883_sample *sample;
	unsigned total_samples = 0;
	int err;
	int i;
	(void) arg; /* unused */

	const size_t bytes_to_read = CHANNELS * SAMPLES_PER_FRAME *
		SAMPLE_SIZE;
	jack_default_audio_sample_t* framebuf = malloc (bytes_to_read);
	extern jack_ringbuffer_t* ringbuffer;
	extern pthread_mutex_t threadLock;
	extern pthread_cond_t dataReady;

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_mutex_lock(&threadLock);

	while (listeners && !halt_tx) {
		pthread_cond_wait(&dataReady, &threadLock);

		while ((jack_ringbuffer_read_space(ringbuffer) >= bytes_to_read)) {

			tmp_packet = free_packets;
			if (NULL == tmp_packet)
				goto cleanup;
			header0 =
				(seventeen22_header *) (((char *)tmp_packet->vaddr) + 18);
			header1 = (six1883_header *) (header0 + 1);
			free_packets = tmp_packet->next;

			/* unfortuntely unless this thread is at rtprio
			 * you get pre-empted between fetching the time
			 * and programming the packet and get a late packet
			 */
			tmp_packet->attime = last_time + PACKET_IPG;
			last_time += PACKET_IPG;

			jack_ringbuffer_read (ringbuffer, (char*)&framebuf[0], bytes_to_read);

			header0->seq_number = seqnum++;
			if (seqnum % 4 == 0)
				header0->timestamp_valid = 0;

			else
				header0->timestamp_valid = 1;

			time_stamp = htonl(time_stamp);
			header0->timestamp = time_stamp;
			time_stamp = ntohl(time_stamp);
			time_stamp += PACKET_IPG;
			header1->data_block_continuity = total_samples;
			total_samples += SAMPLES_PER_FRAME*CHANNELS;
			sample =
				(six1883_sample *) (((char *)tmp_packet->vaddr) +
						(18 + sizeof(seventeen22_header) +
						 sizeof(six1883_header)));

			for (i = 0; i < SAMPLES_PER_FRAME * CHANNELS; ++i) {
				uint32_t tmp = htonl(MAX_SAMPLE_VALUE * framebuf[i]);
				sample[i].label = 0x40;
				memcpy(&(sample[i].value), &(tmp),
						sizeof(sample[i].value));
			}

			err = igb_xmit(&igb_dev, 0, tmp_packet);

			if (!err) {
				continue;
			}

			if (ENOSPC == err) {

				/* put back for now */
				tmp_packet->next = free_packets;
				free_packets = tmp_packet;
			}

cleanup:		igb_clean(&igb_dev, &cleaned_packets);
			i = 0;
			while (cleaned_packets) {
			    i++;
			    tmp_packet = cleaned_packets;
			    cleaned_packets = cleaned_packets->next;
			    tmp_packet->next = free_packets;
			    free_packets = tmp_packet;
			}
		}
	}
	return NULL;
}


static void run_packetizer(void)
{
	jack_acquire_real_time_scheduling(packetizer_id, 70);
	unleash_jack = 1;
	pthread_join (packetizer_id, NULL);
}

int main(int argc, char *argv[])
{
	unsigned i;
	int err;
	struct igb_dma_alloc a_page;
	struct igb_packet a_packet;
	int c;
	int rc = 0;
	char *interface = NULL;
	gPtpTimeData td;
	uint64_t now_local, now_8021as;
	uint64_t update_8021as;
	unsigned delta_8021as, delta_local;
	jack_client_t* _jackclient;

	for (;;) {
		c = getopt(argc, argv, "hi:");
		if (c < 0)
			break;
		switch (c) {
		case 'h':
			usage();
			break;
		case 'i':
			if (interface) {
				printf
				    ("only one interface per daemon is supported\n");
				usage();
			}
			interface = strdup(optarg);
			break;
		}
	}
	if (optind < argc)
		usage();
	if (NULL == interface) {
		usage();
	}
	rc = mrp_connect();
	if (rc) {
		printf("socket creation failed\n");
		return errno;
	}
	err = pci_connect();
	if (err) {
		printf("connect failed (%s) - are you running as root?\n",
		       strerror(errno));
		return errno;
	}
	err = igb_init(&igb_dev);
	if (err) {
		printf("init failed (%s) - is the driver really loaded?\n",
		       strerror(errno));
		return errno;
	}
	err = igb_dma_malloc_page(&igb_dev, &a_page);
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

	mrp_monitor();

	/* 
	 * should use mrp_get_domain() above but this is a simplification 
	 */

	domain_a_valid = 1;
	domain_class_a_id = MSRP_SR_CLASS_A;
	domain_class_a_priority = MSRP_SR_CLASS_A_PRIO;
	domain_class_a_vid = 2;
	printf("detected domain Class A PRIO=%d VID=%04x...\n", domain_class_a_priority,
	       domain_class_a_vid);

#define PKT_SZ	100

	mrp_register_domain(&domain_class_a_id, &domain_class_a_priority, &domain_class_a_vid);
	igb_set_class_bandwidth(&igb_dev, PACKET_IPG / 125000, 0, PKT_SZ - 22,
				0);

	memset(STREAM_ID, 0, sizeof(STREAM_ID));
	memcpy(STREAM_ID, STATION_ADDR, sizeof(STATION_ADDR));

	a_packet.dmatime = a_packet.attime = a_packet.flags = 0;
	a_packet.map.paddr = a_page.dma_paddr;
	a_packet.map.mmap_size = a_page.mmap_size;
	a_packet.offset = 0;
	a_packet.vaddr = a_page.dma_vaddr + a_packet.offset;
	a_packet.len = PKT_SZ;
	free_packets = NULL;
	seqnum = 0;

	/* divide the dma page into buffers for packets */
	for (i = 1; i < ((a_page.mmap_size) / PKT_SZ); i++) {
		tmp_packet = malloc(sizeof(struct igb_packet));
		if (NULL == tmp_packet) {
			printf("failed to allocate igb_packet memory!\n");
			return errno;
		}
		*tmp_packet = a_packet;
		tmp_packet->offset = (i * PKT_SZ);
		tmp_packet->vaddr += tmp_packet->offset;
		tmp_packet->next = free_packets;
		memset(tmp_packet->vaddr, 0, PKT_SZ);	/* MAC header at least */
		memcpy(tmp_packet->vaddr, DEST_ADDR, sizeof(DEST_ADDR));
		memcpy(tmp_packet->vaddr + 6, STATION_ADDR,
		       sizeof(STATION_ADDR));

		/* Q-tag */
		((char *)tmp_packet->vaddr)[12] = 0x81;
		((char *)tmp_packet->vaddr)[13] = 0x00;
		((char *)tmp_packet->vaddr)[14] =
		    ((domain_class_a_priority << 13 | domain_class_a_vid)) >> 8;
		((char *)tmp_packet->vaddr)[15] =
		    ((domain_class_a_priority << 13 | domain_class_a_vid)) & 0xFF;
		((char *)tmp_packet->vaddr)[16] = 0x22;	/* 1722 eth type */
		((char *)tmp_packet->vaddr)[17] = 0xF0;

		/* 1722 header update + payload */
		header0 =
		    (seventeen22_header *) (((char *)tmp_packet->vaddr) + 18);
		header0->cd_indicator = 0;
		header0->subtype = 0;
		header0->sid_valid = 1;
		header0->version = 0;
		header0->reset = 0;
		header0->reserved0 = 0;
		header0->gateway_valid = 0;
		header0->reserved1 = 0;
		header0->timestamp_uncertain = 0;
		memset(&(header0->stream_id), 0, sizeof(header0->stream_id));
		memcpy(&(header0->stream_id), STATION_ADDR,
		       sizeof(STATION_ADDR));
		header0->length = htons(32);
		header1 = (six1883_header *) (header0 + 1);
		header1->format_tag = 1;
		header1->packet_channel = 0x1F;
		header1->packet_tcode = 0xA;
		header1->app_control = 0x0;
		header1->reserved0 = 0;
		header1->source_id = 0x3F;
		header1->data_block_size = 1;
		header1->fraction_number = 0;
		header1->quadlet_padding_count = 0;
		header1->source_packet_header = 0;
		header1->reserved1 = 0;
		header1->eoh = 0x2;
		header1->format_id = 0x10;
		header1->format_dependent_field = 0x02;
		header1->syt = 0xFFFF;
		tmp_packet->len =
		    18 + sizeof(seventeen22_header) + sizeof(six1883_header) +
		    (SAMPLES_PER_FRAME * CHANNELS * sizeof(six1883_sample));
		free_packets = tmp_packet;
	}

	/* 
	 * subtract 16 bytes for the MAC header/Q-tag - pktsz is limited to the 
	 * data payload of the ethernet frame .
	 *
	 * IPG is scaled to the Class (A) observation interval of packets per 125 usec
	 */

	_jackclient = init_jack();

	fprintf(stderr, "advertising stream ...\n");
	mrp_advertise_stream(STREAM_ID, DEST_ADDR, domain_class_a_vid, PKT_SZ - 16,
			     PACKET_IPG / 125000, domain_class_a_priority, 3900);
	fprintf(stderr, "awaiting a listener ...\n");
	mrp_await_listener(STREAM_ID);
	printf("got a listener ...\n");
	halt_tx = 0;

	gptpinit();
	gptpscaling(&td);

	if( igb_get_wallclock( &igb_dev, &now_local, NULL ) != 0 ) {
	  fprintf( stderr, "Failed to get wallclock time\n" );
	  return -1;
	}
	update_8021as = td.local_time - td.ml_phoffset;
	delta_local = (unsigned)(now_local - td.local_time);
	delta_8021as = (unsigned)(td.ml_freqoffset * delta_local);
	now_8021as = update_8021as + delta_8021as;

	last_time = now_local + XMIT_DELAY;
	time_stamp = now_8021as + RENDER_DELAY;

	rc = nice(-20);

	pthread_create (&packetizer_id, NULL, packetizer_thread, NULL);
	run_packetizer();

	rc = nice(0);

	stop_jack(_jackclient);

	if (halt_tx == 0)
		printf("listener left ...\n");
	halt_tx = 1;

	mrp_unadvertise_stream(STREAM_ID, DEST_ADDR, domain_class_a_vid, PKT_SZ - 16,
			       PACKET_IPG / 125000, domain_class_a_priority, 3900);

	igb_set_class_bandwidth(&igb_dev, 0, 0, 0, 0);	/* disable Qav */

	rc = mrp_disconnect();

	igb_dma_free_page(&igb_dev, &a_page);

	err = igb_detach(&igb_dev);

	pthread_exit(NULL);

	return 0;
}
