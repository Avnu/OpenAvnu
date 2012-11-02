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

#include "mrpdclient.h"

/* global variables */
static SOCKET control_socket = SOCKET_ERROR;
static int udp_port = 0;

int mrpdclient_init(int port)
{
	struct sockaddr_in addr;
	int rc;

	control_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (control_socket == INVALID_SOCKET)
		goto out;

	rc = setsockopt(control_socket, SOL_SOCKET, SO_RCVTIMEO,
			(const char *)&rcv_timeout, sizeof(rcv_timeout));
	if (rc != 0)
		goto out;
	rc = setsockopt(control_socket, SOL_SOCKET, SO_SNDTIMEO,
			(const char *)&rcv_timeout, sizeof(rcv_timeout));
	if (rc != 0)
		goto out;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(0);

	rc = bind(control_socket, (struct sockaddr *)&addr,
		  sizeof(struct sockaddr));
	if (rc <= SOCKET_ERROR)
		goto out;

	udp_port = port;
	return (0);

 out:
	if (control_socket != SOCKET_ERROR)
		closesocket(control_socket);
	control_socket = SOCKET_ERROR;
	return (-1);
}

int mprdclient_close(void)
{
	if (control_socket != SOCKET_ERROR)
		closesocket(control_socket);
	return 0;
}

int mrpdclient_recv(ptr_process_msg fn)
{
	char *msgbuf;
	int bytes = 0;

	msgbuf = (char *)malloc(MRPDCLIENT_MAX_FRAME_SIZE);
	if (NULL == msgbuf)
		return -1;

	memset(msgbuf, 0, MRPDCLIENT_MAX_FRAME_SIZE);
	bytes = recv(control_socket, msgbuf, MRPDCLIENT_MAX_FRAME_SIZE, 0);
	if (bytes <= SOCKET_ERROR) {
		goto out;
	}

	return fn(msgbuf, bytes);
 out:
	free(msgbuf);
	return (-1);
}

int mprdclient_sendto(char *notify_data, int notify_len)
{
	struct sockaddr_in addr;
	socklen_t addr_len;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(udp_port);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr_len = sizeof(addr);

	if (control_socket != SOCKET_ERROR)
		return (sendto
			(control_socket, notify_data, notify_len, 0,
			 (struct sockaddr *)&addr, addr_len));
	else
		return (0);
}
