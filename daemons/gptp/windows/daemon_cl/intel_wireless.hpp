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

#ifndef INTEL_WIRELESS_HPP
#define INTEL_WIRELESS_HPP

#include <windows_hal.hpp>
#include <Windows.h>
#include <stdint.h>

#define INTEL_MAC_ADDR_LENGTH				6
#define INTEL_MAX_ADAPTERS					20
#define CORRELATEDTIME_TRANSACTION_TIMEOUT	110/*ms*/

enum _WIRELESS_EVENT_TYPE;

typedef struct INTEL_EVENT
{
	BYTE			BtMacAddress[INTEL_MAC_ADDR_LENGTH + 1];
	///< Source of event (MAC address)
	_WIRELESS_EVENT_TYPE	eType; ///< Event type
	HRESULT			hErrCode; ///< Error code
	DWORD			dwSize;
	BYTE*			data;
} INTEL_EVENT, *PINTEL_EVENT;

typedef void(*INTEL_EVENT_CALLBACK) ( INTEL_EVENT iEvent, void *pContext );

typedef struct INTEL_CALLBACK
{
	INTEL_EVENT_CALLBACK	fnIntelCallback;
	void			*pContext;

} INTEL_CALLBACK, *PINTEL_CALLBACK;

/// Adapter handle
typedef DWORD HADAPTER;

//intel Header structure
typedef struct _INTEL_WIFI_HEADER
{
	DWORD	dwStructVersion;
	//Structure version for which this header is created
	DWORD	dwSize;
	//size of the structure for which this header is created
	DWORD	dwFlags;				//For future use
	DWORD	dwReserved;				//For future use
} INTEL_WIFI_HEADER, *PINTEL_WIFI_HEADER;

typedef enum INTEL_ADAPTER_TYPE
{
	eStone,		// Stone Peak - without BT Support
	eStoneBT,	// Stone Peak - with BT Support
	eSnowfield	// Snowfield Peak
} INTEL_ADAPTER_TYPE;

#define INTEL_ADAPTER_NAME_SIZE			64
#define INTEL_ADAPTER_DESCRIPTION_SIZE		64
#define INTEL_ADAPTER_CLSGUID_SIZE		64
#define INTEL_REGPATH_SIZE			512

#define INTEL_STRUCT_VER_LATEST			156

typedef enum INTEL_ADAPTER_PROTOCOL
{
	eUnknownProtocol,	///< Unknown adapter.
	e802_11,		///< WiFi
} INTEL_ADAPTER_PROTOCOL;

typedef struct INTEL_ADAPTER_LIST_ENTRY
{
	HADAPTER hAdapter;
	///< Adapter handle
	UCHAR btMACAddress[INTEL_MAC_ADDR_LENGTH];
	///< Adapter MAC address
	CHAR szAdapterName[INTEL_ADAPTER_NAME_SIZE];
	///< Adapter OS CLSGUID
	CHAR szAdapterDescription[INTEL_ADAPTER_DESCRIPTION_SIZE];
	///< Display name
	CHAR szAdapterClsguid[INTEL_ADAPTER_CLSGUID_SIZE];
	///< Muroc CLSGUID
	CHAR szRegistryPath[INTEL_REGPATH_SIZE];
	///< Adapter registry root
	CHAR szPnPId[INTEL_REGPATH_SIZE];		///< Plug-and-Play ID
	BOOL bEnabled;					///< enabled in driver
	INTEL_ADAPTER_TYPE eAdapterType;		///< Adapter type
	INTEL_ADAPTER_PROTOCOL eAdapterProtocol;	///< Adapter type
} INTEL_ADAPTER_LIST_ENTRY, *PINTEL_ADAPTER_LIST_ENTRY;

typedef struct INTEL_ADAPTER_LIST
{
	int count; ///< Number of entries
	INTEL_ADAPTER_LIST_ENTRY adapter[INTEL_MAX_ADAPTERS];
	///< Array of adapter entries
} INTEL_ADAPTER_LIST, *PINTEL_ADAPTER_LIST;


typedef HRESULT ( APIENTRY *GetAdapterList_t )
	( PINTEL_ADAPTER_LIST* ppAdapterList );
typedef HRESULT ( APIENTRY *WifiCmdTimingMeasurementRequest_t )
	( HADAPTER, PINTEL_WIFI_HEADER, void * );
typedef HRESULT ( APIENTRY *WifiCmdTimingPtmWa_t )
	(HADAPTER, PINTEL_WIFI_HEADER, void *);
typedef HRESULT ( APIENTRY *RegisterIntelCallback_t ) ( PINTEL_CALLBACK );
typedef HRESULT ( APIENTRY *DeregisterIntelCallback_t )
	( INTEL_EVENT_CALLBACK );

extern GetAdapterList_t GetAdapterList;
extern WifiCmdTimingMeasurementRequest_t WifiCmdTimingMeasurementRequest;
extern WifiCmdTimingPtmWa_t WifiCmdTimingPtmWa;
extern RegisterIntelCallback_t RegisterIntelCallback;
extern DeregisterIntelCallback_t DeregisterIntelCallback;

typedef struct _TIMINGMSMT_REQUEST *tm_request_t;
typedef struct WirelessTimestamperCallbackArg
*WirelessTimestamperCallbackArg_t;
typedef void( *WirelessTimestamperCallback_t )
( WirelessTimestamperCallbackArg_t arg );

class IntelWirelessAdapter : public WindowsWirelessAdapter
{
private:
	HADAPTER hAdapter;

	// Get correlated time operation may timeout
	// Use condition wait to manage this
	SRWLOCK ct_lock;
	CONDITION_VARIABLE ct_cond;
	bool ct_done;
	int ct_miss;

	uint64_t gp2_last;
	unsigned gp2_ext;

public:
	bool initialize( void );
	void shutdown( void );
	bool refreshCrossTimestamp();
	bool initiateTimingRequest( TIMINGMSMT_REQUEST *tm_request );
	bool attachAdapter( uint8_t *mac_addr );
	bool registerTimestamper( WindowsWirelessTimestamper *timestamper );
	bool deregisterTimestamper( WindowsWirelessTimestamper *timestamper );
	IntelWirelessAdapter();

	uint64_t calc_rollover( uint64_t gp2 );

	friend void IntelWirelessTimestamperEventHandler
	( INTEL_EVENT iEvent, void *pContext );
};

inline IntelWirelessAdapter::IntelWirelessAdapter()
{
	gp2_ext = 0;
	gp2_last = ULLONG_MAX;
}

#endif
