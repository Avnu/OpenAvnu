/******************************************************************************

  Copyright (c) 2009-2012, Intel Corporation 
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

#include <WinSock2.h>
#include <iphlpapi.h>
#include <windows_hal.hpp>
#include <IPCListener.hpp>
#include <PeerList.hpp>
#include <debugout.hpp>


bool WindowsTimestamper::HWTimestamper_gettime(Timestamp *system_time, Timestamp *device_time)
{
	DWORD buf[6];
	DWORD returned;
	uint64_t now_net, now_tsc;
	DWORD result;

	memset(buf, 0xFF, sizeof(buf));
	if ((result = readOID(OID_INTEL_GET_SYSTIM, buf, sizeof(buf), &returned)) != ERROR_SUCCESS) return false;

	now_net = (((uint64_t)buf[1]) << 32) | buf[0];
	now_net = scaleNativeClockToNanoseconds(now_net);
	*device_time = nanoseconds64ToTimestamp(now_net);
	device_time->_version = version;

	now_tsc = (((uint64_t)buf[3]) << 32) | buf[2];
	now_tsc = scaleTSCClockToNanoseconds(now_tsc);
	*system_time = nanoseconds64ToTimestamp(now_tsc);
	system_time->_version = version;

	return true;
}

#define FREQ_TEST_TIME (3000)
#define FREQ_PRECISION_BITS (28)

bool WindowsTimestamper::findNetworkClockFrequency() {
	DWORD buf[6];
	DWORD returned;
	DWORD result;
	uint64_t net_start, net_end;
	uint64_t tsc_start, tsc_end;
	unsigned measured_test_time;

	memset(buf, 0xFF, sizeof(buf));
	if ((result = readOID(OID_INTEL_GET_SYSTIM, buf, sizeof(buf), &returned)) != ERROR_SUCCESS) return false;
	net_start = (((uint64_t)buf[1]) << 32) | buf[0];
	tsc_start = (((uint64_t)buf[3]) << 32) | buf[2];

	Sleep(FREQ_TEST_TIME);

	memset(buf, 0xFF, sizeof(buf));
	if ((result = readOID(OID_INTEL_GET_SYSTIM, buf, sizeof(buf), &returned)) != ERROR_SUCCESS) return false;

	net_end = (((uint64_t)buf[1]) << 32) | buf[0];
	tsc_end = (((uint64_t)buf[3]) << 32) | buf[2];

	measured_test_time = (unsigned)(((tsc_end - tsc_start)<< FREQ_PRECISION_BITS) / tsc_hz.QuadPart);

	netclock_hz.QuadPart = ((net_end - net_start)<< FREQ_PRECISION_BITS) / measured_test_time;

	return true;
}


bool WindowsTimestamper::HWTimestamper_init(InterfaceLabel *iface_label, OSNetworkInterface *net_iface, OSLockFactory *lock_factory, OSThreadFactory *thread_factory, OSTimerFactory *timer_factory) {
	char network_card_id[64];
	bool ret;
	LinkLayerAddress *addr;

	if (!initialized) {
		addr = dynamic_cast<LinkLayerAddress *>(iface_label);
		if (addr == NULL) return false;
		PIP_ADAPTER_INFO pAdapterInfo;
		IP_ADAPTER_INFO AdapterInfo[32];       // Allocate information for up to 32 NICs
		DWORD dwBufLen = sizeof(AdapterInfo);  // Save memory size of buffer

		DWORD dwStatus = GetAdaptersInfo(AdapterInfo, &dwBufLen);
		if (dwStatus != ERROR_SUCCESS) return false;

		for (pAdapterInfo = AdapterInfo; pAdapterInfo != NULL; pAdapterInfo = pAdapterInfo->Next) {
			if (pAdapterInfo->AddressLength == ETHER_ADDR_OCTETS && *addr == LinkLayerAddress(pAdapterInfo->Address)) {
				break;
			}
		}

		if (pAdapterInfo == NULL) return false;

		XPTPD_WDEBUG("Adapter UID: %s\n", pAdapterInfo->AdapterName);
		PLAT_strncpy(network_card_id, NETWORK_CARD_ID_PREFIX, 63);
		PLAT_strncpy(network_card_id + strlen(network_card_id), pAdapterInfo->AdapterName, 63 - strlen(network_card_id));

		miniport = CreateFile(network_card_id,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, 0, NULL);
		if (miniport == INVALID_HANDLE_VALUE) return false;

		QueryPerformanceFrequency(&tsc_hz);
		tsc_hz.QuadPart <<= 10;

		// tsc_hz must have a valid value here
		ret = findNetworkClockFrequency();
		if (!ret)
			return false;
	}

	EthernetTimestamper::HWTimestamper_init(iface_label, net_iface, lock_factory, thread_factory, timer_factory);

	return true;
}

