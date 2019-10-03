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
 * MODULE : AVB Queue Manager
 */

#include <openavb_types.h>

#define AVB_LOG_COMPONENT "QMGR"
//#define AVB_LOG_LEVEL AVB_LOG_LEVEL_DEBUG
#include "openavb_log.h"
#include "openavb_trace.h"

#include "openavb_qmgr.h"
#include "avb_sched.h"

#if (AVB_FEATURE_IGB)
#include "openavb_igb.h"
#endif
#if (AVB_FEATURE_ATL)
#include "openavb_atl.h"
#endif

#define AVB_DEFAULT_QDISC_MODE AVB_SHAPER_HWQ_PER_CLASS

// We have a singleton Qmgr, so we use file-static data here

// Qdisc configuration
typedef struct {
#if (AVB_FEATURE_IGB)
	device_t *igb_dev;
#endif
#if (AVB_FEATURE_ATL)
	device_t *atl_dev;
#endif
	int mode;
	int ifindex;
	char ifname[IFNAMSIZ + 10]; // Include space for the socket type prefix (e.g. "simple:eth0")
	U32 linkKbit;
	U32 nsrKbit;
	U32 linkMTU;
	int ref;
} qdisc_data_t;

static qdisc_data_t qdisc_data = {
#if (AVB_FEATURE_IGB)
	NULL,
#endif
#if (AVB_FEATURE_ATL)
	NULL,
#endif
	0, 0, {0}, 0, 0, 0, 0
};

// We do get accessed from multiple threads, so need a mutex
pthread_mutex_t qmgr_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#define LOCK()  	pthread_mutex_lock(&qmgr_mutex)
#define UNLOCK()	pthread_mutex_unlock(&qmgr_mutex)

// Information for each SR class
typedef struct {
	unsigned classBytesPerSec;
} qmgrClass_t;

// Information for each active stream
typedef struct {
	unsigned streamBytesPerSec;
	unsigned classRate;
	unsigned maxIntervalFrames;
	unsigned maxFrameSize;
} qmgrStream_t;

// Arrays to hold info for classes and streams
static qmgrClass_t  qmgr_classes[MAX_AVB_SR_CLASSES];
static qmgrStream_t qmgr_streams[MAX_AVB_STREAMS];

// Make sure that the scheme we're using to encode the class/stream
// into the fwmark will work!  (we encode class and stream into 16-bits)
#if MAX_AVB_STREAMS_PER_CLASS > (1 << TC_AVB_CLASS_SHIFT)
#error MAX_AVB_STREAMS_PER_CLASS too large for FWMARK encoding
#endif

static bool setupHWQueue(int nClass, unsigned classBytesPerSec)
{
	int err = 0;
#if (AVB_FEATURE_IGB) || (AVB_FEATURE_ATL)
	U32 class_a_bytes_per_sec, class_b_bytes_per_sec;
#endif

	AVB_TRACE_ENTRY(AVB_TRACE_QUEUE_MANAGER);

#if (AVB_FEATURE_IGB) || (AVB_FEATURE_ATL)
	if (nClass == SR_CLASS_A) {
		class_a_bytes_per_sec = classBytesPerSec;
		class_b_bytes_per_sec = qmgr_classes[SR_CLASS_B].classBytesPerSec;
	} else {
		class_a_bytes_per_sec = qmgr_classes[SR_CLASS_A].classBytesPerSec;
		class_b_bytes_per_sec = classBytesPerSec;
	}
#if (AVB_FEATURE_IGB)
	err = igb_set_class_bandwidth2(qdisc_data.igb_dev, class_a_bytes_per_sec, class_b_bytes_per_sec);
	if (err)
		AVB_LOGF_ERROR("Adding stream; igb_set_class_bandwidth failed: %s", strerror(err));
#else // (AVB_FEATURE_ATL)
	err = atl_set_class_bandwidth(qdisc_data.atl_dev, 
								  class_a_bytes_per_sec, 
								  class_b_bytes_per_sec);
	if (err)
		AVB_LOGF_ERROR("Adding stream; atl_set_class_bandwidth failed: %s", strerror(err));
#endif
#endif

	AVB_TRACE_EXIT(AVB_TRACE_QUEUE_MANAGER);
	return !err;
}

/* Add a stream.
 *
 * 	nClass = index of class (A, B, etc.)
 * 	classRate = class observation interval (8000, 4000, etc.)
 * 	maxIntervalFrames = frames per interval
 * 	maxFrameSize = max size of frames
 *
 */
U16 openavbQmgrAddStream(SRClassIdx_t nClass, unsigned classRate, unsigned maxIntervalFrames, unsigned maxFrameSize)
{
	unsigned fullFrameSize = maxFrameSize + OPENAVB_AVTP_ETHER_FRAME_OVERHEAD + 1;
	unsigned long streamBytesPerSec = fullFrameSize * maxIntervalFrames * classRate;
	int idx, nStream;
	U16 fwmark = INVALID_FWMARK;

	AVB_TRACE_ENTRY(AVB_TRACE_QUEUE_MANAGER);
	LOCK();

	AVB_LOGF_DEBUG("Adding stream; class=%d, rate=%u frames=%u, size=%u(%u), bytes/sec=%lu",
				   nClass, classRate, maxIntervalFrames, maxFrameSize,
				   fullFrameSize, streamBytesPerSec);

	if ((int)nClass < 0 || nClass >= MAX_AVB_SR_CLASSES || streamBytesPerSec == 0) {
		AVB_LOG_ERROR("Adding stream; invalid argument");
	}
	else {
		// Find an unused stream in the appropriate SR class
		for (nStream = 0, idx = nClass * MAX_AVB_STREAMS_PER_CLASS; nStream < MAX_AVB_STREAMS_PER_CLASS; nStream++, idx++) {
			if (0 == qmgr_streams[idx].streamBytesPerSec) {
				fwmark = TC_AVB_MARK(nClass, nStream);
				break;
			}
		}

		if (fwmark == INVALID_FWMARK) {
			AVB_LOGF_ERROR("Adding stream; too many streams in class %d", nClass);
		} else {

			if (qdisc_data.mode != AVB_SHAPER_DISABLED) {
				if (!setupHWQueue(nClass, qmgr_classes[nClass].classBytesPerSec + streamBytesPerSec)) {
					fwmark = INVALID_FWMARK;
				}
			}

			if (fwmark != INVALID_FWMARK) {
				// good to go - update stream
				qmgr_streams[idx].streamBytesPerSec = streamBytesPerSec;
				qmgr_streams[idx].classRate = classRate;
				qmgr_streams[idx].maxIntervalFrames = maxIntervalFrames;
				qmgr_streams[idx].maxFrameSize = maxFrameSize;
				// and class
				qmgr_classes[nClass].classBytesPerSec += streamBytesPerSec;

				AVB_LOGF_DEBUG("Added stream; classBPS=%u, streamBPS=%u", qmgr_classes[nClass].classBytesPerSec, qmgr_streams[idx].streamBytesPerSec);
			}
		}
	}

	UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_QUEUE_MANAGER);
	return fwmark;
}


void openavbQmgrRemoveStream(U16 fwmark)
{
	AVB_TRACE_ENTRY(AVB_TRACE_QUEUE_MANAGER);

	if (fwmark == INVALID_FWMARK) {
		AVB_LOG_ERROR("Removing stream; invalid argument");
		AVB_TRACE_EXIT(AVB_TRACE_QUEUE_MANAGER);
		return;
	}

	int nClass = TC_AVB_MARK_CLASS(fwmark);
	int nStream  = TC_AVB_MARK_STREAM(fwmark);
	int idx = nClass * MAX_AVB_STREAMS_PER_CLASS + nStream;

	LOCK();

	if (nStream < 0
		|| nStream >= MAX_AVB_STREAMS
		|| nClass < 0
		|| nClass >= MAX_AVB_SR_CLASSES
		|| qmgr_streams[idx].streamBytesPerSec == 0)
	{
		// something is wrong
		AVB_LOG_ERROR("Removing stream; invalid argument or data");
	}
	else {
		if (qdisc_data.mode != AVB_SHAPER_DISABLED) {
			setupHWQueue(nClass, qmgr_classes[nClass].classBytesPerSec - qmgr_streams[idx].streamBytesPerSec);
		}

		// update class
		qmgr_classes[nClass].classBytesPerSec -= qmgr_streams[idx].streamBytesPerSec;
		AVB_LOGF_DEBUG("Removed stream; classBPS=%u, streamBPS=%u", qmgr_classes[nClass].classBytesPerSec, qmgr_streams[idx].streamBytesPerSec);
		// and stream
		memset(&qmgr_streams[idx], 0, sizeof(qmgrStream_t));
	}

	UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_QUEUE_MANAGER);
}

bool openavbQmgrInitialize(int mode, int ifindex, const char* ifname, unsigned mtu, unsigned link_kbit, unsigned nsr_kbit)
{
	AVB_TRACE_ENTRY(AVB_TRACE_QUEUE_MANAGER);
	bool ret = FALSE;

//	if (!ifname || mtu == 0 || link_kbit == 0 || nsr_kbit == 0) {
//		AVB_LOG_ERROR("Initializing QMgr; invalid argument");
//		return FALSE;
//	}

	LOCK();

	if (qdisc_data.ref++ > 0) {
		AVB_LOG_DEBUG("Already initialized");

		UNLOCK();
		AVB_TRACE_EXIT(AVB_TRACE_QUEUE_MANAGER);
		return TRUE;
	}

	if (mode >= 0)
		qdisc_data.mode = mode;
	else
		qdisc_data.mode = AVB_DEFAULT_QDISC_MODE;


	AVB_LOGF_DEBUG("Initializing QMgr; mode=%d, idx=%d, mtu=%u, link_kbit=%u, nsr_kbit=%u",
				   qdisc_data.mode, ifindex, mtu, link_kbit, nsr_kbit);

#if (AVB_FEATURE_IGB)
	if ( qdisc_data.mode != AVB_SHAPER_DISABLED
	     && (qdisc_data.igb_dev = igbAcquireDevice()) == 0)
	{
		AVB_LOG_ERROR("Initializing QMgr; unable to acquire igb device");
	}
	else
#endif
#if (AVB_FEATURE_ATL)
	if ( qdisc_data.mode != AVB_SHAPER_DISABLED
	     && (qdisc_data.atl_dev = atlAcquireDevice(ifname)) == 0)
	{
		AVB_LOG_ERROR("Initializing QMgr; unable to acquire atl device");
	}
	else
#endif
	{
		// Initialize data for classes and streams
		memset(qmgr_classes, 0, sizeof(qmgr_classes));
		memset(qmgr_streams, 0, sizeof(qmgr_streams));

		// Save the configuration
		if (ifname)
			strncpy(qdisc_data.ifname, ifname, sizeof(qdisc_data.ifname) - 1);
		qdisc_data.ifindex = ifindex;
		qdisc_data.linkKbit = link_kbit;
		qdisc_data.linkMTU = mtu;
		qdisc_data.nsrKbit  = nsr_kbit;

		if (qdisc_data.mode == AVB_SHAPER_DISABLED) {
			ret = TRUE;
		} else {
			ret = TRUE;
			// igb device aquired, nothing more to do
		}
	}

	UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_QUEUE_MANAGER);
	return ret;
}

void openavbQmgrFinalize(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_QUEUE_MANAGER);
	LOCK();

	if (--qdisc_data.ref == 0 && qdisc_data.mode != AVB_SHAPER_DISABLED) {
		int nClass;
		for (nClass = SR_CLASS_A; nClass < MAX_AVB_SR_CLASSES; nClass++) {
			int nStream, idx;
			for (nStream = 0, idx = nClass * MAX_AVB_STREAMS_PER_CLASS; nStream < MAX_AVB_STREAMS_PER_CLASS; nStream++, idx++) {
				if (qmgr_streams[idx].streamBytesPerSec) {
					U16 fwmark = TC_AVB_MARK(nClass, nStream);
					openavbQmgrRemoveStream(fwmark);
				}
			}
		}

#if (AVB_FEATURE_IGB)
		igbReleaseDevice(qdisc_data.igb_dev);
		qdisc_data.igb_dev = NULL;
#endif
#if (AVB_FEATURE_ATL)
		atlReleaseDevice(qdisc_data.atl_dev);
		qdisc_data.atl_dev = NULL;
#endif
	}

	UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_QUEUE_MANAGER);
}
