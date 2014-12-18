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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined WIN32
#include <winsock2.h>
#endif

#include "mrpd.h"
#include "mrpdclient.h"

#define VERSION_STR	"0.0"

static const char *version_str =
    "mrpl v" VERSION_STR "\n" "Copyright (c) 2012, Intel Corporation\n";

static void usage(void)
{
	fprintf(stderr,
		"\n"
		"usage: mrpl [-lj]"
		"\n"
		"options:\n"
		"    -h  show this message\n"
		"    -l  leave a stream\n"
		"    -j  join a steam\n" "\n" "%s" "\n", version_str);
	exit(1);
}

int main(int argc, char *argv[])
{
	int c;
	SOCKET mrpd_sock = SOCKET_ERROR;
	int rc = 0;
	char *msgbuf;
	int leave = 0;

#if defined WIN32
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(1, 1), &wsa_data);
#endif

	for (;;) {
		c = getopt(argc, argv, "hlj");

		if (c < 0)
			break;

		switch (c) {
		case 'h':
			usage();
			break;
		case 'l':
			leave = 1;
			break;
		case 'j':
			leave = 0;
			break;
		}
	}
	if (optind < argc)
		usage();

	mrpd_sock = mrpdclient_init();
	if (mrpd_sock == SOCKET_ERROR) {
		printf("mrpdclient_init failed\n");
		return EXIT_FAILURE;
	}

	msgbuf = malloc(MRPDCLIENT_MAX_MSG_SIZE);
	if (NULL == msgbuf) {
		printf("malloc failed\n");
		return EXIT_FAILURE;
	}

	memset(msgbuf, 0, MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf, "S+D:C=6,P=3,V=0002");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (rc != MRPDCLIENT_MAX_MSG_SIZE) {
		printf("mrpdclient_sendto failed\n");
		return EXIT_FAILURE;
	}

	memset(msgbuf, 0, MRPDCLIENT_MAX_MSG_SIZE);
	if (leave)
		sprintf(msgbuf, "S-L:L=A0369F022EEE0000,D=2");
	else
		sprintf(msgbuf, "S+L:L=A0369F022EEE0000,D=2");

	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	free(msgbuf);
	if (rc != MRPDCLIENT_MAX_MSG_SIZE) {
		printf("mrpdclient_sendto failed\n");
		return EXIT_FAILURE;
	}

	rc = mrpdclient_close(&mrpd_sock);
	if (-1 == rc)
		return EXIT_FAILURE;
	else
		return EXIT_SUCCESS;
}
