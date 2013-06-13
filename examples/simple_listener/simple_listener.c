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


#include <arpa/inet.h>
#include <errno.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <pcap/pcap.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <sndfile.h>

//#define DEBUG
#define PCAP
#define LIBSND

#define MAX_MRPD_CMDSZ 1500
#define MRPD_PORT_DEFAULT 7500

#define ETHERNET_HEADER_SIZE 18
#define SEVENTEEN22_HEADER_PART1_SIZE 4
#define STREAM_ID_SIZE 8
#define SEVENTEEN22_HEADER_PART2_SIZE 10
#define SIX1883_HEADER_SIZE 10
#define HEADER_SIZE ETHERNET_HEADER_SIZE + SEVENTEEN22_HEADER_PART1_SIZE + STREAM_ID_SIZE + SEVENTEEN22_HEADER_PART2_SIZE + SIX1883_HEADER_SIZE 

#define SAMPLES_PER_SECOND 48000
#define SAMPLES_PER_FRAME 6
#define CHANNELS 2

struct six1883_sample{
	uint8_t label;
	uint8_t value[3];
};

struct ethernet_header{
	u_char dst[6];
	u_char src[6];
	u_char stuff[4];
	u_char type[2];
};

typedef int (*process_msg) (char *buf, int buflen);

// global
unsigned char stream_id[8];
volatile int talker = 0;
int control_socket;
pcap_t* handle;
u_char ETHER_TYPE[] = { 0x22, 0xf0 };
SNDFILE* snd_file;

static void help()
{
	fprintf(stderr, "\n"
		"Usage: listener [-h] -i interface -f file_name.wav"
		"\n"
		"Options:\n"
		"    -h  show this message\n"
		"    -i  specify interface for AVB connection\n"
		"    -f  set the name of the output wav-file\n" 
		"\n" "%s" "\n");
	exit(1);
}

void pcap_callback(u_char* args, const struct pcap_pkthdr* packet_header, const u_char* packet)
{
	unsigned char* test_stream_id;
	struct ethernet_header* eth_header;
	struct six1883_sample* sample; 
	uint32_t buf;
	uint32_t *mybuf;
	uint32_t frame[2] = { 0 , 0 };

#ifdef DEBUG
	fprintf(stdout,"Got packet.\n");
#endif	

	eth_header = (struct ethernet_header*)(packet);

#ifdef DEBUG
	fprintf(stdout,"Ether Type: 0x%02x%02x\n", eth_header->type[0], eth_header->type[1]);
#endif

	if (0 == memcmp(ETHER_TYPE,eth_header->type,sizeof(eth_header->type)))
	{		
		test_stream_id = (unsigned char*)(packet + ETHERNET_HEADER_SIZE + SEVENTEEN22_HEADER_PART1_SIZE);

#ifdef DEBUG
		fprintf(stderr, "Received stream id: %02x%02x%02x%02x%02x%02x%02x%02x\n ",
			     test_stream_id[0], test_stream_id[1],
			     test_stream_id[2], test_stream_id[3],
			     test_stream_id[4], test_stream_id[5],
			     test_stream_id[6], test_stream_id[7]);
#endif

		if (0 == memcmp(test_stream_id, stream_id, sizeof(STREAM_ID_SIZE)))
		{

#ifdef DEBUG
			fprintf(stdout,"Stream ids matched.\n");
#endif

			//sample = (struct six1883_sample*) (packet + HEADER_SIZE);
			mybuf = (uint32_t*) (packet + HEADER_SIZE);
			for(int i = 0; i < SAMPLES_PER_FRAME * CHANNELS; i += 2)
			{	
				memcpy(&frame[0], &mybuf[i], sizeof(frame));

				frame[0] = ntohl(frame[0]);   /* convert to host-byte order */
				frame[1] = ntohl(frame[1]);
				frame[0] &= 0x00ffffff;       /* ignore leading label */
				frame[1] &= 0x00ffffff;
				frame[0] <<= 8;               /* left-align remaining PCM-24 sample */
				frame[1] <<= 8;

				sf_writef_int(snd_file, frame, 1);
			}
		}	
	}
}

int create_socket()
{
	struct sockaddr_in addr;
	control_socket = socket(AF_INET, SOCK_DGRAM, 0);
		
	/** in POSIX fd 0,1,2 are reserved */
	if (2 > control_socket)
	{
		if (-1 > control_socket)
			close(control_socket);
	return -1;
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(0);
	
	if(0 > (bind(control_socket, (struct sockaddr*)&addr, sizeof(addr)))) 
	{
		fprintf(stderr, "Could not bind socket.\n");
		close(control_socket);
		return -1;
	}
	return 0;
}


int msg_process(char *buf, int buflen)
{
	uint32_t id;
	fprintf(stderr, "Msg: %s\n", buf);
 	int l = 0;
	if ('S' == buf[l++] && 'N' == buf[l++] && 'E' == buf[l++] && 'T' == buf[++l])
	{
		while ('S' != buf[l++]);
		l++;
		for(int j = 0; j < 8 ; l+=2, j++)
		{
			sscanf(&buf[l],"%02x",&id);
			stream_id[j] = (unsigned char)id;
		}
		talker = 1;
	}
	return (0);
}

int recv_msg()
{
	char *databuf;
	int bytes = 0;

	databuf = (char *)malloc(2000);
	if (NULL == databuf)
		return -1;

	memset(databuf, 0, 2000);
	bytes = recv(control_socket, databuf, 2000, 0);
	if (bytes <= -1) 
	{
		free(databuf);
		return (-1);
	}
	return msg_process(databuf, bytes);

}

int await_talker()
{
	while (0 == talker)	
		recv_msg();
	return 0;
}

int send_msg(char *data, int data_len)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MRPD_PORT_DEFAULT);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	inet_aton("127.0.0.1", &addr.sin_addr);
	if (-1 != control_socket)
		return (sendto(control_socket, data, data_len, 0, (struct sockaddr*)&addr, (socklen_t)sizeof(addr)));
	else 
		return 0;
}

int mrp_disconnect()
{
	int rc;
	char *msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "BYE");
	rc = send_msg(msgbuf, 1500);

	free(msgbuf);
	return rc;
}
	
int report_domain_status()
{
	int rc;
	char* msgbuf = malloc(1500);

	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S+D:C=6,P=3,V=0002");
	
	rc = send_msg(msgbuf, 1500);

	free(msgbuf);
	return rc;
}

int send_ready()
{
	char *databuf;
	int rc;
	databuf = malloc(1500);
	if (NULL == databuf)
		return -1;
	memset(databuf, 0, 1500);
	sprintf(databuf, "S+L:L=%02x%02x%02x%02x%02x%02x%02x%02x, D=2",
		     stream_id[0], stream_id[1],
		     stream_id[2], stream_id[3],
		     stream_id[4], stream_id[5],
		     stream_id[6], stream_id[7]);
	rc = send_msg(databuf, 1500);

#ifdef DEBUG
	fprintf(stdout,"Ready-Msg: %s\n", databuf);
#endif 

	free(databuf);
	return rc;
}

int send_leave()
{
	char *databuf;
	int rc;
	databuf = malloc(1500);
	if (NULL == databuf)
		return -1;
	memset(databuf, 0, 1500);
	sprintf(databuf, "S-L:L=%02x%02x%02x%02x%02x%02x%02x%02x, D=3",
		     stream_id[0], stream_id[1],
		     stream_id[2], stream_id[3],
		     stream_id[4], stream_id[5],
		     stream_id[6], stream_id[7]);
	rc = send_msg(databuf, 1500);
	free(databuf);
	return rc;
}

void sigint_handler(int signum)
{
	fprintf(stdout,"Leaving...\n");
	
	if (0 != talker)
		send_leave();

	if (2 > control_socket)
	{
		close(control_socket);
		mrp_disconnect();
	}

#ifdef PCAP
	if (NULL != handle) 
	{
		pcap_breakloop(handle);
		pcap_close(handle);
	}
#endif
	
#ifdef LIBSND
	sf_write_sync(snd_file);
	sf_close(snd_file);
#endif
}

int main(int argc, char *argv[])
{
	int ret;
	char* file_name = NULL;
	char* dev = NULL;
	char errbuf[PCAP_ERRBUF_SIZE];
	struct bpf_program comp_filter_exp;		/* The compiled filter expression */
	char filter_exp[] = "ether dst 91:E0:F0:00:0e:80";	/* The filter expression */
	struct pcap_pkthdr header;	/* header pcap gives us */
	const u_char* packet;		/* actual packet */

	signal(SIGINT, sigint_handler);

	int c;
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

	if (create_socket())
	{
		fprintf(stderr, "Socket creation failed.\n");
		return (errno);
	}

	report_domain_status();

	fprintf(stdout,"Waiting for talker...\n");
	await_talker();	

#ifdef DEBUG
	fprintf(stdout,"Send ready-msg...\n");
#endif
	send_ready();
		
#ifdef LIBSND
	SF_INFO* sf_info = (SF_INFO*)malloc(sizeof(SF_INFO));

	memset(sf_info, 0, sizeof(SF_INFO));
	
	sf_info->samplerate = SAMPLES_PER_SECOND;
	sf_info->channels = CHANNELS;
	sf_info->format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;

	if (0 == sf_format_check(sf_info))
	{
		fprintf(stderr, "Wrong format.");
		return -1;
	}
			
	if (NULL == (snd_file = sf_open(file_name, SFM_WRITE, sf_info)))
	{
		fprintf(stderr, "Could not create file.");
		return -1;
	}
	fprintf(stdout,"Created file called %s\n", file_name);	
#endif

#ifdef PCAP		
	/** session, get session handler */
	/* take promiscuous vs. non-promiscuous sniffing? (0 or 1) */
	handle = pcap_open_live(dev, BUFSIZ, 1, -1, errbuf);
	if (NULL == handle) 
	{
		fprintf(stderr, "Could not open device %s: %s\n", dev, errbuf);
		return -1;
	}

#ifdef DEBUG
	fprintf(stdout,"Got session handler.\n");
#endif
	/* compile and apply filter */
	if (-1 == pcap_compile(handle, &comp_filter_exp, filter_exp, 0, PCAP_NETMASK_UNKNOWN))
	{
		fprintf(stderr, "Could not parse filter %s: %s\n", filter_exp, pcap_geterr(handle));
		return -1;
	}

	if (-1 == pcap_setfilter(handle, &comp_filter_exp)) 
	{
		fprintf(stderr, "Could not install filter %s: %s\n", filter_exp, pcap_geterr(handle));
		return -1;
	}

#ifdef DEBUG
	fprintf(stdout,"Compiled and applied filter.\n");
#endif

	/** loop forever and call callback-function for every received packet */
	pcap_loop(handle, -1, pcap_callback, NULL);
#endif

	return 0;
}
