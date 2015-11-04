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

#ifndef WINDOWS_HAL_HPP
#define WINDOWS_HAL_HPP

#include <minwindef.h>
#include "avbts_osnet.hpp"
#include "avbts_oslock.hpp"
#include "avbts_oscondition.hpp"
#include "avbts_ostimerq.hpp"
#include "avbts_ostimer.hpp"
#include "avbts_osthread.hpp"
#include "packet.hpp"
#include "ieee1588.hpp"
#include "iphlpapi.h"
#include "ipcdef.hpp"

#include "avbts_osipc.hpp"

#include <ethtimestamper.hpp>

#include <ntddndis.h>

#include <map>
#include <list>

#include <windows_hal_common.hpp>

#define ONE_WAY_PHY_DELAY 7500
#define NETWORK_CARD_ID_PREFIX "\\\\.\\"
#define OID_INTEL_GET_RXSTAMP 0xFF020264
#define OID_INTEL_GET_TXSTAMP 0xFF020263
#define OID_INTEL_GET_SYSTIM  0xFF020262
#define OID_INTEL_SET_SYSTIM  0xFF020261

class WindowsTimestamper : public EthernetTimestamper, public WindowsTimestamperBase {
private:
    // No idea whether the underlying implementation is thread safe
	HANDLE miniport;
	LARGE_INTEGER netclock_hz;
	DWORD readOID( NDIS_OID oid, void *output_buffer, DWORD size, DWORD *size_returned ) {
		NDIS_OID oid_l = oid;
		DWORD rc = DeviceIoControl(
			miniport,
			IOCTL_NDIS_QUERY_GLOBAL_STATS,
			&oid_l,
			sizeof(oid_l),
			output_buffer,
			size,
			size_returned,
			NULL );
		if( rc == 0 ) return GetLastError();
		return ERROR_SUCCESS;
	}
	Timestamp nanoseconds64ToTimestamp( uint64_t time ) {
		Timestamp timestamp;
		timestamp.nanoseconds = time % 1000000000;
		timestamp.seconds_ls = (time / 1000000000) & 0xFFFFFFFF;
		timestamp.seconds_ms = (uint16_t)((time / 1000000000) >> 32);
		return timestamp;
	}
	uint64_t scaleNativeClockToNanoseconds( uint64_t time ) {
		long double scaled_output = ((long double)netclock_hz.QuadPart)/1000000000;
		scaled_output = ((long double) time)/scaled_output;
		return (uint64_t) scaled_output;
	}
public:
	virtual bool HWTimestamper_init( InterfaceLabel *iface_label, OSNetworkInterface *net_iface, OSLockFactory *lock_factory, OSThreadFactory *thread_factory, OSTimerFactory *timer_factory );
	virtual bool HWTimestamper_gettime(Timestamp *system_time, Timestamp *device_time);
	bool findNetworkClockFrequency();

	virtual int HWTimestamper_txtimestamp( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp, bool last )
	{
		DWORD buf[4], buf_tmp[4];
		DWORD returned = 0;
		uint64_t tx_r,tx_s;
		DWORD result;
		while(( result = readOID( OID_INTEL_GET_TXSTAMP, buf_tmp, sizeof(buf_tmp), &returned )) == ERROR_SUCCESS ) {
			memcpy( buf, buf_tmp, sizeof( buf ));
		}
		if( result != ERROR_GEN_FAILURE ) {
			XPTPD_ERROR("Error is: %d", result );
			return -1;
		}
		if( returned != sizeof(buf_tmp) ) return -72;
		tx_r = (((uint64_t)buf[1]) << 32) | buf[0];
		tx_s = scaleNativeClockToNanoseconds( tx_r );
		tx_s += ONE_WAY_PHY_DELAY;
		timestamp = nanoseconds64ToTimestamp( tx_s );
		timestamp._version = version;

		return 0;
	}

	virtual int HWTimestamper_rxtimestamp( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp, bool last )
	{
		DWORD buf[4], buf_tmp[4];
		DWORD returned;
		uint64_t rx_r,rx_s;
		DWORD result;
		uint16_t packet_sequence_id;
		while(( result = readOID( OID_INTEL_GET_RXSTAMP, buf_tmp, sizeof(buf_tmp), &returned )) == ERROR_SUCCESS ) {
			memcpy( buf, buf_tmp, sizeof( buf ));
		}
		if( result != ERROR_GEN_FAILURE ) return -1;
		if( returned != sizeof(buf_tmp) ) return -72;
		packet_sequence_id = *((uint32_t *) buf+3) >> 16;
		if (PLAT_ntohs(packet_sequence_id) != sequenceId)
			return -72;
		rx_r = (((uint64_t)buf[1]) << 32) | buf[0];
		rx_s = scaleNativeClockToNanoseconds( rx_r );
		rx_s -= ONE_WAY_PHY_DELAY;
		timestamp = nanoseconds64ToTimestamp( rx_s );
		timestamp._version = version;

		return 0;
	}
};

#endif
