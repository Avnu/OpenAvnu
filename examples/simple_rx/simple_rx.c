/*
Copyright (c) 2013 Katja Rohloff <Katja.Rohloff@uni-jena.de>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/******************************************************************************

  Copyright (c) 2016, Intel Corporation
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
#include <signal.h>

#include <pcap/pcap.h>
#include <sndfile.h>

#include <search.h>
#include <pci/pci.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>

#include "igb.h"
#include "listener_mrp_client.h"

#define DEBUG 0
#define PCAP 0
#define LIBSND 1
#define DIRECT_RX 1

#if DIRECT_RX
#undef PCAP
#endif

#define VERSION_STR "1.1"

#define ETHERNET_HEADER_SIZE (18)
#define SEVENTEEN22_HEADER_PART1_SIZE (4)
#define STREAM_ID_SIZE (8)
#define SEVENTEEN22_HEADER_PART2_SIZE (10)
#define SIX1883_HEADER_SIZE (10)
#define HEADER_SIZE (ETHERNET_HEADER_SIZE		\
			+ SEVENTEEN22_HEADER_PART1_SIZE \
			+ STREAM_ID_SIZE		\
			+ SEVENTEEN22_HEADER_PART2_SIZE \
			+ SIX1883_HEADER_SIZE)
#define SAMPLES_PER_SECOND (48000)
#define SAMPLES_PER_FRAME (6)
#define CHANNELS (2)

#if DIRECT_RX
#define IGB_BIND_NAMESZ (24)
#define IGB_QUEUE_0 (0)

#define PKT_SZ   (2048)
#define PKT_NUM  (256)

#ifndef ETH_P_IEEE1722
#define ETH_P_IEEE1722 0x22F0
#endif

struct myqelem {
	struct myqelem *q_forw;
	struct myqelem *q_back;
	void	*q_data;
};
#endif /* DIRECT_RX */

struct mrp_listener_ctx *ctx_sig;//Context pointer for signal handler

struct ethernet_header{
	u_char dst[6];
	u_char src[6];
	u_char stuff[4];
	u_char type[2];
};

/* globals */

static const char *version_str = "simple_listener v" VERSION_STR "\n"
    "Copyright (c) 2012, Intel Corporation\n";

pcap_t* glob_pcap_handle;
u_char glob_ether_type[] = { 0x22, 0xf0 };
SNDFILE* glob_snd_file;

#if DIRECT_RX
/* Global variable for signal handler */
volatile int halt_rx_sig;

struct myqelem *glob_pkt_head  = NULL;
struct myqelem *glob_page_head = NULL;
struct igb_packet		*glob_rx_packets[PKT_NUM];
struct igb_dma_alloc	glob_dma_pages[PKT_NUM];

/* used to configure the device to accept multicast packets */
int glob_sock = -1;

void igb_stop_rx(device_t *igb_dev);
void cleanup(void);
#endif /* DIRECT_RX */

static void help()
{
	fprintf(stderr, "\n"
		"Usage: listener [-h] -i interface -f file_name.wav"
		"\n"
		"Options:\n"
		"    -h  show this message\n"
		"    -i  specify interface for AVB connection\n"
		"    -f  set the name of the output wav-file\n"
		"\n" "%s" "\n", version_str);
	exit(EXIT_FAILURE);
}

void pcap_callback(u_char* args, const struct pcap_pkthdr* packet_header, const u_char* packet)
{
	unsigned char* test_stream_id;
	struct ethernet_header* eth_header;
	uint32_t *buf;
	uint32_t frame[2] = { 0 , 0 };
	int i;
	struct mrp_listener_ctx *ctx = (struct mrp_listener_ctx*) args;
	(void) packet_header; /* unused */

#if DEBUG
	fprintf(stdout,"Got packet.\n");
#endif /* DEBUG*/

	eth_header = (struct ethernet_header*)(packet);

#if DEBUG
	fprintf(stdout,"Ether Type: 0x%02x%02x\n", eth_header->type[0], eth_header->type[1]);
#endif /* DEBUG*/

	if (0 == memcmp(glob_ether_type,eth_header->type,sizeof(eth_header->type)))
	{
		test_stream_id = (unsigned char*)(packet + ETHERNET_HEADER_SIZE + SEVENTEEN22_HEADER_PART1_SIZE);

#if DEBUG
		fprintf(stderr, "Received stream id: %02x%02x%02x%02x%02x%02x%02x%02x\n ",
			     test_stream_id[0], test_stream_id[1],
			     test_stream_id[2], test_stream_id[3],
			     test_stream_id[4], test_stream_id[5],
			     test_stream_id[6], test_stream_id[7]);
#endif /* DEBUG*/

		if (0 == memcmp(test_stream_id, ctx->stream_id, sizeof(STREAM_ID_SIZE)))
		{

#if DEBUG
			fprintf(stdout,"Stream ids matched.\n");
#endif /* DEBUG*/
			buf = (uint32_t*) (packet + HEADER_SIZE);
			for(i = 0; i < SAMPLES_PER_FRAME * CHANNELS; i += 2)
			{
				memcpy(&frame[0], &buf[i], sizeof(frame));

				frame[0] = ntohl(frame[0]);   /* convert to host-byte order */
				frame[1] = ntohl(frame[1]);
				frame[0] &= 0x00ffffff;       /* ignore leading label */
				frame[1] &= 0x00ffffff;
				frame[0] <<= 8;               /* left-align remaining PCM-24 sample */
				frame[1] <<= 8;

				sf_writef_int(glob_snd_file, (const int *)frame, 1);
			}
		}
	}
}

void sigint_handler(int signum)
{
	fprintf(stdout,"Received signal %d:leaving...\n", signum);

#if PCAP
	if (NULL != glob_pcap_handle)
	{
		pcap_breakloop(glob_pcap_handle);
		pcap_close(glob_pcap_handle);
	}
#endif /* PCAP */

#if DIRECT_RX
	halt_rx_sig = 0;
#endif /* DIRECT_RX */
}

#if DIRECT_RX
int join_mcast(char *interface, struct mrp_listener_ctx *ctx)
{
	struct sockaddr_ll addr;
	struct ifreq if_request;
	int rc;
	struct packet_mreq multicast_req;

	if (NULL == ctx)
		return -1;

	glob_sock = socket(PF_PACKET, SOCK_RAW, htons(0x2272));
	if (glob_sock < 0)
		return -1;

	memset(&if_request, 0, sizeof(if_request));
	strncpy(if_request.ifr_name, interface, sizeof(if_request.ifr_name)-1);

	rc = ioctl(glob_sock, SIOCGIFINDEX, &if_request);
	if (rc < 0) {
		/* glob_sock will be closed by cleanup() */
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sll_ifindex = if_request.ifr_ifindex;
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(0x2272);

	rc = bind(glob_sock, (struct sockaddr *)&addr, sizeof(addr));
	if (0 != rc)
		return -1;

	rc = setsockopt(glob_sock, SOL_SOCKET, SO_BINDTODEVICE, interface,
			strlen(interface));
	if (0 != rc)
		return -1;

	memset(&multicast_req, 0, sizeof(multicast_req));
	multicast_req.mr_ifindex = if_request.ifr_ifindex;
	multicast_req.mr_type = PACKET_MR_MULTICAST;
	multicast_req.mr_alen = 6;
	memcpy(multicast_req.mr_address, (void*)ctx->dst_mac, 6);

	rc = setsockopt(glob_sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
			&multicast_req, sizeof(multicast_req));
	if (0 != rc)
		return -1;

	return 0;
}

int pci_connect(device_t *igb_dev)
{
	char devpath[IGB_BIND_NAMESZ];
	struct pci_access *pacc;
	struct pci_dev *dev;
	int err;

	memset(igb_dev, 0, sizeof(device_t));
	pacc = pci_alloc();
	pci_init(pacc);
	pci_scan_bus(pacc);
	for (dev = pacc->devices; dev; dev = dev->next) {
		pci_fill_info(dev,
			      PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);
		igb_dev->pci_vendor_id = dev->vendor_id;
		igb_dev->pci_device_id = dev->device_id;
		igb_dev->domain = dev->domain;
		igb_dev->bus = dev->bus;
		igb_dev->dev = dev->dev;
		igb_dev->func = dev->func;
		snprintf(devpath, IGB_BIND_NAMESZ, "%04x:%02x:%02x.%d",
			 dev->domain, dev->bus, dev->dev, dev->func);

		err = igb_probe(igb_dev);

		if (err)
			continue;

		printf("attaching to %s\n", devpath);

		if (igb_attach(devpath, igb_dev)) {
			printf("attach failed! (%s)\n", strerror(errno));
			continue;
		}
		if (igb_attach_rx(igb_dev)) {
			printf("rx attach failed! (%s)\n", strerror(err));
			continue;
		}
		goto out;
	}
	pci_cleanup(pacc);
	return -ENXIO;
out:
	pci_cleanup(pacc);
	return 0;
}

int igb_start_rx(device_t *igb_dev, struct mrp_listener_ctx *ctx)
{
	int rc = -1;
	int err;

	uint32_t i;
	uint32_t count = 0;

	struct myqelem *page_qelem		= NULL;
	struct myqelem *prev_page_qelem	= NULL;
	struct myqelem *pkt_qelem		= NULL;
	struct myqelem *prev_pkt_qelem	= NULL;

	struct igb_dma_alloc	*a_page		= NULL;
	struct igb_packet		*a_packet	= NULL;

	uint8_t test_filter[128];
	uint8_t filter_mask[64];
	uint32_t filter_len = 0;
	struct ethhdr *ethhdr = NULL;
	uint16_t *ethtype = NULL;

	if (0 != pci_connect(igb_dev))
		goto out;

	if (0 != igb_init(igb_dev))
		goto out;

	memset (glob_dma_pages, 0, sizeof(glob_dma_pages));
	memset (glob_rx_packets, 0, sizeof(glob_rx_packets));

	while (count < PKT_NUM) {
		page_qelem = calloc(1, sizeof(struct myqelem));
		if (!page_qelem)
			goto out;

		if (!glob_page_head)
			glob_page_head = page_qelem;

		a_page = (struct igb_dma_alloc*)calloc(1, sizeof(struct igb_dma_alloc));
		if (!a_page)
			goto out;

		page_qelem->q_data = (void*)a_page;
		insque(page_qelem, prev_page_qelem);
		prev_page_qelem = page_qelem;

		err = igb_dma_malloc_page(igb_dev, a_page);
		if (err) {
			printf("malloc failed (%s) - out of memory?\n",
			       strerror(err));
			goto out;
		}

		if (a_page->mmap_size < PKT_SZ)
			goto out;

		/* divide the dma page into buffers for packets */
		for (i = 0; i < a_page->mmap_size/PKT_SZ; i++) {
			pkt_qelem = calloc(1, sizeof(struct myqelem));
			if (!pkt_qelem)
				goto out;

			if (!glob_pkt_head)
				glob_pkt_head = pkt_qelem;

			a_packet = (struct igb_packet*)calloc(1, sizeof(struct igb_packet));
			if (a_packet == NULL) {
				printf("failed to allocate igb_packet memory!\n");
				err = -errno;
				goto out;
			}

			a_packet->dmatime = 0;
			a_packet->attime = 0;
			a_packet->flags = 0;
			a_packet->map.paddr = a_page->dma_paddr;
			a_packet->map.mmap_size = a_page->mmap_size;
			a_packet->offset = (i * PKT_SZ);
			a_packet->vaddr = a_page->dma_vaddr + a_packet->offset;
			a_packet->len = PKT_SZ;
			memset(a_packet->vaddr, 0, a_packet->len);
			if (prev_pkt_qelem)
				((struct igb_packet*)prev_pkt_qelem->q_data)->next = a_packet;	

			igb_refresh_buffers(igb_dev, IGB_QUEUE_0, &a_packet, 1);

			pkt_qelem->q_data = (void*)a_packet;
			insque(pkt_qelem, prev_pkt_qelem);
			prev_pkt_qelem = pkt_qelem;

			count++;
		}
	}

	/* filtering example */
	memset(filter_mask, 0, sizeof(filter_mask));
	memset(test_filter, 0, sizeof(test_filter));

	/* the filter for IEEE1722 with VLAN */

	/* accept packets toward the multicast dst mac given by MRP */
	ethhdr = (struct ethhdr *)test_filter;
	memcpy((void*)ethhdr->h_dest, (void*)ctx->dst_mac, ETH_ALEN);

	/* ethtype in ethernet frame = 802.1Q */
	ethhdr->h_proto = htons(ETH_P_8021Q);

	/* real ethtype = IEEE1722 */
	ethtype = (uint16_t*)(test_filter + ETH_HLEN + 2);
	*ethtype = htons(ETH_P_IEEE1722);

	filter_mask[0] = 0x3F; /* 00111111b = dst mac */
	filter_mask[1] = 0x30; /* 00110000b = ethtype */
	filter_mask[2] = 0x03; /* 00000011b = ethtype after a vlan tag */

	/* length must be 8 byte-aligned */
	filter_len = ((ETHERNET_HEADER_SIZE + (8u - 1u)) / 8u) * 8u;

	/* install the filter on queue0 */
	rc = igb_setup_flex_filter(igb_dev, IGB_QUEUE_0, 0, filter_len,
								test_filter, filter_mask);
out:
	if (rc != 0)
		igb_stop_rx(igb_dev);

	return rc;
}

void igb_stop_rx(device_t *igb_dev)
{
	struct myqelem *qelem = NULL;

	igb_clear_flex_filter(igb_dev, 0);

	while (glob_pkt_head) {
		qelem = glob_pkt_head;
		glob_pkt_head = qelem->q_forw;
		if (qelem->q_data)
			free(qelem->q_data);
		free(qelem);
	}

	while (glob_page_head) {
		qelem = glob_page_head;
		glob_page_head = qelem->q_forw;
		if (qelem->q_data){
			igb_dma_free_page(igb_dev, (struct igb_dma_alloc*)qelem->q_data);
			free(qelem->q_data);
		}
		free(qelem);
	}

	igb_detach(igb_dev);
}

void igb_process_rx(device_t *igb_dev, struct mrp_listener_ctx *ctx)
{
	uint32_t i;
	uint32_t count;
	uint8_t  *packet;
	uint32_t *buf;
	uint32_t frame[2] = { 0 , 0 };
	uint8_t  *test_stream_id;

	struct igb_packet *igb_pkt = NULL;
	struct ethhdr* eth_hdr;
	uint16_t *ethtype = NULL;

	halt_rx_sig = 1;

	while (halt_rx_sig) {

		/* put back */
		if (igb_pkt) {
			igb_refresh_buffers(igb_dev, IGB_QUEUE_0, &igb_pkt, 1);
			igb_pkt = NULL;
		}

		count = 1;
		igb_receive(igb_dev, IGB_QUEUE_0, &igb_pkt, &count);
		if (igb_pkt == NULL) {
			continue;
		}

		packet = igb_pkt->vaddr;
		eth_hdr = (struct ethhdr*)(packet);
		switch (htons(eth_hdr->h_proto)) {
			case ETH_P_8021Q:
				ethtype = (uint16_t*)(packet + ETH_HLEN + 2);
				if (*ethtype != htons(ETH_P_IEEE1722))
					continue; 	/* drop the packet */
				break;
			case ETH_P_IEEE1722:
				break;
			default:
				continue;	/* drop the packet */
				break;
		}

		test_stream_id = (uint8_t*)(packet + ETHERNET_HEADER_SIZE + 
										SEVENTEEN22_HEADER_PART1_SIZE);
		buf = (uint32_t*)(packet + HEADER_SIZE);

		if (ETH_P_IEEE1722 == htons(eth_hdr->h_proto)) {
			/* I210 would strip the VLAN tag field when CTRL.VME = 1b */
			/* pull 4 bytes the VLAN tag size */
			test_stream_id = ((uint8_t*)test_stream_id - 4);
			buf = ((uint32_t*)buf - 1);
		}

		if (0 == memcmp(test_stream_id, ctx->stream_id, STREAM_ID_SIZE))
		{
			for(i = 0; i < SAMPLES_PER_FRAME * CHANNELS; i += 2)
			{
				memcpy(&frame[0], &buf[i], sizeof(frame));

				frame[0] = ntohl(frame[0]);   /* convert to host-byte order */
				frame[1] = ntohl(frame[1]);
				frame[0] &= 0x00ffffff;       /* ignore leading label */
				frame[1] &= 0x00ffffff;
				frame[0] <<= 8;               /* left-align remaining PCM-24 sample */
				frame[1] <<= 8;

				sf_writef_int(glob_snd_file, (const int *)frame, 1);
			}
		}
	}
}

void cleanup(void)
{
	int ret;

	if (0 != ctx_sig->talker) {
		ret = send_leave(ctx_sig);
		if (ret)
			printf("send_leave failed\n");
	}

	if (2 > ctx_sig->control_socket)
	{
		close(ctx_sig->control_socket);
		ctx_sig->control_socket = -1;
		ret = mrp_disconnect(ctx_sig);
		if (ret)
			printf("mrp_disconnect failed\n");
	}

#if LIBSND
	if (glob_snd_file) {
		sf_write_sync(glob_snd_file);
		sf_close(glob_snd_file);
		glob_snd_file = NULL;
	}
#endif /* LIBSND */

	if (glob_sock >= 0) {
		close (glob_sock);
		glob_sock = -1;
	}

	halt_rx_sig = 0;
}
#endif /* DIRECT_RX */

int main(int argc, char *argv[])
{
	char* file_name = NULL;
	char* dev = NULL;
#if PCAP
	char errbuf[PCAP_ERRBUF_SIZE];
	struct bpf_program comp_filter_exp;		/* The compiled filter expression */
	char filter_exp[100];				/* The filter expression */
#endif
	struct mrp_listener_ctx *ctx = malloc(sizeof(struct mrp_listener_ctx));
	struct mrp_domain_attr *class_a = malloc(sizeof(struct mrp_domain_attr));
	struct mrp_domain_attr *class_b = malloc(sizeof(struct mrp_domain_attr));

#if LIBSND
	SF_INFO* sf_info = NULL;
#endif /* LIBSND */

#if DIRECT_RX
	device_t igb_dev;
#endif /* DIRECT_RX */

	int c,rc;

	if ((NULL == ctx) || (NULL == class_a) || (NULL == class_b)) {
		fprintf(stderr, "failed allocating memory\n");
		rc = EXIT_FAILURE;
		goto out;
	}

	ctx_sig = ctx;
	signal(SIGINT, sigint_handler);

	while((c = getopt(argc, argv, "hi:f:")) > 0)
	{
		switch (c)
		{
		case 'h':
			help();
			break;
		case 'i':
			dev = strdup(optarg);
			break;
		case 'f':
			file_name = strdup(optarg);
			break;
		default:
          		fprintf(stderr, "Unrecognized option!\n");
		}
	}

	if ((NULL == dev) || (NULL == file_name))
		help();

	rc = mrp_listener_client_init(ctx);
	if (rc)
	{
		printf("failed to initialize global variables\n");
		goto out;
	}

	if (create_socket(ctx))
	{
		fprintf(stderr, "Socket creation failed.\n");
		rc = EXIT_FAILURE;
		goto out;
	}

	rc = mrp_monitor(ctx);
	if (rc)
	{
		printf("failed creating MRP monitor thread\n");
		goto out;
	}
	rc=mrp_get_domain(ctx, class_a, class_b);
	if (rc)
	{
		printf("failed calling mrp_get_domain()\n");
		goto out;
	}

	printf("detected domain Class A PRIO=%d VID=%04x...\n",class_a->priority,class_a->vid);

	rc = report_domain_status(class_a,ctx);
	if (rc) {
		printf("report_domain_status failed\n");
		goto out;
	}

	rc = join_vlan(class_a, ctx);
	if (rc) {
		printf("join_vlan failed\n");
		goto out;
	}

	fprintf(stdout,"Waiting for talker...\n");
	await_talker(ctx);

#if DEBUG
	fprintf(stdout,"Send ready-msg...\n");
#endif /* DEBUG */
	rc = send_ready(ctx);
	if (rc) {
		printf("send_ready failed\n");
		goto out;
	}

#if LIBSND
	sf_info = (SF_INFO*)malloc(sizeof(SF_INFO));
	if (NULL == sf_info) {
		fprintf(stderr, "failed allocating memory\n");
		rc = EXIT_FAILURE;
		goto out;
	}

	memset(sf_info, 0, sizeof(SF_INFO));

	sf_info->samplerate = SAMPLES_PER_SECOND;
	sf_info->channels = CHANNELS;
	sf_info->format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;

	if (0 == sf_format_check(sf_info))
	{
		fprintf(stderr, "Wrong format.");
		rc = EXIT_FAILURE;
		goto out;
	}

	if (NULL == (glob_snd_file = sf_open(file_name, SFM_WRITE, sf_info)))
	{
		fprintf(stderr, "Could not create file.");
		rc = EXIT_FAILURE;
		goto out;
	}
	fprintf(stdout,"Created file called %s\n", file_name);
#endif /* LIBSND */

#if PCAP
	/** session, get session handler */
	/* take promiscuous vs. non-promiscuous sniffing? (0 or 1) */
	glob_pcap_handle = pcap_open_live(dev, BUFSIZ, 1, -1, errbuf);
	if (NULL == glob_pcap_handle)
	{
		fprintf(stderr, "Could not open device %s: %s\n", dev, errbuf);
		rc = EXIT_FAILURE;
		goto out;
	}

#if DEBUG
	fprintf(stdout,"Got session pcap handler.\n");
#endif /* DEBUG */
	/* compile and apply filter */
	sprintf(filter_exp,"ether dst %02x:%02x:%02x:%02x:%02x:%02x",ctx->dst_mac[0],ctx->dst_mac[1],ctx->dst_mac[2],ctx->dst_mac[3],ctx->dst_mac[4],ctx->dst_mac[5]);
	if (-1 == pcap_compile(glob_pcap_handle, &comp_filter_exp, filter_exp, 0, PCAP_NETMASK_UNKNOWN))
	{
		fprintf(stderr, "Could not parse filter %s: %s\n", filter_exp, pcap_geterr(glob_pcap_handle));
		rc = EXIT_FAILURE;
		goto out;
	}

	if (-1 == pcap_setfilter(glob_pcap_handle, &comp_filter_exp))
	{
		fprintf(stderr, "Could not install filter %s: %s\n", filter_exp, pcap_geterr(glob_pcap_handle));
		rc = EXIT_FAILURE;
		goto out;
	}

#if DEBUG
	fprintf(stdout,"Compiled and applied filter.\n");
#endif /* DEBUG */

	/** loop forever and call callback-function for every received packet */
	pcap_loop(glob_pcap_handle, -1, pcap_callback, (u_char*)ctx);
#endif /* PCAP */

#if DIRECT_RX
	if (0 != join_mcast(dev, ctx)) {
		fprintf(stderr, "Could not join the mcast group\n");
		rc = EXIT_FAILURE;
		goto out;
	}

	if (0 != igb_start_rx(&igb_dev, ctx)) {
		fprintf(stderr, "Could not start the igb device\n");
		rc = EXIT_FAILURE;
		goto out;
	}

	igb_process_rx(&igb_dev, ctx);

#endif
	rc = EXIT_SUCCESS;
out:
#if DIRECT_RX
	igb_stop_rx(&igb_dev);
	cleanup();
#endif /* DIRECT_RX */

	if (ctx)
		free(ctx);
	if (class_a)
		free(class_a);
	if (class_b)
		free(class_b);
	if (dev)
		free(dev);
	if (file_name)
		free(file_name);
#if LIBSND
	if (sf_info)
		free(sf_info);
#endif /* LIBSND */

	return rc;
}

