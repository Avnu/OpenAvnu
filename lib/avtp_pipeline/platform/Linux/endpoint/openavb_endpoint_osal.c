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
#include "openavb_endpoint.h"
#include "openavb_endpoint_cfg.h"
#include "openavb_rawsock.h"

#define	AVB_LOG_COMPONENT	"Endpoint"
//#define AVB_LOG_LEVEL AVB_LOG_LEVEL_DEBUG
#include "openavb_pub.h"
#include "openavb_log.h"

// the following are from openavb_endpoint.c
extern openavb_endpoint_cfg_t 	x_cfg;
extern bool endpointRunning;
static pthread_t endpointServerHandle;
static void* endpointServerThread(void *arg);


bool startEndpoint(int mode, int ifindex, const char* ifname, unsigned mtu, unsigned link_kbit, unsigned nsr_kbit)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	LOG_EAVB_CORE_VERSION();

	// Ensure that we're running as root
	//  (need to be root to use raw sockets)
	uid_t euid = geteuid();
	if (euid != (uid_t)0) {
		fprintf(stderr, "Error: needs to run as root\n\n");
		goto error;
	}

	// Set endpoint configuration
	memset(&x_cfg, 0, sizeof(openavb_endpoint_cfg_t));
	x_cfg.fqtss_mode = mode;
	x_cfg.ifindex = ifindex;
	x_cfg.mtu = mtu;

	x_cfg.link_kbit = link_kbit;
	x_cfg.nsr_kbit = nsr_kbit;

	openavbReadConfig(DEFAULT_INI_FILE, DEFAULT_SAVE_INI_FILE, &x_cfg);

	if_info_t ifinfo;
	if (ifname && openavbCheckInterface(ifname, &ifinfo)) {
		strncpy(x_cfg.ifname, ifname, sizeof(x_cfg.ifname));
		memcpy(x_cfg.ifmac, &ifinfo.mac, ETH_ALEN);
		x_cfg.ifindex = ifinfo.index;
		x_cfg.mtu = ifinfo.mtu;
	}

	endpointRunning = TRUE;
	int err = pthread_create(&endpointServerHandle, NULL, endpointServerThread, NULL);
	if (err) {
		AVB_LOGF_ERROR("Failed to start endpoint thread: %s", strerror(err));
		goto error;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return true;

error:
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return false;
}

void stopEndpoint()
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	endpointRunning = FALSE;
	pthread_join(endpointServerHandle, NULL);

	openavbUnconfigure(&x_cfg);

	AVB_LOG_INFO("Shutting down");

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
}

static void* endpointServerThread(void *arg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	while (endpointRunning) {
		int err = avbEndpointLoop();
		if (err) {
			AVB_LOG_ERROR("Make sure that mrpd daemon is started.");
			SLEEP(1);
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return NULL;
}
