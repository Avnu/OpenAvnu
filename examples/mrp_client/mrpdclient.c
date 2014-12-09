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

#if defined WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
static int rcv_timeout = 100;	/* 100 ms */
#elif defined __linux__
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1
#define closesocket(s) close(s);
static struct timeval rcv_timeout = {
	.tv_sec = 0,
	.tv_usec = 100 * 1000	/* 100 ms */
};
#endif

#include "mrpd.h"
#include "mrpdclient.h"

int mrpdclient_init(void)
{
	SOCKET mrpd_sock = SOCKET_ERROR;
	struct sockaddr_in addr;
	int rc;

	mrpd_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (mrpd_sock == INVALID_SOCKET)
		goto out;

	rc = setsockopt(mrpd_sock, SOL_SOCKET, SO_RCVTIMEO,
			(const char *)&rcv_timeout, sizeof(rcv_timeout));
	if (rc != 0)
		goto out;
	rc = setsockopt(mrpd_sock, SOL_SOCKET, SO_SNDTIMEO,
			(const char *)&rcv_timeout, sizeof(rcv_timeout));
	if (rc != 0)
		goto out;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(0);

	rc = bind(mrpd_sock, (struct sockaddr *)&addr,
		  sizeof(struct sockaddr));
	if (rc <= SOCKET_ERROR)
		goto out;

	return mrpd_sock;

 out:
	if (mrpd_sock != SOCKET_ERROR)
		closesocket(mrpd_sock);

	return SOCKET_ERROR;
}

int mrpdclient_close(SOCKET *mrpd_sock)
{
	int rc;

	if (NULL == mrpd_sock)
		return -1;

	rc = mrpdclient_sendto(*mrpd_sock, "BYE", strlen("BYE") + 1);
	if (rc != strlen("BYE") + 1)
		return -1;

	if (SOCKET_ERROR != *mrpd_sock)
		closesocket(*mrpd_sock);
	*mrpd_sock = SOCKET_ERROR;

	return 0;
}

int mrpdclient_recv(SOCKET mrpd_sock, ptr_process_mrpd_msg fn)
{
	char *msgbuf;
	int bytes = 0;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	msgbuf = (char *)malloc(MRPDCLIENT_MAX_MSG_SIZE);
	if (NULL == msgbuf)
		return -1;

	memset(msgbuf, 0, MRPDCLIENT_MAX_MSG_SIZE);
	bytes = recv(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE, 0);
	if (bytes <= SOCKET_ERROR) {
		goto out;
	}

	return fn(msgbuf, bytes);
 out:
	free(msgbuf);
	return (-1);
}

int mrpdclient_sendto(SOCKET mrpd_sock, char *notify_data, int notify_len)
{
	struct sockaddr_in addr;
	socklen_t addr_len;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MRPD_PORT_DEFAULT);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr_len = sizeof(addr);

	return sendto(mrpd_sock, notify_data, notify_len, 0,
		(struct sockaddr *)&addr, addr_len);
}
