/*************************************************************************************************************
Copyright (c) 2012-2017, Harman International Industries, Incorporated
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
#include "openavb_ether_hal.h"
#include "openavb_list.h"

#define	AVB_LOG_COMPONENT	"AVDECC"

#include "openavb_pub.h"
#include "openavb_log.h"

// the following are from openavb_avdecc.c
extern openavb_avdecc_cfg_t gAvdeccCfg;
extern bool avdeccRunning;

static pthread_t avdeccServerHandle;
static void* avdeccServerThread(void *arg);

static bool avdeccInitSucceeded;

bool startAVDECC(int mode, int ifindex, const char* ifname, unsigned mtu, unsigned link_kbit, unsigned nsr_kbit)
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

	// Set the configuration
	memset(&gAvdeccCfg, 0, sizeof(openavb_avdecc_cfg_t));
	if (ifname)
		strncpy(gAvdeccCfg.ifname, ifname, sizeof(gAvdeccCfg.ifname));

	igbGetMacAddr(gAvdeccCfg.ifmac);

	// Get the AVDECC configuration
	openavbReadAvdeccConfig(DEFAULT_AVDECC_INI_FILE, &gAvdeccCfg);

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
	for (i = 0; avdeccRunning && !avdeccInitSucceeded && i < 5; ++i) {
		SLEEP(1);
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

void stopAVDECC()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	avdeccRunning = FALSE;
	pthread_join(avdeccServerHandle, NULL);

	AVB_LOG_INFO("Shutting down");

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

		AVB_LOG_INFO("AVDECC Started");
		avdeccInitSucceeded = TRUE;

		// Wait until AVDECC is finished.
		while (avdeccRunning) {
			SLEEP(1);
		}

		// Stop AVDECC
		openavbAvdeccStop();

		AVB_LOG_INFO("AVDECC Stopped");
	}

	// Shutdown AVDECC
	openavbAvdeccCleanup();

	avdeccRunning = FALSE;

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return NULL;
}
