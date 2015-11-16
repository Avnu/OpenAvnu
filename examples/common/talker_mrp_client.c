/******************************************************************************

  Copyright (c) 2012, Intel Corporation
  Copyright (c) 2014, Parrot SA
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

#include "talker_mrp_client.h"

/* global variables */

volatile int mrp_okay;
volatile int mrp_error = 0;;
pthread_t monitor_thread;
pthread_attr_t monitor_attr;


/*
 * private
 */

int mrp_talker_client_init(struct mrp_talker_ctx *ctx)
{
	int i;
	ctx->control_socket = -1;
	ctx->halt_tx = 0;
	ctx->listeners = 0;
	ctx->domain_a_valid = 0;
	ctx->domain_class_a_id = 0;
	ctx->domain_class_a_priority = 0;
	ctx->domain_class_a_vid = 0;
	ctx->domain_b_valid = 0;
	ctx->domain_class_b_id = 0;
	ctx->domain_class_b_priority = 0;
	ctx->domain_class_b_vid = 0;
	for (i=0;i<8;i++)
	{
		ctx->monitor_stream_id[i] = 0;
	}
	return 0;
}

int send_mrp_msg(char *notify_data, int notify_len, struct mrp_talker_ctx *ctx)
{
	struct sockaddr_in addr;
	socklen_t addr_len;

	if (ctx->control_socket == -1)
		return -1;
	if (notify_data == NULL)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MRPD_PORT_DEFAULT);
	inet_aton("127.0.0.1", &addr.sin_addr);
	addr_len = sizeof(addr);
	return sendto(ctx->control_socket, notify_data, notify_len, 0,
			 (struct sockaddr *)&addr, addr_len);
}

int process_mrp_msg(char *buf, int buflen, struct mrp_talker_ctx *ctx)
{

	/*
	 * 1st character indicates application
	 * [MVS] - MAC, VLAN or STREAM
	 */
	unsigned int id;
	unsigned int priority;
	unsigned int vid;
	int i, j, k;
	unsigned int substate;
	unsigned char recovered_streamid[8];
	k = 0;
 next_line:if (k >= buflen)
		return 0;
	switch (buf[k]) {
	case 'E':
		printf("%s from mrpd\n", buf);
		fflush(stdout);
		mrp_error = 1;
		break;
	case 'O':
		mrp_okay = 1;
		break;
	case 'M':
	case 'V':
		printf("%s unhandled from mrpd\n", buf);
		fflush(stdout);

		/* unhandled for now */
		break;
	case 'L':

		/* parse a listener attribute - see if it matches our monitor_stream_id */
		i = k;
		while (buf[i] != 'D')
			i++;
		i += 2;		/* skip the ':' */
		sscanf(&(buf[i]), "%d", &substate);
		while (buf[i] != 'S')
			i++;
		i += 2;		/* skip the ':' */
		for (j = 0; j < 8; j++) {
			sscanf(&(buf[i + 2 * j]), "%02x", &id);
			recovered_streamid[j] = (unsigned char)id;
		} printf
		    ("FOUND STREAM ID=%02x%02x%02x%02x%02x%02x%02x%02x ",
		     recovered_streamid[0], recovered_streamid[1],
		     recovered_streamid[2], recovered_streamid[3],
		     recovered_streamid[4], recovered_streamid[5],
		     recovered_streamid[6], recovered_streamid[7]);
		switch (substate) {
		case 0:
			printf("with state ignore\n");
			break;
		case 1:
			printf("with state askfailed\n");
			break;
		case 2:
			printf("with state ready\n");
			break;
		case 3:
			printf("with state readyfail\n");
			break;
		default:
			printf("with state UNKNOWN (%d)\n", substate);
			break;
		}
		if (substate > MSRP_LISTENER_ASKFAILED) {
			if (memcmp
			    (recovered_streamid, ctx->monitor_stream_id,
			     sizeof(recovered_streamid)) == 0) {
				ctx->listeners = 1;
				printf("added listener\n");
			}
		}
		fflush(stdout);

		/* try to find a newline ... */
		while ((i < buflen) && (buf[i] != '\n') && (buf[i] != '\0'))
			i++;
		if (i == buflen)
			return 0;
		if (buf[i] == '\0')
			return 0;
		i++;
		k = i;
		goto next_line;
		break;
	case 'D':
		i = k + 4;

		/* save the domain attribute */
		sscanf(&(buf[i]), "%d", &id);
		while (buf[i] != 'P')
			i++;
		i += 2;		/* skip the ':' */
		sscanf(&(buf[i]), "%d", &priority);
		while (buf[i] != 'V')
			i++;
		i += 2;		/* skip the ':' */
		sscanf(&(buf[i]), "%x", &vid);
		if (id == 6) {
			ctx->domain_class_a_id = id;
			ctx->domain_class_a_priority = priority;
			ctx->domain_class_a_vid = vid;
			ctx->domain_a_valid = 1;
		} else {
			ctx->domain_class_b_id = id;
			ctx->domain_class_b_priority = priority;
			ctx->domain_class_b_vid = vid;
			ctx->domain_b_valid = 1;
		}
		while ((i < buflen) && (buf[i] != '\n') && (buf[i] != '\0'))
			i++;
		if ((i == buflen) || (buf[i] == '\0'))
			return 0;
		i++;
		k = i;
		goto next_line;
		break;
	case 'T':

		/* as simple_talker we don't care about other talkers */
		i = k;
		while ((i < buflen) && (buf[i] != '\n') && (buf[i] != '\0'))
			i++;
		if (i == buflen)
			return 0;
		if (buf[i] == '\0')
			return 0;
		i++;
		k = i;
		goto next_line;
		break;
	case 'S':

		/* handle the leave/join events */
		switch (buf[k + 4]) {
		case 'L':
			i = k + 5;
			while (buf[i] != 'D')
				i++;
			i += 2;	/* skip the ':' */
			sscanf(&(buf[i]), "%d", &substate);
			while (buf[i] != 'S')
				i++;
			i += 2;	/* skip the ':' */
			for (j = 0; j < 8; j++) {
				sscanf(&(buf[i + 2 * j]), "%02x", &id);
				recovered_streamid[j] = (unsigned char)id;
			} printf
			    ("EVENT on STREAM ID=%02x%02x%02x%02x%02x%02x%02x%02x ",
			     recovered_streamid[0], recovered_streamid[1],
			     recovered_streamid[2], recovered_streamid[3],
			     recovered_streamid[4], recovered_streamid[5],
			     recovered_streamid[6], recovered_streamid[7]);
			switch (substate) {
			case 0:
				printf("with state ignore\n");
				break;
			case 1:
				printf("with state askfailed\n");
				break;
			case 2:
				printf("with state ready\n");
				break;
			case 3:
				printf("with state readyfail\n");
				break;
			default:
				printf("with state UNKNOWN (%d)\n", substate);
				break;
			}
			switch (buf[k + 1]) {
			case 'L':
				printf("got a leave indication\n");
				if (memcmp
				    (recovered_streamid, ctx->monitor_stream_id,
				     sizeof(recovered_streamid)) == 0) {
					ctx->listeners = 0;
					printf("listener left\n");
				}
				break;
			case 'J':
			case 'N':
				printf("got a new/join indication\n");
				if (substate > MSRP_LISTENER_ASKFAILED) {
					if (memcmp
					    (recovered_streamid,
					     ctx->monitor_stream_id,
					     sizeof(recovered_streamid)) == 0)
						ctx->listeners = 1;
				}
				break;
			}

			/* only care about listeners ... */
		default:
			return 0;
			break;
		}
		break;
	case '\0':
		break;
	}
	return 0;
}

void *mrp_monitor_thread(void *arg)
{
	char *msgbuf;
	struct sockaddr_in client_addr;
	struct msghdr msg;
	struct iovec iov;
	int bytes = 0;
	struct pollfd fds;
	int rc;
	struct mrp_talker_ctx *ctx = (struct mrp_talker_ctx*) arg;

	msgbuf = (char *)malloc(MAX_MRPD_CMDSZ);
	if (NULL == msgbuf)
		return NULL;
	while (!ctx->halt_tx) {
		fds.fd = ctx->control_socket;
		fds.events = POLLIN;
		fds.revents = 0;
		rc = poll(&fds, 1, 100);
		if (rc < 0) {
			free(msgbuf);
			pthread_exit(NULL);
		}
		if (rc == 0)
			continue;
		if ((fds.revents & POLLIN) == 0) {
			free(msgbuf);
			pthread_exit(NULL);
		}
		memset(&msg, 0, sizeof(msg));
		memset(&client_addr, 0, sizeof(client_addr));
		memset(msgbuf, 0, MAX_MRPD_CMDSZ);
		iov.iov_len = MAX_MRPD_CMDSZ;
		iov.iov_base = msgbuf;
		msg.msg_name = &client_addr;
		msg.msg_namelen = sizeof(client_addr);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		bytes = recvmsg(ctx->control_socket, &msg, 0);
		if (bytes < 0)
			continue;
		process_mrp_msg(msgbuf, bytes, ctx);
	}
	free(msgbuf);
	pthread_exit(NULL);
}

/*
 * public
 */

int mrp_connect(struct mrp_talker_ctx *ctx)
{
	struct sockaddr_in addr;
	int sock_fd = -1;
	int rc;
	sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock_fd < 0)
		goto out;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MRPD_PORT_DEFAULT);
	inet_aton("127.0.0.1", &addr.sin_addr);
	memset(&addr, 0, sizeof(addr));
	ctx->control_socket = sock_fd;
	rc = pthread_attr_init(&monitor_attr);
	rc |= pthread_create(&monitor_thread, NULL, mrp_monitor_thread, ctx);
	return rc;
	return 0;
 out:	if (sock_fd != -1)
		close(sock_fd);
	sock_fd = -1;
	return -1;
}

int mrp_disconnect(struct mrp_talker_ctx *ctx)
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 64);
	sprintf(msgbuf, "BYE");
	mrp_okay = 0;
	rc = send_mrp_msg(msgbuf, 1500, ctx);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int mrp_monitor(void)
{
	int rc;
	rc = pthread_attr_init(&monitor_attr);
	rc |= pthread_create(&monitor_thread, NULL, mrp_monitor_thread, NULL);
	return rc;
}

int mrp_register_domain(struct mrp_domain_attr *reg_class, struct mrp_talker_ctx *ctx)
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);

	sprintf(msgbuf, "S+D:C=%d,P=%d,V=%04x", reg_class->id, reg_class->priority, reg_class->vid);
	mrp_okay = 0;
	rc = send_mrp_msg(msgbuf, 1500, ctx);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}


int
mrp_advertise_stream(uint8_t * streamid,
		     uint8_t * destaddr,
		     int pktsz, int interval, int latency, struct mrp_talker_ctx *ctx)
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);

	sprintf(msgbuf, "S++:S=%02X%02X%02X%02X%02X%02X%02X%02X"
		",A=%02X%02X%02X%02X%02X%02X"
		",V=%04X"
		",Z=%d"
		",I=%d"
		",P=%d"
		",L=%d", streamid[0], streamid[1], streamid[2],
		streamid[3], streamid[4], streamid[5], streamid[6],
		streamid[7], destaddr[0], destaddr[1], destaddr[2],
		destaddr[3], destaddr[4], destaddr[5], ctx->domain_class_a_vid, pktsz,
		interval, ctx->domain_class_a_priority << 5, latency);
	mrp_okay = 0;
	rc = send_mrp_msg(msgbuf, 1500, ctx);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int
mrp_unadvertise_stream(uint8_t * streamid,
		       uint8_t * destaddr,
		       int pktsz, int interval, int latency, struct mrp_talker_ctx *ctx)
{
	char *msgbuf;
	int rc;
	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S--:S=%02X%02X%02X%02X%02X%02X%02X%02X"
		",A=%02X%02X%02X%02X%02X%02X"
		",V=%04X"
		",Z=%d"
		",I=%d"
		",P=%d"
		",L=%d", streamid[0], streamid[1], streamid[2],
		streamid[3], streamid[4], streamid[5], streamid[6],
		streamid[7], destaddr[0], destaddr[1], destaddr[2],
		destaddr[3], destaddr[4], destaddr[5], ctx->domain_class_a_vid, pktsz,
		interval, ctx->domain_class_a_priority << 5, latency);
	mrp_okay = 0;
	rc = send_mrp_msg(msgbuf, 1500, ctx);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}


int mrp_await_listener(unsigned char *streamid, struct mrp_talker_ctx *ctx)
{
	char *msgbuf;
	int rc;

	memcpy(ctx->monitor_stream_id, streamid, sizeof(ctx->monitor_stream_id));
	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S??");
	rc = send_mrp_msg(msgbuf, 1500, ctx);
	free(msgbuf);
	if (rc != 1500)
		return -1;

	/* either already there ... or need to wait ... */
	while (!ctx->halt_tx && (ctx->listeners == 0))
		usleep(20000);

	return 0;
}

/*
 * actually not used
 */

int mrp_get_domain(struct mrp_talker_ctx *ctx,struct mrp_domain_attr *class_a,struct mrp_domain_attr *class_b)
{
	char *msgbuf;
	int ret;

	/* we may not get a notification if we are joining late,
	 * so query for what is already there ...
	 */
	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S??");
	ret = send_mrp_msg(msgbuf, 1500, ctx);
	free(msgbuf);
	if (ret != 1500)
		return -1;
	while (!ctx->halt_tx && (ctx->domain_a_valid == 0) && (ctx->domain_b_valid == 0))
		usleep(20000);
	class_a->id = 0;
	class_a->priority = 0;
	class_a->vid = 0;
	class_b->id = 0;
	class_b->priority = 0;
	class_b->vid = 0;
	if (ctx->domain_a_valid) {
		class_a->id = ctx->domain_class_a_id;
		class_a->priority = ctx->domain_class_a_priority;
		class_a->vid = ctx->domain_class_a_vid;
	}
	if (ctx->domain_b_valid) {
		class_b->id = ctx->domain_class_b_id;
		class_b->priority = ctx->domain_class_b_priority;
		class_b->vid = ctx->domain_class_b_vid;
	}
	return 0;
}

int mrp_join_vlan(struct mrp_domain_attr *reg_class, struct mrp_talker_ctx *ctx)
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "V++:I=%04x\n",reg_class->vid);
	rc = send_mrp_msg(msgbuf, 1500, ctx);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}


// TODO remove
int recv_mrp_okay()
{
	while ((mrp_okay == 0) && (mrp_error == 0))
		usleep(20000);
	return 0;
}
