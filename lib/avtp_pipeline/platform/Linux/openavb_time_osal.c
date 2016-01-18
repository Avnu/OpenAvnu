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

#include <inttypes.h>
#include <linux/ptp_clock.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "igb.h"
#include "avb.h"

#include "openavb_platform.h"
#include "openavb_time_osal.h"
#include "openavb_time_hal.h"
#include "openavb_trace.h"

#define	AVB_LOG_COMPONENT	"osalTime"
#include "openavb_pub.h"
#include "openavb_log.h"

//#include "openavb_time_util_osal.h"

static pthread_mutex_t gOSALTimeInitMutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK()  	pthread_mutex_lock(&gOSALTimeInitMutex)
#define UNLOCK()	pthread_mutex_unlock(&gOSALTimeInitMutex)

static bool bInitialized = FALSE;
static int gIgbShmFd = -1;
static char *gIgbMmap = NULL;
gPtpTimeData gPtpTD;

static bool x_timeInit(void) {
	AVB_TRACE_ENTRY(AVB_TRACE_TIME);

	if (!halTimeInitialize()) {
		AVB_LOG_ERROR("HAL Time Init failed");
		AVB_TRACE_EXIT(AVB_TRACE_TIME);
		return FALSE;
	}

	if (!gptpinit(&gIgbShmFd, &gIgbMmap)) {
		AVB_LOG_ERROR("GPTP init failed");
		AVB_TRACE_EXIT(AVB_TRACE_TIME);
		return FALSE;
	}

	if (!gptpscaling(&gPtpTD, gIgbMmap)) {
		AVB_LOG_ERROR("GPTP scaling failed");
		AVB_TRACE_EXIT(AVB_TRACE_TIME);
		return FALSE;
	}

	AVB_LOGF_INFO("local_time = %" PRIu64, gPtpTD.local_time);
	AVB_LOGF_INFO("ml_phoffset = %" PRId64 ", ls_phoffset = %" PRId64, gPtpTD.ml_phoffset, gPtpTD.ls_phoffset);
	AVB_LOGF_INFO("ml_freqffset = %Lf, ls_freqoffset = %Lf", gPtpTD.ml_freqoffset, gPtpTD.ls_freqoffset);

	AVB_TRACE_EXIT(AVB_TRACE_TIME);
	return TRUE;
}

static bool x_getPTPTime(U64 *timeNsec) {
	AVB_TRACE_ENTRY(AVB_TRACE_TIME);

	if (!gptpscaling(&gPtpTD, gIgbMmap)) {
		AVB_LOG_ERROR("GPTP scaling failed");
		AVB_TRACE_EXIT(AVB_TRACE_TIME);
		return FALSE;
	}

	uint64_t now_local;
	uint64_t update_8021as;
	int64_t delta_8021as;
	int64_t delta_local;

	if (gptplocaltime(&gPtpTD, &now_local)) {
		update_8021as = gPtpTD.local_time - gPtpTD.ml_phoffset;
		delta_local = now_local - gPtpTD.local_time;
		delta_8021as = gPtpTD.ml_freqoffset * delta_local;
		*timeNsec = update_8021as + delta_8021as;

		AVB_TRACE_EXIT(AVB_TRACE_TIME);
		return TRUE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TIME);
	return FALSE;
}

bool osalAVBTimeInit(void) {
	AVB_TRACE_ENTRY(AVB_TRACE_TIME);

	LOCK();
	if (!bInitialized) {
		if (x_timeInit())
			bInitialized = TRUE;
	}
	UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_TIME);
	return TRUE;
}

bool osalAVBTimeClose(void) {
	AVB_TRACE_ENTRY(AVB_TRACE_TIME);

	gptpdeinit(gIgbShmFd, gIgbMmap);

	halTimeFinalize();

	AVB_TRACE_EXIT(AVB_TRACE_TIME);
	return TRUE;
}

bool osalClockGettime(openavb_clockId_t openavbClockId, struct timespec *getTime) {
	AVB_TRACE_ENTRY(AVB_TRACE_TIME);

	if (openavbClockId < OPENAVB_CLOCK_WALLTIME) {
		clockid_t clockId = CLOCK_MONOTONIC;
		switch (openavbClockId) {
		case OPENAVB_CLOCK_REALTIME:
			clockId = CLOCK_REALTIME;
			break;
		case OPENAVB_CLOCK_MONOTONIC:
			clockId = CLOCK_MONOTONIC;
			break;
		case OPENAVB_TIMER_CLOCK:
			clockId = CLOCK_MONOTONIC;
			break;
		case OPENAVB_CLOCK_WALLTIME:
			break;
		}
		if (!clock_gettime(clockId, getTime)) return TRUE;
	}
	else if (openavbClockId == OPENAVB_CLOCK_WALLTIME) {
		U64 timeNsec;
		if (!x_getPTPTime(&timeNsec)) {
			AVB_TRACE_EXIT(AVB_TRACE_TIME);
			return FALSE;
		}
		getTime->tv_sec = timeNsec / NANOSECONDS_PER_SECOND;
		getTime->tv_nsec = timeNsec % NANOSECONDS_PER_SECOND;
		AVB_TRACE_EXIT(AVB_TRACE_TIME);
		return TRUE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_TIME);
	return FALSE;
}

bool osalClockGettime64(openavb_clockId_t openavbClockId, U64 *timeNsec) {
	if (openavbClockId < OPENAVB_CLOCK_WALLTIME) {
		clockid_t clockId = CLOCK_MONOTONIC;
		switch (openavbClockId) {
		case OPENAVB_CLOCK_REALTIME:
			clockId = CLOCK_REALTIME;
			break;
		case OPENAVB_CLOCK_MONOTONIC:
			clockId = CLOCK_MONOTONIC;
			break;
		case OPENAVB_TIMER_CLOCK:
			clockId = CLOCK_MONOTONIC;
			break;
		case OPENAVB_CLOCK_WALLTIME:
			break;
		}
		struct timespec getTime;
		if (!clock_gettime(clockId, &getTime)) {
			*timeNsec = ((U64)getTime.tv_sec * (U64)NANOSECONDS_PER_SECOND) + (U64)getTime.tv_nsec;
			AVB_TRACE_EXIT(AVB_TRACE_TIME);
			return TRUE;
		}
	}
	else if (openavbClockId == OPENAVB_CLOCK_WALLTIME) {
		AVB_TRACE_EXIT(AVB_TRACE_TIME);
		return x_getPTPTime(timeNsec);
	}
	AVB_TRACE_EXIT(AVB_TRACE_TIME);
	return FALSE;
}


