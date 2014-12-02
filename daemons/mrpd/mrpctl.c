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

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "mrpd.h"
#include "mrpdclient.h"

#define VERSION_STR	"0.0"
static const char *version_str =
	"mrpctl v" VERSION_STR "\n"
	"Copyright (c) 2012, Intel Corporation\n";

int process_ctl_msg(char *buf, int buflen)
{
	(void)buflen; /* unused */

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

	if (buf[1] == ':')
		printf("?? RESP:\n%s", buf);
	else
		printf("MRPD ---> %s", buf);
	fflush(stdout);
	free(buf);

	return 0;
}

void usage(void)
{
	fprintf(stderr, 
		"\n"
		"usage: mrpctl [-h]"
		"\n"
		"options:\n"
		"    -h  show this message\n"
		"\n"
		"%s"
		"\n", version_str);
	exit(1);
}


int main(int argc, char *argv[])
{
	int c;
	SOCKET mrpd_sock = SOCKET_ERROR;
	int rc = 0;
	char *msgbuf;

	for (;;) {
		c = getopt(argc, argv, "h");
		if (c < 0)
			break;
		switch (c) {
		case 'h':
			usage();
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

#ifdef XXX
	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"M++:M=010203040506");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;
	
	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"M++:M=ffffffffffff");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"V++:I=0002");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"M++:M=060504030201");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"M++:S=1");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"M--:M=060504030201");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"M--:S=1");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"M+?:M=060504030201");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"M+?:S=1");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"M--:M=060504030201");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"M--:S=1");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"V--:I=0002");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"V+?:I=0002");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"V--:I=0002");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"S+?:S=DEADBEEFBADFCA11,A=112233445566,V=0002,Z=576,I=8000,P=96,L=1000");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"S--:S=DEADBEEFBADFCA11");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"S++:S=FFEEDDCCBBAA9988,A=112233445567,V=0002,Z=576,I=8000,P=96,L=1000");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"S+L:L=DEADBEEFBADFCA11,D=2");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"S+L:L=F00F00F00F00F000,D=2");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf, "S+D:C=6,P=3,V=0002");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf, "S-D:C=6,P=3,V=0002");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"S-L:L=F00F00F00F00F000");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;
#endif /* XXX */

	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"V++:I=0002"); /* JOIN_IN VID 2: SR_PVID, SRP traffic */
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;

#ifdef YYYY	
	memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf,"S+L:L=0050c24edb0a0001,D=2");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (-1 == rc) goto out;
#endif /* YYYY */

	do {
		/* query MMRP Registrar MAC Address database */
		memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
		sprintf(msgbuf,"M??");
		rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
		if (-1 == rc) goto out;

		/* query MVRP Registrar VID database */
		memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
		sprintf(msgbuf,"V??");
		rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
		if (-1 == rc) goto out;

		/* query MSRP Registrar database */
		memset(msgbuf,0,MRPDCLIENT_MAX_MSG_SIZE);
		sprintf(msgbuf,"S??");
		rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
		if (-1 == rc) goto out;

		/* yield replies */
		rc = mrpdclient_recv(mrpd_sock, process_ctl_msg);
		if (-1 == rc) goto out;

		sleep(1);
	} while (1);

	free(msgbuf);
	rc = mrpdclient_close(&mrpd_sock);

out:
	if (-1 == rc)
		return EXIT_FAILURE;
	else
		return EXIT_SUCCESS;
}
