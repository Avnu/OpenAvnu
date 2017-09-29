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

/*
* MODULE SUMMARY : AVDECC main implementation.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "openavb_avdecc_pub.h"
#include "openavb_plugin.h"
#include "openavb_trace_pub.h"

#define	AVB_LOG_COMPONENT	"AVDECC Main"
#include "openavb_log_pub.h"

bool avdeccRunning = TRUE;

/***********************************************
 * Signal handler - used to respond to signals.
 * Allows graceful cleanup.
 */
static void openavbAvdeccSigHandler(int signal)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HOST);

	if (signal == SIGINT || signal == SIGTERM) {
		if (avdeccRunning) {
			AVB_LOG_INFO("AVDECC shutting down");
			avdeccRunning = FALSE;
		}
		else {
			// Force shutdown
			exit(2);
		}
	}
	else {
		AVB_LOG_ERROR("Unexpected signal");
	}

	AVB_TRACE_EXIT(AVB_TRACE_HOST);
}

void openavbAvdeccHostUsage(char *programName)
{
	printf(
		"\n"
		"Usage: %s [options] file...\n"
		"  -I val     Use given (val) interface globally, can be overriden by giving the ifname= option to the config line.\n"
		"  -l val     Filename of the log file to use.  If not specified, results will be logged to stderr.\n"
		"\n"
		"Examples:\n"
		"  %s talker.ini\n"
		"    Control 1 stream with data from the ini file.\n\n"
		"  %s talker1.ini talker2.ini\n"
		"    Control 2 streams with data from the ini files.\n\n"
		"  %s -I eth0 talker1.ini listener2.ini\n"
		"    Control 2 streams with data from the ini files, using the eth0 interface.\n\n"
		,
		programName, programName, programName, programName);
}

/**********************************************
 * main
 */
int main(int argc, char *argv[])
{
	AVB_TRACE_ENTRY(AVB_TRACE_HOST);

	char *programName;
	char *optIfnameGlobal = NULL;
	char *optLogFileName = NULL;

	programName = strrchr(argv[0], '/');
	programName = programName ? programName + 1 : argv[0];

	if (argc < 2) {
		openavbAvdeccHostUsage(programName);
		exit(-1);
	}

	// Process command line
	bool optDone = FALSE;
	while (!optDone) {
		int opt = getopt(argc, argv, "hI:l:");
		if (opt != EOF) {
			switch (opt) {
				case 'I':
					optIfnameGlobal = strdup(optarg);
					break;
				case 'l':
					optLogFileName = strdup(optarg);
					break;
				case 'h':
				default:
					openavbAvdeccHostUsage(programName);
					exit(-1);
			}
		}
		else {
			optDone = TRUE;
		}
	}

	int iniIdx = optind;
	int tlCount = argc - iniIdx;

	if (!osalAvdeccInitialize(optLogFileName, optIfnameGlobal, (const char **) (argv + iniIdx), tlCount)) {
		osalAvdeccFinalize();
		exit(-1);
	}

	// Setup signal handler
	// We catch SIGINT and shutdown cleanly
	bool err;
	struct sigaction sa;
	sa.sa_handler = openavbAvdeccSigHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	err = sigaction(SIGINT, &sa, NULL);
	if (err)
	{
		AVB_LOG_ERROR("Failed to setup SIGINT handler");
		osalAvdeccFinalize();
		exit(-1);
	}
	err = sigaction(SIGTERM, &sa, NULL);
	if (err)
	{
		AVB_LOG_ERROR("Failed to setup SIGTERM handler");
		osalAvdeccFinalize();
		exit(-1);
	}
	err = sigaction(SIGUSR1, &sa, NULL);
	if (err)
	{
		AVB_LOG_ERROR("Failed to setup SIGUSR1 handler");
		osalAvdeccFinalize();
		exit(-1);
	}

	// Ignore SIGPIPE signals.
	signal(SIGPIPE, SIG_IGN);

	while (avdeccRunning) {
		SLEEP_MSEC(1);
	}

	osalAvdeccFinalize();

	if (optLogFileName) {
		free(optLogFileName);
		optLogFileName = NULL;
	}

	AVB_TRACE_EXIT(AVB_TRACE_HOST);
	exit(0);
}
