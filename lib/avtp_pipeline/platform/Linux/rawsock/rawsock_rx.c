/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
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
 
Attributions: The inih library portion of the source code is licensed from 
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt. 
Complete license and copyright information can be found at 
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <glib.h>
#include "./openavb_rawsock.h"

// Common usage with VTAG 0x8100:				./rawsock_rx -i eth0 -t 33024 -d 1 -s 1 
// Common usage without VTAG 0x22F0:			./rawsock_rx -i eth0 -t 8944 -d 1 -s 1

#define MAX_NUM_FRAMES 10
#define TIMESPEC_TO_NSEC(ts) (((uint64_t)ts.tv_sec * (uint64_t)NANOSECONDS_PER_SECOND) + (uint64_t)ts.tv_nsec)

static bool bRunning = TRUE;

static char* interface = NULL;
static int ethertype = -1;
static char* macaddr_s = NULL;
static int dumpFlag = 0;
static int reportSec = 1;

static GOptionEntry entries[] =
{
  { "interface", 'i', 0, G_OPTION_ARG_STRING, &interface, "network interface",               "NAME" },
  { "ethertype", 't', 0, G_OPTION_ARG_INT,    &ethertype, "ethernet protocol",               "NUM" },
  { "mac",       'a', 0, G_OPTION_ARG_STRING, &macaddr_s, "MAC address",                     "MAC" },
  { "dump",      'd', 0, G_OPTION_ARG_INT,    &dumpFlag,  "Dump packets (1=yes, 0=no)",      "DUMP" }, 
  { "rptsec",    's', 0, G_OPTION_ARG_INT,    &reportSec, "report interval in seconds",      "RPTSEC" },
  { NULL }
};

void dumpAscii(U8 *pFrame, int i, int *j)
{
	char c;

	printf("  ");
	
	while (*j <= i) {
		c = pFrame[*j];
		*j += 1;
		if (!isprint(c) || isspace(c))
			c = '.';
		printf("%c", c);
	}
}

void dumpFrameContent(U8 *pFrame, U32 len)
{
	int i = 0, j = 0;
	while (TRUE) {
		if (i % 16 == 0) {
			if (i != 0 ) {
				// end of line stuff
				dumpAscii(pFrame, (i < len ? i : len), &j);
				printf("\n");

				if (i >= len)
					break;
			}
			if (i+1 < len) {
				// start of line stuff
				printf("0x%4.4d:  ", i);
			}
		}
		else if (i % 2 == 0) {
			printf("  ");
		}

		if (i >= len)
			printf("  ");
		else
			printf("%2.2x", pFrame[i]);

		i += 1;
	}
}

void dumpFrame(U8 *pFrame, U32 len, hdr_info_t *hdr)
{
	printf("Frame received, ethertype=0x%x len=%u\n", hdr->ethertype, len);
	printf("src: %s\n", ether_ntoa((const struct ether_addr*)hdr->shost));
	printf("dst: %s\n", ether_ntoa((const struct ether_addr*)hdr->dhost));
	if (hdr->vlan) {
		printf("VLAN pcp=%u, vid=%u\n", (unsigned)hdr->vlan_pcp, hdr->vlan_vid); 
	}
	dumpFrameContent(pFrame, len);
	printf("\n");
}

int main(int argc, char* argv[])
{
	GError *error = NULL;
	GOptionContext *context;
	//U8 *macaddr;
	struct ether_addr *macaddr;

	context = g_option_context_new("- rawsock listenr");
	g_option_context_add_main_entries(context, entries, NULL);
	if (!g_option_context_parse(context, &argc, &argv, &error))
	{
		printf("error: %s\n", error->message);
		exit(1);
	}

	if (interface == NULL || ethertype == -1) {
		printf("error: must specify network interface and ethertype\n");
		exit(2);
	}

	void* rs = openavbRawsockOpen(interface, TRUE, FALSE, ethertype, 0, MAX_NUM_FRAMES);
	if (!rs) {
		printf("error: failed to open raw socket (are you root?)\n");
		exit(3);
	}

	if (macaddr_s) {
	    macaddr = ether_aton(macaddr_s);
	    if (macaddr) {
	        // if (openavbRawsockRxMulticast(rs, TRUE, macaddr) == FALSE) {
			if (openavbRawsockRxMulticast(rs, TRUE, macaddr->ether_addr_octet) == FALSE) {
	            printf("error: failed to add multicast mac address\n");
	            exit(4);
	        }
	    }
	    else
	        printf("warning: failed to convert multicast mac address\n");
	}

	U8 *pBuf, *pFrame;
	U32 offset, len;
	hdr_info_t hdr;
	
	struct timespec now;
	static U32 packetCnt = 0;
	static U64 nextReportInterval = 0;

	clock_gettime(CLOCK_MONOTONIC, &now);
	nextReportInterval = TIMESPEC_TO_NSEC(now) + (NANOSECONDS_PER_SECOND * reportSec);

	while (bRunning) {
		pBuf = openavbRawsockGetRxFrame(rs, OPENAVB_RAWSOCK_BLOCK, &offset, &len);
		pFrame = pBuf + offset;
		openavbRawsockRxParseHdr(rs, pBuf, &hdr);
		if (dumpFlag) {
			dumpFrame(pFrame, len, &hdr);
		}
		openavbRawsockRelRxFrame(rs, pBuf);

		packetCnt++;

		clock_gettime(CLOCK_MONOTONIC, &now);
		U64 nowNSec = TIMESPEC_TO_NSEC(now);;

		if (reportSec > 0) {
			if (nowNSec > nextReportInterval) {
				printf("RX Packets: %d\n", packetCnt);
				packetCnt = 0;
				nextReportInterval = nowNSec + (NANOSECONDS_PER_SECOND * reportSec);
			}
		}
	}

	openavbRawsockClose(rs);
	return 0;
}
