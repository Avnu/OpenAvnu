/*************************************************************************************************************
Copyright (c) 2012-2013, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
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
#include "openavb_srp.h"

#define	AVB_LOG_COMPONENT	"Endpoint"
#include "openavb_pub.h"
#include "openavb_log.h"

// the following are from openavb_endpoint.c
extern openavb_endpoint_cfg_t 	x_cfg;
extern bool endpointRunning;

static void sigHandler(int signal)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	AVB_LOGF_DEBUG("Received signal: %d", signal);
	// SIGINT means we should shut ourselves down
	if (signal == SIGINT)
		endpointRunning = FALSE;
	else if (signal == SIGUSR2)
		openavbSrpLogAllStreams();

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
}

inline int startPTP(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	// make sure ptp, a seperate process, starts and is using the same interface as endpoint
	int retVal = 0;
	char ptpCmd[80];
	memset(ptpCmd, 0, 80);
	snprintf(ptpCmd, 80, "./openavb_gptp %s -i %s &", x_cfg.ptp_start_opts, x_cfg.ifname);
	AVB_LOGF_INFO("PTP start command: %s", ptpCmd);
	if (system(ptpCmd) != 0) {
		AVB_LOG_ERROR("PTP failed to start - Exiting");
		retVal = -1;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return retVal;
}

inline int stopPTP(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	int retVal = 0;
	if (system("killall -s SIGINT openavb_gptp") != 0) {
		retVal = -1;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return retVal;
}

/*************************************************************
 *
 * main() - sets up signal handlers, and reads configuration file
 * 
 */
int main(int argc, char* argv[])
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	LOG_EAVB_CORE_VERSION();

	char *inifile = NULL;
	int exitVal = -1;

	avbLogInit();

	do {
		
		switch (argc) {
			case 1:
				inifile = DEFAULT_INI_FILE;
				break;
			case 2:
				inifile = argv[1];
				break;
			default:
				fprintf(stderr, "Error: usage is:\n\t%s [endpoint.ini]\n\n", argv[0]);
				break;;
		}
	
		// Ensure that we're running as root
		//  (need to be root to use raw sockets)
		uid_t euid = geteuid();
		if (euid != (uid_t)0) {
			fprintf(stderr, "Error: %s needs to run as root\n\n", argv[0]);
			break;
		}
	
		// Setup signal handlers
		//
		// We catch SIGINT and shutdown cleanly
		endpointRunning = TRUE;
	
		struct sigaction sa;
		sa.sa_handler = sigHandler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0; // not SA_RESTART

		if(sigaction(SIGUSR2, &sa, NULL) == -1)
		{
			AVB_LOG_ERROR("Failed to setup USR2 signal handler");
		}

		if (sigaction(SIGINT, &sa, NULL) == -1)
		{
			AVB_LOG_ERROR("Failed to setup signal handler");
			break;
		}
	
		// Parse our INI file
		memset(&x_cfg, 0, sizeof(openavb_endpoint_cfg_t));
		AVB_LOGF_INFO("Reading configuration: %s", inifile);
	
		if (openavbReadConfig(inifile, &x_cfg) == 0) {
			if(avbEndpointLoop() < 0)
			   break;
		}else {
			AVB_LOG_ERROR("Failed to read configuration");
		}
	
		AVB_LOG_INFO("Shutting down");

		openavbUnconfigure(&x_cfg);
	
		exitVal = 0;
	} while (0);

	avbLogExit();

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	exit(exitVal);
}
