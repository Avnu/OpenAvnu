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
#include "openavb_rawsock.h"
#include "openavb_avtp.h"
#ifdef UBUNTU
static int   ethertype = ETHERTYPE_AVTP;
#else
static int   ethertype = ETHERTYPE_8021Q;
#endif

#define MAX_NUM_FRAMES 10

static bool bRunning = TRUE;

static char* interface = NULL;

#define NUM_STREAMS 	256
#define REPORT_SECONDS  10

static GOptionEntry entries[] =
{
  { "interface", 'i', 0, G_OPTION_ARG_STRING, &interface, "network interface", "NAME" },
  { NULL }
};

int timespec_cmp(struct timespec *a, struct timespec *b)
{
	if (a->tv_sec > b->tv_sec)
		return 1;
	else if (a->tv_sec < b->tv_sec)
		return -1;
	else {
		if (a->tv_nsec > b->tv_nsec)
			return 1;
		else if (a->tv_nsec < b->tv_nsec)
			return -1;
	}
	return 0;
}

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

	context = g_option_context_new("- rawsock listenr");
	g_option_context_add_main_entries(context, entries, NULL);
	if (!g_option_context_parse(context, &argc, &argv, &error))
	{
		printf("error: %s\n", error->message);
		exit(1);
	}

	if (interface == NULL) {
		printf("error: must specify network interface\n");
		exit(2);
	}

	void* rs = openavbRawsockOpen(interface, TRUE, FALSE, ethertype, 0, MAX_NUM_FRAMES);
	if (!rs) {
		printf("error: failed to open raw socket (are you root?)\n");
		exit(3);
	}

	hdr_info_t hdr;
	U8 *pBuf, *pFrame, tmp8;
	U32 offset, len;
	U16 uid, i;

	long nTotal = 0, nRecv[NUM_STREAMS];
	for (i = 0; i < NUM_STREAMS; i++)
		nRecv[i] = 0;
	
	struct timespec now, report;
	clock_gettime(CLOCK_MONOTONIC, &report);
	report.tv_sec += REPORT_SECONDS;

	while (bRunning) {
		pBuf = openavbRawsockGetRxFrame(rs, 1000, &offset, &len);
		if (pBuf) {
			pFrame = pBuf + offset;

			offset = openavbRawsockRxParseHdr(rs, pBuf, &hdr);
			if ((int)offset < 0) {
				printf("error parsing frame header");
			}
			else {
#ifndef UBUNTU
				if (hdr.ethertype == ETHERTYPE_8021Q) {
					// Oh!  Need to look past the VLAN tag
					U16 vlan_bits = ntohs(*(U16 *)(pFrame + offset));
					hdr.vlan = TRUE;
					hdr.vlan_vid = vlan_bits & 0x0FFF;
					hdr.vlan_pcp = (vlan_bits >> 13) & 0x0007;
					offset += 2;
					hdr.ethertype = ntohs(*(U16 *)(pFrame + offset));
					offset += 2;
				}
#endif

				if (hdr.ethertype == ETHERTYPE_AVTP) {
					//dumpFrame(pFrame + offset, len - offset, &hdr);

					// Look for stream data frames
					// (ignore control frames, including MAAP)
					tmp8 = *(pFrame + offset);
					if ((tmp8 & 0x80) == 0) {
						// Find the unique ID in the streamID
						uid = htons(*(U16*)(pFrame + offset + 10));
						if (uid < NUM_STREAMS)
							nRecv[uid]++;
						nTotal++;
					}
				}
			}

			openavbRawsockRelRxFrame(rs, pBuf);
		}
		
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (timespec_cmp(&now, &report) >= 0) {
			printf("total=%ld\t", nTotal);
			nTotal = 0;
			for (i = 0; i < NUM_STREAMS-1; i++) {
				if (nRecv[i] > 0) {
					printf("%d=%ld\t", i, nRecv[i]);
					nRecv[i] = 0;
				}
			}
			printf("\n");
			report.tv_sec += REPORT_SECONDS;
		}
	}

	openavbRawsockClose(rs);
	return 0;
}
