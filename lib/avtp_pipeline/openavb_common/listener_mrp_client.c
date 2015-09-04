/*
  Copyright (c) 2013 Katja Rohloff <Katja.Rohloff@uni-jena.de>
  Copyright (c) 2014, Parrot SA

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

#include "listener_mrp_client.h"

/* global variables */

int control_socket;
volatile int talker = 0;
unsigned char stream_id[8];

/*
 * private
 */

int send_msg(char *data, int data_len)
{
	struct sockaddr_in addr;

	if (control_socket == -1)
		return -1;
	if (data == NULL)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MRPD_PORT_DEFAULT);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	inet_aton("127.0.0.1", &addr.sin_addr);
	return sendto(control_socket, data, data_len, 0,
		(struct sockaddr*)&addr, (socklen_t)sizeof(addr));
}

int msg_process(char *buf, int buflen)
{
	uint32_t id;
	int j, l;

	fprintf(stderr, "Msg: %s\n", buf);
	if (strncmp(buf, "SNE T:", 6) == 0 || strncmp(buf, "SJO T:", 6) == 0)
	{
		l = 6; /* skip "Sxx T:" */
		while ((l < buflen) && ('S' != buf[l++]));
		if (l == buflen)
			return -1;
		l++;
		for(j = 0; j < 8 ; l+=2, j++)
		{
			sscanf(&buf[l],"%02x",&id);
			stream_id[j] = (unsigned char)id;
		}
		talker = 1;
	}
	return 0;
}

int recv_msg()
{
	char *databuf;
	int bytes = 0;
	int ret;

	databuf = (char *)malloc(1500);
	if (NULL == databuf)
		return -1;

	memset(databuf, 0, 1500);
	bytes = recv(control_socket, databuf, 1500, 0);
	if (bytes <= -1)
	{
		free(databuf);
		return -1;
	}
	ret = msg_process(databuf, bytes);
	free(databuf);

	return ret;
}

/*
 * public
 */

int create_socket() // TODO FIX! =:-|
{
	struct sockaddr_in addr;
	control_socket = socket(AF_INET, SOCK_DGRAM, 0);
		
	/** in POSIX fd 0,1,2 are reserved */
	if (2 > control_socket)
	{
		if (-1 > control_socket)
			close(control_socket);
	return -1;
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(0);
	
	if(0 > (bind(control_socket, (struct sockaddr*)&addr, sizeof(addr)))) 
	{
		fprintf(stderr, "Could not bind socket.\n");
		close(control_socket);
		return -1;
	}
	return 0;
}

int report_domain_status()
{
	char* msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S+D:C=6,P=3,V=0002");
	rc = send_msg(msgbuf, 1500);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int join_vlan()
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "V++:I=0002");
	rc = send_msg(msgbuf, 1500);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int await_talker()
{
	while (0 == talker)	
		recv_msg();
	return 0;
}

int send_ready()
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
	rc = send_msg(databuf, 1500);
	free(databuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int send_leave()
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
	rc = send_msg(databuf, 1500);
	free(databuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int mrp_disconnect()
{
	int rc;
	char *msgbuf = malloc(1500);

	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);

	sprintf(msgbuf, "BYE");
	rc = send_msg(msgbuf, 1500);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}
