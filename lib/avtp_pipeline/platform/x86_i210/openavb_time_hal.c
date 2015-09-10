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

#include "openavb_platform.h"
#include "openavb_time_hal.h"
#include "openavb_ether_hal.h"

#include "openavb_trace.h"

#define	AVB_LOG_COMPONENT	"halTime"
#include "openavb_pub.h"
#include "openavb_log.h"

static device_t *igb_dev = NULL;

bool halTimeInitialize(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TIME);
	igb_dev = igbAcquireDevice();
	AVB_TRACE_EXIT(AVB_TRACE_TIME);
	return TRUE;
}

bool halTimeFinalize(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TIME);
	igbReleaseDevice(igb_dev);
	AVB_TRACE_EXIT(AVB_TRACE_TIME);
	return TRUE;
}

bool halTimeGetLocaltime(U64 *localTime64)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TIME);

	if (igb_get_wallclock(igb_dev, localTime64, NULL ) > 0) {
		IF_LOG_INTERVAL(1000) AVB_LOG_ERROR("Failed to get wallclock time");
		AVB_TRACE_EXIT(AVB_TRACE_TIME);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TIME);
	return TRUE;
}

