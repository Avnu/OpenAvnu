/*
 * Avbplaybin plugin: Plays audio/video data received over network using AVB.
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
/* gst-launch-1.0 avbplaybin talkerip=192.168.1.100 port=7600 interface=eth0 */


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

#include <../../../common/avb.h>
#include "gst_avb_playbin.h"

/* Declare Macros*/
#define AVB_DEFAULT_INTERFACE           "eth0"
#define AVB_DEFAULT_TALKER_IP		"127.0.0.1"

/* global macros */
#define HAND_SHAKE_PORT		7600

#define NUM_OF_BUFFERS		50

#define FRAME_SIZE 1500

#define PAYLOAD_SIZE	1024

/* external function */
extern int gstreamer_main(void);

unsigned char DEST_ADDR[] = { 0x91, 0xE0, 0xF0, 0x00, 0x0E, 0x80 };

/* global variables*/
device_t igb_dev;
int start_of_input_data = 0;
int g_start_feed_socket = 0;
int g_exit_app = 0;
char *talker_ip;
int port;
int err;
char *interface1;
unsigned int avb_init;
struct sockaddr_ll ifsock_addr;
int socket_d;
seventeen22_header *h1722;
long long int frame_sequence = 0;
unsigned char frame[FRAME_SIZE];
int size;
struct tailq_entry *qptr;
struct sched_param sched;


TAILQ_HEAD(t_enqueue, tailq_entry) buffer_queue;
TAILQ_HEAD(t_dequeue, tailq_entry) free_queue;

pthread_mutex_t buffer_queue_lock;
pthread_mutex_t free_queue_lock;

enum
{
	PROP_0, 
	PROP_INTERFACE,
	PROP_TALKERIP,
	PROP_PORT
};
GST_DEBUG_CATEGORY_STATIC (avbsrc_debug);
#define GST_CAT_DEFAULT (avbsrc_debug)

static GstStaticPadTemplate src_template =
				GST_STATIC_PAD_TEMPLATE ("src",
							 GST_PAD_SRC,
							 GST_PAD_ALWAYS,
							 GST_STATIC_CAPS_ANY);

static GstFlowReturn gst_avbsrc_create (GstPushSrc * psrc, GstBuffer ** buf);
static void
gst_avbsrc_set_property (GObject * object, guint prop_id,
			 const GValue * value, GParamSpec * pspec);
static void
gst_avbsrc_get_property (GObject * object, guint prop_id,
			 GValue * value, GParamSpec * pspec);

#define gst_avbsrc_parent_class parent_class
G_DEFINE_TYPE (GstAVBSrc, gst_avbsrc, GST_TYPE_PUSH_SRC);

static void
gst_avbsrc_class_init (GstAVBSrcClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	GstBaseSrcClass *gstbasesrc_class;
	GstPushSrcClass *gstpushsrc_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;
	gstbasesrc_class = (GstBaseSrcClass *) klass;
	gstpushsrc_class = (GstPushSrcClass *) klass;

	GST_DEBUG_CATEGORY_INIT (avbsrc_debug, "avbsrc", 0, "AVB src");

	gobject_class->set_property = gst_avbsrc_set_property;
	gobject_class->get_property = gst_avbsrc_get_property;

	g_object_class_install_property(gobject_class, PROP_INTERFACE,
					g_param_spec_string ("interface",
					"Interface",
					"The Interface Name b/w sink and source",
					interface1,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_class, PROP_TALKERIP,
	g_param_spec_string("talkerip", "Talker_IP",
			     "The IP address of a talker", talker_ip,
			     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(G_OBJECT_CLASS (klass), PROP_PORT,
					 g_param_spec_int ("port", "Port",
					"The port to receive the packets from, 0=allocate",
					0, G_MAXUINT16, port,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gst_element_class_add_pad_template (gstelement_class,
					    gst_static_pad_template_get (&src_template));

	gst_element_class_set_static_metadata (gstelement_class,
		"AVB packet receiver",
		"Source/Network",
		"Receive data over the network via Ethernet AVB",
		"Symphony-Teleca");

	gstpushsrc_class->create = gst_avbsrc_create;

}

static void
gst_avbsrc_init (GstAVBSrc * avbsrc)
{
	avbsrc->interface = interface1;
	avbsrc->talker_ip = talker_ip;
	avbsrc->port=port;
}



static void 
gst_avbsrc_set_property (GObject * object, guint prop_id, const GValue * value,
	GParamSpec * pspec)
{
	switch (prop_id) {
		case PROP_INTERFACE:
			g_free (interface1);

			if (g_value_get_string (value) == NULL)
				interface1 = g_strdup (AVB_DEFAULT_INTERFACE);
			else
				interface1 = g_value_dup_string (value);
			break;

		case PROP_TALKERIP:
			g_free(talker_ip);

			if (g_value_get_string (value) == NULL)
				talker_ip = g_strdup (AVB_DEFAULT_TALKER_IP);
			else
				talker_ip = g_value_dup_string (value);
			break;

		case PROP_PORT:
			g_free(port);
			port = g_value_get_int (value);
			break;

		default:
			break;
	}
}

static void 
gst_avbsrc_get_property (GObject * object, guint prop_id, GValue * value,
			 GParamSpec * pspec)
{
	GstAVBSrc *avbsrc;

	avbsrc = GST_AVBSRC (object);	

	switch (prop_id) {
		case PROP_INTERFACE:
			g_value_set_string (value, interface1);
			break;

		case PROP_TALKERIP:
			g_value_set_string (value, talker_ip);
			break;

		case PROP_PORT:
			g_value_set_int (value, port);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

struct tailq_entry {
	TAILQ_ENTRY(tailq_entry) entries;
	uint32_t payload_length;
	uint8_t *payload_data;
};

/**
 * start_feed_socket_init() - initializes the socket.
 *
 * This function creates a socket which is used to exchange
 * data between transmitter and listener.
 *
 * Returns -EINVAL on error, zero on success.
 */
static int start_feed_socket_init()
{
	int sock;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("Opening socket");
		return -EINVAL;
	}
	g_start_feed_socket = sock;

	return 0;
}

/**
 * send_data_status() - Send the status to transmitter
 * @status: 0/1 for stop and start respectively
 *
 * This function sends 0 to transmitter to stop transmitting the data
 * for a while, there is enough of data available with gstreamer to handle.
 * sends 1 to transmitter to start back again with the data transmission.
 *
 * Returns 0
 */
int send_data_status(int status)
{
	struct sockaddr_in server;
	unsigned int length;
	char buffer[5];
	int n;
	int i;

	bzero(buffer, strlen(buffer));
	if (status == 0)
		/* send the transmitter a msg to stop sending */
		strcpy(buffer, "0");
	else if (status == 1)
		/* send the transmitter a msg to start sending */
		strcpy(buffer, "1");
	else if (status == 2)
		/* send the transmitter a msg to exit */
		strcpy(buffer, "2");

	server.sin_family = AF_INET;
	inet_aton(talker_ip, &server.sin_addr);
	server.sin_port = htons(port);
	length = sizeof(struct sockaddr_in);

	for(i = 0; i < 10; i++) {
		n = sendto(g_start_feed_socket, buffer, strlen(buffer), 0, 
		   (const struct sockaddr *)&server,length);
		if (n >= 0)
			break;
	}
	if (i == 10) {
		printf("Sendto Failed to \n");
	}

	return 0;
}

void flag_exit_app(int flag)
{
	g_exit_app = flag;
}

/**
 * initiliaze_queue() - Intializes the buffer queue
 * @max_size: the payload size of frame.
 *
 * Returns -EINVAL on error and zero on success.
 */
static int initiliaze_queue(int32_t max_size)
{
	int32_t i;
	struct tailq_entry *q[NUM_OF_BUFFERS];

	for (i = 0; i < NUM_OF_BUFFERS; i++) {
		q[i] = (struct tailq_entry *) malloc(sizeof(struct tailq_entry));
		q[i]->payload_length = 0;
		q[i]->payload_data = malloc(max_size);
		TAILQ_INSERT_TAIL(&free_queue, q[i], entries);
	}

	return 0;
}

/**
 * freequeue() - free's the buffer queue created.
 *
 */
static void freequeue()
{
	struct tailq_entry *q, *next_item;

	for (q = TAILQ_FIRST(&buffer_queue); q != NULL; q = next_item)
	{
		next_item = TAILQ_NEXT(q, entries);
		/* Remove the item from the tail queue. */
		TAILQ_REMOVE(&buffer_queue, q, entries);
		free(q->payload_data);
		free(q);
	}
	for (q = TAILQ_FIRST(&free_queue); q != NULL; q = next_item)
	{
		next_item = TAILQ_NEXT(q, entries);
		/* Remove the item from the tail queue. */
		TAILQ_REMOVE(&free_queue, q, entries);
		free(q->payload_data);
		free(q);
	}

	return;
}

/**
 * read_data_from_queue() - Gstreamer Callback to feed in data
 * to appsource whenever the need-data signal is occured on the bus
 */
int read_data_from_queue( void* ptr)
{
	struct tailq_entry *q = NULL;
	int i = 0;

        if( ptr != NULL) {
		while (!buffer_queue.tqh_first) {
			usleep(1);
		}

		pthread_mutex_lock(&(buffer_queue_lock));
		q = buffer_queue.tqh_first;
		TAILQ_REMOVE(&buffer_queue, q, entries);
		pthread_mutex_unlock(&(buffer_queue_lock));
		memcpy(ptr, q->payload_data, q->payload_length);
	
		i = q->payload_length;

		pthread_mutex_lock(&(free_queue_lock));
		q->payload_length = 0;
		TAILQ_INSERT_TAIL(&free_queue, q, entries);
		pthread_mutex_unlock(&(free_queue_lock));
	}
	return i;
}

/**
 * gstreamer_main_loop()- Initializes the gstreamer pipeline
 */
void * gstreamer_main_loop(void *arg)
{
        while (!start_of_input_data ) {
	     usleep(1);
	}
	gstreamer_main();
	while (1) {
		usleep(10);
	}
	return NULL;
}

void sigint_handler(int signum)
{
	printf("got SIGINT\n");
	send_data_status(2);
	freequeue();
	close(socket_d);
	exit(0);
}


static GstFlowReturn
gst_avbsrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{

	if(!avb_init)
	{
		struct packet_mreq mreq;
		struct ifreq device;
		uint32_t buff_size;
		pthread_t tid;
		int ifindex;

		/* add a signal handler for interruption Ctl+c */
		signal(SIGINT, sigint_handler);

		buff_size = PAYLOAD_SIZE;

		start_feed_socket_init();

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

		socket_d = socket(AF_PACKET, SOCK_RAW, htons(ETHER_TYPE_AVTP));
		if (socket_d == -1) {
			printf("failed to open event socket: %s \n", strerror(errno));
			return -1;
		}

		memset(&device, 0, sizeof(device));
		memcpy(device.ifr_name, interface1, IFNAMSIZ);
		err = ioctl(socket_d, SIOCGIFINDEX, &device);
		if (err == -1) {
			printf("Failed to get interface index: %s\n", strerror(errno));
			return -1;
		}

		ifindex = device.ifr_ifindex;
		memset(&ifsock_addr, 0, sizeof(ifsock_addr));
		ifsock_addr.sll_family = AF_PACKET;
		ifsock_addr.sll_ifindex = ifindex;
		ifsock_addr.sll_protocol = htons(ETHER_TYPE_AVTP);
		err = bind(socket_d, (struct sockaddr *) & ifsock_addr, sizeof(ifsock_addr));
		if (err == -1) {
			printf("Call to bind() failed: %s\n", strerror(errno));
			return -1;
		}

		memset(&mreq, 0, sizeof(mreq));
		mreq.mr_ifindex = ifindex;
		mreq.mr_type = PACKET_MR_MULTICAST;
		mreq.mr_alen = 6;
		memcpy(mreq.mr_address, DEST_ADDR, mreq.mr_alen);
		err = setsockopt(socket_d, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
		if (err == -1) {
			printf ("Unable to add PTP multicast addresses to port id: %u\n", ifindex);
			return -1;
		}

		TAILQ_INIT(&buffer_queue);
		TAILQ_INIT(&free_queue);
		pthread_mutex_init(&(buffer_queue_lock), NULL);
		pthread_mutex_init(&(free_queue_lock), NULL);

		if (initiliaze_queue(buff_size) < 0)
			return -EINVAL;

		err = pthread_create(&tid, NULL, gstreamer_main_loop, NULL);
		if (err != 0)
			printf("can't create thread :[%s]\n", strerror(err));

		memset(frame, 0, sizeof(frame));

		size = sizeof(ifsock_addr);
		frame_sequence = 0;
		start_of_input_data = 1;

		memset(&sched, 0 , sizeof (sched));
		sched.sched_priority = 25;
		sched_setscheduler(0, SCHED_RR, &sched);
	}
	
	while (1) {
		if (g_exit_app)
			break;

		err = recvfrom(socket_d, frame, FRAME_SIZE, 0, (struct sockaddr *) &ifsock_addr, (socklen_t *)&size);
		if (err > 0) {
			while (!free_queue.tqh_first) {
				usleep(1);
			}
			fprintf(stderr,"frame sequence = %lld\n", frame_sequence++);

			pthread_mutex_lock(&(free_queue_lock));
			qptr = free_queue.tqh_first;
			TAILQ_REMOVE(&free_queue, qptr, entries);
			h1722 = (seventeen22_header *)((uint8_t*)frame + sizeof(eth_header));
			qptr->payload_length = ntohs(h1722->length) - sizeof(six1883_header);
			memcpy(qptr->payload_data, (uint8_t *)((uint8_t*)frame + sizeof(eth_header) + sizeof(seventeen22_header) 
					+ sizeof(six1883_header)), qptr->payload_length);

			pthread_mutex_lock(&(buffer_queue_lock));
			TAILQ_INSERT_TAIL(&buffer_queue, qptr, entries);
			pthread_mutex_unlock(&(buffer_queue_lock));
			pthread_mutex_unlock(&(free_queue_lock));
			start_of_input_data = 1;
		} else {
 			printf("Failed to receive frame  !!!\n");
                }
	}

	usleep(100);
	pthread_exit(NULL);
	close(socket_d);
	freequeue();

	return GST_FLOW_OK;
}

static gboolean plugin_init(GstPlugin * plugin)
{
	if (!gst_element_register(plugin, "avbplaybin", GST_RANK_NONE,
				  GST_TYPE_AVBSRC))
		return FALSE;

	return TRUE;

}

GST_PLUGIN_DEFINE (
	GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	avbplaybin,
	"Ethernet AVB Source Element",
	plugin_init,
	VERSION,
	"LGPL",
	"GStreamer",
	"http://gstreamer.net/"
)



