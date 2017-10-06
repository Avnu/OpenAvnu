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

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "openavb_platform.h"
#include "openavb_trace.h"
#include "openavb_shaper.h"
#include "openavb_list.h"
#include "openavb_rawsock.h"

#define	AVB_LOG_COMPONENT	"Endpoint Shaper"
//#define AVB_LOG_LEVEL AVB_LOG_LEVEL_DEBUG
#include "openavb_pub.h"
#include "openavb_log.h"


/*******************************************************************************
 * Shaper proxies
 ******************************************************************************/

#define STREAMDA_LENGTH 18

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif
#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

typedef struct shaper_reservation
{
	int in_use;
	SRClassIdx_t sr_class;
	int measurement_interval; // microseconds
	int max_frame_size; // bytes
	int max_frames_per_interval; // number of frames per measurement_interval
	unsigned char stream_da[ETH_ALEN];
} shaper_reservation;

#define MAX_SHAPER_RESERVATIONS 4
static shaper_reservation shaperReservationList[MAX_SHAPER_RESERVATIONS];

static bool shaperRunning = FALSE;
static pthread_t shaperThreadHandle;
static void* shaperThread(void *arg);

enum shaperState_t {
	SHAPER_STATE_UNKNOWN,
	SHAPER_STATE_CONNECTED, // Connected, but shaping not enabled.
	SHAPER_STATE_ENABLED, // Shaping enabled by the daemon
	SHAPER_STATE_NOT_AVAILABLE, // Daemon not found or not connected.
	SHAPER_STATE_ERROR
};
static enum shaperState_t shaperState = SHAPER_STATE_UNKNOWN;

static int socketfd = -1;
static char interfaceOnly[IFNAMSIZ];
static char shaperDaemonPort[6] = {0};

static MUTEX_HANDLE(shaperMutex);
#define SHAPER_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(shaperMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define SHAPER_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(shaperMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }


/* Local function to interact with the SHAPER daemon. */
static void* shaperThread(void *arg)
{
	struct addrinfo hints, *ai, *p;
	int ret;

	fd_set master, read_fds;
	int fdmax;

	char recvbuffer[200];
	int recvbytes;

	AVB_LOG_DEBUG("Shaper Thread Starting");

	/* Create a localhost socket. */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	if ((ret = getaddrinfo("localhost", shaperDaemonPort, &hints, &ai)) != 0) {
		AVB_LOGF_ERROR("getaddrinfo failure %s", gai_strerror(ret));
		shaperState = SHAPER_STATE_ERROR;
		return NULL;
	}

	for(p = ai; p != NULL; p = p->ai_next) {
		socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (socketfd == -1) {
			continue;
		}
		ret = connect(socketfd, p->ai_addr, p->ai_addrlen);
		if (ret == -1) {
			close(socketfd);
			socketfd = -1;
			continue;
		} else {
			break;
		}
	}

	freeaddrinfo(ai);

	if (p == NULL) {
		AVB_LOGF_ERROR("Shaper:  Unable to connect to the daemon, error %d (%s)", errno, strerror(errno));
		shaperState = SHAPER_STATE_ERROR;
		return NULL;
	}

	if (fcntl(socketfd, F_SETFL, O_NONBLOCK) < 0)
	{
		AVB_LOG_ERROR("Shaper:  Could not set the socket to non-blocking");
		close(socketfd);
		socketfd = -1;
		shaperState = SHAPER_STATE_ERROR;
		return NULL;
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&master);
	FD_SET(STDIN_FILENO, &master);
	FD_SET(socketfd, &master);
	fdmax = socketfd;


	/*
	 * Main event loop
	 */

	shaperState = SHAPER_STATE_CONNECTED;
	AVB_LOG_INFO("Shaper daemon available");

	while (shaperRunning || shaperState == SHAPER_STATE_ENABLED)
	{
		if (!shaperRunning && shaperState == SHAPER_STATE_ENABLED)
		{
			// TODO:  Tell the SHAPER daemon to stop shaping.
			break;
		}

		/* Wait for something to happen. */
		struct timeval tv_timeout = { 1, 0 };
		read_fds = master;
		ret = select(fdmax+1, &read_fds, NULL, NULL, &tv_timeout);
		if (ret < 0)
		{
			AVB_LOGF_ERROR("Shaper:  select() error %d (%s)", errno, strerror(errno));
			shaperState = SHAPER_STATE_ERROR;
			break;
		}

		/* Handle any responses received. */
		if (FD_ISSET(socketfd, &read_fds))
		{
			while ((recvbytes = recv(socketfd, recvbuffer, sizeof(recvbuffer) - 1, 0)) > 0)
			{
				/* Log the response data */
				recvbuffer[recvbytes] = '\0';
				if (strncmp(recvbuffer, "ERROR:", 6) == 0) {
					AVB_LOGF_ERROR("Shaper Response:  %s", recvbuffer + 7);
				}
				else if (strncmp(recvbuffer, "WARNING:", 8) == 0) {
					AVB_LOGF_WARNING("Shaper Response:  %s", recvbuffer + 9);
				}
				else if (strncmp(recvbuffer, "DEBUG:", 6) == 0) {
					AVB_LOGF_DEBUG("Shaper Response:  %s", recvbuffer + 7);
				}
				else {
					AVB_LOGF_DEBUG("Shaper Response:  %s", recvbuffer);
				}
			}
			if (recvbytes == 0)
			{
				/* The SHAPER daemon closed the connection.  Assume it shut down, and we should too. */
				// AVDECC_TODO:  Should we try to reconnect?
				AVB_LOG_ERROR("Shaper daemon exited.");
				shaperState = SHAPER_STATE_ERROR;
				break;
			}
			if (recvbytes < 0 && errno != EWOULDBLOCK)
			{
				/* Something went wrong.  Abort! */
				AVB_LOGF_ERROR("Shaper:  Error %d reading from network socket (%s)", errno, strerror(errno));
				shaperState = SHAPER_STATE_ERROR;
				break;
			}
		}
	}

	if (shaperState < SHAPER_STATE_NOT_AVAILABLE) {
		shaperState = SHAPER_STATE_NOT_AVAILABLE;
	}

	close(socketfd);
	socketfd = -1;

	AVB_LOG_DEBUG("Shaper Thread Done");
	return NULL;
}

bool openavbShaperInitialize(const char *ifname, unsigned int shaperPort)
{
	AVB_TRACE_ENTRY(AVB_TRACE_SHAPER);

	MUTEX_ATTR_HANDLE(mta);
	MUTEX_ATTR_INIT(mta);
	MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
	MUTEX_ATTR_SET_NAME(mta, "shaperMutex");
	MUTEX_CREATE_ERR();
	MUTEX_CREATE(shaperMutex, mta);
	MUTEX_LOG_ERR("Could not create/initialize 'shaperMutex' mutex");

	memset(shaperReservationList, 0, sizeof(shaperReservationList));

	if (shaperPort == 0) {
		shaperState = SHAPER_STATE_NOT_AVAILABLE;
	}
	else {
		shaperState = SHAPER_STATE_UNKNOWN;
		sprintf(shaperDaemonPort, "%u", shaperPort);

		// Save the interface-only name to pass to the daemon later.
		const char *ifonly = strchr(ifname, ':');
		if (ifonly) {
			ifonly++; // Go past the colon
		} else {
			ifonly = ifname; // No colon in interface name
		}
		strncpy(interfaceOnly, ifonly, sizeof(interfaceOnly));
		interfaceOnly[sizeof(interfaceOnly) - 1] = '\0';

		shaperRunning = TRUE;
		int err = pthread_create(&shaperThreadHandle, NULL, shaperThread, NULL);
		if (err) {
			shaperRunning = FALSE;
			shaperState = SHAPER_STATE_ERROR;
			AVB_LOGF_ERROR("Failed to start SHAPER thread: %s", strerror(err));
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_SHAPER);
	return true;
}

void openavbShaperFinalize()
{
	AVB_TRACE_ENTRY(AVB_TRACE_SHAPER);

	if (shaperRunning) {
		// Stop the SHAPER thread.
		shaperRunning = FALSE;
		pthread_join(shaperThreadHandle, NULL);
	}

	MUTEX_CREATE_ERR();
	MUTEX_DESTROY(shaperMutex);
	MUTEX_LOG_ERR("Error destroying mutex");

	AVB_TRACE_EXIT(AVB_TRACE_SHAPER);
}

bool openavbShaperDaemonAvailable(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_SHAPER);

	if (!shaperRunning) {
		AVB_TRACE_EXIT(AVB_TRACE_SHAPER);
		return FALSE;
	}

	// Wait for the daemon to connect.
	if (shaperState == SHAPER_STATE_UNKNOWN) {
		SLEEP_MSEC(50);
	}

	if (shaperState > SHAPER_STATE_UNKNOWN && shaperState < SHAPER_STATE_NOT_AVAILABLE) {
		AVB_TRACE_EXIT(AVB_TRACE_SHAPER);
		return TRUE;
	}

	if (shaperState != SHAPER_STATE_NOT_AVAILABLE) {
		AVB_LOG_WARNING("Shaper Daemon not available.");
	}
	AVB_TRACE_EXIT(AVB_TRACE_SHAPER);
	return FALSE;
}

void* openavbShaperHandle(SRClassIdx_t sr_class, int measurement_interval_usec, int max_frame_size_bytes, int max_frames_per_interval, const unsigned char * stream_da)
{
	// If the daemon is not available, abort!
	if (socketfd == -1 || !openavbShaperDaemonAvailable())
	{
		AVB_LOG_ERROR("Shaper reservation attempted with no daemon available");
		return NULL;
	}

	// Find the first available index.
	int i;
	for (i = 0; i < MAX_SHAPER_RESERVATIONS; ++i)
	{
		if (!(shaperReservationList[i].in_use))
		{
			break;
		}
	}
	if (i >= MAX_SHAPER_RESERVATIONS)
	{
		AVB_LOG_ERROR("No Shaper reservations are available");
		return NULL;
	}

	// Fill in the information.
	shaperReservationList[i].in_use = TRUE;
	shaperReservationList[i].sr_class = sr_class;
	shaperReservationList[i].measurement_interval = measurement_interval_usec;
	shaperReservationList[i].max_frame_size = max_frame_size_bytes;
	shaperReservationList[i].max_frames_per_interval = max_frames_per_interval;
	memcpy(shaperReservationList[i].stream_da, stream_da, ETH_ALEN);

	// Send the information to the Shaper daemon.
	// Reserving Bandwidth Example:  -ri eth2 -c A -s 125 -b 74 -f 1 -a ff:ff:ff:ff:ff:11\n
	char szCommand[200];
	sprintf(szCommand, "-ri %s -c %c -s %u -b %u -f %u -a %02x:%02x:%02x:%02x:%02x:%02x",
		interfaceOnly,
		( shaperReservationList[i].sr_class == SR_CLASS_A ? 'A' : 'B' ),
		shaperReservationList[i].measurement_interval,
		shaperReservationList[i].max_frame_size,
		shaperReservationList[i].max_frames_per_interval,
		shaperReservationList[i].stream_da[0],
		shaperReservationList[i].stream_da[1],
		shaperReservationList[i].stream_da[2],
		shaperReservationList[i].stream_da[3],
		shaperReservationList[i].stream_da[4],
		shaperReservationList[i].stream_da[5]);
	AVB_LOGF_DEBUG("Sending Shaper command:  %s", szCommand);
	strcat(szCommand, "\n");
	if (send(socketfd, szCommand, strlen(szCommand), 0) < 0)
	{
		/* Something went wrong.  Abort! */
		AVB_LOGF_ERROR("Shaper:  Error %d writing to network socket (%s)", errno, strerror(errno));
		shaperState = SHAPER_STATE_ERROR;
		return NULL;
	}

	shaperState = SHAPER_STATE_ENABLED;

	// TODO:  Verify that the command was successful.

	return (void *)(&(shaperReservationList[i]));
}

void openavbShaperRelease(void* handle)
{
	int i;
	for (i = 0; i < MAX_SHAPER_RESERVATIONS; ++i)
	{
		if (handle == (void *)(&(shaperReservationList[i])))
		{
			if (shaperReservationList[i].in_use)
			{
				// Send the information to the Shaper daemon.
				// Unreserving Bandwidth Example:  -ua ff:ff:ff:ff:ff:11\n
				char szCommand[200];
				sprintf(szCommand, "-ua %02x:%02x:%02x:%02x:%02x:%02x",
					shaperReservationList[i].stream_da[0],
					shaperReservationList[i].stream_da[1],
					shaperReservationList[i].stream_da[2],
					shaperReservationList[i].stream_da[3],
					shaperReservationList[i].stream_da[4],
					shaperReservationList[i].stream_da[5]);

				shaperReservationList[i].in_use = FALSE;

				AVB_LOGF_DEBUG("Sending Shaper command:  %s", szCommand);
				strcat(szCommand, "\n");
				if (send(socketfd, szCommand, strlen(szCommand), 0) < 0)
				{
					/* Something went wrong. */
					AVB_LOGF_ERROR("Shaper:  Error %d writing to network socket (%s)", errno, strerror(errno));
					shaperState = SHAPER_STATE_ERROR;
				}
				else {
					// TODO:  Verify that the command was successful.
				}
			}
			break;
		}
	}
}
