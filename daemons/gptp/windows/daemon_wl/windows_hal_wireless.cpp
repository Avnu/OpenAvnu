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

#include <windows_hal_wireless.hpp>

#define CORRELATEDTIME_TRANSACTION_TIMEOUT (110/*ms*/)

void WirelessTimestamperCallback(LPVOID arg) {
	WirelessTimestamperCallbackArg *larg =
		(WirelessTimestamperCallbackArg *)arg;
	WirelessDialog tmp1, tmp2;
	LinkLayerAddress *peer_addr = NULL;

	switch (larg->iEvent_type) {
	default:
	case TIMINGMSMT_CONFIRM_EVENT:
		tmp1.action_devclk = larg->event_data.confirm.T1 * 100;
		tmp1.ack_devclk = larg->event_data.confirm.T4 * 100;
		tmp1.dialog_token = (BYTE)larg->event_data.confirm.DialogToken;
		XPTPD_WDEBUG("Got confirm, %hhu(%llu,%llu)", tmp1.dialog_token, tmp1.action_devclk, tmp1.ack_devclk);
		peer_addr = new LinkLayerAddress(larg->event_data.confirm.PeerMACAddress);
		larg->timestamper->timingMeasurementConfirmCB(*peer_addr, &tmp1);
		break;
	case TIMINGMSMT_EVENT:
		tmp1/*prev*/.action_devclk = larg->event_data.indication.indication.T1;
		tmp1/*prev*/.ack_devclk = larg->event_data.indication.indication.T4;
		tmp1/*prev*/.dialog_token = (BYTE)larg->event_data.indication.indication.FollowUpDialogToken;
		tmp2/*curr*/.action_devclk = larg->event_data.indication.indication.T2;
		tmp2/*curr*/.ack_devclk = larg->event_data.indication.indication.T3;
		tmp2/*curr*/.dialog_token = (BYTE)larg->event_data.indication.indication.DialogToken;
		peer_addr = new LinkLayerAddress(larg->event_data.indication.indication.PeerMACAddress);

		larg->timestamper->timeMeasurementIndicationCB(*peer_addr, &tmp2, &tmp1, larg->event_data.indication.indication.VendorSpecifics.Data + 4,
			larg->event_data.indication.indication.VendorSpecifics.Length - 4);
		break;
	case TIMINGMSMT_CORRELATEDTIME_EVENT:
		auto got_timestamp = false;
		static UINT x{};
		AcquireSRWLockExclusive(&larg->timestamper->correlatedtime_lock);
		if (larg->timestamper->correlatedtime_missed_count > 0) {
			--larg->timestamper->correlatedtime_missed_count;
		}
		else {
			got_timestamp = true;
			larg->timestamper->system_counter = larg->event_data.ptm_wa.TSC;
			larg->timestamper->system_time =
				Timestamp((uint64_t)(((long double)larg->timestamper->system_counter) / (((long double)larg->timestamper->getSystemFrequency().QuadPart) / 1000000000)));
			uint64_t gp2_rollover = 10 * (larg->event_data.ptm_wa.LocalClk & 0xFFFFFFFF);
			gp2_rollover = larg->timestamper->_32to64(gp2_rollover);
			larg->timestamper->device_time = Timestamp(gp2_rollover);
			larg->timestamper->correlatedtime_done = true;
			WakeAllConditionVariable(&larg->timestamper->correlatedtime_cond);
		}
		ReleaseSRWLockExclusive(&larg->timestamper->correlatedtime_lock);
		break;
	}
	if (peer_addr != NULL)
		delete peer_addr;
	delete larg;
}

bool WindowsWirelessTimestamper::HWTimestamper_init
(InterfaceLabel **iface_label, OSNetworkInterface *iface,
	OSLockFactory *lock_factory, OSThreadFactory *thread_factory,
	OSTimerFactory *timer_factory) {
	bool ret = true;

	if (!initialized) {
		if (!InitIntelWireless()) return false;
		if (dynamic_cast<LinkLayerAddress *>(iface_label[0]) == NULL)
			return false;
		local_interface_address = *dynamic_cast<LinkLayerAddress *>(iface_label[0]);
		uint8_t mac_addr_local[ETHER_ADDR_OCTETS];
		local_interface_address.toOctetArray(mac_addr_local);
		if (!IntelFindAdapter(&hAdapter, mac_addr_local)) {
			ret = false;
			goto done;
		}

		InitializeConditionVariable(&correlatedtime_cond);
		InitializeSRWLock(&correlatedtime_lock);
		correlatedtime_missed_count = 0;

		QueryPerformanceFrequency(&tsc_hz);

		IntelRegisterTimestamper(this);
	}

	ret = WirelessTimestamper::HWTimestamper_init(iface_label[1], iface, lock_factory, thread_factory, timer_factory);

done:
	return ret;
}

net_result WindowsWirelessTimestamper::_requestTimingMeasurement
(LinkLayerAddress dest, uint8_t dialog_token, WirelessDialog *prev_dialog,
	uint8_t *follow_up, int followup_length) {
	net_result ret = net_succeed;
	TIMINGMSMT_REQUEST *tm_request;
	unsigned oui = 0xC28000;
	uint8_t *msgbuf = new uint8_t[sizeof(*tm_request) + followup_length + OUI_LENGTH + 1]; // 1 byte of type
	tm_request = (TIMINGMSMT_REQUEST *)msgbuf;
	memset(tm_request, 0, sizeof(*tm_request));
	tm_request->DialogToken = dialog_token;
	tm_request->FollowUpDialogToken = prev_dialog->dialog_token;
	if (tm_request->FollowUpDialogToken != 0 && prev_dialog->action_devclk != 0) {
		tm_request->T1 = (DWORD)(prev_dialog->action_devclk & 0xFFFFFFFF);
		tm_request->T4 = (DWORD)(prev_dialog->ack_devclk & 0xFFFFFFFF);
		tm_request->VendorSpecifics.Length = followup_length + OUI_LENGTH + 1; // 1 byte type
		tm_request->VendorSpecifics.ElementId = 0xDD;
		memcpy(tm_request->VendorSpecifics.Data, &oui, OUI_LENGTH);
		tm_request->VendorSpecifics.Data[3] = 0;  // Type field this is always set to zero
		memcpy(tm_request->VendorSpecifics.Data + 4, follow_up, followup_length);
	}
	dest.toOctetArray(tm_request->PeerMACAddress);
	if(!IntelInitiateTimingMeasurement(hAdapter,tm_request)) {
		XPTPD_ERROR("Failed to send timing measurement request\n");
		ret = net_fatal;
	}

	if (ret == net_succeed) {
		XPTPD_WDEBUG("Request TM success(!), %hhu(T1=%u,T4=%u)", tm_request->DialogToken, tm_request->T1, tm_request->T4);
	}
	delete msgbuf;
	return ret;
}

#define GETTIME_MISS_MAX 5

bool WindowsWirelessTimestamper::HWTimestamper_gettime(Timestamp *system_time, Timestamp *device_time) {
	bool ret = true;
	DWORD wait_return = TRUE;
	DWORD start;
	DWORD wait;
	start = GetTickCount();

	lock();
	AcquireSRWLockExclusive(&correlatedtime_lock);
	correlatedtime_done = false;
	IntelInitiateCrossTimestamp(hAdapter);
	while (!correlatedtime_done && wait_return == TRUE) {
		DWORD now = GetTickCount();
		wait = now - start < CORRELATEDTIME_TRANSACTION_TIMEOUT ? CORRELATEDTIME_TRANSACTION_TIMEOUT - (now - start) : 0;
		wait_return = SleepConditionVariableSRW(&correlatedtime_cond, &correlatedtime_lock, wait, 0);
	}
	if (wait_return == TRUE) {
		*system_time = this->system_time;
		*device_time = this->device_time;
	}
	else if (wait_return == FALSE && GetLastError() == ERROR_TIMEOUT && correlatedtime_missed_count <= GETTIME_MISS_MAX) {
		++correlatedtime_missed_count;
		// Extrapolate new value
		LARGE_INTEGER tsc_now;
		QueryPerformanceCounter(&tsc_now);
		unsigned device_delta = (unsigned)(((long double)(tsc_now.QuadPart - system_counter)) / (((long double)getSystemFrequency().QuadPart) / 1000000000));
		device_delta = (unsigned)(device_delta*getLocalSystemRatio());
		*system_time = Timestamp((uint64_t)(((long double)tsc_now.QuadPart) / ((long double)getSystemFrequency().QuadPart / 1000000000)));
		*device_time = Timestamp(device_delta) + this->device_time;
	}
	else {
		ret = false;
	}
	ReleaseSRWLockExclusive(&correlatedtime_lock);
	unlock();

	return ret;
}

WindowsWirelessTimestamper::~WindowsWirelessTimestamper() {
	IntelDeRegisterTimestamper(this);
	IntelWirelessShutdown();
}
