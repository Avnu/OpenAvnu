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
#include <arpa/inet.h>
#include <unistd.h>
#include <stddef.h>
#include <pthread.h>

#include <pci/pci.h>

//#include "avb.h"
#include "avb_gptp.h"
#include "avb_srp.h"
#include "avb_avtp.h"
#ifndef AVB_FEATURE_ATL
/* IGB has not been disabled, so assume it is enabled. */
#define AVB_FEATURE_ATL 1
#endif
#include "avb_atl.h"
#include "async_pcap_storing.h"

#define VERSION_STR "0.1.1"

#define RAMP_SIZE (256)
#define ETH_HDR_LEN (14) //18 with VLAN
#define IPV4_HDR_LEN (20)
#define IPV6_HDR_LEN (40)
#define UDP_HDR_LEN (8)
#define SRC_CHANNELS (2)
#define GAIN (0.5)
#define SAMPLES_PER_FRAME (60)
#define SAMPLE_SIZE (4)
//#define L2_PACKET_IPG (125000) /* (1) packet every 125 usec */
#define L4_PORT ((uint16_t)5004)
#define PKT_SZ (100)

#define DEFAULT_ETHERTYPE 0x22f0
#define DEFAULT_UDP_PORT 17220

volatile int halt_tx_sig;//Global variable for signal handler

enum {
	spp_pat_zero_lt = 0,
	spp_pat_ramp_lt,
	spp_pat_iramp_lt,
	spp_pat_lt_sin,
	spp_pat_1722_sin,
	spp_pat_pcap = -1,
};

struct refill_pkt {
		u64 refill_time;
		char *payload;
		uint32_t pyld_size;
		struct refill_pkt *next;
};

/* globals */
unsigned char glob_station_addr[] = { 0, 0, 0, 0, 0, 0 };
//unsigned char glob_stream_id[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
/* IEEE 1722 reserved address */
unsigned char glob_l2_dest_addr[] = { 0x91, 0xE0, 0xF0, 0x00, 0x0e, 0x80 };
unsigned char glob_l3_dest_addr[] = { 224, 0, 0, 115 };
unsigned short glob_l3_dest_ipv6_addr[] = {0xFF02, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFF17, 0xFC0F};
unsigned char glob_l3_ip_addr[] = {0, 0, 0 ,0};
unsigned short glob_l3_ipv6_addr[] = {0, 0, 0 ,0, 0, 0, 0, 0};

#ifndef _LOG_H_DEFINED_
#define _LOG_H_DEFINED_

#define LOG_LVL_CRITICAL 1
#define LOG_LVL_ERROR 2
#define LOG_LVL_WARNING 3
#define LOG_LVL_INFO 4
#define LOG_LVL_VERBOSE 5
#define LOG_LVL_DEBUG 6

u_int32_t log_level;
void dump(u_int32_t lvl, const char *name, u_int8_t *buf, u_int32_t len) {};
#endif

static void *async_pcap_context = NULL;

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

uint16_t inet_checksum(char *ip, int len)
{
	uint32_t sum = 0;  /* assume 32 bit long, 16 bit short */
	while(len > 1){
		sum += (((uint8_t *)ip)[0] << 8) + ((uint8_t *)ip)[1]; 
		ip += 2;
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

void l3_to_l2_multicast(unsigned char *l2, unsigned char *l3) 
{
    l2[0] = 0x1;
    l2[1] = 0x0;
    l2[2] = 0x5e;
    l2[3] = l3[1] & 0x7F;
    l2[4] = l3[2];
    l2[5] = l3[3];
}

void l3_to_l2_ipv6_multicast(unsigned char *l2, unsigned short *l3) 
{
    l2[0] = 0x33;
    l2[1] = 0x33;
    l2[2] = ( l3[6] >> 8 ) & 0xFF;
    l2[3] = (l3[6] & 0xFF);
    l2[4] = ( l3[7] >> 8 ) & 0xFF;
    l2[5] = ( l3[7] & 0xFF);
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
	logprint(LOG_LVL_CRITICAL, "got SIGINT\n");
	halt_tx_sig = signum;
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

bool get_local_ipv6_address(char *interface)
{
    FILE *fptr;
    char *ch;
    ssize_t read;
    size_t len = 0;
    bool found = false;
    
    fptr = fopen("/proc/net/if_inet6", "r");
    if (fptr == NULL)
    {
        printf("Cannot open file \n");
        return(0);
    }
    
    while ((read = getline(&ch, &len, fptr)) != -1) {
        if (strstr(ch, interface) != NULL){
            found = true;
            char piece[4];
            char *ptr;
            long ret;
            
            for (int i=0; i < 8; i++){
                memcpy(piece, ch, 4);
                ret = strtoul(piece, &ptr, 16);
                glob_l3_ipv6_addr[i] = (unsigned short)ret;
                ch += 4;
            }
            break;
        }
    }
    fclose(fptr);
    if (!found)
         return 1;
    
    return 0;
}

int get_local_ipv4_address(char *interface)
{
	struct ifreq if_request;
	int lsock;
	int rc;
        
    lsock = socket(PF_PACKET, SOCK_RAW, htons(0x800));
    if (lsock < 0)
        return -1;

    memset(&if_request, 0, sizeof(if_request));
    strncpy(if_request.ifr_name, interface, sizeof(if_request.ifr_name) - 1);
    rc = ioctl(lsock, SIOCGIFADDR, &if_request);
    
    if (rc < 0) {
        close(lsock);
        return -1;
    }
    memcpy(glob_l3_ip_addr, if_request.ifr_addr.sa_data+2, sizeof(glob_l3_ip_addr));
    close(lsock);
    
	return 0;
}

static int add_ipv4_headers(char *pkt, uint16_t pkt_len)
{
	uint16_t checksum;
    pkt[0] = 0x45;
    pkt[1] = 0x00;
    pkt[2] = (pkt_len >> 8) & 0xff;
    pkt[3] = pkt_len & 0xFF;
    pkt[4] = 0x12;
    pkt[5] = 0x34;
    pkt[6] = 0x40;
    pkt[7] = 0x00;
    pkt[8] = 0x01;
    pkt[9] = 0x11;
    pkt[10] = 0;
    pkt[11] = 0;
    pkt[12] = glob_l3_ip_addr[0];
    pkt[13] = glob_l3_ip_addr[1];
    pkt[14] = glob_l3_ip_addr[2];
    pkt[15] = glob_l3_ip_addr[3];
    pkt[16] = glob_l3_dest_addr[0];
    pkt[17] = glob_l3_dest_addr[1];
    pkt[18] = glob_l3_dest_addr[2];
    pkt[19] = glob_l3_dest_addr[3];
    checksum = inet_checksum(pkt, IPV4_HDR_LEN);
    pkt[10] = (checksum >> 8) & 0xFF;
    pkt[11] = checksum & 0xFF;
    
    return IPV4_HDR_LEN;
}

static int add_ipv6_headers(char *pkt, uint16_t pkt_len)
{
    pkt[0] = 0x60;
    pkt[4] = (pkt_len >> 8) & 0xFF;
    pkt[5] = pkt_len & 0xFF;
    pkt[6] = 0x11;
    pkt[7] = 0x08;

    pkt[8] = glob_l3_ipv6_addr[0] >> 8 & 0xFF;
    pkt[9] = glob_l3_ipv6_addr[0];
    
    pkt[10] = glob_l3_ipv6_addr[1] >> 8 & 0xFF;
    pkt[11] = glob_l3_ipv6_addr[1];
    
    pkt[12] = glob_l3_ipv6_addr[2] >> 8 & 0xFF;
    pkt[13] = glob_l3_ipv6_addr[2];
    
    pkt[14] = glob_l3_ipv6_addr[3] >> 8 & 0xFF;
    pkt[15] = glob_l3_ipv6_addr[3];
    
    pkt[16] = glob_l3_ipv6_addr[4] >> 8 & 0xFF;
    pkt[17] = glob_l3_ipv6_addr[4];
    
    pkt[18] = glob_l3_ipv6_addr[5] >> 8 & 0xFF;
    pkt[19] = glob_l3_ipv6_addr[5];
    
    pkt[20] = glob_l3_ipv6_addr[6] >> 8 & 0xFF;
    pkt[21] = glob_l3_ipv6_addr[6];
    
    pkt[22] = glob_l3_ipv6_addr[7] >> 8 & 0xFF;
    pkt[23] = glob_l3_ipv6_addr[7];

    for(int i = 24, j = 0; i < 40; i++ ){
        if (i % 2 == 0) pkt[i] = (glob_l3_dest_ipv6_addr[j] >> 8) & 0xFF;
        else{ 
            pkt[i] = (glob_l3_dest_ipv6_addr[j] & 0xFF);
            j++;
        }
    }
    
    return IPV6_HDR_LEN;
}

static int add_udp_headers(char *pkt, const int udp_port, uint16_t len)
{
    pkt[0] = (L4_PORT >> 8) & 0xFF;
    pkt[1] = L4_PORT & 0xFF;
    
    pkt[2] = (udp_port >> 8) & 0xFF;
    pkt[3] = udp_port & 0xFF;
    
    pkt[4] = (len >> 8) & 0xFF;
    pkt[5] = len & 0xFF;
    pkt[6] = 0;
    pkt[7] = 0;
    return UDP_HDR_LEN;
}

static void fill_data_by_pattern(int pattern, char *pkt, uint32_t pkt_size, char *pcap, uint32_t pcap_size)
{	
		unsigned k;
		switch( pattern ) {
		case spp_pat_1722_sin:
			{
			/* 1722 header update + payload */
			seventeen22_header *l2_header0 = (seventeen22_header *)pkt;
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
			break;
			}
		case spp_pat_ramp_lt:
			for( k = 0; k < pkt_size; k ++) {
				pkt[k] = k % RAMP_SIZE;
			}
			break;
		case spp_pat_iramp_lt:
			for( k = 0; k < pkt_size; k ++) {
				pkt[k] = RAMP_SIZE-(k % RAMP_SIZE)-1;
			}
			break;
		case spp_pat_zero_lt:
			memset(pkt, 0, pkt_size);
			break;
		case spp_pat_lt_sin:
			break;
		case spp_pat_pcap:
			memcpy(pkt, pcap, pcap_size);
			break;
		}	
}
static void usage(void)
{
	fprintf(stderr, "\n"
		"usage: send_packet_precisely [-h] -i interface-name [-I iteration-count]"
		"[-c packet-in-iteration][-T launch-time-offset][-t launch-time-increment]"
		"[-f pcap-file | -p pattern][-q 0|1][-o pcap-file]"
		"\n"
		"options:\n"
		"    -h  show this message\n"
		"    -i  specify interface for AVB connection\n"
		"    -I  specify iteration count\n"
		"    -c  specify sent packet count per iteration\n"
		"    -s  specify sent packet payload size (0 or default - defined by pattern)\n"
		"    -T specify launch time to send first packet. If it is negative - it is an offset from current time\n"
		"    -t specify launch time increment between iterations\n"
		"    -f  PCAP file name with packet pattern (requested launchtime "
		"will be added at the end of this packet\n"
		"    -p  specify sent packet pattern: 0-'zeros+lt', 1-'ramp+lt', 2-'inverse ramp+lt', 3-'lt+sin', 4-'AVTP1722+sin'\n"
		"    -r  specify packet refill pattern: 0-'zeros+lt', 1-'ramp+lt', 2-'inverse ramp+lt', 3-'lt+sin', 4-'AVTP1722+sin'\n"
		"    -R specify launch time offset to refill packet. (Default value:) 0 - disable feature.\n"
		"    -q  specify TSN queue (if supported by HW) 0 (default value) or 1\n"
		"    -v specify VLAN Id (HEX) [None by default]\n"
		"    -e specify Ethertype (HEX) [0x22f0 by default]\n"
		"    -V specify VLAN PCP [None by default, 0 if VLAN id is specified]\n"
		"    -o PCAP file name to store all send packets\n"
		"    -l Log level: 0 - Critical, 1 - Error, 2 - Warning, 3 - Info [Default], 4 - Verbose\n"
		"\n"
		"send_packet_precisely v" VERSION_STR "\n"
    	"Copyright (c) 2019, Aquantia Corporation\n"
		);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	unsigned i, j, k;
	int err, sent, confirmed;
	device_t atl_dev = {0};
	/* Uncomment next line to sync with gPTP 
	int atl_shm_fd = -1;
	char *atl_mmap = NULL;
	gPtpTimeData td;
	uint64_t now_local, now_8021as;
	uint64_t update_8021as;
	unsigned delta_8021as, delta_local;
	*/
	struct atl_dma_alloc *a_pages;
	uint32_t packet_per_page;
	uint32_t a_page_count;
	struct atl_packet a_packet;
	uint32_t a_packet_count;
	struct atl_packet *tmp_packet;
	struct atl_packet *cleaned_packets;
	struct atl_packet *free_packets;
	int c, hw_vlan = 0;
	unsigned int iteration_count = 1;
	unsigned int packet_count = 1;
	u_int64_t launch_time_offset = 1000000000u; //1 s
	u_int64_t launch_time_increment = 125000u; //125 us
	u_int64_t last_time = 0;
	int rc = 0;
	char *interface = NULL;
	char *pcap_file = NULL;
	char *output_file = NULL;
	char *pcap_buf = NULL;
    char *token;
    const char delim[2] = {':'};
    
	uint32_t pcap_size = 0;
	int32_t vlan_id = -1;
	int32_t vlan_pcp = -1;
	int32_t et = -1;
	int pattern = spp_pat_pcap;
	int refill_pattern = spp_pat_pcap;
	u_int64_t refill_lt_offset = 0u; //0
	uint16_t seqnum;
	uint64_t time_stamp = 0;
	int32_t queue = 0;
	int32_t lt_offset = -1;
	int32_t seq_offset = -1;
	int32_t sin_offset = -1;
	int32_t lt_refill_offset = -1;
	int32_t seq_refill_offset = -1;
	int32_t sin_refill_offset = -1;
    
    bool ipv4 = false;
    bool ipv6 = false;
    int16_t udp_port = -1;
	uint16_t pseudohdr_chksum = 0;
	uint8_t dest_addr[6];
	size_t eth_hdr_size = ETH_HDR_LEN;
    size_t total_hdr_size = eth_hdr_size;
	size_t packet_size = 0, payload_size = 0, payload_refill_size = 0;

	struct refill_pkt *last_refill = NULL;
	struct refill_pkt *next_refill = NULL;

	struct refill_pkt *refill_pkt = NULL;
	//struct mrp_domain_attr *class_a = malloc(sizeof(struct mrp_domain_attr));
	//struct mrp_domain_attr *class_b = malloc(sizeof(struct mrp_domain_attr));

	for (;;) {
		c = getopt(argc, argv, "h246i:I:c:T:t:f:p:q:v:V:e:R:r:s:S:l:u:d:o:");
		if (c < 0)
			break;
		switch (c) {
		case 'h':
			usage();
			break;
		case 'i':
			if (interface) {
				logprint(LOG_LVL_WARNING, "only one interface per daemon is supported\n");
				usage();
			}
			interface = strdup(optarg);
			atl_dev.ifname = strdup(optarg);
			break;
		case 'o':
			output_file = strdup(optarg);
			break;
		case 'f':
			if (pcap_file || pattern != -1) {
				logprint(LOG_LVL_WARNING, "only one pcap file or pattern is supported\n");
				usage();
			}
			pcap_file = strdup(optarg);
			break;
		case 'p':
			if (pcap_file || pattern != -1) {
				logprint(LOG_LVL_WARNING, "only one from pcap file and pattern is supported\n");
				usage();
			}
			pattern = strtoul( optarg, NULL, 10 );
			break;
		case 'r':
			if (pcap_file || refill_pattern != -1) {
				logprint(LOG_LVL_WARNING, "only one from pcap file and pattern is supported\n");
				usage();
			}
			refill_pattern = strtoul( optarg, NULL, 10 );
			break;
		case 'R':
			refill_lt_offset = strtoull( optarg, NULL, 10 );
			break;
		case 's':
			payload_size = strtoul( optarg, NULL, 10 );
			break;
		case 'S':
			payload_refill_size = strtoul( optarg, NULL, 10 );
			break;
		case 'c':
			packet_count = strtoul( optarg, NULL, 10 );
			break;
		case 'I':
			iteration_count = strtoul( optarg, NULL, 10 );
			break;
		case 't':
			launch_time_increment = strtoull( optarg, NULL, 10 );
			break;
		case 'T':
			{
				int64_t launch_time = strtoll( optarg, NULL, 10 );
				if( launch_time > 0 ) {
					time_stamp = (u_int64_t)launch_time;
				} else {
					launch_time_offset = (u_int64_t)(-launch_time);
				}
			}
			break;
		case 'q':
			queue = strtoul( optarg, NULL, 10 );
			break;
		case 'e':
			if( ipv4 || ipv6 || udp_port >= 0 ) {
                logprint(LOG_LVL_WARNING, "L2 packet cannot be mixed with IP/UDP settings\n");
                usage();
			}
			et = strtol( optarg, NULL, 16 );
			break;
		case 'v':
			vlan_id = strtol( optarg, NULL, 16 );
			break;
		case 'V':
			vlan_pcp = strtol( optarg, NULL, 10 );
			break;
		case 'l':
			log_level = strtol( optarg, NULL, 10 );
			break;
		case '2':
			if( ipv4 || ipv6 || udp_port >= 0 ) {
                logprint(LOG_LVL_WARNING, "L2 packet cannot be mixed with IP/UDP settings\n");
                usage();
			}
			break;
        case '4':
			if( et >= 0 || ipv6 ) {
                logprint(LOG_LVL_WARNING, "IPv4 packet cannot be mixed with L2/IPv6 settings\n");
                usage();
			}
			ipv4 = true;
			break;

        case '6':
			if( et >= 0 || ipv4 ) {
                logprint(LOG_LVL_WARNING, "IPv6 packet cannot be mixed with L2/IPv4 settings\n");
                usage();
			}
			ipv6 = true;
			break;
        case 'u':
 			if( et >= 0 ) {
                logprint(LOG_LVL_WARNING, "UDP packet cannot be mixed with L2 settings\n");
                usage();
			}
            udp_port = strtol( optarg, NULL, 10 );
            break;
        
        case 'd':
			if( ipv4 || ipv6 || udp_port >= 0 ) {
                logprint(LOG_LVL_WARNING, "L2 packet cannot be mixed with IP/UDP settings\n");
                usage();
			}
            token = strtok(optarg, delim);
			i = 0;
	        while(token != NULL)
	        {
		        glob_l2_dest_addr[i] = strtol(token, NULL, 16);
		        token = strtok(NULL, delim);
                i++;
	        }
			if( et < 0 )
				et = DEFAULT_ETHERTYPE;

            break;
        }
	}

	if (optind < argc)
		usage();
	if (NULL == interface) {
		usage();
	}
	if( (pattern < 0 && pcap_file == NULL) || pattern >= spp_pat_1722_sin ) {
		logprint(LOG_LVL_ERROR, "Must specify valid pattern or pcap file\n" );
		usage();
	}
	if( refill_lt_offset > 0 && (refill_pattern < 0 && pcap_file == NULL) || refill_pattern >= spp_pat_1722_sin ) {
		logprint(LOG_LVL_ERROR, "Must specify valid refill pattern or pcap file\n" );
		usage();
	}
	if( refill_lt_offset > 0 && pattern == refill_pattern ) {
		logprint(LOG_LVL_ERROR, "Fill pattern and refill pattern should be different.\n" );
		usage();
	}
    if ( udp_port >= 0 && ipv4 == ipv6 ) {
        logprint(LOG_LVL_ERROR, "IPv4 or IPv6 must be specified for UDP port!.\n" );
		usage();
    } 

    if( (ipv4 || ipv6 ) && udp_port < 0 ){
		udp_port = DEFAULT_UDP_PORT;
	}

    if ( udp_port >= 0 ){
        if ( ipv4 ) {
			et = 0x0800;
			total_hdr_size += IPV4_HDR_LEN + UDP_HDR_LEN;
		}
        if ( ipv6 ) {
			et = 0x86DD;
			total_hdr_size += IPV6_HDR_LEN + UDP_HDR_LEN;
		}
    } else if( et < 0 ) {
		et = DEFAULT_ETHERTYPE;
	}
	if( vlan_id >= 0 || vlan_pcp >= 0 ) {
		eth_hdr_size += 0x4;
        total_hdr_size += 0x4;
		if( vlan_id < 0 ) vlan_id = 0;
		if( vlan_pcp < 0 ) vlan_pcp = 0;
	}
    
	logprint(LOG_LVL_DEBUG, "Ethertype %x\n", et);
	logprint(LOG_LVL_DEBUG, "VLAN: PCP %d TAG %03x\n", vlan_pcp, vlan_id);

	logprint(LOG_LVL_VERBOSE, "Fill pattern %x\n", pattern);
	switch(pattern){
		case spp_pat_zero_lt: //zero + launch time
		case spp_pat_ramp_lt: //ramp + launch time
		case spp_pat_iramp_lt: //inversed ramp + launch time
			if( !payload_size ) {
				payload_size = RAMP_SIZE;
			}
			packet_size = total_hdr_size + sizeof(uint64_t) + payload_size;
			lt_offset = total_hdr_size + payload_size;
			break;
		case spp_pat_lt_sin: //launch time + sin
			if( !payload_size ) {
				payload_size = SAMPLES_PER_FRAME*SRC_CHANNELS*SAMPLE_SIZE;
			}
			packet_size = total_hdr_size + sizeof(uint64_t) + payload_size;
			lt_offset = total_hdr_size;
			sin_offset = total_hdr_size + sizeof(uint64_t);
			break;
		case spp_pat_1722_sin: //1722 + sin
			if( !payload_size ) {
				payload_size = SAMPLES_PER_FRAME*SRC_CHANNELS*SAMPLE_SIZE;
			}
			packet_size = total_hdr_size + sizeof(seventeen22_header) + payload_size;
			/* 1722 header update + payload */
			lt_offset = total_hdr_size + 8;//offsetof(seventeen22_header, timestamp);
			seq_offset = total_hdr_size + 2; //offsetof(seventeen22_header, seq_number);
			sin_offset = total_hdr_size + sizeof(seventeen22_header);
			break;
		default:
			//read file
			packet_size = PKT_SZ;
	}

	logprint(LOG_LVL_VERBOSE, "Refill pattern %x\n", refill_pattern);
	switch(refill_pattern){
		case spp_pat_zero_lt: //zero + launch time
		case spp_pat_ramp_lt: //ramp + launch time
		case spp_pat_iramp_lt: //inversed ramp + launch time
			if( !payload_refill_size ) {
				payload_refill_size = RAMP_SIZE;
			}
			lt_refill_offset = payload_refill_size;
			break;
		case spp_pat_lt_sin: //launch time + sin
			if( !payload_refill_size ) {
				payload_refill_size = SAMPLES_PER_FRAME*SRC_CHANNELS*SAMPLE_SIZE;
			}
			lt_refill_offset = 0;
			sin_refill_offset = sizeof(uint64_t);
			break;
		case spp_pat_1722_sin: //1722 + sin
			if( !payload_refill_size ) {
				payload_refill_size = SAMPLES_PER_FRAME*SRC_CHANNELS*SAMPLE_SIZE;
			}
			/* 1722 header update + payload */
			lt_refill_offset = 8;//offsetof(seventeen22_header, timestamp);
			seq_refill_offset = 2; //offsetof(seventeen22_header, seq_number);
			sin_refill_offset = sizeof(seventeen22_header);
			break;
	}

	packet_per_page = atl_getpagesize() / packet_size;
	a_packet_count = iteration_count * packet_count;
	if( a_packet_count > ATL_AVB_RING_SIZE ) {
		a_packet_count = ATL_AVB_RING_SIZE;
	}
	a_page_count = (a_packet_count + packet_per_page - 1) / packet_per_page;

	logprint(LOG_LVL_DEBUG, "Allocate page count %x\n", a_page_count);
	a_pages = malloc(a_page_count*sizeof(*a_pages));
	if( a_pages == NULL ){
		logprint(LOG_LVL_ERROR, "malloc failed (%s) - out of memory?\n",
		       strerror(errno));
		return errno;
	}

	err = pci_connect(&atl_dev);
	if (err) {
		logprint(LOG_LVL_CRITICAL, "connect failed (%s) - are you running as root?\n",
		       strerror(errno));
		return errno;
	}
	err = atl_init(&atl_dev);
	if (err) {
		logprint(LOG_LVL_CRITICAL, "init failed (%s) - is the driver really loaded?\n",
		       strerror(errno));
		return errno;
	}

	signal(SIGINT, sigint_handler);

	rc = get_mac_address(interface);
	if (rc) {
		logprint(LOG_LVL_ERROR, "failed to open interface\n");
		usage();
		goto error_alloc_dma;
	}
    
    if (ipv4 | ipv6) {
        if (ipv4) rc = get_local_ipv4_address(interface);
        else rc = get_local_ipv6_address(interface);
        if (rc) {
            logprint(LOG_LVL_CRITICAL, "Failed to obtain ip address!\n",
		       strerror(errno));
			goto error_alloc_dma;
        }
	}

    if( ipv4 ) l3_to_l2_multicast(dest_addr, glob_l3_dest_addr);
    else if( ipv6 ) l3_to_l2_ipv6_multicast(dest_addr, glob_l3_dest_ipv6_addr);
    else memcpy(dest_addr, glob_l2_dest_addr, sizeof(dest_addr));

	err = atl_set_class_bandwidth(&atl_dev, 
								  queue ? 0 : packet_count*8000*(packet_size + 24), 
								  queue ? packet_count*8000*(packet_size + 24) : 0); // 24 - IPG+PREAMBLE+FCS
	if (err) {
		logprint(LOG_LVL_ERROR, "A bandwidth Request failed\n");
		goto error_alloc_dma;
	}
	//memset(glob_stream_id, 0, sizeof(glob_stream_id));
	//memcpy(glob_stream_id, glob_station_addr, sizeof(glob_station_addr));

	for( i = 0; i < a_page_count; i++ ) {
		err = atl_dma_malloc_page(&atl_dev, &a_pages[i]);
		if (err) {
			logprint(LOG_LVL_ERROR, "malloc failed (%s) - out of memory?\n",
				strerror(errno));
			goto error_alloc_dma;
		}
	}

	if( output_file ) {
		if( async_pcap_initialize_context(output_file, a_page_count*packet_per_page, packet_size, &async_pcap_context) ) {
			logprint(LOG_LVL_ERROR, "Cannot create async_pcap_context. %s\n",
				strerror(errno));
			return errno;
		}
		logprint(LOG_LVL_VERBOSE, "Create async_pcap_context. File path %s\n", output_file);
	}

	a_packet.offset = 0;
	a_packet.next = NULL;
	free_packets = NULL;
	seqnum = 0;
	j = 0;
	/* divide the dma page into buffers for packets */
	for (i = 0; i < a_packet_count; i++) {
		tmp_packet = malloc(sizeof(struct atl_packet));
		if (NULL == tmp_packet) {
			logprint(LOG_LVL_ERROR, "failed to allocate atl_packet memory!\n");
			goto error_alloc_packet;
		}

		if( !(i % packet_per_page) ) {
			a_packet.map.paddr = a_pages[j].dma_paddr;
			a_packet.map.mmap_size = a_pages[j].mmap_size;
			a_packet.vaddr = a_pages[j].dma_vaddr + a_packet.offset;
			a_packet.len = packet_size;
			j++;
		}

		*tmp_packet = a_packet;
		tmp_packet->offset = (i % packet_per_page) * packet_size;
		tmp_packet->vaddr += tmp_packet->offset;
		tmp_packet->next = free_packets;
		memset(tmp_packet->vaddr, 0, packet_size);	/* MAC header at least */
		memcpy(tmp_packet->vaddr, dest_addr, sizeof(dest_addr));
		memcpy(tmp_packet->vaddr + 6, glob_station_addr,
		       sizeof(glob_station_addr));

		if( refill_lt_offset > 0 ) {
			refill_pkt = (struct refill_pkt *)malloc(sizeof(struct refill_pkt));
			if( !refill_pkt ) {
				logprint(LOG_LVL_ERROR, "No memory for packet refilling\n");
				goto error_alloc_packet;
			}
			tmp_packet->extra = refill_pkt;
		} else {
			tmp_packet->extra = NULL;
		}
		
		k = ETH_HDR_LEN - 2;
		/* Q-tag */
		tmp_packet->vlan = 0;
		if( hw_vlan && vlan_id >= 0 ) {
			tmp_packet->vlan = vlan_id;
		}else if( vlan_id >= 0 ) {
			((char *)tmp_packet->vaddr)[k++] = 0x81;
			((char *)tmp_packet->vaddr)[k++] = 0x00;
			((char *)tmp_packet->vaddr)[k++] = ((vlan_pcp << 5) | (vlan_id >> 8)) & 0xff;
				//((ctx->domain_class_a_priority << 13 | ctx->domain_class_a_vid)) >> 8;
			((char *)tmp_packet->vaddr)[k++] = vlan_id & 0xff;
				//((ctx->domain_class_a_priority << 13 | ctx->domain_class_a_vid)) & 0xFF;
		}
		((char *)tmp_packet->vaddr)[k++] = (et >> 8) & 0xff;	/* 1722 eth type */
		((char *)tmp_packet->vaddr)[k++] = (et >> 0) & 0xff;

		if( udp_port >= 0 ) {
			if( ipv4 ) {
				k += add_ipv4_headers((char *)tmp_packet->vaddr + k, packet_size - eth_hdr_size);
			} else {
				k += add_ipv6_headers((char *)tmp_packet->vaddr + k, packet_size - eth_hdr_size - IPV6_HDR_LEN);
			}
            add_udp_headers((char *)tmp_packet->vaddr + k, udp_port, packet_size - k);
        }

		fill_data_by_pattern(pattern, ((char *)tmp_packet->vaddr) + total_hdr_size, payload_size, pcap_buf, pcap_size);
		tmp_packet->len = packet_size;
		free_packets = tmp_packet;
		//dump(0,"New AVB packet", (u_int8_t *)tmp_packet->vaddr, packet_size);
	}

	if( udp_port >= 0 ) {
		char pseudo_hdr[40] = {0};
		if( ipv4 ) {
			pseudo_hdr[0] = glob_l3_ip_addr[0];
			pseudo_hdr[1] = glob_l3_ip_addr[1];
			pseudo_hdr[2] = glob_l3_ip_addr[2];
			pseudo_hdr[3] = glob_l3_ip_addr[3];
			pseudo_hdr[4] = glob_l3_dest_addr[0];
			pseudo_hdr[5] = glob_l3_dest_addr[1];
			pseudo_hdr[6] = glob_l3_dest_addr[2];
			pseudo_hdr[7] = glob_l3_dest_addr[3];
			pseudo_hdr[8] = 0x00;
			pseudo_hdr[9] = 0x11; //UDP
			pseudo_hdr[10] = ((packet_size - k) >> 8) & 0xFF;
			pseudo_hdr[11] = (packet_size - k) & 0xFF;
		}
        else {
            pseudo_hdr[0] = (glob_l3_ipv6_addr[0] >> 8) & 0xFF;
            pseudo_hdr[1] = glob_l3_ipv6_addr[0] & 0xFF;
            
            pseudo_hdr[2] = (glob_l3_ipv6_addr[1] >> 8) & 0xFF;
            pseudo_hdr[3] = glob_l3_ipv6_addr[1] & 0xFF;
            
            pseudo_hdr[4] = (glob_l3_ipv6_addr[2] >> 8) & 0xFF;
            pseudo_hdr[5] = glob_l3_ipv6_addr[2] & 0xFF;
            
            pseudo_hdr[6] = (glob_l3_ipv6_addr[3] >> 8) & 0xFF;
            pseudo_hdr[7] = glob_l3_ipv6_addr[3] & 0xFF;
            
            pseudo_hdr[8] = (glob_l3_ipv6_addr[4] >> 8) & 0xFF;
            pseudo_hdr[9] = glob_l3_ipv6_addr[4] & 0xFF;
            
            pseudo_hdr[10] = (glob_l3_ipv6_addr[5] >> 8) & 0xFF;
            pseudo_hdr[11] = glob_l3_ipv6_addr[5] & 0xFF;
            
            pseudo_hdr[12] = (glob_l3_ipv6_addr[6] >> 8) & 0xFF;
            pseudo_hdr[13] = glob_l3_ipv6_addr[6] & 0xFF;
            
            pseudo_hdr[14] = (glob_l3_ipv6_addr[7] >> 8) & 0xFF;
            pseudo_hdr[15] = glob_l3_ipv6_addr[7] & 0xFF;
            
            pseudo_hdr[16] = (glob_l3_dest_ipv6_addr[0] >> 8) & 0xFF;
            pseudo_hdr[17] = glob_l3_dest_ipv6_addr[0] & 0xFF;
            
            pseudo_hdr[18] = (glob_l3_dest_ipv6_addr[1] >> 8) & 0xFF;
            pseudo_hdr[19] = glob_l3_dest_ipv6_addr[1] & 0xFF;
            
            pseudo_hdr[20] = (glob_l3_dest_ipv6_addr[2] >> 8) & 0xFF;
            pseudo_hdr[21] = glob_l3_dest_ipv6_addr[2] & 0xFF;
            
            pseudo_hdr[22] = (glob_l3_dest_ipv6_addr[3] >> 8) & 0xFF;
            pseudo_hdr[23] = glob_l3_dest_ipv6_addr[3] & 0xFF;
            
            pseudo_hdr[24] = (glob_l3_dest_ipv6_addr[4] >> 8) & 0xFF;
            pseudo_hdr[25] = glob_l3_dest_ipv6_addr[4] & 0xFF;
            
            pseudo_hdr[26] = (glob_l3_dest_ipv6_addr[5] >> 8) & 0xFF;
            pseudo_hdr[27] = glob_l3_dest_ipv6_addr[5] & 0xFF;
            
            pseudo_hdr[28] = (glob_l3_dest_ipv6_addr[6] >> 8) & 0xFF;
            pseudo_hdr[29] = glob_l3_dest_ipv6_addr[6] & 0xFF;
            
            pseudo_hdr[30] = (glob_l3_dest_ipv6_addr[7] >> 8) & 0xFF;
            pseudo_hdr[31] = glob_l3_dest_ipv6_addr[7] & 0xFF;
            
            pseudo_hdr[32] = 0;
			pseudo_hdr[33] = 0;
            
            pseudo_hdr[34] = ((packet_size - k) >> 8) & 0xFF;
			pseudo_hdr[35] = (packet_size - k) & 0xFF;
            
            pseudo_hdr[36] = 0;
            pseudo_hdr[37] = 0;
            pseudo_hdr[38] = 0;
            
            pseudo_hdr[39] = 0x11;
            
        }
		pseudohdr_chksum = ~inet_checksum(pseudo_hdr, 40);
	}

	halt_tx_sig = 0;

	/* Uncomment for sync time with gPTP
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
	*/

	if(atl_get_wallclock( &atl_dev, &last_time, NULL ) > 0) {
		logprint(LOG_LVL_ERROR, "Failed to get wallclock time\n" );
		return EXIT_FAILURE;
	}
	if( !time_stamp ) {
		time_stamp = last_time + launch_time_offset;//now_8021as + RENDER_DELAY;
	}
	logprint(LOG_LVL_INFO, "Current time: %lu, launch time: %lu\n", last_time, time_stamp);

	logprint(LOG_LVL_DEBUG, "Packet offsets: lt %d, seq_offset %d, sin_offset %d\n", lt_offset, seq_offset, sin_offset);
	rc = nice(-20);
	sent = confirmed = 0;
	j = 0;
	while ( !halt_tx_sig && (iteration_count || sent > confirmed )) {
		uint32_t empty_space = 0;
		if( next_refill ) {
			if(atl_get_wallclock( &atl_dev, &last_time, NULL ) > 0) {
					logprint(LOG_LVL_ERROR, "Failed to get wallclock time\n" );
					break;
			}

			while( next_refill && next_refill->refill_time < last_time ) {
				uint64_t prev_seq, prev_lt;
				refill_pkt = next_refill;
				prev_lt = ((uint64_t *)(refill_pkt->payload + (lt_offset - total_hdr_size)))[0];
				if( seq_refill_offset >= 0 && seq_offset >= 0 ) {
					prev_seq = ((uint64_t *)(refill_pkt->payload + (seq_offset - total_hdr_size)))[0];
				}
				fill_data_by_pattern(refill_pattern, refill_pkt->payload, refill_pkt->pyld_size, pcap_buf, pcap_size);
				if( sin_refill_offset >= 0 ) {
						get_samples(refill_pkt->pyld_size / (SRC_CHANNELS*SAMPLE_SIZE), 
												(int32_t *)(refill_pkt->payload + sin_refill_offset));
				}
				*((uint64_t *)(refill_pkt->payload + lt_refill_offset)) = prev_lt;
				if( seq_refill_offset >= 0 ) {
					*((uint64_t *)(refill_pkt->payload + seq_refill_offset)) = seq_offset >= 0 ? prev_seq : htonl(seqnum++);
				}
				if( udp_port >= 0 ) {
					char *udp_hdr = refill_pkt->payload - UDP_HDR_LEN;
					int16_t chksum = 0;
					udp_hdr[6] = (pseudohdr_chksum >> 8) & 0xFF;
					udp_hdr[7] = pseudohdr_chksum & 0xFF;
					chksum = inet_checksum(udp_hdr, packet_size - k);
					udp_hdr[6] = (chksum >> 8) & 0xff;
					udp_hdr[7] = chksum & 0xff;
				}

				logprint(LOG_LVL_VERBOSE, "refill packet with refill time %lu at time %lu\n", next_refill->refill_time, last_time);
				next_refill = next_refill->next;
			}

			if( !next_refill ) {
				last_refill = next_refill;
			} 
		}
		empty_space = atl_get_xmit_space(&atl_dev, queue);
		tmp_packet = free_packets;
		if (tmp_packet && iteration_count && empty_space > packet_count) {
			struct atl_packet *send_packets = NULL, *last_attached = NULL;
			while( tmp_packet && j < packet_count && empty_space > 1 ) {
				free_packets = tmp_packet->next;
				tmp_packet->next = NULL;
				if( sin_offset >= 0 ) {
					get_samples( payload_size / (SRC_CHANNELS*SAMPLE_SIZE), (int32_t *)(((char *)tmp_packet->vaddr) + sin_offset));
				}

				/* unfortuntely unless this thread is at rtprio
					* you get pre-empted between fetching the time
					* and programming the packet and get a late packet
					*/
				a_packet.hwtime = a_packet.flags = 0;
				tmp_packet->attime = time_stamp;
				logprint(LOG_LVL_VERBOSE, "Packet %p, time %lu\n", tmp_packet, time_stamp);
				*((uint64_t *)(((char *)tmp_packet->vaddr) + lt_offset)) = htole64(time_stamp);
				if( seq_offset >= 0 ) {
					*((uint64_t *)((char *)tmp_packet->vaddr + seq_offset)) = htonl(seqnum++);
				}

				if( udp_port >= 0 ) {
					char *udp_hdr = ((char *)tmp_packet->vaddr) + total_hdr_size - UDP_HDR_LEN;
					int16_t chksum = 0;
					udp_hdr[6] = (pseudohdr_chksum >> 8) & 0xFF;
					udp_hdr[7] = pseudohdr_chksum & 0xFF;
					chksum = inet_checksum(udp_hdr, packet_size - k);
					udp_hdr[6] = (chksum >> 8) & 0xff;
					udp_hdr[7] = chksum & 0xff;
				}
				
				if( send_packets == NULL ) {
					send_packets = tmp_packet;
				}
				if( last_attached ) {
					last_attached->next = tmp_packet;
				}
				last_attached = tmp_packet;
				dump(LOG_LVL_VERBOSE, "Prepare AVB packet", (u_int8_t *)tmp_packet->vaddr, packet_size);

				if( refill_lt_offset > 0 ) {
					refill_pkt = (struct refill_pkt *)tmp_packet->extra;
					refill_pkt->payload = ((char *)tmp_packet->vaddr) + total_hdr_size;
					refill_pkt->pyld_size = payload_refill_size;
					refill_pkt->refill_time = time_stamp - refill_lt_offset;
					refill_pkt->next = NULL;
					if( last_refill )
						last_refill->next = refill_pkt;
					last_refill = refill_pkt;
					if( !next_refill ) {
						next_refill = refill_pkt;
					}
				}

				tmp_packet = free_packets;
				j++;
			}

			logprint(LOG_LVL_DEBUG, "Prepare %d packets.\n", j);
			err = atl_xmit(&atl_dev, queue, &send_packets);

			while( send_packets ) {
				// Clean up non-send packets and refill areas
				logprint(LOG_LVL_DEBUG, "Clean up unsent packet %p.\n", send_packets);
				tmp_packet = send_packets;
				send_packets = tmp_packet->next;
				if( refill_lt_offset > 0 && next_refill ) {
					struct refill_pkt *tmp_refill = next_refill;
					refill_pkt = (struct refill_pkt *)tmp_packet->extra;

					if( next_refill != refill_pkt ) {
						while( tmp_refill && tmp_refill->next != refill_pkt ) {
							tmp_refill = tmp_refill->next;
						}

						if( tmp_refill && tmp_refill->next == refill_pkt ) {
							tmp_refill->next = NULL;
							last_refill = tmp_refill;
						}
					} else {
						next_refill = NULL;
						last_refill = NULL;
					}
				}
				tmp_packet->next = free_packets;
				free_packets = tmp_packet;
				j--;
			}

			if( err < 0 ) {
				break;
			}

			if( j == packet_count ) {
				sent += packet_count;
				time_stamp += launch_time_increment;
				iteration_count--;
				logprint(LOG_LVL_DEBUG, "Complete iteration. Rest iteration count %d.\n", iteration_count);
				j = 0;
				continue;
			}
		}

		cleaned_packets = NULL;
		err = atl_clean(&atl_dev, &cleaned_packets);
		if( err < 0 ) {
			break;
		}

		if( !err ) {
			continue;
		}
		tmp_packet = cleaned_packets;
		i = 0;
		while( tmp_packet ) {
			if( tmp_packet->flags & ATL_PKT_FLAG_HW_TS_VLD ){
				logprint(LOG_LVL_INFO, "AVB Packet %05d sent time %016lu\n", confirmed+i, tmp_packet->hwtime);

				if( async_pcap_context ) {
					logprint(LOG_LVL_DEBUG, "Dump packet to pcap file\n");
					async_pcap_store_packet(async_pcap_context, tmp_packet->vaddr, tmp_packet->len, tmp_packet->hwtime);
				}
			}
			i++;
			if( refill_lt_offset > 0 ) {
				//Fill packet with original pattern
				fill_data_by_pattern(pattern, refill_pkt->payload, refill_pkt->pyld_size, pcap_buf, pcap_size);
			}
			tmp_packet = tmp_packet->next;
		}

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
		confirmed += err;
	}
	logprint(LOG_LVL_VERBOSE, "Iteration_count %d, sent %d, confirmed %d\n", iteration_count, sent, confirmed);
	//sleep(5);
	rc = nice(0);
	atl_stop_tx(&atl_dev, queue, &cleaned_packets);
error_alloc_packet:
	tmp_packet = free_packets;
	while( tmp_packet ) {
		free_packets = tmp_packet->next;
		if( tmp_packet->extra ) {
			free(tmp_packet->extra);
		}
		free(tmp_packet);
		tmp_packet = free_packets;
	}

	tmp_packet = cleaned_packets;
	while( tmp_packet ) {
		cleaned_packets = tmp_packet->next;
		if( tmp_packet->extra ) {
			free(tmp_packet->extra);
		}
		free(tmp_packet);
		tmp_packet = cleaned_packets;
	}

	atl_set_class_bandwidth(&atl_dev, 0, 0);	/* disable Qav ??? */ 
	for( i = 0; i < a_page_count; i++){
		atl_dma_free_page(&atl_dev, &a_pages[i]);
	}
error_alloc_dma:
	free( a_pages );

	// rc = gptpdeinit(&atl_shm_fd, &atl_mmap);
	err = atl_detach(&atl_dev);

	if( async_pcap_context ) {
		async_pcap_release_context(async_pcap_context);
		async_pcap_context = NULL;
	}

	pthread_exit(NULL);
	
	return EXIT_SUCCESS;
}
