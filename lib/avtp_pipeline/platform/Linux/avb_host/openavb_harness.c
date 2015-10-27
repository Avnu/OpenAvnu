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

/*
* MODULE SUMMARY : Talker listener test host implementation.
*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include "openavb_tl_pub.h"
#include "openavb_osal_pub.h"
#include "openavb_plugin.h"
#include "openavb_trace_pub.h"
#ifdef AVB_FEATURE_GSTREAMER
#include <gst/gst.h>
#endif
#include <inttypes.h>

#define	AVB_LOG_COMPONENT	"TL Harness"
#include "openavb_log_pub.h"

bool bRunning = TRUE;

// Platform indendent mapping modules
extern bool openavbMapPipeInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec);
extern bool openavbMapAVTPAudioInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec);
extern bool openavbMapCtrlInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec);
extern bool openavbMapH264Initialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec);
extern bool openavbMapMjpegInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec);
extern bool openavbMapMpeg2tsInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec);
extern bool openavbMapNullInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec);
extern bool openavbMapUncmpAudioInitialize(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec);

// Platform indendent interface modules
extern bool openavbIntfEchoInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB);
extern bool openavbIntfCtrlInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB);
extern bool openavbIntfLoggerInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB);
extern bool openavbIntfNullInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB);
extern bool openavbIntfToneGenInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB);
extern bool openavbIntfViewerInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB);

// Linux interface modules
extern bool openavbIntfAlsaInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB);
extern bool openavbIntfMjpegGstInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB);
extern bool openavbIntfMpeg2tsFileInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB);
extern bool openavbIntfMpeg2tsGstInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB);
extern bool openavbIntfWavFileInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB);
extern bool openavbIntfH264RtpGstInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB);



/***********************************************
 * Signal handler - used to respond to signals.
 * Allows graceful cleanup.
 */
static void openavbTLSigHandler(int signal)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HOST);

	if (signal == SIGINT) {
		AVB_LOG_INFO("Host shutting down");
		bRunning = FALSE;
	}
	else if (signal == SIGUSR1) {
		AVB_LOG_DEBUG("Waking up streaming thread");
	}
	else {
		AVB_LOG_ERROR("Unexpected signal");
	}

	AVB_TRACE_EXIT(AVB_TRACE_HOST);
}

void openavbTlHarnessUsage(char *programName)
{
	printf(
		"\n"
		"Usage: %s [options] file...\n"
		"  -a val     Override stream address in each configuration file.\n"
		"  -h         Prints this message.\n"
		"  -i         Enables interactive mode.\n"
		"  -s val     Stream count. Starts 'val' number of streams for each configuration file. stream_uid will be overriden.\n"
		"  -d val     Last byte of destination address from static pool. Full address will be 91:e0:f0:00:fe:val.\n"
		"  -I val     Use given (val) interface globally, can be overriden by giving the ifname= option to the config line.\n"
		"\n"
		"Examples:\n"
		"  %s talker.ini\n"
		"    Start 1 stream with data from the ini file.\n\n"
		"  %s talker1.ini talker2.ini\n"
		"    Start 2 streams with data from the ini files.\n\n"
		"  %s -I eth0 talker1.ini talker2.ini\n"
		"    Start 2 streams with data from the ini files, both talkers use eth0 interface.\n\n"
		"  %s -I eth0 talker1.ini talker2.ini listener1.ini,ifname=pcap:eth0\n"
		"    Start 3 streams with data from the ini files, talkers 1&2 use eth0 interface, listener1 use pcap:eth0.\n\n"
		"  %s listener.ini,stream_addr=84:7E:40:2C:8F:DE\n"
		"    Start 1 stream and override the sream_addr in the ini file.\n\n"
		"  %s -i -s 8 -a 84:7E:40:2C:8F:DE listener.ini\n"
		"    Work interactively with 8 streams overriding the stream_uid and stream_addr of each.\n\n"
		,
		programName, programName, programName, programName, programName, programName, programName);
}

void openavbTlHarnessMenu()
{
	printf(
		"\n"
		" MENU OPTIONS\n"
		" s            Start all streams\n"
		" t            Stop all streams\n"
		" l            List streams\n"
		" 0-99         Toggle the state of the numbered stream\n"
		" m            Display this menu\n"
		" z            Stats\n"
		" x            Exit\n"
		);
}


/**********************************************
 * main
 */
int main(int argc, char *argv[])
{
	AVB_TRACE_ENTRY(AVB_TRACE_HOST);

	// Command line vars
	char *programName;
	char *optStreamAddr = NULL;
	bool optInteractive = FALSE;
	int optStreamCount = 1;
	bool optStreamCountSet = FALSE;
	bool optDestAddrSet = FALSE;
	U8 destAddr[ETH_ALEN] = {0x91, 0xe0, 0xf0, 0x00, 0xfe, 0x00};
	char *optIfnameGlobal = NULL;

	// Talker listener vars
	int iniIdx = 0;
	int iniCount = 0;
	int tlCount = 0;
	char **tlIniList = NULL;
	tl_handle_t *tlHandleList = NULL;

	// General vars
	int i1, i2;

	// Setup signal handler. Catch SIGINT and shutdown cleanly
	struct sigaction sa;
	sa.sa_handler = openavbTLSigHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0; // not SA_RESTART
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);

	registerStaticMapModule(openavbMapPipeInitialize);
	registerStaticMapModule(openavbMapAVTPAudioInitialize);
	registerStaticMapModule(openavbMapCtrlInitialize);
	registerStaticMapModule(openavbMapH264Initialize);
	registerStaticMapModule(openavbMapMjpegInitialize);
	registerStaticMapModule(openavbMapMpeg2tsInitialize);
	registerStaticMapModule(openavbMapNullInitialize);
	registerStaticMapModule(openavbMapUncmpAudioInitialize);

	registerStaticIntfModule(openavbIntfEchoInitialize);
	registerStaticIntfModule(openavbIntfCtrlInitialize);
	registerStaticIntfModule(openavbIntfLoggerInitialize);
	registerStaticIntfModule(openavbIntfNullInitialize);
	//registerStaticIntfModule(openavbIntfToneGenInitialize);
	registerStaticIntfModule(openavbIntfViewerInitialize);
	registerStaticIntfModule(openavbIntfAlsaInitialize);
	registerStaticIntfModule(openavbIntfMjpegGstInitialize);
	registerStaticIntfModule(openavbIntfMpeg2tsFileInitialize);
	registerStaticIntfModule(openavbIntfMpeg2tsGstInitialize);
	registerStaticIntfModule(openavbIntfWavFileInitialize);
	registerStaticIntfModule(openavbIntfH264RtpGstInitialize);

	// Process command line
	programName = strrchr(argv[0], '/');
	programName = programName ? programName + 1 : argv[0];

	if (argc < 2) {
		openavbTlHarnessUsage(programName);
		exit(-1);
	}

	bool optDone = FALSE;
	while (!optDone) {
		int opt = getopt(argc, argv, "a:his:d:I:");
		if (opt != EOF) {
			switch (opt) {
				case 'a':
					optStreamAddr = strdup(optarg);
					break;
				case 'i':
					optInteractive = TRUE;
					break;
				case 's':
					optStreamCount = atoi(optarg);
					optStreamCountSet = TRUE;
					break;
				case 'd':
					optDestAddrSet = TRUE;
					destAddr[5] = strtol(optarg, NULL, 0);
					break;
				case 'I':
					optIfnameGlobal = strdup(optarg);
					break;
				case '?':
				default:
					openavbTlHarnessUsage(programName);
					exit(-1);
			}
		}
		else {
			optDone = TRUE;
		}
	}

	osalAVBInitialize(optIfnameGlobal);

	// Setup the talker listener counts and lists
	iniIdx = optind;
	iniCount = argc - iniIdx;
	tlCount = iniCount * optStreamCount;

	tlIniList = calloc(1, sizeof(char *) * tlCount);
	if (!tlIniList) {
		AVB_LOG_ERROR("Unable to allocate ini list");
		osalAVBFinalize();
		exit(-1);
	}

	tlHandleList = calloc(1, sizeof(tl_handle_t) * tlCount);
	if (!tlHandleList) {
		AVB_LOG_ERROR("Unable to allocate handle list");
		osalAVBFinalize();
		exit(-1);
	}

	if (!openavbTLInitialize(tlCount)) {
		AVB_LOG_ERROR("Unable to initialize talker listener library");
		osalAVBFinalize();
		exit(-1);
	}

	// Populate the ini file list
	int tlIndex = 0;
	for (i1 = 0; i1 < iniCount; i1++) {
		char iniFile[1024];
		char sStreamAddr[31] = "";
		char sDestAddr[29] = "";

		if (optStreamAddr) {
			snprintf(sStreamAddr, sizeof(sStreamAddr), ",stream_addr=%s", optStreamAddr);
		}

		if (optDestAddrSet) {
			snprintf(sDestAddr, sizeof(sDestAddr), ",dest_addr="ETH_FORMAT, ETH_OCTETS(destAddr));
		}

		if (!optStreamCountSet) {
			snprintf(iniFile, sizeof(iniFile), "%s%s%s", argv[i1 + iniIdx], sDestAddr, sStreamAddr);
			if (optIfnameGlobal && !strcasestr(iniFile, ",ifname=")) {
				snprintf(iniFile + strlen(iniFile), sizeof(iniFile), ",ifname=%s", optIfnameGlobal);
			}
			tlIniList[tlIndex++] = strdup(iniFile);
		}
		else {
			for (i2 = 0; i2 < optStreamCount; i2++) {
				snprintf(iniFile, sizeof(iniFile), "%s%s%s,stream_uid=%d", argv[i1 + iniIdx], sDestAddr, sStreamAddr, tlIndex);
				if (optIfnameGlobal && !strcasestr(iniFile, ",ifname=")) {
					snprintf(iniFile + strlen(iniFile), sizeof(iniFile), ",ifname=%s", optIfnameGlobal);
				}
				tlIniList[tlIndex++] = strdup(iniFile);
				if (optDestAddrSet) {
					destAddr[5]++;
					snprintf(sDestAddr, sizeof(sDestAddr), ",dest_addr="ETH_FORMAT, ETH_OCTETS(destAddr));
				}
			}
		}
	}

#ifdef AVB_FEATURE_GSTREAMER
	// If we're supporting the interface modules which use GStreamer,
	// initialize GStreamer here to avoid errors.
	gst_init(0, NULL);
#endif

	// Open all streams
	for (i1 = 0; i1 < tlCount; i1++) {
		printf("Opening: %s\n", tlIniList[i1]);
		tlHandleList[i1] = openavbTLOpen();
	}

	// Parse ini and configure all streams
	for (i1 = 0; i1 < tlCount; i1++) {
		printf("Configuring: %s\n", tlIniList[i1]);
		openavb_tl_cfg_t cfg;
		openavb_tl_cfg_name_value_t NVCfg;

		openavbTLInitCfg(&cfg);
		memset(&NVCfg, 0, sizeof(NVCfg));

		if (!openavbTLReadIniFileOsal(tlHandleList[i1], tlIniList[i1], &cfg, &NVCfg)) {
			printf("Error reading ini file: %s\n", tlIniList[i1]);
			osalAVBFinalize();
			exit(-1);
		}
		if (!openavbTLConfigure(tlHandleList[i1], &cfg, &NVCfg)) {
			printf("Error configuring: %s\n", tlIniList[i1]);
			osalAVBFinalize();
			exit(-1);
		}

		int i2;
		for (i2 = 0; i2 < NVCfg.nLibCfgItems; i2++) {
			free(NVCfg.libCfgNames[i2]);
			free(NVCfg.libCfgValues[i2]);
		}
	}

	if (!optInteractive) {
		// Non-interactive mode
		// Run the streams
		for (i1 = 0; i1 < tlCount; i1++) {
			printf("Starting: %s\n", tlIniList[i1]);
			openavbTLRun(tlHandleList[i1]);
		}

		while (bRunning) {
			sleep(1);
		}

		for (i1 = 0; i1 < tlCount; i1++) {
			if (tlHandleList[i1] && openavbTLIsRunning(tlHandleList[i1])) {
				printf("Stopping: %s\n", tlIniList[i1]);
				openavbTLStop(tlHandleList[i1]);
			}
		}
	}
	else {
		// Interactive mode

		openavbTlHarnessMenu();
		while (bRunning) {
			char buf[16];
			printf("> ");
			if (fgets(buf, sizeof(buf), stdin) == NULL) {
				openavbTlHarnessMenu();
				continue;
			}

			switch (buf[0]) {
				case 's':
					// Start all streams
					{
						int i1;
						for (i1 = 0; i1 < tlCount; i1++) {
							if (tlHandleList[i1] && !openavbTLIsRunning(tlHandleList[i1])) {
								printf("Starting: %s\n", tlIniList[i1]);
								openavbTLRun(tlHandleList[i1]);
							}
						}
					}
					break;
				case 't':
					// Stop all streams
					{
						int i1;
						for (i1 = 0; i1 < tlCount; i1++) {
							if (tlHandleList[i1] && openavbTLIsRunning(tlHandleList[i1])) {
								printf("Stopping: %s\n", tlIniList[i1]);
								openavbTLStop(tlHandleList[i1]);
							}
						}
					}
					break;
				case 'l':
					// List all streams
					{
						int i1;
						for (i1 = 0; i1 < tlCount; i1++) {
							if (tlHandleList[i1] && openavbTLIsRunning(tlHandleList[i1])) {
								printf("%02d: [Started] %s\n", i1, tlIniList[i1]);
							}
							else {
								printf("%02d: [Stopped] %s\n", i1, tlIniList[i1]);
							}
						}
					}
					break;
				case 'm':
					// Display menu
					openavbTlHarnessMenu();
					break;
				case 'z':
					// Stats
					{
						int i1;
						for (i1 = 0; i1 < tlCount; i1++) {
							if (tlHandleList[i1] && openavbTLIsRunning(tlHandleList[i1])) {
								printf("%02d: [Started] %s\n", i1, tlIniList[i1]);
								if (openavbTLGetRole(tlHandleList[i1]) == AVB_ROLE_TALKER) {
									printf("     Talker totals: calls=%" PRIu64 ", frames=%" PRIu64 ", late=%" PRIu64 ", bytes=%" PRIu64 "\n",
										openavbTLStat(tlHandleList[i1], TL_STAT_TX_CALLS),
										openavbTLStat(tlHandleList[i1], TL_STAT_TX_FRAMES),
										openavbTLStat(tlHandleList[i1], TL_STAT_TX_LATE),
										openavbTLStat(tlHandleList[i1], TL_STAT_TX_BYTES));
								}
								else if (openavbTLGetRole(tlHandleList[i1]) == AVB_ROLE_LISTENER) {
									printf("     Listener totals: calls=%" PRIu64 ", frames=%" PRIu64 ", lost=%" PRIu64 ", bytes=%" PRIu64 "\n",
										openavbTLStat(tlHandleList[i1], TL_STAT_RX_CALLS),
										openavbTLStat(tlHandleList[i1], TL_STAT_RX_FRAMES),
										openavbTLStat(tlHandleList[i1], TL_STAT_RX_LOST),
										openavbTLStat(tlHandleList[i1], TL_STAT_RX_BYTES));
								}
							}
							else {
								printf("%02d: [Stopped] %s\n", i1, tlIniList[i1]);
							}
						}
					}
					break;
				case 'x':
					// Exit
					{
						int i1;
						for (i1 = 0; i1 < tlCount; i1++) {
							if (tlHandleList[i1] && openavbTLIsRunning(tlHandleList[i1])) {
								printf("Stopping: %s\n", tlIniList[i1]);
								openavbTLStop(tlHandleList[i1]);
							}
						}
						bRunning = FALSE;
					}
					break;
				default:
					// Check for number
					{
						if (isdigit(buf[0])) {
							int idx = atoi(buf);
							if (idx < tlCount) {
								if (tlHandleList[idx] && openavbTLIsRunning(tlHandleList[idx])) {
									// Stop the stream
									printf("Stopping: %s\n", tlIniList[idx]);
									openavbTLStop(tlHandleList[idx]);
								}
								else {
									// Start the stream
									printf("Starting: %s\n", tlIniList[idx]);
									openavbTLRun(tlHandleList[idx]);
								}
							}
							else {
								openavbTlHarnessMenu();
							}
						}
						else {
							openavbTlHarnessMenu();
						}
					}
					break;
			}

		}
	}

	// Close the streams
	for (i1 = 0; i1 < tlCount; i1++) {
		if (tlHandleList[i1]) {
			printf("Stopping: %s\n", tlIniList[i1]);

			openavbTLClose(tlHandleList[i1]);

			tlHandleList[i1] = NULL;
		}
	}

	openavbTLCleanup();

	for (i1 = 0; i1 < tlCount; i1++) {
		if (tlIniList[i1]) {
			free(tlIniList[i1]);
			tlIniList[i1] = NULL;
		}
	}

	if (tlIniList) {
		free(tlIniList);
		tlIniList = NULL;
	}

	if (tlHandleList) {
		free(tlHandleList);
		tlHandleList = NULL;
	}

	if (optStreamAddr) {
		free(optStreamAddr);
		optStreamAddr = NULL;
	}

#ifdef AVB_FEATURE_GSTREAMER
	// If we're supporting the interface modules which use GStreamer,
	// De-initialize GStreamer to clean up resources.
	gst_deinit();
#endif

	osalAVBFinalize();
	AVB_TRACE_EXIT(AVB_TRACE_HOST);
	exit(0);
}
