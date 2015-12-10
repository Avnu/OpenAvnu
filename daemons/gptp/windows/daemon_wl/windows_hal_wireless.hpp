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

#ifndef WINDOWS_HAL_WIRELESS_HPP
#define WINDOWS_HAL_WIRELESS_HPP

#include <windows_hal_common.hpp>
#include <wltimestamper.hpp>
#include <intel_wireless.hpp>

class WindowsWirelessTimestamper : public WirelessTimestamper, public WindowsTimestamperBase {
private:
	HADAPTER hAdapter;
	uint64_t system_counter;
	Timestamp system_time;
	Timestamp device_time;
	CONDITION_VARIABLE correlatedtime_cond;
	SRWLOCK correlatedtime_lock;
	bool correlatedtime_done;
	int correlatedtime_missed_count;
	LinkLayerAddress local_interface_address;
public:
	virtual bool HWTimestamper_init
		(InterfaceLabel **iface_label, OSNetworkInterface *iface,
			OSLockFactory *lock_factory, OSThreadFactory *thread_factory,
			OSTimerFactory *timer_factory);

	virtual net_result _requestTimingMeasurement
		(LinkLayerAddress dest, uint8_t dialog_token, WirelessDialog *prev_dialog,
			uint8_t *follow_up, int followup_length);
	virtual bool HWTimestamper_gettime(Timestamp *system_time, Timestamp *device_time);

	WindowsWirelessTimestamper(OSConditionFactory *condition_factory) : WirelessTimestamper(condition_factory) {
		hAdapter = NULL;
	}
	LinkLayerAddress getLocalInterfaceAddress(void) {
		return local_interface_address;
	}
	friend void WirelessTimestamperCallback(LPVOID arg);
	~WindowsWirelessTimestamper();
};

struct S8021AS_Indication {
	TIMINGMSMT_EVENT_DATA indication;
	BYTE followup[79]; // (34 header + 10 followup + 32 TLV + 3 OUI + 1 type) - 1 byte contained in indication struct = 79
};

union TimeSyncEventData {
	TIMINGMSMT_CONFIRM_EVENT_DATA confirm;
	S8021AS_Indication indication;
	WIRELESS_CORRELATEDTIME ptm_wa;
};

struct WirelessTimestamperCallbackArg {
	WIRELESS_EVENT_TYPE iEvent_type;
	WindowsWirelessTimestamper *timestamper;
	TimeSyncEventData event_data;
};

#endif/*WINDOWS_HAL_WIRELESS_HPP*/
