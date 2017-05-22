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
#include "openavb_platform.h"
#include "openavb_trace.h"
#include "openavb_avdecc_cfg.h"
#include "openavb_avdecc_read_ini_pub.h"
#include "openavb_avdecc_msg.h"
#include "openavb_list.h"

#define	AVB_LOG_COMPONENT	"AVDECC"

#include "openavb_pub.h"
#include "openavb_log.h"

// the following are from openavb_avdecc.c
extern openavb_avdecc_cfg_t gAvdeccCfg;
extern openavb_tl_data_cfg_t * streamList;
extern bool avdeccRunning;

static pthread_t avdeccServerHandle;
static void* avdeccServerThread(void *arg);

static bool avdeccInitSucceeded;

bool startAvdecc(const char* ifname, const char **inifiles, int numfiles)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);
	LOG_EAVB_CORE_VERSION();

	// Ensure that we're running as root
	//  (need to be root to use raw sockets)
	uid_t euid = geteuid();
	if (euid != (uid_t)0) {
		fprintf(stderr, "Error: needs to run as root\n\n");
		goto error;
	}

	// Get the AVDECC configuration
	memset(&gAvdeccCfg, 0, sizeof(openavb_avdecc_cfg_t));
	openavbReadAvdeccConfig(DEFAULT_AVDECC_INI_FILE, &gAvdeccCfg);

	// Determine which interface to use.
	if (ifname) {
		strncpy(gAvdeccCfg.ifname, ifname, sizeof(gAvdeccCfg.ifname));
	} else if (gAvdeccCfg.ifname[0] == '\0') {
		AVB_LOG_ERROR("No interface specified.  Use the -I flag, or add one to " DEFAULT_AVDECC_INI_FILE ".");
		goto error;
	}

	// Read the information from the supplied INI files.
	openavb_tl_data_cfg_t * prevStream = NULL, * newStream;
	U32 i1;
	for (i1 = 0; i1 < numfiles; i1++) {

		char iniFile[1024];
		snprintf(iniFile, sizeof(iniFile), "%s", inifiles[i1]);
		// Create a new item with this INI information.
		newStream = malloc(sizeof(openavb_tl_data_cfg_t));
		if (!newStream) {
			AVB_LOG_ERROR("Out of memory");
			goto error;
		}
		memset(newStream, 0, sizeof(openavb_tl_data_cfg_t));
		if (!openavbReadTlDataIniFile(iniFile, newStream)) {
			AVB_LOGF_ERROR("Error reading ini file: %s", inifiles[i1]);
			goto error;
		}
		// Append this item to the list of items.
		if (!prevStream) {
			// First item.
			streamList = newStream;
		} else {
			// Subsequent item.
			prevStream->next = newStream;
		}
		prevStream = newStream;
	}

	/* Run AVDECC in its own thread. */
	avdeccRunning = TRUE;
	avdeccInitSucceeded = FALSE;
	int err = pthread_create(&avdeccServerHandle, NULL, avdeccServerThread, NULL);
	if (err) {
		AVB_LOGF_ERROR("Failed to start AVDECC thread: %s", strerror(err));
		goto error;
	}

	/* Wait a while to see if the thread was able to start AVDECC. */
	int i;
	for (i = 0; avdeccRunning && !avdeccInitSucceeded && i < 5000; ++i) {
		SLEEP_MSEC(1);
	}
	if (!avdeccInitSucceeded) {
		goto error;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return true;

error:
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return false;
}

void stopAvdecc()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	avdeccRunning = FALSE;
	pthread_join(avdeccServerHandle, NULL);

	AVB_LOG_INFO("Shutting down");

	// Free the INI file items.
	while (streamList) {
		openavb_tl_data_cfg_t * del = streamList;
		streamList = streamList->next;
		free(del);
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
}

static void* avdeccServerThread(void *arg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	while (avdeccRunning) {
		if (!openavbAvdeccInitialize()) {
			AVB_LOG_ERROR("Failed to initialize AVDECC");
			break;
		}

		AVB_LOG_DEBUG("AVDECC Initialized");

		if (!openavbAvdeccStart()) {
			AVB_LOG_ERROR("Failed to start AVDECC");
			openavbAvdeccStop();
			break;
		}

		if (!openavbAvdeccMsgServerOpen()) {
			AVB_LOG_ERROR("Failed to start AVDECC Server");
			openavbAvdeccStop();
			break;
		}
		else {
			AVB_LOG_INFO("AVDECC Started");
			avdeccInitSucceeded = TRUE;

			// Wait until AVDECC is finished.
			while (avdeccRunning) {
				openavbAvdeccMsgSrvrService();
			}

			openavbAvdeccMsgServerClose();
		}

		// Stop AVDECC
		AVB_LOG_DEBUG("AVDECC Stopping");
		openavbAvdeccStop();
		AVB_LOG_INFO("AVDECC Stopped");
	}

	// Shutdown AVDECC
	openavbAvdeccCleanup();

	avdeccRunning = FALSE;

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return NULL;
}
