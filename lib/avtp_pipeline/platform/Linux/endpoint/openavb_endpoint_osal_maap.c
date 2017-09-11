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
#include "openavb_maap.h"
#include "maap_iface.h"
#include "openavb_list.h"
#include "openavb_rawsock.h"

// These are used to support saving the MAAP Address
#include "openavb_endpoint_cfg.h"
extern openavb_endpoint_cfg_t x_cfg;

#define	AVB_LOG_COMPONENT	"Endpoint MAAP"
//#define AVB_LOG_LEVEL AVB_LOG_LEVEL_DEBUG
#include "openavb_pub.h"
#include "openavb_log.h"

#define MAAP_DYNAMIC_POOL_BASE 0x91E0F0000000LL /**< MAAP dynamic allocation pool base address - Defined in IEEE 1722-2016 Table B.9 */
#define MAAP_DYNAMIC_POOL_SIZE 0xFE00 /**< MAAP dynamic allocation pool size - Defined in IEEE 1722-2016 Table B.9 */


/*******************************************************************************
 * MAAP proxies
 ******************************************************************************/

typedef struct {
	struct ether_addr destAddr;
	bool taken;
} maapAlloc_t;

static maapAlloc_t maapAllocList[MAX_AVB_STREAMS];
static openavbMaapRestartCb_t *maapRestartCallback = NULL;
static struct ether_addr *maapPreferredAddress = NULL;

static bool maapRunning = FALSE;
static pthread_t maapThreadHandle;
static void* maapThread(void *arg);

enum maapState_t {
	MAAP_STATE_UNKNOWN,
	MAAP_STATE_INITIALIZING, // Waiting for initialization
	MAAP_STATE_REQUESTING, // Waiting address block request
	MAAP_STATE_CONNECTED, // Have block of addresses
	MAAP_STATE_RELEASING, // Releasing the block of addresses
	MAAP_STATE_RELEASED, // Released the block of addresses
	MAAP_STATE_NOT_AVAILABLE, // MAAP not configured, or freed
	MAAP_STATE_ERROR
};
static enum maapState_t maapState = MAAP_STATE_UNKNOWN;
static int maapReservationId = 0;

static char maapDaemonPort[6] = {0};

static MUTEX_HANDLE(maapMutex);
#define MAAP_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(maapMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define MAAP_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(maapMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

static unsigned long long MaapMacAddrToLongLong(const struct ether_addr *addr)
{
	unsigned long long nAddress = 0ull;
	int i;
	for (i = 0; i < ETH_ALEN; ++i) {
		nAddress = (nAddress << 8) | addr->ether_addr_octet[i];
	}
	return nAddress;
}

static void MaapResultToMacAddr(unsigned long long nAddress, struct ether_addr *destAddr)
{
	int i;
	for (i = ETH_ALEN - 1; i >= 0; --i) {
		destAddr->ether_addr_octet[i] = (U8)(nAddress & 0xFF);
		nAddress >>= 8;
	}
}

static void process_maap_notify(Maap_Notify *mn, int socketfd)
{
	assert(mn);

	switch (mn->result)
	{
	case MAAP_NOTIFY_ERROR_NONE:
		/* No error.  Don't display anything. */
		break;
	case MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION:
		AVB_LOG_ERROR("MAAP is not initialized, so the command cannot be performed.");
		break;
	case MAAP_NOTIFY_ERROR_ALREADY_INITIALIZED:
		AVB_LOG_ERROR("MAAP is already initialized, so the values cannot be changed.");
		break;
	case MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE:
		AVB_LOG_ERROR("The MAAP reservation is not available, or yield cannot allocate a replacement block. "
			"Try again with a smaller address block size.");
		break;
	case MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID:
		AVB_LOG_ERROR("The MAAP reservation ID is not valid, so cannot be released or report its status.");
		break;
	case MAAP_NOTIFY_ERROR_OUT_OF_MEMORY:
		AVB_LOG_ERROR("The MAAP application is out of memory.");
		break;
	case MAAP_NOTIFY_ERROR_INTERNAL:
		AVB_LOG_ERROR("The MAAP application experienced an internal error.");
		break;
	default:
		AVB_LOGF_ERROR("The MAAP application returned an unknown error %d.", mn->result);
		break;
	}

	switch (mn->kind)
	{
	case MAAP_NOTIFY_INITIALIZED:
		if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
			AVB_LOGF_DEBUG("MAAP initialized:  0x%012llx-0x%012llx (Size: %d)",
				(unsigned long long) mn->start,
				(unsigned long long) mn->start + mn->count - 1,
				(unsigned int) mn->count);

			// We successfully initialized.  Reserve a block of addresses.
			Maap_Cmd maapcmd;
			memset(&maapcmd, 0, sizeof(Maap_Cmd));
			maapcmd.kind = MAAP_CMD_RESERVE;
			maapcmd.start = 0; // No preferred address
			maapcmd.count = MAX_AVB_STREAMS;
			if (maapPreferredAddress != NULL) {
				// Suggest the addresses from the previous time this application was run.
				maapcmd.start = MaapMacAddrToLongLong(maapPreferredAddress);
				AVB_LOGF_DEBUG("MAAP Preferred Address:  0x%012llx", maapcmd.start);
			}
			if (send(socketfd, (char *) &maapcmd, sizeof(Maap_Cmd), 0) < 0)
			{
				/* Something went wrong.  Abort! */
				AVB_LOGF_ERROR("MAAP:  Error %d writing to network socket (%s)", errno, strerror(errno));
				maapState = MAAP_STATE_ERROR;
			} else {
				maapState = MAAP_STATE_REQUESTING;
			}
		} else {
			AVB_LOGF_ERROR("MAAP previously initialized to 0x%012llx-0x%012llx (Size: %d)",
				(unsigned long long) mn->start,
				(unsigned long long) mn->start + mn->count - 1,
				(unsigned int) mn->count);
			maapState = MAAP_STATE_ERROR;
		}
		break;
	case MAAP_NOTIFY_ACQUIRING:
		if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
			AVB_LOGF_DEBUG("MAAP address range %d querying:  0x%012llx-0x%012llx (Size %d)",
				mn->id,
				(unsigned long long) mn->start,
				(unsigned long long) mn->start + mn->count - 1,
				mn->count);
		} else {
			AVB_LOGF_ERROR("MAAP unknown address range %d acquisition error", mn->id);
			maapState = MAAP_STATE_ERROR;
		}
		break;
	case MAAP_NOTIFY_ACQUIRED:
		if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
			AVB_LOGF_INFO("MAAP address range %d acquired:  0x%012llx-0x%012llx (Size %d)",
				mn->id,
				(unsigned long long) mn->start,
				(unsigned long long) mn->start + mn->count - 1,
				mn->count);

			// Update the stored addresses.
			MAAP_LOCK();
			int i = 0;
			for (i = 0; i < mn->count && i < MAX_AVB_STREAMS; i++) {
				MaapResultToMacAddr(mn->start + i, &(maapAllocList[i].destAddr));
				if (maapAllocList[i].taken && maapRestartCallback) {
					// Use the callback to notify that a change has occurred.
					MAAP_UNLOCK();
					maapRestartCallback(&(maapAllocList[i]), &(maapAllocList[i].destAddr));
					MAAP_LOCK();
				}
			}
			MAAP_UNLOCK();

			// Save the address so we can request it in the future.
			if (maapPreferredAddress != NULL) {
				struct ether_addr assignedAddress;
				MaapResultToMacAddr((unsigned long long) mn->start, &assignedAddress);
				if (memcmp(maapPreferredAddress, &assignedAddress, sizeof(struct ether_addr)) != 0) {
					// Save the updated settings.  Done immediately, so they won't get lost on device reboot.
					memcpy(maapPreferredAddress, &assignedAddress, sizeof(struct ether_addr));
					openavbSaveConfig(DEFAULT_SAVE_INI_FILE, &x_cfg);
				}
			}

			// We now have addresses we can use.
			maapReservationId = mn->id;
			maapState = MAAP_STATE_CONNECTED;
		} else if (mn->id != -1) {
			AVB_LOGF_ERROR("MAAP address range %d of size %d not acquired",
				mn->id, mn->count);
			maapState = MAAP_STATE_ERROR;
		} else {
			AVB_LOGF_ERROR("MAAP address range of size %d not acquired",
				mn->count);
			maapState = MAAP_STATE_ERROR;
		}
		break;
	case MAAP_NOTIFY_RELEASED:
		if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
			AVB_LOGF_DEBUG("MAAP address range %d released:  0x%012llx-0x%012llx (Size %d)",
				mn->id,
				(unsigned long long) mn->start,
				(unsigned long long) mn->start + mn->count - 1,
				mn->count);
			maapState = MAAP_STATE_RELEASED;
		} else {
			AVB_LOGF_WARNING("MAAP address range %d not released",
				mn->id);
		}
		break;
	case MAAP_NOTIFY_STATUS:
		if (mn->result == MAAP_NOTIFY_ERROR_NONE) {
			AVB_LOGF_DEBUG("MAAP ID %d is address range 0x%012llx-0x%012llx (Size %d)",
				mn->id,
				(unsigned long long) mn->start,
				(unsigned long long) mn->start + mn->count - 1,
				mn->count);
		} else {
			AVB_LOGF_DEBUG("MAAP ID %d is not valid",
				mn->id);
		}
		break;
	case MAAP_NOTIFY_YIELDED:
		if (mn->result != MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION && mn->result != MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID) {
			if (mn->result != MAAP_NOTIFY_ERROR_NONE) {
				AVB_LOGF_ERROR("MAAP Address range %d yielded:  0x%012llx-0x%012llx (Size %d).  A new address range will not be allocated.",
					mn->id,
					(unsigned long long) mn->start,
					(unsigned long long) mn->start + mn->count - 1,
					mn->count);
				maapState = MAAP_STATE_ERROR;
			}
			else {
				AVB_LOGF_WARNING("MAAP Address range %d yielded:  0x%012llx-0x%012llx (Size %d)",
					mn->id,
					(unsigned long long) mn->start,
					(unsigned long long) mn->start + mn->count - 1,
					mn->count);
			}
		} else {
			AVB_LOGF_ERROR("ID %d is not valid", mn->id);
		}
		break;
	default:
		AVB_LOGF_ERROR("MAAP notification type %d not recognized", mn->kind);
		break;
	}
}


/* Local function to interact with the MAAP daemon. */
static void* maapThread(void *arg)
{
	int socketfd;
	struct addrinfo hints, *ai, *p;
	int ret;

	fd_set master, read_fds;
	int fdmax;

	char recvbuffer[200];
	int recvbytes;
	Maap_Cmd maapcmd;

	AVB_LOG_DEBUG("MAAP Thread Starting");

	/* Create a localhost socket. */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	if ((ret = getaddrinfo("localhost", maapDaemonPort, &hints, &ai)) != 0) {
		AVB_LOGF_ERROR("getaddrinfo failure %s", gai_strerror(ret));
		maapState = MAAP_STATE_ERROR;
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
			continue;
		} else {
			break;
		}
	}

	freeaddrinfo(ai);

	if (p == NULL) {
		AVB_LOGF_ERROR("MAAP:  Unable to connect to the daemon, error %d (%s)", errno, strerror(errno));
		maapState = MAAP_STATE_ERROR;
		return NULL;
	}

	if (fcntl(socketfd, F_SETFL, O_NONBLOCK) < 0)
	{
		AVB_LOG_ERROR("MAAP:  Could not set the socket to non-blocking");
		close(socketfd);
		maapState = MAAP_STATE_ERROR;
		return NULL;
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&master);
	FD_SET(STDIN_FILENO, &master);
	FD_SET(socketfd, &master);
	fdmax = socketfd;

	// Initialize the MAAP daemon.
	// We will request a block of addresses when we get a response to the initialization.
	maapState = MAAP_STATE_INITIALIZING;
	memset(&maapcmd, 0, sizeof(Maap_Cmd));
	maapcmd.kind = MAAP_CMD_INIT;
	maapcmd.start = MAAP_DYNAMIC_POOL_BASE;
	maapcmd.count = MAAP_DYNAMIC_POOL_SIZE;
	if (send(socketfd, (char *) &maapcmd, sizeof(Maap_Cmd), 0) < 0)
	{
		/* Something went wrong.  Abort! */
		AVB_LOGF_ERROR("MAAP:  Error %d writing to network socket (%s)", errno, strerror(errno));
		maapState = MAAP_STATE_ERROR;
		return NULL;
	}


	/*
	 * Main event loop
	 */

	while (maapRunning || maapState <= MAAP_STATE_CONNECTED)
	{
		if (!maapRunning && maapState == MAAP_STATE_CONNECTED)
		{
			// Tell the MAAP daemon to release the addresses.
			memset(&maapcmd, 0, sizeof(Maap_Cmd));
			maapcmd.kind = MAAP_CMD_RELEASE;
			maapcmd.id = maapReservationId;
			if (send(socketfd, (char *) &maapcmd, sizeof(Maap_Cmd), 0) > 0)
			{
				maapState = MAAP_STATE_RELEASING;
				// Use the wait below to give the daemon time to respond.
			}
			else
			{
				maapState = MAAP_STATE_NOT_AVAILABLE;
				break;
			}
		}

		/* Wait for something to happen. */
		struct timeval tv_timeout = { 1, 0 };
		read_fds = master;
		ret = select(fdmax+1, &read_fds, NULL, NULL, &tv_timeout);
		if (ret < 0)
		{
			AVB_LOGF_ERROR("MAAP:  select() error %d (%s)", errno, strerror(errno));
			maapState = MAAP_STATE_ERROR;
			break;
		}

		/* Handle any responses received. */
		if (FD_ISSET(socketfd, &read_fds))
		{
			while ((recvbytes = recv(socketfd, recvbuffer, sizeof(Maap_Notify), 0)) > 0)
			{
				recvbuffer[recvbytes] = '\0';

				/* Process the response data (will be binary). */
				if (recvbytes == sizeof(Maap_Notify))
				{
					process_maap_notify((Maap_Notify *) recvbuffer, socketfd);
				}
				else
				{
					AVB_LOGF_WARNING("MAAP:  Received unexpected response of size %d", recvbytes);
				}
			}
			if (recvbytes == 0)
			{
				/* The MAAP daemon closed the connection.  Assume it shut down, and we should too. */
				// AVDECC_TODO:  Should we try to reconnect?
				AVB_LOG_ERROR("MAAP daemon exited.");
				maapState = MAAP_STATE_ERROR;
				break;
			}
			if (recvbytes < 0 && errno != EWOULDBLOCK)
			{
				/* Something went wrong.  Abort! */
				AVB_LOGF_ERROR("MAAP:  Error %d reading from network socket (%s)", errno, strerror(errno));
				maapState = MAAP_STATE_ERROR;
				break;
			}
		}
	}

	if (maapState < MAAP_STATE_NOT_AVAILABLE) {
		maapState = MAAP_STATE_NOT_AVAILABLE;
	}

	close(socketfd);

	AVB_LOG_DEBUG("MAAP Thread Done");
	return NULL;
}

bool openavbMaapInitialize(const char *ifname, unsigned int maapPort, struct ether_addr *maapPrefAddr, openavbMaapRestartCb_t* cbfn)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAAP);

	// Save the supplied callback function.
	maapRestartCallback = cbfn;
	maapPreferredAddress = maapPrefAddr;

	MUTEX_ATTR_HANDLE(mta);
	MUTEX_ATTR_INIT(mta);
	MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
	MUTEX_ATTR_SET_NAME(mta, "maapMutex");
	MUTEX_CREATE_ERR();
	MUTEX_CREATE(maapMutex, mta);
	MUTEX_LOG_ERR("Could not create/initialize 'maapMutex' mutex");

	// Default to using addresses from the MAAP locally administered Pool.
	int i = 0;
	U8 destAddr[ETH_ALEN] = {0x91, 0xe0, 0xf0, 0x00, 0xfe, 0x80};
	for (i = 0; i < MAX_AVB_STREAMS; i++) {
		memcpy(maapAllocList[i].destAddr.ether_addr_octet, destAddr, ETH_ALEN);
		maapAllocList[i].taken = false;
		destAddr[5] += 1;
	}

	if (maapPort == 0) {
		maapState = MAAP_STATE_NOT_AVAILABLE;
	}
	else {
		maapState = MAAP_STATE_UNKNOWN;
		sprintf(maapDaemonPort, "%u", maapPort);

		maapRunning = TRUE;
		int err = pthread_create(&maapThreadHandle, NULL, maapThread, NULL);
		if (err) {
			maapRunning = FALSE;
			maapState = MAAP_STATE_ERROR;
			AVB_LOGF_ERROR("Failed to start MAAP thread: %s", strerror(err));
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAAP);
	return true;
}

void openavbMaapFinalize()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAAP);

	maapRestartCallback = NULL;
	maapPreferredAddress = NULL;

	if (maapRunning) {
		// Stop the MAAP thread.
		maapRunning = FALSE;
		pthread_join(maapThreadHandle, NULL);
	}

	MUTEX_CREATE_ERR();
	MUTEX_DESTROY(maapMutex);
	MUTEX_LOG_ERR("Error destroying mutex");

	AVB_TRACE_EXIT(AVB_TRACE_MAAP);
}

bool openavbMaapDaemonAvailable(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAAP);

	if (!maapRunning) {
		AVB_TRACE_EXIT(AVB_TRACE_MAAP);
		return FALSE;
	}

	// Wait up to 10 seconds for the daemon to give us a block of addresses.
	int j;
	for (j = 0; j < 10 * 1000 / 50 && maapState < MAAP_STATE_CONNECTED; ++j) {
		AVB_LOG_DEBUG("Waiting for MAAP Daemon status");
		SLEEP_MSEC(50);
	}

	if (maapState != MAAP_STATE_CONNECTED) {
		if (maapState != MAAP_STATE_NOT_AVAILABLE) {
			AVB_LOG_WARNING("MAAP Daemon not available.  Fallback multicast address allocation will be used.");
		}
		AVB_TRACE_EXIT(AVB_TRACE_MAAP);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAAP);
	return TRUE;
}

void* openavbMaapAllocate(int count, /* out */ struct ether_addr *addr)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAAP);
	assert(count == 1);

	// Wait up to 10 seconds for the daemon to give us a block of addresses.
	int j;
	for (j = 0; j < 10 * 1000 / 50 && maapState < MAAP_STATE_CONNECTED; ++j) {
		AVB_LOG_DEBUG("Waiting for MAAP Daemon status");
		SLEEP_MSEC(50);
	}

	MAAP_LOCK();

	// Find the next non-allocated address.
	int i = 0;
	while (i < MAX_AVB_STREAMS && maapAllocList[i].taken) {
		i++;
	}

	// Allocate an address from the pool.
	if (i < MAX_AVB_STREAMS) {
		maapAllocList[i].taken = true;
		memcpy(addr, maapAllocList[i].destAddr.ether_addr_octet, sizeof(struct ether_addr));
		AVB_LOGF_INFO("Allocated MAAP address " ETH_FORMAT, ETH_OCTETS(addr->ether_addr_octet));

		MAAP_UNLOCK();

		AVB_TRACE_EXIT(AVB_TRACE_MAAP);
		return &maapAllocList[i];
	}

	MAAP_UNLOCK();

	AVB_LOG_ERROR("All MAAP addresses already allocated");
	AVB_TRACE_EXIT(AVB_TRACE_MAAP);
	return NULL;
}

void openavbMaapRelease(void* handle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAAP);

	MAAP_LOCK();
	maapAlloc_t *elem = handle;
	elem->taken = false;
	AVB_LOGF_DEBUG("Freed MAAP address " ETH_FORMAT, ETH_OCTETS(elem->destAddr.ether_addr_octet));
	MAAP_UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_MAAP);
}
