/*
Copyright (c) 2013 Katja Rohloff <katja.rohloff@uni-jena.de>

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

#define _GNU_SOURCE

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
#include <assert.h>

#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <jack/weakmacros.h>
#include <jack/thread.h>

#include <sndfile.h>

#define LIBSND

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
#define SAMPLE_SIZE 4

#define DEFAULT_RINGBUFFER_SIZE 32768

#define MAX_SAMPLE_VALUE ((1U << ((sizeof(int32_t)*8)-1))-1)

struct ethernet_header{
	u_char dst[6];
	u_char src[6];
	u_char stuff[4];
	u_char type[2];
};

// global
unsigned char stream_id[8];
volatile int talker = 0;
int control_socket;
pcap_t* handle;
u_char ETHER_TYPE[] = { 0x22, 0xf0 };
SNDFILE* snd_file;

static jack_port_t** outputports;
static jack_default_audio_sample_t** out;
jack_ringbuffer_t* ringbuffer;

jack_client_t* client;

volatile int ready = 0;

static void help()
{
	fprintf(stderr, "\n"
		"Usage: listener [-h] -i interface -f file_name.wav"
		"\n"
		"Options:\n"
		"    -h  show this message\n"
		"    -i  specify interface for AVB connection\n"
		"\n" "%s" "\n");
	exit(1);
}

int send_msg(char *data, int data_len)
{
	int rc;
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(MRPD_PORT_DEFAULT);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	inet_aton("127.0.0.1", &addr.sin_addr);

	rc = sendto(control_socket, data, data_len, 0, (struct sockaddr*)&addr, (socklen_t)sizeof(addr));
	return rc;
}

int send_process(char type)
{ 
	int rc;
	char* msgbuf = malloc(1500);

	if (NULL == msgbuf) {
		return -1;
	}

	memset(msgbuf, 0, 1500);
	
	switch(type) {
	case 'M':
		sprintf(msgbuf, "BYE");
		break;
	case 'D':
		sprintf(msgbuf, "S+D:C=6,P=3,V=0002");
		break;
	case 'R':
		sprintf(msgbuf, "S+L:L=%02x%02x%02x%02x%02x%02x%02x%02x, D=2",
		     stream_id[0], stream_id[1],
		     stream_id[2], stream_id[3],
		     stream_id[4], stream_id[5],
		     stream_id[6], stream_id[7]);
		break;
	case 'L':
		sprintf(msgbuf, "S-L:L=%02x%02x%02x%02x%02x%02x%02x%02x, D=3",
		     stream_id[0], stream_id[1],
		     stream_id[2], stream_id[3],
		     stream_id[4], stream_id[5],
		     stream_id[6], stream_id[7]);
		break;
	default:
		return -1;
	}

	rc = send_msg(msgbuf, 1500);

	free(msgbuf);
	return rc;
}

int create_socket()
{
	struct sockaddr_in addr;
	
	control_socket = socket(AF_INET, SOCK_DGRAM, 0);

	/** POSIX: fd 0,1,2 are reserved */
	if (2 > control_socket) {
		close(control_socket);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(0);

	if(0 > (bind(control_socket, (struct sockaddr*)&addr, sizeof(addr)))) {
		fprintf(stderr, "Could not bind socket.\n");
		close(control_socket);
		return -1;
	}

	return 0;
}

int recvmsg_process(char *buf, int buflen)
{
	uint32_t id;
	int l = 0;
	
	if ('S' == buf[l++] && 'N' == buf[l++] && 'E' == buf[l++] && 'T' == buf[++l]) {

		while ('S' != buf[l++]);
		l++;

		for(int j = 0; j < 8 ; l+=2, j++) {
			sscanf(&buf[l],"%02x",&id);
			stream_id[j] = (unsigned char)id;
		}
		talker = 1;
	}

	return 0;
}

int recv_msg()
{
	int bytes = 0;
	char* msgbuf = malloc(2000);
	
	if (NULL == msgbuf) {
		return -1;
	}

	memset(msgbuf, 0, 2000);

	bytes = recv(control_socket, msgbuf, 2000, 0);

	if (bytes <= -1){
		free(msgbuf);
		return -1;
	}

	return recvmsg_process(msgbuf, bytes);
}

int await_talker()
{
	while (0 == talker) {	
		recv_msg();
	}

	return 0;
}

void shutdown_all(int sig)
{
	fprintf(stdout,"Leaving...\n");

	if (0 != talker) {
		send_process('L'); /** send leave */
	}

	send_process('M'); /** mrp disconnect */
	close(control_socket);

	if (NULL != handle) {
		pcap_breakloop(handle);
		pcap_close(handle);
	}

#ifdef LIBSND
	if (NULL != snd_file) {
		sf_write_sync(snd_file);
		sf_close(snd_file);
	}
#endif

	if (NULL != client) {
		fprintf(stdout, "jack\n");
		jack_client_close(client);
		jack_ringbuffer_free(ringbuffer);
	}

	exit(0);
}

void pcap_callback(u_char* args, const struct pcap_pkthdr* packet_header, const u_char* packet)
{
	unsigned char* test_stream_id;
	struct ethernet_header* eth_header;
	
	uint32_t* mybuf;
	uint32_t frame[CHANNELS];
	jack_default_audio_sample_t jackframe[CHANNELS];

	int cnt;
	static int total;

	eth_header = (struct ethernet_header*)(packet);

	if (0 != memcmp(ETHER_TYPE,eth_header->type,sizeof(eth_header->type))) {
		return;
	}

	test_stream_id = (unsigned char*)(packet + ETHERNET_HEADER_SIZE + SEVENTEEN22_HEADER_PART1_SIZE);
	if (0 != memcmp(test_stream_id, stream_id, sizeof(STREAM_ID_SIZE))) {
		return;
	}
		
	mybuf = (uint32_t*) (packet + HEADER_SIZE);
			
	for(int i = 0; i < SAMPLES_PER_FRAME * CHANNELS; i+=CHANNELS) {	

		memcpy(&frame[0], &mybuf[i], sizeof(frame));

		for(int j = 0; j < CHANNELS; j++) {
			
			frame[j] = ntohl(frame[j]);   /* convert to host-byte order */
			frame[j] &= 0x00ffffff;       /* ignore leading label */
			frame[j] <<= 8;               /* left-align remaining PCM-24 sample */
			
			jackframe[j] = ((int32_t)frame[j])/(float)(MAX_SAMPLE_VALUE);
		}

		if ((cnt = jack_ringbuffer_write_space(ringbuffer)) >= SAMPLE_SIZE * CHANNELS) {
			jack_ringbuffer_write(ringbuffer, (void*)&jackframe[0], SAMPLE_SIZE * CHANNELS);
 		
		} else {
			fprintf(stdout, "Only %i bytes available after %i samples.\n", cnt, total);
		}

		if (jack_ringbuffer_write_space(ringbuffer) <= SAMPLE_SIZE * CHANNELS * DEFAULT_RINGBUFFER_SIZE / 4) {
			/** Ringbuffer has only 25% or less write space available, it's time to tell jackd 
			to read some data. */
			ready = 1;
		}

#ifdef LIBSND
		sf_writef_float(snd_file, jackframe, 1);
#endif
	}
}

static int process_jack(jack_nframes_t nframes, void* arg)
{
	int cnt;

	if (!ready) {
		return 0;
	}

	for(int i = 0; i < CHANNELS; i++) {
		out[i] = jack_port_get_buffer(outputports[i], nframes);
	}

	for(size_t i = 0; i < nframes; i++) {
		
		if (jack_ringbuffer_read_space(ringbuffer) >= SAMPLE_SIZE * CHANNELS) {

			for(int j = 0; j < CHANNELS; j++){
				jack_ringbuffer_read (ringbuffer, (char*)(out[j]+i), SAMPLE_SIZE);
			}

		} else {
			printf ("underrun\n");
			ready = 0;

			return 0;
		}
	}

	return 0;
}

void jack_shutdown(void* arg)
{
	printf("JACK shutdown\n");
	shutdown_all(0);
}

jack_client_t* init_jack(void)
{
	const char* client_name = "simple_listener";
	const char* server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;

	client = jack_client_open (client_name, options, &status, server_name);

	if (NULL == client) {
		fprintf (stderr, "jack_client_open() failed\n ");
		shutdown_all(0);
		exit (1);
	}

	if (status & JackServerStarted) {
		fprintf (stderr, "JACK server started\n");
	}

	if (status & JackNameNotUnique) {
		client_name = jack_get_client_name(client);
		fprintf (stderr, "unique name `%s' assigned\n", client_name);
	}

	jack_set_process_callback(client, process_jack, 0);
	jack_on_shutdown(client, jack_shutdown, 0);

	outputports = (jack_port_t**) malloc (CHANNELS * sizeof (jack_port_t*));
	out = (jack_default_audio_sample_t**) malloc (CHANNELS * sizeof (jack_default_audio_sample_t*));
	ringbuffer = jack_ringbuffer_create (SAMPLE_SIZE * DEFAULT_RINGBUFFER_SIZE * CHANNELS);
	jack_ringbuffer_mlock(ringbuffer);

	memset(out, 0, sizeof (jack_default_audio_sample_t*)*CHANNELS);
	memset(ringbuffer->buf, 0, ringbuffer->size);

	for(int i = 0; i < CHANNELS; i++) {
		
		char* portName;
		if (asprintf(&portName, "output%d", i) < 0) {
			fprintf(stderr, "could not create portname for port %d\n", i);
			shutdown_all(0);
			exit(1);
		}	
		
		outputports[i] = jack_port_register (client, portName, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
		if (NULL == outputports[i]) {
			fprintf (stderr, "cannot register output port \"%d\"!\n", i);
			shutdown_all(0);
			exit (1);
		}
	}

	const char** ports;
	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client\n");		
		shutdown_all(0);
		exit(1);
	}

	ports = jack_get_ports(client, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
	if(NULL == ports) { 
		fprintf (stderr, "no physical playback ports\n");		
		shutdown_all(0);
		exit(1);
	}

	int i = 0;
	while(i < CHANNELS && NULL != ports[i]) {
		if (jack_connect(client, jack_port_name(outputports[i]), ports[i]))
			fprintf (stderr, "cannot connect output ports\n");
		i++;
	}

	free(ports);
}


int main(int argc, char *argv[])
{
	int ret;

	char* dev = NULL;
	char errbuf[PCAP_ERRBUF_SIZE];
	struct bpf_program comp_filter_exp;		/** The compiled filter expression */
	char filter_exp[] = "ether dst 91:E0:F0:00:0e:80";	/** The filter expression */
	struct pcap_pkthdr header;	/** header pcap gives us */
	const u_char* packet;		/** actual packet */
	
	signal(SIGINT, shutdown_all);
	
	int c;
	while((c = getopt(argc, argv, "hi:")) > 0) 
	{
		switch (c) 
		{
		case 'h': 
			help();
			break;
		case 'i':
			dev = strdup(optarg);
			break;
		default:
          		fprintf(stderr, "Unrecognized option!\n");
		}
	}

	if (NULL == dev) {
		help();
	}

	if (create_socket()) {
		fprintf(stderr, "Socket creation failed.\n");
		return (errno);
	}

	send_process('D'); /** report domain status */

	init_jack();
	
	fprintf(stdout,"Waiting for talker...\n");
	await_talker();	

	send_process('R'); /** send_ready */

#ifdef LIBSND
	char* filename = "listener.wav";
	SF_INFO* sf_info = (SF_INFO*)malloc(sizeof(SF_INFO));

	memset(sf_info, 0, sizeof(SF_INFO));

	sf_info->samplerate = SAMPLES_PER_SECOND;
	sf_info->channels = CHANNELS;
	sf_info->format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;

	if (0 == sf_format_check(sf_info)) {
		fprintf(stderr, "Wrong format.\n");
		shutdown_all(0);
		return -1;
	}

	if (NULL == (snd_file = sf_open(filename, SFM_WRITE, sf_info))) {
		fprintf(stderr, "Could not create file %s.\n", filename);
		shutdown_all(0);
		return -1;
	}
#endif

	/** session, get session handler */
	handle = pcap_open_live(dev, BUFSIZ, 1, -1, errbuf);
	if (NULL == handle) {
		fprintf(stderr, "Could not open device %s: %s.\n", dev, errbuf);
		shutdown_all(0);
		return -1;
	}

	/** compile and apply filter */
	if (-1 == pcap_compile(handle, &comp_filter_exp, filter_exp, 0, PCAP_NETMASK_UNKNOWN)) {
		fprintf(stderr, "Could not parse filter %s: %s.\n", filter_exp, pcap_geterr(handle));
		shutdown_all(0);
		return -1;
	}

	if (-1 == pcap_setfilter(handle, &comp_filter_exp)) {
		fprintf(stderr, "Could not install filter %s: %s.\n", filter_exp, pcap_geterr(handle));
		shutdown_all(0);
		return -1;
	}
	
	/** loop forever and call callback-function for every received packet */
	pcap_loop(handle, -1, pcap_callback, NULL);

	usleep(-1);
	return 0;
}
