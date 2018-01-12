/******************************************************************************

Copyright (c) 2009-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

3. Neither the name of the Intel Corporation nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#include <intel_wireless.hpp>
#include <windows_hal.hpp>
#include <work_queue.hpp>
#include <memory>
#include <map>

#define INTEL_EVENT_OFFSET 0x1141
#define GP2_ROLLOVER 4294967296ULL

GetAdapterList_t GetAdapterList;
WifiCmdTimingMeasurementRequest_t WifiCmdTimingMeasurementRequest;
WifiCmdTimingPtmWa_t WifiCmdTimingPtmWa;
RegisterIntelCallback_t RegisterIntelCallback;
DeregisterIntelCallback_t DeregisterIntelCallback;

struct TimestamperContext
{
	WindowsWorkQueue work_queue;
	WindowsWirelessTimestamper *timestamper;
	~TimestamperContext()
	{
		work_queue.stop();
	}
};

typedef std::map< LinkLayerAddress, TimestamperContext > TimestamperContextMap;

class LockedTimestamperContextMap
{
public:
	LockedTimestamperContextMap()
	{
		InitializeSRWLock(&lock);
	}
	TimestamperContextMap map;
	SRWLOCK lock;
};

LockedTimestamperContextMap timestamperContextMap;
HANDLE IntelTimestamperThread;
bool IntelTimestamperThreadStop = false;

void IntelWirelessTimestamperEventHandler( INTEL_EVENT iEvent, void *pContext )
{
	WindowsWirelessTimestamper *timestamper;
	IntelWirelessAdapter *adapter;
	LockedTimestamperContextMap *timestamperContextMap =
		(LockedTimestamperContextMap *)pContext;
	WirelessTimestamperCallbackArg *arg =
		new WirelessTimestamperCallbackArg();
	int vendor_extension_copy_length;
	bool error = false;
	bool submit = false; // If true submit the event for later processing

	// We share driver callback with other events, subtract the offset for
	// timing measurement related events and check this is time sync event
	iEvent.eType =
		(WIRELESS_EVENT_TYPE) (iEvent.eType - INTEL_EVENT_OFFSET);
	switch( iEvent.eType )
	{
	default:
		return;
	case TIMINGMSMT_CONFIRM_EVENT:
	case TIMINGMSMT_EVENT:
	case TIMINGMSMT_CORRELATEDTIME_EVENT:
		break;
	}

	LinkLayerAddress event_source( iEvent.BtMacAddress );
	arg->iEvent_type = (WIRELESS_EVENT_TYPE) iEvent.eType;
	AcquireSRWLockShared( &timestamperContextMap->lock );
	if( timestamperContextMap->map.find( event_source )
	    != timestamperContextMap->map.cend())
	{
		arg->timestamper =
			timestamperContextMap->map[event_source].timestamper;
	} else
	{
		goto bail;
	}

	timestamper = dynamic_cast <WindowsWirelessTimestamper *>
		(arg->timestamper);
	if( timestamper == NULL )
	{
		GPTP_LOG_ERROR( "Unexpected timestamper type encountered" );
		goto bail;
	}
	adapter = dynamic_cast <IntelWirelessAdapter *>
		(timestamper->getAdapter( ));
	if( adapter == NULL )
	{
		GPTP_LOG_ERROR( "Unexpected adapter type encountered" );
		goto bail;
	}

	// The driver implementation makes no guarantee of the lifetime of the
	// iEvent object we make a copy and delete the copy when processing is
	// complete
	switch( arg->iEvent_type )
	{
	case TIMINGMSMT_CONFIRM_EVENT:
		arg->event_data.confirm =
			*(TIMINGMSMT_CONFIRM_EVENT_DATA *)iEvent.data;
		arg->event_data.confirm.T1 =
			adapter->calc_rollover( arg->event_data.confirm.T1 );
		arg->event_data.confirm.T4 =
			adapter->calc_rollover( arg->event_data.confirm.T4 );
		submit = true;

		break;

	case TIMINGMSMT_EVENT:
		arg->event_data.indication.indication =
			*(TIMINGMSMT_EVENT_DATA *)iEvent.data;
		arg->event_data.indication.indication.T2 =
			adapter->calc_rollover
			( arg->event_data.indication.indication.T2/100 );
		arg->event_data.indication.indication.T3 =
			adapter->calc_rollover
			( arg->event_data.indication.indication.T3/100 );
		// Calculate copy length, at most followup message
		// (See S8021AS indication)
		vendor_extension_copy_length = arg->event_data.
			indication.indication.WiFiVSpecHdr.Length - 1;
		vendor_extension_copy_length -= vendor_extension_copy_length -
			sizeof(arg->event_data.indication.followup) > 0 ?
			vendor_extension_copy_length -
			sizeof(arg->event_data.indication.followup) : 0;
		// Copy the rest of the vendor specific field
		memcpy( arg->event_data.indication.followup,
			((BYTE *)iEvent.data) +
			sizeof( arg->event_data.indication.indication ),
			vendor_extension_copy_length );
		submit = true;
		break;
	case TIMINGMSMT_CORRELATEDTIME_EVENT:
		AcquireSRWLockExclusive(&adapter->ct_lock);
		if (adapter->ct_miss > 0)
		{
			--adapter->ct_miss;
		} else
		{
			arg->event_data.ptm_wa =
				*(WIRELESS_CORRELATEDTIME *)iEvent.data;
			arg->event_data.ptm_wa.LocalClk =
				adapter->calc_rollover
				(arg->event_data.ptm_wa.LocalClk);
			adapter->ct_done = true;
			// No need to schedule this, it returns immediately
			WirelessTimestamperCallback(arg);
			WakeAllConditionVariable(&adapter->ct_cond);
		}
		ReleaseSRWLockExclusive(&adapter->ct_lock);

		break;
	}

	if (submit &&
	    !timestamperContextMap->map[event_source].work_queue.submit
	    ( WirelessTimestamperCallback, arg ))
		GPTP_LOG_ERROR("Failed to submit WiFi event");
bail:
	ReleaseSRWLockShared(&timestamperContextMap->lock);
}

DWORD WINAPI IntelWirelessLoop(LPVOID arg)
{
	// Register for callback
	INTEL_CALLBACK tempCallback;
	tempCallback.fnIntelCallback = IntelWirelessTimestamperEventHandler;
	tempCallback.pContext = arg;

	if (RegisterIntelCallback(&tempCallback) != S_OK) {
		GPTP_LOG_ERROR( "Failed to register WiFi callback" );
		return 0;
	}

	while (!IntelTimestamperThreadStop)
	{
		SleepEx(320, true);
	}

	DeregisterIntelCallback(tempCallback.fnIntelCallback);

	return 0;
}

bool IntelWirelessAdapter::initialize(void)
{
	HMODULE MurocApiDLL = LoadLibrary("MurocApi.dll");

	GetAdapterList = (GetAdapterList_t)
		GetProcAddress(MurocApiDLL, "GetAdapterList");
	if( GetAdapterList == NULL )
		return false;

	RegisterIntelCallback =	(RegisterIntelCallback_t)
		GetProcAddress(MurocApiDLL, "RegisterIntelCallback");
	if( RegisterIntelCallback == NULL )
		return false;

	DeregisterIntelCallback = (DeregisterIntelCallback_t)
		GetProcAddress(MurocApiDLL, "DeregisterIntelCallback");
	if( DeregisterIntelCallback == NULL )
		return false;

	WifiCmdTimingPtmWa = (WifiCmdTimingPtmWa_t)
		GetProcAddress(MurocApiDLL, "WifiCmdTimingPtmWa");
	if( WifiCmdTimingPtmWa == NULL )
		return false;

	WifiCmdTimingMeasurementRequest = (WifiCmdTimingMeasurementRequest_t)
		GetProcAddress(MurocApiDLL, "WifiCmdTimingMeasurementRequest");
	if (WifiCmdTimingMeasurementRequest == NULL)
		return false;

	// Initialize crosstimestamp condition
	InitializeConditionVariable(&ct_cond);
	InitializeSRWLock(&ct_lock);
	ct_miss = 0;

	// Spawn thread
	IntelTimestamperThread = CreateThread
		( NULL, 0, IntelWirelessLoop, &timestamperContextMap, 0, NULL);
	return true;
}

void IntelWirelessAdapter::shutdown(void) {
	// Signal thread exit
	IntelTimestamperThreadStop = true;
	// Join thread
	WaitForSingleObject(IntelTimestamperThread, INFINITE);
}

bool IntelWirelessAdapter::refreshCrossTimestamp(void) {
	INTEL_WIFI_HEADER header;
	WIRELESS_CORRELATEDTIME ptm_wa;
	DWORD wait_return = TRUE;
	DWORD start = 0;

	AcquireSRWLockExclusive(&ct_lock);
	ct_done = false;
	header.dwSize = sizeof(ptm_wa);
	header.dwStructVersion = INTEL_STRUCT_VER_LATEST;
	HRESULT hres = WifiCmdTimingPtmWa(hAdapter, &header, &ptm_wa);
	if (hres != S_OK)
		return false;

	while (!ct_done && wait_return == TRUE) {
		DWORD now = GetTickCount();
		start = start == 0 ? now : start;
		DWORD wait = now - start < CORRELATEDTIME_TRANSACTION_TIMEOUT ?
					   CORRELATEDTIME_TRANSACTION_TIMEOUT -
					   (now - start) : 0;
		wait_return = SleepConditionVariableSRW
					   ( &ct_cond, &ct_lock, wait, 0 );
	}
	ct_miss += wait_return != TRUE ? 1 : 0;
	ReleaseSRWLockExclusive(&ct_lock);

	return wait_return == TRUE ? true : false;
}

bool IntelWirelessAdapter::initiateTimingRequest
( TIMINGMSMT_REQUEST *tm_request )
{
	INTEL_WIFI_HEADER header;
	HRESULT err;

	memset(&header, 0, sizeof(header));
	// Proset wants an equivalent, but slightly different struct definition
	header.dwSize = sizeof(*tm_request) - sizeof(PTP_SPEC) + 1;
	header.dwStructVersion = INTEL_STRUCT_VER_LATEST;
	if(( err = WifiCmdTimingMeasurementRequest
	     (hAdapter, &header, tm_request)) != S_OK )
		return false;

	return true;
}

// Find Intel adapter given MAC address and return adapter ID
//
// @adapter(out): Adapter identifier used for all driver interaction
// @mac_addr: HW address of the adapter
// @return: *false* if adapter is not found or driver communication failure
//		occurs, otherwise *true*
//
bool IntelWirelessAdapter::attachAdapter(uint8_t *mac_addr)
{
	PINTEL_ADAPTER_LIST adapter_list;
	int i;

	if (GetAdapterList(&adapter_list) != S_OK)
		return false;

	GPTP_LOG_VERBOSE("Driver query returned %d adapters\n",
			 adapter_list->count);
	for (i = 0; i < adapter_list->count; ++i) {
		if( memcmp(adapter_list->adapter[i].btMACAddress,
			   mac_addr, ETHER_ADDR_OCTETS) == 0 )
			break;
	}

	if (i == adapter_list->count)
	{
		GPTP_LOG_ERROR("Driver failed to find the requested adapter");
		return false;
	}
	hAdapter = adapter_list->adapter[i].hAdapter;

	return true;
}

// Register timestamper with Intel wireless subsystem.
//
// @timestamper: timestamper object that should receive events
// @source: MAC address of local interface
// @return: *false* if the MAC address has already been registered, *true* if
//
bool IntelWirelessAdapter::registerTimestamper
(WindowsWirelessTimestamper *timestamper)
{
	bool ret = false;

	AcquireSRWLockExclusive(&timestamperContextMap.lock);
	// Do not "re-add" the same timestamper
	if( timestamperContextMap.map.find
	    ( *timestamper->getPort()->getLocalAddr() )
	    == timestamperContextMap.map.cend())
	{
		// Initialize per timestamper context and add
		timestamperContextMap.map
			[*timestamper->getPort()->getLocalAddr()].
			work_queue.init(0);
		timestamperContextMap.map
			[*timestamper->getPort()->getLocalAddr()].
			timestamper = timestamper;
		ret = true;
	}
	ReleaseSRWLockExclusive(&timestamperContextMap.lock);

	return ret;
}

bool IntelWirelessAdapter::deregisterTimestamper
( WindowsWirelessTimestamper *timestamper )
{
	bool ret;

	TimestamperContextMap::iterator iter;
	AcquireSRWLockExclusive(&timestamperContextMap.lock);
	// Find timestamper
	iter = timestamperContextMap.map.find
		( *timestamper->getPort()->getLocalAddr( ));
	if( iter != timestamperContextMap.map.end( ))
	{
		// Shutdown work queue
		iter->second.work_queue.stop();
		// Remove timestamper from list
		timestamperContextMap.map.erase(iter);
		ret = true;
	}
	ReleaseSRWLockExclusive(&timestamperContextMap.lock);

	return ret;
}

uint64_t IntelWirelessAdapter::calc_rollover( uint64_t gp2 )
{
	gp2 = gp2 & 0xFFFFFFFF;
	uint64_t l_gp2_ext;

	if (gp2_last == ULLONG_MAX)
	{
		gp2_last = gp2;

		return gp2;
	}

	if (gp2 < GP2_ROLLOVER/4 && gp2_last > (GP2_ROLLOVER*3)/4)
		++gp2_ext;

	l_gp2_ext = gp2_ext;
	if( gp2_last < GP2_ROLLOVER / 4 && gp2 >(GP2_ROLLOVER * 3) / 4 )
		--l_gp2_ext;
	else
		gp2_last = gp2;

	return gp2 + ( l_gp2_ext * GP2_ROLLOVER );
}
