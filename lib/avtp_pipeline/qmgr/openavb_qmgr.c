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
 * MODULE : AVB Queue Manager
 */

#include <openavb_types.h>
#include "openavb_ether_hal.h"
#define AVB_LOG_COMPONENT "QMGR"
#include "openavb_log.h"
#include "openavb_trace.h"

#include "openavb_qmgr.h"
#include "avb_sched.h"

static int fqtss_mode;

bool openavbQmgrInitialize(int mode, int ifindex, char* ifname, unsigned mtu, unsigned link_kbit, unsigned nsr_kbit)
{
  	fqtss_mode = mode;
	return TRUE;
}

void openavbQmgrFinalize(void)
{
}

U16 openavbQmgrAddStream(SRClassIdx_t nClass, unsigned classRate, unsigned maxIntervalFrames, unsigned maxFrameSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_QUEUE_MANAGER);
  
	unsigned long streamBytesPerSec = (maxFrameSize + OPENAVB_AVTP_ETHER_FRAME_OVERHEAD) * maxIntervalFrames * classRate;
	int fwmarkClass = AVB_CLASS_NR;
	if (nClass == SR_CLASS_A)
		fwmarkClass = AVB_CLASS_A;
	else 
		fwmarkClass = AVB_CLASS_B;
	int fwmark = TC_AVB_MARK(streamBytesPerSec, fwmarkClass);

	AVB_LOGF_DEBUG("Adding stream; class=%d, rate=%u frames=%u, size=%u, bytes/sec=%lu", nClass, classRate, maxIntervalFrames, maxFrameSize, streamBytesPerSec);

	if (fqtss_mode == FQTSS_MODE_HW_CLASS) {
		halTrafficShaperAddStream(TC_AVB_MARK_CLASS(fwmark), streamBytesPerSec);
	}  

	AVB_TRACE_EXIT(AVB_TRACE_QUEUE_MANAGER);
	return fwmark;
}

void openavbQmgrRemoveStream(U16 fwmark, unsigned classRate, unsigned maxIntervalFrames, unsigned maxFrameSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_QUEUE_MANAGER);

	if (fqtss_mode == FQTSS_MODE_HW_CLASS) {
		halTrafficShaperRemoveStream(TC_AVB_MARK_CLASS(fwmark), TC_AVB_MARK_STREAM(fwmark));
	}  
	
	AVB_TRACE_EXIT(AVB_TRACE_QUEUE_MANAGER);
}
