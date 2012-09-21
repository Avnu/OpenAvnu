/******************************************************************************

  Copyright (c) 2012, Intel Corporation 
  Copyright (c) 2012, AudioScience, Inc
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

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#if defined WIN32
#include <winsock2.h>
#endif

#include "mrpd.h"
#include "mrpdclient.h"

/* global variables */
#define VERSION_STR	"0.0"

static const char *version_str =
    "mrpq v" VERSION_STR "\n" "Copyright (c) 2012, Intel Corporation\n";

int process_ctl_msg(char *buf, int buflen)
{
	(void)buflen;

	/*
	 * M?? - query MMRP Registrar MAC Address database
	 * M+? - JOIN_MT a MAC address
	 * -- note M++ doesn't exist apparently- MMRP doesn't use 'New' --
	 * M-- - LV a MAC address
	 * V?? - query MVRP Registrar VID database
	 * V++ - JOIN_IN a VID (VLAN ID)
	 * V+? - JOIN_MT a VID (VLAN ID)
	 * V-- - LV a VID (VLAN ID)
	 */

	/* XXX */
	printf("<%s\n", buf);
	fflush(stdout);
	free(buf);
	return (0);
}

void process_events(void)
{

	/* wait for events, demux the received packets, process packets */
}

int main(int argc, char *argv[])
{
	int rc = 0;
	char *msgbuf;
#if defined WIN32
	WSADATA wsa_data;
#endif
	(void)argc;
	(void)argv;

#if defined WIN32
	WSAStartup(MAKEWORD(1, 1), &wsa_data);
#endif

	printf("%s\n", version_str);

	rc = mrpdclient_init(MRPD_PORT_DEFAULT);
	if (rc) {
		printf("init failed\n");
		return -1;
	}

	msgbuf = (char *)malloc(1500);

	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "M??");
	printf(">M??\n");
	rc = mprdclient_sendto(msgbuf, 1500);
	rc = mrpdclient_recv(process_ctl_msg);
	if (rc <= SOCKET_ERROR)
		printf("recv error\n");

	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "V??");
	printf(">V??\n");
	rc = mprdclient_sendto(msgbuf, 1500);
	rc = mrpdclient_recv(process_ctl_msg);
	if (rc <= SOCKET_ERROR)
		printf("recv error\n");

	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S??");
	printf(">S??\n");
	rc = mprdclient_sendto(msgbuf, 1500);
	rc = mrpdclient_recv(process_ctl_msg);
	if (rc <= SOCKET_ERROR)
		printf("recv error\n");

	sprintf(msgbuf, "BYE");
	rc = mprdclient_sendto(msgbuf, 1500);
	mprdclient_close();

#if defined WIN32
	WSACleanup();
#endif
	return (rc);
}
