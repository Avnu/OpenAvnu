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

#include <inttypes.h>
#include <linux/ptp_clock.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "avb_gptp.h"

#include "openavb_platform.h"
#include "openavb_grandmaster_osal.h"
#include "openavb_trace.h"

#define	AVB_LOG_COMPONENT	"osalGrandmaster"
#include "openavb_pub.h"
#include "openavb_log.h"

static pthread_mutex_t gOSALGrandmasterInitMutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK()		pthread_mutex_lock(&gOSALGrandmasterInitMutex)
#define UNLOCK()	pthread_mutex_unlock(&gOSALGrandmasterInitMutex)

static bool bInitialized = FALSE;
static int gShmFd = -1;
static char *gMmap = NULL;
gPtpTimeData gPtpTD;

static bool x_grandmasterInit(void) {
	AVB_TRACE_ENTRY(AVB_TRACE_GRANDMASTER);

	if (gptpinit(&gShmFd, &gMmap) < 0) {
		AVB_LOG_ERROR("Grandmaster init failed");
		AVB_TRACE_EXIT(AVB_TRACE_GRANDMASTER);
		return FALSE;
	}

	if (gptpgetdata(gMmap, &gPtpTD) < 0) {
		AVB_LOG_ERROR("Grandmaster data fetch failed");
		AVB_TRACE_EXIT(AVB_TRACE_GRANDMASTER);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_GRANDMASTER);
	return TRUE;
}

bool osalAVBGrandmasterInit(void) {
	AVB_TRACE_ENTRY(AVB_TRACE_GRANDMASTER);

	LOCK();
	if (!bInitialized) {
		if (x_grandmasterInit())
			bInitialized = TRUE;
	}
	UNLOCK();

	AVB_TRACE_EXIT(AVB_TRACE_GRANDMASTER);
	return bInitialized;
}

bool osalAVBGrandmasterClose(void) {
	AVB_TRACE_ENTRY(AVB_TRACE_GRANDMASTER);

	gptpdeinit(&gShmFd, &gMmap);

	AVB_TRACE_EXIT(AVB_TRACE_GRANDMASTER);
	return TRUE;
}

bool osalAVBGrandmasterGetCurrent(
	uint8_t gptp_grandmaster_id[],
	uint8_t * gptp_domain_number )
{
	AVB_TRACE_ENTRY(AVB_TRACE_GRANDMASTER);

	if (gptpgetdata(gMmap, &gPtpTD) < 0) {
		AVB_LOG_ERROR("Grandmaster data fetch failed");
		AVB_TRACE_EXIT(AVB_TRACE_TIME);
		return FALSE;
	}

	if (gptp_grandmaster_id != NULL) { memcpy(gptp_grandmaster_id, gPtpTD.gptp_grandmaster_id, 8); }
	if (gptp_domain_number != NULL) { *gptp_domain_number = gPtpTD.gptp_domain_number; }

	AVB_TRACE_EXIT(AVB_TRACE_GRANDMASTER);
	return TRUE;
}

bool osalClockGrandmasterGetInterface(
	uint8_t clock_identity[],
	uint8_t * priority1,
	uint8_t * clock_class,
	int16_t * offset_scaled_log_variance,
	uint8_t * clock_accuracy,
	uint8_t * priority2,
	uint8_t * domain_number,
	int8_t * log_sync_interval,
	int8_t * log_announce_interval,
	int8_t * log_pdelay_interval,
	uint16_t * port_number)
{
	AVB_TRACE_ENTRY(AVB_TRACE_GRANDMASTER);

	if (gptpgetdata(gMmap, &gPtpTD) < 0) {
		AVB_LOG_ERROR("Grandmaster data fetch failed");
		AVB_TRACE_EXIT(AVB_TRACE_TIME);
		return FALSE;
	}

	if (clock_identity != NULL) { memcpy(clock_identity, gPtpTD.clock_identity, 8); }
	if (priority1 != NULL) { *priority1 = gPtpTD.priority1; }
	if (clock_class != NULL) { *clock_class = gPtpTD.clock_class; }
	if (offset_scaled_log_variance != NULL) { *offset_scaled_log_variance = gPtpTD.offset_scaled_log_variance; }
	if (clock_accuracy != NULL) { *clock_accuracy = gPtpTD.clock_accuracy; }
	if (priority2 != NULL) { *priority2 = gPtpTD.priority2; }
	if (domain_number != NULL) { *domain_number = gPtpTD.domain_number; }
	if (log_sync_interval != NULL) { *log_sync_interval = gPtpTD.log_sync_interval; }
	if (log_announce_interval != NULL) { *log_announce_interval = gPtpTD.log_announce_interval; }
	if (log_pdelay_interval != NULL) { *log_pdelay_interval = gPtpTD.log_pdelay_interval; }
	if (port_number != NULL) { *port_number = gPtpTD.port_number; }

	AVB_TRACE_EXIT(AVB_TRACE_GRANDMASTER);
	return TRUE;
}
