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

#include <pci/pci.h>
#include "igb.h"
#include "avb.h"


#include "openavb_ether_hal.h"
#include "openavb_osal.h"

#define	AVB_LOG_COMPONENT	"HAL Ethernet"
#include "openavb_pub.h"
#include "openavb_log.h"
#include "openavb_trace.h"

#define MAX_MTU 1536

static pthread_mutex_t gHALTimeInitMutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK()  	pthread_mutex_lock(&gHALTimeInitMutex)
#define UNLOCK()	pthread_mutex_unlock(&gHALTimeInitMutex)

static bool bInitialized = FALSE;
static device_t gIgbDev;

static bool x_HalEtherInit(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	int rslt;

	rslt = pci_connect(&gIgbDev);
	if (rslt) {
		AVB_LOGF_ERROR("connect failed (%s) - are you running as root?", strerror(errno));
		AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
		return FALSE;
	}

	rslt = igb_init(&gIgbDev);
	if (rslt) {
		AVB_LOGF_ERROR("init failed (%s) - is the driver really loaded?\n", strerror(errno));
		AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return TRUE;
}


// Initialize HAL layer Ethernet driver
bool halEthernetInitialize(U8 *macAddr, bool gmacAutoNegotiate)
{
	LOCK();
	if (!bInitialized) {
		if (x_HalEtherInit())
			bInitialized = TRUE;
	}
	UNLOCK();

	return bInitialized;
}

bool halEthernetFinalize(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);

	// TODO_OPENAVB : shutdown igb

	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return TRUE;
}

bool halEtherAddMulticast(U8 *multicastAddr, bool add)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return TRUE;	
}

U8 *halGetRxBufAVTP(U32 *frameSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return NULL;
}

// Get next Rx packet. NOTE: Expected to be called from a single task (socket task)
U8 *halGetRxBufAVB(U32 *frameSize, bool *bPtpCBUsed)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return NULL;
}

U8 *halGetRxBufGEN(U32 *frameSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return NULL;
}

// Send Tx Packet to driver
bool halSendTxBuffer(U8 *pDataBuf, U32 frameSize, int class)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return FALSE;
}

// Is the link up. Returns TRUE if it is.
bool halIsLinkUp(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
    return FALSE;
}

// Returns the MTU
U32 halGetMTU(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return MAX_MTU;
}

U8 *halGetMacAddr(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
    return NULL;
}

bool halGetLocaltime(U64 *localTime64)
{
	AVB_TRACE_ENTRY(AVB_TRACE_HAL_ETHER);
	if (igb_get_wallclock(&gIgbDev, localTime64, NULL ) > 0) {
		AVB_LOG_ERROR("Failed to get wallclock time");
		AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
		return FALSE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_HAL_ETHER);
	return TRUE;
}

void halTrafficShaperAddStream(int class, uint32_t streamsBytesPerSec)
{

// TODO_OPENAVB : it appears the igb_set_class_bandwidth() helper function is designed about the concept of a single Class A and single Class B 
// stream. Perhaps it could be used in the short term, however, a final solution may be to talk to the igb driver directly supply the E1000 reg
// values directly.
// 
//	igb_set_class_bandwidth(dev)

// TODO_OPENAVB
//	if (class == AVB_CLASS_B) {
// 	}
//	else if (class == AVB_CLASS_A) {
//	}
}

void halTrafficShaperRemoveStream(int class, uint32_t streamsBytesPerSec)
{

// TODO_OPENAVB : it appears the igb_set_class_bandwidth() helper function is designed about the concept of a single Class A and single Class B 
// stream. Perhaps it could be used in the short term, however, a final solution may be to talk to the igb driver directly supply the E1000 reg
// values directly.
// 
//	igb_set_class_bandwidth(dev)

// TODO_OPENAVB
//	if (class == AVB_CLASS_B) {
//  	}
//	else if (class == AVB_CLASS_A) {
//	}
}




