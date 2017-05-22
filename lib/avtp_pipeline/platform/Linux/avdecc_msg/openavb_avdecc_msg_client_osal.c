/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
Copyright (c) 2016-2017, Harman International Industries, Incorporated
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

#ifndef OPENAVB_AVDECC_MSG_CLIENT_OSAL_C
#define OPENAVB_AVDECC_MSG_CLIENT_OSAL_C

static void socketClose(int socketHandle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);
	if (socketHandle != AVB_AVDECC_MSG_HANDLE_INVALID) {
		close(socketHandle);
	}
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
}

static bool openavbAvdeccMsgClntSendToServer(int socketHandle, openavbAvdeccMessage_t *msg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (!msg || socketHandle == AVB_AVDECC_MSG_HANDLE_INVALID) {
		AVB_LOG_ERROR("Client send: invalid argument passed");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return FALSE;
	}

	ssize_t nWrite = write(socketHandle, msg, OPENAVB_AVDECC_MSG_LEN);
	AVB_LOGF_VERBOSE("Sent message, len=%zu, nWrite=%zu", OPENAVB_AVDECC_MSG_LEN, nWrite);

	if (nWrite < OPENAVB_AVDECC_MSG_LEN) {
		if (nWrite < 0) {
			AVB_LOGF_ERROR("Client failed to write socket: %s", strerror(errno));
		}
		else if (nWrite == 0) {
			AVB_LOG_ERROR("Client send: socket closed unexpectedly");
		}
		else {
			AVB_LOG_ERROR("Client send: short write");
		}
		socketClose(socketHandle);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return TRUE;
}

int openavbAvdeccMsgClntOpenSrvrConnection(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);
	struct sockaddr_un server;
	server.sun_family = AF_UNIX;
	snprintf(server.sun_path, UNIX_PATH_MAX, AVB_AVDECC_MSG_UNIX_PATH);

	int h = socket(AF_UNIX, SOCK_STREAM, 0);
	if (h < 0) {
		AVB_LOGF_ERROR("Failed to open socket: %s", strerror(errno));
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return AVB_AVDECC_MSG_HANDLE_INVALID;
	}

	AVB_LOGF_DEBUG("Connecting to %s", server.sun_path);
	int rslt = connect(h, (struct sockaddr*)&server, sizeof(struct sockaddr_un));
	if (rslt < 0) {
		AVB_LOGF_DEBUG("Failed to connect socket: %s", strerror(errno));
		socketClose(h);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return AVB_AVDECC_MSG_HANDLE_INVALID;
	}

	AVB_LOG_DEBUG("Connected to AVDECC Msg");
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return h;
}

void openavbAvdeccMsgClntCloseSrvrConnection(int socketHandle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);
	socketClose(socketHandle);
	AVB_LOG_DEBUG("Closed connection to AVDECC Msg");
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
}

bool openavbAvdeccMsgClntService(int socketHandle, int timeout)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);
	bool rc = FALSE;

	if (socketHandle == AVB_AVDECC_MSG_HANDLE_INVALID) {
		AVB_LOG_ERROR("Client service: invalid socket");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return FALSE;
	}

	struct pollfd fds[1];
	memset(fds, 0, sizeof(struct pollfd));
	fds[0].fd = socketHandle;
	fds[0].events = POLLIN;

	AVB_LOG_VERBOSE("Waiting for event...");
	int pRet = poll(fds, 1, timeout);

	if (pRet == 0) {
		AVB_LOG_VERBOSE("Poll timeout");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return TRUE;
	}
	else if (pRet < 0) {
		if (errno == EINTR) {
			AVB_LOG_VERBOSE("Poll interrupted");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
			return TRUE;
		}
		else {
			AVB_LOGF_ERROR("Poll error: %s", strerror(errno));
		}
	}
	else {
		AVB_LOGF_DEBUG("Poll returned %d events", pRet);
		// only one fd, so it's readable.
		openavbAvdeccMessage_t msgBuf;
		memset(&msgBuf, 0, OPENAVB_AVDECC_MSG_LEN);
		ssize_t nRead = read(socketHandle, &msgBuf, OPENAVB_AVDECC_MSG_LEN);

		if (nRead < OPENAVB_AVDECC_MSG_LEN) {
			// sock closed
			if (nRead == 0) {
				AVB_LOG_ERROR("Socket closed unexpectedly");
			}
			else if (nRead < 0) {
				AVB_LOGF_ERROR("Socket read error: %s", strerror(errno));
			}
			else {
				AVB_LOG_ERROR("Socket read to short");
			}
			socketClose(socketHandle);
		}
		else {
			// got a message
			if (openavbAvdeccMsgClntReceiveFromServer(socketHandle, &msgBuf)) {
				rc = TRUE;
			}
			else {
				AVB_LOG_ERROR("Invalid message received");
				socketClose(socketHandle);
			}
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return rc;
}

#endif // OPENAVB_AVDECC_MSG_CLIENT_OSAL_C
