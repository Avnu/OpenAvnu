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

#include "mrp_client.h"

#define AVB_LOG_COMPONENT "MRP"
#include "openavb_log.h"

/* global variables */

int control_socket = -1;

volatile int halt_tx = 0;
volatile int listeners = 0;
volatile int mrp_okay;
volatile int mrp_error = 0;;

volatile int domain_a_valid = 0;
int domain_class_a_id = 0;
int domain_class_a_priority = 0;
u_int16_t domain_class_a_vid = 0;

volatile int domain_b_valid = 0;
int domain_class_b_id = 0;
int domain_class_b_priority = 0;
u_int16_t domain_class_b_vid = 0;

pthread_t monitor_thread;
pthread_attr_t monitor_attr;
unsigned char monitor_stream_id[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/*
 * private
 */

int send_mrp_msg(char *notify_data, int notify_len)
{
	struct sockaddr_in addr;
	socklen_t addr_len;

	if (control_socket == -1)
		return -1;
	if (notify_data == NULL)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MRPD_PORT_DEFAULT);
	inet_aton("127.0.0.1", &addr.sin_addr);
	addr_len = sizeof(addr);
	return sendto(control_socket, notify_data, notify_len, 0,
			 (struct sockaddr *)&addr, addr_len);
}

int process_mrp_msg(char *buf, int buflen)
{
	/*
	 * 1st character indicates application
	 * [MVS] - MAC, VLAN or STREAM
	 */
	unsigned int id;
	unsigned int priority;
	unsigned int vid;
	unsigned int max_frame_size;
	unsigned int max_interval_frames;
	unsigned int priority_and_rank;
	unsigned int latency;
	int i, j;
	unsigned int substate;
	unsigned char recovered_streamid[8];
	unsigned char dest_addr[6];
	int offset = 0;

#if (AVB_LOG_LEVEL_DEBUG <= AVB_LOG_LEVEL)
	// Display this response in a user-friendly format.
	static char szMrpData[MAX_MRPD_CMDSZ];
	int nMrpDataLength = (buflen < MAX_MRPD_CMDSZ - 1 ? buflen : MAX_MRPD_CMDSZ - 1);
	strncpy(szMrpData, buf, nMrpDataLength);
	szMrpData[nMrpDataLength] = '\0';
	while (nMrpDataLength > 0 &&
			(buf[nMrpDataLength - 1] == '\0' || buf[nMrpDataLength - 1] == '\r' || buf[nMrpDataLength - 1] == '\n')) {
		// Remove the trailing whitespace.
		szMrpData[--nMrpDataLength] = '\0';
	}
	AVB_LOGF_DEBUG("MRP Response:  %s", szMrpData);
#endif

	while (offset < buflen && buf[offset] != '\0') {

		switch (buf[offset]) {
		case 'E':
			mrp_error = 1;
			break;

		case 'O':
			mrp_okay = 1;
			break;

		case 'M':
		case 'V':
			/* unhandled for now */
			break;

		case 'L':

			/* parse a listener attribute - see if it matches our monitor_stream_id */
			i = offset;
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
			}
			AVB_LOGF_DEBUG
				("FOUND STREAM ID=%02x%02x%02x%02x%02x%02x%02x%02x ",
				 recovered_streamid[0], recovered_streamid[1],
				 recovered_streamid[2], recovered_streamid[3],
				 recovered_streamid[4], recovered_streamid[5],
				 recovered_streamid[6], recovered_streamid[7]);
			switch (substate) {
			case 0:
				AVB_LOG_DEBUG("with state ignore");
				break;
			case 1:
				AVB_LOG_DEBUG("with state askfailed");
				break;
			case 2:
				AVB_LOG_DEBUG("with state ready");
				break;
			case 3:
				AVB_LOG_DEBUG("with state readyfail");
				break;
			default:
				AVB_LOGF_DEBUG("with state UNKNOWN (%d)", substate);
				break;
			}
			mrp_attach_cb(recovered_streamid, substate);
			if (substate > MSRP_LISTENER_ASKFAILED) {
				if (memcmp
					(recovered_streamid, monitor_stream_id,
					 sizeof(recovered_streamid)) == 0) {
					listeners = 1;
					AVB_LOG_DEBUG("added listener");
				}
			}
			break;

		case 'D':
			i = offset + 4;

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
				domain_class_a_id = id;
				domain_class_a_priority = priority;
				domain_class_a_vid = vid;
				domain_a_valid = 1;
			} else {
				domain_class_b_id = id;
				domain_class_b_priority = priority;
				domain_class_b_vid = vid;
				domain_b_valid = 1;
			}
			break;

		case 'T':
			i = offset;
			while (buf[i] != 'S')
				i++;
			// skip S=
			i += 2;
			for (j = 0; j < 8; j++) {
				sscanf(&(buf[i + 2 * j]), "%02x", &id);
				recovered_streamid[j] = (unsigned char)id;
			}
			while (buf[i] != 'A')
				i++;
			// skip A=
			i += 2;
			for (j = 0; j < 6; j++) {
				sscanf(&(buf[i + 2 * j]), "%02x", &id);
				dest_addr[j] = (unsigned char)id;
			}
			i+= j*2 + 1;

			sscanf(&(buf[i]), "V=%d,Z=%d,I=%d,P=%d,L=%d",
				   &vid,
				   &max_frame_size,
				   &max_interval_frames,
				   &priority_and_rank,
				   &latency);

			mrp_register_cb(recovered_streamid, 0, dest_addr, max_frame_size, max_interval_frames, vid, latency);
			break;

		case 'S':

			/* handle the leave/join events */
			switch (buf[offset + 4]) {
			case 'L':
				i = offset + 5;
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
				}
				AVB_LOGF_DEBUG
					("EVENT on STREAM ID=%02x%02x%02x%02x%02x%02x%02x%02x ",
					 recovered_streamid[0], recovered_streamid[1],
					 recovered_streamid[2], recovered_streamid[3],
					 recovered_streamid[4], recovered_streamid[5],
					 recovered_streamid[6], recovered_streamid[7]);
				switch (substate) {
				case 0:
					AVB_LOG_DEBUG("with state ignore");
					break;
				case 1:
					AVB_LOG_DEBUG("with state askfailed");
					break;
				case 2:
					AVB_LOG_DEBUG("with state ready");
					break;
				case 3:
					AVB_LOG_DEBUG("with state readyfail");
					break;
				default:
					AVB_LOGF_DEBUG("with state UNKNOWN (%d)", substate);
					break;
				}
				switch (buf[offset + 1]) {
				case 'L':
					mrp_attach_cb(recovered_streamid, substate);
					AVB_LOGF_DEBUG("got a leave indication substate %d", substate);
					if (memcmp
						(recovered_streamid, monitor_stream_id,
						 sizeof(recovered_streamid)) == 0) {
						listeners = 0;
						AVB_LOG_DEBUG("listener left");
					}
					break;
				case 'J':
				case 'N':
					AVB_LOGF_DEBUG("got a new/join indication substate %d", substate);
					mrp_attach_cb(recovered_streamid, substate);
					if (substate > MSRP_LISTENER_ASKFAILED) {
						if (memcmp
							(recovered_streamid,
							 monitor_stream_id,
							 sizeof(recovered_streamid)) == 0)
							listeners = 1;
					}
					break;
				}
				break;

			case 'T':
				i = offset + 5;
				while (buf[i] != 'S')
					i++;
				// skip S=
				i += 2;
				for (j = 0; j < 8; j++) {
					sscanf(&(buf[i + 2 * j]), "%02x", &id);
					recovered_streamid[j] = (unsigned char)id;
				}
				while (buf[i] != 'A')
					i++;
				// skip A=
				i += 2;
				for (j = 0; j < 6; j++) {
					sscanf(&(buf[i + 2 * j]), "%02x", &id);
					dest_addr[j] = (unsigned char)id;
				}
				i+= j*2 + 1;

				sscanf(&(buf[i]), "V=%d,Z=%d,I=%d,P=%d,L=%d",
					   &vid,
					   &max_frame_size,
					   &max_interval_frames,
					   &priority_and_rank,
					   &latency);

				mrp_register_cb(recovered_streamid, buf[offset+1] == 'J' || buf[offset+1] == 'N', dest_addr, max_frame_size, max_interval_frames, vid, latency);
				break;

			default:
				break;
			}
			break;

		case '\0':
			break;
		}

		// Proceed to the next line.
		while (offset < buflen && buf[offset] != '\n' && buf[offset] != '\0') {
			offset++;
		}
		if (offset < buflen && buf[offset] == '\n') {
			offset++;
		}
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
	(void) arg; /* unused */

	msgbuf = (char *)malloc(MAX_MRPD_CMDSZ);
	if (NULL == msgbuf)
		return NULL;
	while (!halt_tx) {
		fds.fd = control_socket;
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
		bytes = recvmsg(control_socket, &msg, 0);
		if (bytes < 0)
			continue;
		AVB_LOGF_VERBOSE("Msg: %s", msgbuf);
		process_mrp_msg(msgbuf, bytes);
	}
	free(msgbuf);
	pthread_exit(NULL);
}

/*
 * public
 */

int mrp_connect(void)
{
	struct sockaddr_in addr;
	int sock_fd = -1;
	sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock_fd < 0)
		goto out;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MRPD_PORT_DEFAULT);
	inet_aton("127.0.0.1", &addr.sin_addr);
	memset(&addr, 0, sizeof(addr));
	control_socket = sock_fd;
	return 0;
 out:	if (sock_fd != -1)
		close(sock_fd);
	sock_fd = -1;
	return -1;
}

int mrp_disconnect(void)
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "BYE");
	mrp_okay = 0;
	AVB_LOGF_DEBUG("MRP Command (Disconnect):  %s", msgbuf);
	rc = send_mrp_msg(msgbuf, 1500);
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

int mrp_register_domain(int *class_id, int *priority, u_int16_t * vid)
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);

	sprintf(msgbuf, "S+D:C=%d,P=%d,V=%04x", *class_id, *priority, *vid);
	mrp_okay = 0;
	AVB_LOGF_DEBUG("MRP Command (Register Domain):  %s", msgbuf);
	rc = send_mrp_msg(msgbuf, 1500);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}


int
mrp_advertise_stream(uint8_t * streamid,
		     uint8_t * destaddr,
		     u_int16_t vlan,
		     int pktsz, int interval, int priority, int latency)
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
		destaddr[3], destaddr[4], destaddr[5], vlan, pktsz,
		interval, priority << 5, latency);
	mrp_okay = 0;
	AVB_LOGF_DEBUG("MRP Command (Advertise Stream):  %s", msgbuf);
	rc = send_mrp_msg(msgbuf, 1500);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int
mrp_unadvertise_stream(uint8_t * streamid,
		       uint8_t * destaddr,
		       u_int16_t vlan,
		       int pktsz, int interval, int priority, int latency)
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
		destaddr[3], destaddr[4], destaddr[5], vlan, pktsz,
		interval, priority << 5, latency);
	mrp_okay = 0;
	AVB_LOGF_DEBUG("MRP Command (Unadvertise Stream):  %s", msgbuf);
	rc = send_mrp_msg(msgbuf, 1500);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}


int mrp_await_listener(unsigned char *streamid)
{
	char *msgbuf;
	int rc;

	memcpy(monitor_stream_id, streamid, sizeof(monitor_stream_id));
	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S??");
	AVB_LOGF_DEBUG("MRP Command (Await Listener):  %s", msgbuf);
	rc = send_mrp_msg(msgbuf, 1500);
	free(msgbuf);
	if (rc != 1500)
		return -1;

	/* either already there ... or need to wait ... */
	while (!halt_tx && (listeners == 0))
		usleep(20000);

	return 0;
}

/*
 * actually not used
 */

int mrp_get_domain(int *class_a_id, int *a_priority, u_int16_t * a_vid,
		   int *class_b_id, int *b_priority, u_int16_t * b_vid)
{
	char *msgbuf;
	int ret;
	int tries = 5;

	/* we may not get a notification if we are joining late,
	 * so query for what is already there ...
	 */
	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S??");
	AVB_LOGF_DEBUG("MRP Command (Get Domain):  %s", msgbuf);
	ret = send_mrp_msg(msgbuf, 1500);
	free(msgbuf);
	if (ret != 1500)
		return -1;
	while (!halt_tx && (domain_a_valid == 0) && (domain_b_valid == 0) && tries--) {
		usleep(20000);
	}
	*class_a_id = 0;
	*a_priority = 0;
	*a_vid = 0;
	*class_b_id = 0;
	*b_priority = 0;
	*b_vid = 0;
	if (domain_a_valid) {
		*class_a_id = domain_class_a_id;
		*a_priority = domain_class_a_priority;
		*a_vid = domain_class_a_vid;
	}
	if (domain_b_valid) {
		*class_b_id = domain_class_b_id;
		*b_priority = domain_class_b_priority;
		*b_vid = domain_class_b_vid;
	}
	return domain_a_valid || domain_b_valid ? 0 : -1;
}

int mrp_join_vlan()
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "V++:I=0002");
	AVB_LOGF_DEBUG("MRP Command (Join VLAN):  %s", msgbuf);
	rc = send_mrp_msg(msgbuf, 1500);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int mrp_join_listener(uint8_t * streamid)
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S+L:S=%02X%02X%02X%02X%02X%02X%02X%02X"
		",D=2", streamid[0], streamid[1], streamid[2], streamid[3],
		streamid[4], streamid[5], streamid[6], streamid[7]);
	mrp_okay = 0;
	AVB_LOGF_DEBUG("MRP Command (Join Listener):  %s", msgbuf);
	rc = send_mrp_msg(msgbuf, 1500);
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

int mrp_send_ready(uint8_t *stream_id)
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
	AVB_LOGF_DEBUG("MRP Command (Send Ready):  %s", databuf);
	rc = send_mrp_msg(databuf, 1500);
	free(databuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int mrp_send_leave(uint8_t *stream_id)
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

	AVB_LOGF_DEBUG("MRP Command (Send Leave):  %s", databuf);
	rc = send_mrp_msg(databuf, 1500);
	free(databuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}
