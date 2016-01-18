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

#ifndef OPENAVB_ENDPOINT_SERVER_OSAL_C
#define OPENAVB_ENDPOINT_SERVER_OSAL_C

#define AVB_ENDPOINT_LISTEN_FDS	0 // first fds, was last MAX_AVB_STREAMS
#define SOCK_INVALID (-1)
#define POLL_FD_COUNT ((MAX_AVB_STREAMS) + 1)

static int lsock  = SOCK_INVALID;
static struct pollfd fds[POLL_FD_COUNT];
static struct sockaddr_un serverAddr;

static void socketClose(int h)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	if (h < 0 || h >= POLL_FD_COUNT) {
		AVB_LOG_ERROR("Closing socket; invalid handle");
	}
	else {
		openavbEptSrvrCloseClientConnection(h);
		close(fds[h].fd);
		fds[h].fd = SOCK_INVALID;
		fds[h].events = 0;
	}
	
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
}

static bool openavbEptSrvrSendToClient(int h, openavbEndpointMessage_t *msg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	if (h < 0 || h >= POLL_FD_COUNT) {
		AVB_LOG_ERROR("Sending message; invalid handle");
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return FALSE;
	}
	if (!msg) {
		AVB_LOG_ERROR("Sending message; invalid argument passed");
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return FALSE;
	}

	int csock = fds[h].fd;
	if (csock == SOCK_INVALID) {
		AVB_LOG_ERROR("Socket closed unexpectedly");
		return FALSE;
	}

	ssize_t nWrite = write(csock, msg, OPENAVB_ENDPOINT_MSG_LEN);
	AVB_LOGF_VERBOSE("Sent message, len=%zu, nWrite=%zu", OPENAVB_ENDPOINT_MSG_LEN, nWrite);
	if (nWrite < OPENAVB_ENDPOINT_MSG_LEN) {
		if (nWrite < 0) {
			AVB_LOGF_ERROR("Failed to write socket: %s", strerror(errno));
		}
		else if (nWrite == 0) {
			AVB_LOG_ERROR("Socket closed unexpectedly");
		}
		else {
			AVB_LOG_ERROR("Socket write too short");
		}
		socketClose(h);
		AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return TRUE;
}

bool openavbEndpointServerOpen(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	int i;

	for (i=0; i < POLL_FD_COUNT; i++) {
		fds[i].fd = SOCK_INVALID;
		fds[i].events = 0;
	}

	lsock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (lsock < 0) {
		AVB_LOGF_ERROR("Failed to open socket: %s", strerror(errno));
		goto error;
	}
	// serverAddr is file static
	serverAddr.sun_family = AF_UNIX;
	snprintf(serverAddr.sun_path, UNIX_PATH_MAX, AVB_ENDPOINT_UNIX_PATH);

	int rslt = bind(lsock, (struct sockaddr*)&serverAddr, sizeof(struct sockaddr_un));
	if (rslt != 0) {
		AVB_LOGF_ERROR("Failed to create %s: %s", serverAddr.sun_path, strerror(errno));
		AVB_LOG_WARNING("** If endpoint process crashed, run the cleanup script **");
		goto error;
	}

	rslt = listen(lsock, 5);
	if (rslt != 0) {
		AVB_LOGF_ERROR("Failed to listen on socket: %s", strerror(errno));
		goto error;
	}
	AVB_LOGF_DEBUG("Listening on socket: %s", serverAddr.sun_path);

	fds[AVB_ENDPOINT_LISTEN_FDS].fd = lsock;
	fds[AVB_ENDPOINT_LISTEN_FDS].events = POLLIN;

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return TRUE;

  error:
	if (lsock >= 0) {
		close(lsock);
		lsock = -1;
	}
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return FALSE;
}
 
void openavbEptSrvrService(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	struct sockaddr_un addrClient;
	socklen_t lenAddr;
	int i, j;
	int  csock;

	int nfds = POLL_FD_COUNT;
	int pRet;

	AVB_LOG_VERBOSE("Waiting for event...");
	pRet = poll(fds, nfds, 1000);

	if (pRet == 0) {
		AVB_LOG_VERBOSE("poll timeout");
	}
	else if (pRet < 0) {
		if (errno == EINTR) {
			AVB_LOG_VERBOSE("Poll interrupted");
		}
		else {
			AVB_LOGF_ERROR("Poll error: %s", strerror(errno));
		}
	}
	else {
		AVB_LOGF_VERBOSE("Poll returned %d events", pRet);
		for (i=0; i<nfds; i++) {
			if (fds[i].revents != 0) {
				AVB_LOGF_VERBOSE("%d sock=%d, event=0x%x, revent=0x%x", i, fds[i].fd, fds[i].events, fds[i].revents);

				if (i == AVB_ENDPOINT_LISTEN_FDS) {
					// listen sock - indicates new connection from client
					lenAddr = sizeof(addrClient);
					csock = accept(lsock, (struct sockaddr*)&addrClient, &lenAddr);
					if (csock < 0) {
						AVB_LOGF_ERROR("Failed to accept connection: %s", strerror(errno));
					}
					else {
						for (j = 0; j < POLL_FD_COUNT; j++) {
							if (fds[j].fd == SOCK_INVALID) {
								fds[j].fd = csock;
								fds[j].events = POLLIN;
								break;
							}
						}
						if (j >= POLL_FD_COUNT) {
							AVB_LOG_ERROR("Too many client connections");
							close(csock);
						}
					}
				}
				else {
					csock = fds[i].fd;
					openavbEndpointMessage_t  msgBuf;
					memset(&msgBuf, 0, OPENAVB_ENDPOINT_MSG_LEN);
					ssize_t nRead = read(csock, &msgBuf, OPENAVB_ENDPOINT_MSG_LEN);
					AVB_LOGF_VERBOSE("Socket read h=%d,fd=%d: read=%zu, expect=%zu", i, csock, nRead, OPENAVB_ENDPOINT_MSG_LEN);
					
					if (nRead < OPENAVB_ENDPOINT_MSG_LEN) {
						// sock closed
						if (nRead == 0) {
							AVB_LOGF_DEBUG("Socket closed, h=%d", i);
						}
						else if (nRead < 0) {
							AVB_LOGF_ERROR("Socket read, h=%d: %s", i, strerror(errno));
						}
						else {
							AVB_LOGF_ERROR("Short read, h=%d", i);
						}
						socketClose(i);
					}
					else {
						// got a message
						if (!openavbEptSrvrReceiveFromClient(i, &msgBuf)) {
							AVB_LOG_ERROR("Failed to handle message");
							socketClose(i);
						}
					}
				}
			}
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
}

void openavbEndpointServerClose(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	int i;
	for (i = 0; i < POLL_FD_COUNT; i++) {
		if (fds[i].fd != SOCK_INVALID) {
			close(fds[i].fd);
		}
	}
	if (lsock != SOCK_INVALID) {
		close(lsock);
	}

	if (unlink(serverAddr.sun_path) != 0) {
		AVB_LOGF_ERROR("Failed to unlink %s: %s", serverAddr.sun_path, strerror(errno));
	}
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
} 

#endif // OPENAVB_ENDPOINT_SERVER_OSAL_C
