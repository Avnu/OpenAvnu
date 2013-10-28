/*
 * Avbsink plugin: Transmits audio/video data over network using AVB.
 *
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

//Example:
/* gst-launch-1.0 filesrc location=test.mp4 blocksize=1024 ! avbsink interface=eth0 */



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


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

#include "gstavbsink.h"
#include <../../../common/avb.h>

/* Declare Macros */

#define STREAMID 0xABCDEF
#define DEFAULT_INTERFACE "eth0"
#define PACKET_IPG		125000	/* (1) packet every 125 usec */

/* Global Variables */
volatile int halt_tx = 0;
volatile int listeners = 1;
int flag_close_thread = 0;
int control_socket = -1;
int g_start_feeding = 1;
int g_start_feed_socket = 0;
unsigned int avb_init;
device_t igb_dev;
uint32_t payload_len;
unsigned char STATION_ADDR[] = { 0, 0, 0, 0, 0, 0 };
unsigned char STREAM_ID[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* IEEE 1722 reserved address */
unsigned char DEST_ADDR[] = { 0x91, 0xE0, 0xF0, 0x00, 0x0E, 0x80 };
struct igb_dma_alloc a_page;
struct igb_packet *tmp_packet;
struct igb_packet *cleaned_packets;
struct igb_packet *free_packets;
struct sched_param sched;
six1883_header *h61883;
seventeen22_header *h1722;
int seqnum;
unsigned total_samples = 0;
uint32_t pkt_sz;
int32_t read_bytes;
int64_t total_read_bytes;
uint8_t *data_ptr;
void *stream_packet;
int frame_size;
long long int frame_sequence = 0;
char *interface1;
GST_DEBUG_CATEGORY_STATIC (avbsink_debug);

#define GST_CAT_DEFAULT (avbsink_debug)

static GstStaticPadTemplate sink_template =
				GST_STATIC_PAD_TEMPLATE ("sink",
				GST_PAD_SINK,
				GST_PAD_ALWAYS,
				GST_STATIC_CAPS_ANY);

enum
{
	PROP_0 = 0,
	PROP_INTERFACE
};


static void
gst_avbsink_set_property (GObject * object, guint prop_id,
			  const GValue * value, GParamSpec * pspec);

static void
gst_avbsink_get_property (GObject * object, guint prop_id,
			  GValue * value, GParamSpec * pspec);
static GstFlowReturn
gst_avbsink_render (GstBaseSink * sink,
		    GstBuffer * buff);

#define gst_avb_parent_class parent_class
G_DEFINE_TYPE (GstAvbSink, gst_avbsink, GST_TYPE_BASE_SINK);

static void
gst_avbsink_class_init (GstAvbSinkClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	GstBaseSinkClass *gstbasesink_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;
	gstbasesink_class = (GstBaseSinkClass *) klass;
	gobject_class->set_property = gst_avbsink_set_property;
	gobject_class->get_property = gst_avbsink_get_property;
	g_object_class_install_property (gobject_class, PROP_INTERFACE,
	g_param_spec_string ("interface", "Interface","Ethernet AVB Interface",
			     interface1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	gst_element_class_add_pad_template (gstelement_class,
	gst_static_pad_template_get (&sink_template));
	gst_element_class_set_static_metadata (gstelement_class, "AVB packet sender",
						"Sink/Network",
						"Send data over the network via AVB",
						"Symphony-Teleca");
	gstbasesink_class->render = gst_avbsink_render;
	GST_DEBUG_CATEGORY_INIT (avbsink_debug, "avbsink", 0, "AVB sink");
}

static void gst_avbsink_init (GstAvbSink * sink)
{
	sink->interface = interface1;
}

static void
gst_avbsink_set_property (GObject * object, guint prop_id,
			  const GValue * value, GParamSpec * pspec)
{
	switch (prop_id)
	{
		case PROP_INTERFACE:
			g_free(interface1);
			if (g_value_get_string (value) == NULL)
				interface1 = g_strdup (DEFAULT_INTERFACE);
			else
				interface1 = g_value_dup_string (value);
			break;
		default:
			break;
	}
}

static void 
gst_avbsink_get_property (GObject * object, guint prop_id, GValue * value,
			  GParamSpec * pspec)
{
	switch (prop_id)
	{
		case PROP_INTERFACE:
			g_value_set_string (value, interface1);
			break; 
		default:
			break;
	}
}

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

int start_feed_socket_init()
{
	int sock, length, rc, on = 1, flags ;
	struct sockaddr_in server;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("Opening socket");
		return -1;
	}

	g_start_feed_socket = sock;

	/* Allow socket descriptor to be reuseable */
	rc = setsockopt(sock, SOL_SOCKET,  SO_REUSEADDR,
			(char *)&on, sizeof(on));
	if (rc < 0) {
		perror("setsockopt() failed");
		close(sock);
		exit(-1);
	}

	/*
	 * Set socket to be non-blocking.  All of the sockets for
	 * the incoming connections will also be non-blocking since
	 * they will inherit that state from the listening socket.
	 */
	flags = fcntl(sock, F_GETFL, 0);
	rc = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
	if (rc < 0) {
	      perror("ioctl() failed");
	      close(sock);
	      exit(-1);
	}

	length = sizeof(server);
	bzero(&server,length);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr=INADDR_ANY;
	server.sin_port=htons(7600);
	if (bind(sock,(struct sockaddr *)&server,length) < 0) {
	      perror("binding");
	      exit(-1);
	}

	return 0;
}

void * read_start_feed(void *arg)
{
	  int n;
	  char buf[1024];
	  socklen_t fromlen;
	  struct sockaddr_in from;

	  fromlen = sizeof(struct sockaddr_in);

	  while (1) {
		  if (flag_close_thread == 1)
			return NULL;

		  n = recvfrom(g_start_feed_socket, buf, 1024,
			       0 ,(struct sockaddr *)&from, &fromlen);
		if (n > 0) {
			sscanf(buf, "%d", &g_start_feeding);
			printf("\nTransmitter status: %d", g_start_feeding);
			if (g_start_feed_socket == 2) {
				listeners = 0;
				halt_tx = 1;
				/* disable Qav */
				igb_set_class_bandwidth(&igb_dev, 0, 0, 0, 0);
				pthread_exit(NULL);
				exit(0);
			}

		  }
	    }
	    return NULL;
}



void sigint_handler(int signum)
{
	printf("got SIGINT\n");
	halt_tx = signum;
	flag_close_thread = 1;
	exit(0);
}

void igb_cleanup()
{ 
	int err;

	igb_set_class_bandwidth(&igb_dev,0,0,0,0);
	igb_dma_free_page(&igb_dev,&a_page);
	err=igb_detach(&igb_dev);
	exit(0);
}

int get_mac_addr(int8_t *interface)
{
	  int lsock = socket(PF_PACKET, SOCK_RAW, htons(0x800));
	  struct ifreq if_request;
	  int err;

	  if (lsock < 0)
		return -1;

	  memset(&if_request, 0, sizeof(if_request));
	  strncpy(if_request.ifr_name, (const char *)interface, sizeof(if_request.ifr_name));
	  err = ioctl(lsock, SIOCGIFHWADDR, &if_request);
	  if (err < 0) {
	      close(lsock);
		    return -1;
	  }
	  memcpy(STATION_ADDR, if_request.ifr_hwaddr.sa_data,
	      sizeof(STATION_ADDR));
	  close(lsock);

	  return 0;
}

static GstFlowReturn
gst_avbsink_render (GstBaseSink * bsink, GstBuffer * buff)
{
	
	GstMapInfo info;
	unsigned i;
	int err;

	if(!avb_init)
	{
		struct igb_packet a_packet;
		pthread_t tid;

		avb_init = 1;
		pkt_sz = 1024;
		payload_len = 1024;
		pkt_sz += sizeof(six1883_header) + sizeof(seventeen22_header) + sizeof(eth_header);
		if (pkt_sz > 1500) {
			fprintf(stderr,"payload_len is > MAX_ETH_PACKET_LEN - not supported.\n");
			return -EINVAL;
		}
		err = pci_connect(&igb_dev);
		if (err) {
			printf("connect failed (%s) - are you running as root?\n", strerror(errno));
			return (errno);
		}
		
		err = igb_init(&igb_dev);
		if (err) {
			printf("init failed (%s) - is the driver really loaded?\n", strerror(errno));
			return (errno);
		}
		err = igb_dma_malloc_page(&igb_dev, &a_page);
		if (err) {
			printf("malloc failed (%s) - out of memory?\n", strerror(errno));
			return (errno);
		}
		signal(SIGINT, sigint_handler);
		err = get_mac_addr(interface1);
		if (err) {
				printf("failed to open interface(%s)\n",interface1);
		}
		igb_set_class_bandwidth(&igb_dev, PACKET_IPG / 125000, 0, pkt_sz - 22, 0);
		memset(STREAM_ID, 0, sizeof(STREAM_ID));
		memcpy(STREAM_ID, STATION_ADDR, sizeof(STATION_ADDR));
		a_packet.dmatime = a_packet.attime = a_packet.flags = 0;
		a_packet.map.paddr = a_page.dma_paddr;
		a_packet.map.mmap_size = a_page.mmap_size;
		a_packet.offset = 0;
		a_packet.vaddr = a_page.dma_vaddr + a_packet.offset;
		a_packet.len = pkt_sz;
		free_packets = NULL;
		seqnum = 0;
		frame_size = payload_len + sizeof(six1883_header) + sizeof(seventeen22_header) + sizeof(eth_header);
		stream_packet = avb_create_packet(payload_len);
		h1722 = (seventeen22_header *)((uint8_t*)stream_packet + sizeof(eth_header));
		h61883 = (six1883_header *)((uint8_t*)stream_packet + sizeof(eth_header) +
						sizeof(seventeen22_header));

		/*initialize h1722 header */
		avb_initialize_h1722_to_defaults(h1722);
		/* set the length */
		avb_set_1722_length(h1722, htons(payload_len + sizeof(six1883_header)));
		avb_set_1722_stream_id(h1722,reverse_64(STREAMID));
		avb_set_1722_sid_valid(h1722, 0x1);


		/*initialize h61883 header */
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

		/* initialize the source & destination mac address */
		avb_eth_header_set_mac(stream_packet, DEST_ADDR, interface1);

		/* set 1772 eth type */
		avb_1722_set_eth_type(stream_packet);

		/* divide the dma page into buffers for packets */
		for (i = 1; i < ((a_page.mmap_size) / pkt_sz); i++) {
			tmp_packet = malloc(sizeof(struct igb_packet));
			if (!tmp_packet) {
				printf("failed to allocate igb_packet memory!\n");
				return (errno);
			}
			*tmp_packet = a_packet;
			tmp_packet->offset = (i * pkt_sz);
			tmp_packet->vaddr += tmp_packet->offset;
			tmp_packet->next = free_packets;
			memset(tmp_packet->vaddr, 0, pkt_sz);	/* MAC header at least */
			memcpy(((char *)tmp_packet->vaddr), stream_packet, frame_size);
			tmp_packet->len = frame_size;
			free_packets = tmp_packet;
		}

		start_feed_socket_init();
		err = pthread_create(&tid, NULL, read_start_feed, NULL);
		if (err != 0) {
			printf("Failed to create thread :[%s] \n", strerror(err));
			return -1;
		}

		total_read_bytes = 0;
		memset(&sched, 0 , sizeof (sched));
		sched.sched_priority = 25;
		sched_setscheduler(0, SCHED_RR, &sched);
	}

	gst_buffer_map(buff,&info,GST_MAP_READ);
	if(info.size>1024) {
		g_print("Internal Data Flow Error: Expected blocksize=1024 for filesrc\n");
		igb_cleanup();
	}

	if (listeners && !halt_tx && avb_init) {
start:
		if (g_start_feeding == 2)
			goto exit_app;
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
		avb_set_1722_seq_number(h1722, seqnum++);
		if (seqnum % 4 == 0)
			avb_set_1722_timestamp_valid(h1722, 0);
		else
			avb_set_1722_timestamp_valid(h1722, 1);

		data_ptr = (uint8_t *)((uint8_t*)stream_packet + sizeof(eth_header) +
			sizeof(seventeen22_header) + sizeof(six1883_header));

		memcpy((void *)data_ptr,info.data,payload_len);
		usleep(500);
		total_read_bytes += payload_len;
		total_samples += payload_len;
		h61883 = (six1883_header *)((uint8_t*)stream_packet + sizeof(eth_header) + sizeof(seventeen22_header));
		avb_set_61883_data_block_continuity(h61883, total_samples);

		err = igb_xmit(&igb_dev, 0, tmp_packet);
		if (!err) {
			fprintf(stderr,"frame sequence = %lld\n", frame_sequence++);
			do{
				usleep(100);
			} while (g_start_feeding == 0);
			return GST_FLOW_OK;
		} else {
		    fprintf(stderr,"Failed frame sequence = %lld !!!!\n", frame_sequence++);
		}

		if (ENOSPC == err) {
			/* put back for now */
			tmp_packet->next = free_packets;
			free_packets = tmp_packet;
		}
		return GST_FLOW_OK;

cleanup:
		igb_clean(&igb_dev, &cleaned_packets);
		while (cleaned_packets) {
			tmp_packet = cleaned_packets;
			cleaned_packets = cleaned_packets->next;
			tmp_packet->next = free_packets;
			free_packets = tmp_packet;
		}
		  goto start;
	  }

	  if (halt_tx == 0)
		  printf("listener left ...\n"); 

exit_app:
	  halt_tx = 1;
	  igb_set_class_bandwidth(&igb_dev, 0, 0, 0, 0);
	  igb_dma_free_page(&igb_dev, &a_page);
	  igb_detach(&igb_dev);
	  pthread_exit(NULL);

	  return (0);
}


/* Plugin Registration */
static gboolean plugin_init (GstPlugin * plugin)
{
	  if (!gst_element_register (plugin, "avbsink", GST_RANK_NONE,
		  GST_TYPE_AVBSINK))
		  return FALSE;

	  return TRUE;
}

GST_PLUGIN_DEFINE (
		GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		avbsink,
		"Ethernet AVB Sink Element",
		plugin_init,
		VERSION,
		"LGPL",
		"GStreamer",
		"http://gstreamer.net/")
