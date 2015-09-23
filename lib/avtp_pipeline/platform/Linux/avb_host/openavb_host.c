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
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "openavb_tl_pub.h"
#include "openavb_plugin.h"
#include "openavb_trace_pub.h"
#ifdef AVB_FEATURE_GSTREAMER
#include <gst/gst.h>
#endif

#define	AVB_LOG_COMPONENT	"TL Host"
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

void openavbTlHostUsage(char *programName)
{
	printf(
		"\n"
		"Usage: %s [options] file...\n"
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
		,
		programName, programName, programName, programName, programName);
}

/**********************************************
 * main
 */
int main(int argc, char *argv[])
{
	AVB_TRACE_ENTRY(AVB_TRACE_HOST);

	int iniIdx = 0;
	char *programName;
	char *optIfnameGlobal = NULL;

	programName = strrchr(argv[0], '/');
	programName = programName ? programName + 1 : argv[0];

	if (argc < 2) {
		openavbTlHostUsage(programName);
		exit(-1);
	}

	tl_handle_t *tlHandleList = NULL;
	int i1;

	// Process command line
	bool optDone = FALSE;
	while (!optDone) {
		int opt = getopt(argc, argv, "hI:");
		if (opt != EOF) {
			switch (opt) {
				case 'I':
					optIfnameGlobal = strdup(optarg);
					break;
				case 'h':
				default:
					openavbTlHostUsage(programName);
					exit(-1);
			}
		}
		else {
			optDone = TRUE;
		}
	}

	osalAVBInitialize(optIfnameGlobal);

	iniIdx = optind;
	U32 tlCount = argc - iniIdx;

	if (!openavbTLInitialize(tlCount)) {
		AVB_LOG_ERROR("Unable to initialize talker listener library");
		osalAVBFinalize();
		exit(-1);
	}

	// Setup signal handler
	// We catch SIGINT and shutdown cleanly
	bool err;
	struct sigaction sa;
	sa.sa_handler = openavbTLSigHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	err = sigaction(SIGINT, &sa, NULL);
	if (err)
	{
		AVB_LOG_ERROR("Failed to setup SIGINT handler");
		osalAVBFinalize();
		exit(-1);
	}
	err = sigaction(SIGUSR1, &sa, NULL);
	if (err)
	{
		AVB_LOG_ERROR("Failed to setup SIGUSR1 handler");
		osalAVBFinalize();
		exit(-1);
	}

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

	tlHandleList = calloc(1, sizeof(tl_handle_t) * tlCount);

	// Open all streams
	for (i1 = 0; i1 < tlCount; i1++) {
		tlHandleList[i1] = openavbTLOpen();
	}

	// Parse ini and configure all streams
	for (i1 = 0; i1 < tlCount; i1++) {
		openavb_tl_cfg_t cfg;
		openavb_tl_cfg_name_value_t NVCfg;
		char iniFile[1024];

		snprintf(iniFile, sizeof(iniFile), "%s", argv[i1 + iniIdx]);

		if (optIfnameGlobal && !strcasestr(iniFile, ",ifname=")) {
			snprintf(iniFile + strlen(iniFile), sizeof(iniFile), ",ifname=%s", optIfnameGlobal);
		}

		openavbTLInitCfg(&cfg);
		memset(&NVCfg, 0, sizeof(NVCfg));

		if (!openavbTLReadIniFileOsal(tlHandleList[i1], iniFile, &cfg, &NVCfg)) {
			AVB_LOGF_ERROR("Error reading ini file: %s\n", argv[i1 + 1]);
			osalAVBFinalize();
			exit(-1);
		}
		if (!openavbTLConfigure(tlHandleList[i1], &cfg, &NVCfg)) {
			AVB_LOGF_ERROR("Error configuring: %s\n", argv[i1 + 1]);
			osalAVBFinalize();
			exit(-1);
		}

		int i2;
		for (i2 = 0; i2 < NVCfg.nLibCfgItems; i2++) {
			free(NVCfg.libCfgNames[i2]);
			free(NVCfg.libCfgValues[i2]);
		}
	}

#ifdef AVB_FEATURE_GSTREAMER
	// If we're supporting the interface modules which use GStreamer,
	// initialize GStreamer here to avoid errors.
	gst_init(0, NULL);
#endif

	for (i1 = 0; i1 < tlCount; i1++) {
		openavbTLRun(tlHandleList[i1]);
	}

	while (bRunning) {
		sleep(1);
	}

	for (i1 = 0; i1 < tlCount; i1++) {
		openavbTLStop(tlHandleList[i1]);
	}

	for (i1 = 0; i1 < tlCount; i1++) {
		openavbTLClose(tlHandleList[i1]);
	}

	openavbTLCleanup();

#ifdef AVB_FEATURE_GSTREAMER
	// If we're supporting the interface modules which use GStreamer,
	// De-initialize GStreamer to clean up resources.
	gst_deinit();
#endif

	osalAVBFinalize();

	AVB_TRACE_EXIT(AVB_TRACE_HOST);
	exit(0);
}
