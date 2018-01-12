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
#include <gptp_log.hpp>

DWORD WINAPI OSThreadCallback( LPVOID input ) {
	OSThreadArg *arg = (OSThreadArg*) input;
	arg->ret = arg->func( arg->arg );
	return 0;
}

VOID CALLBACK WindowsTimerQueueHandler( PVOID arg_in, BOOLEAN ignore ) {
	WindowsTimerQueueHandlerArg *arg = (WindowsTimerQueueHandlerArg *) arg_in;
	size_t diff;

	// Remove myself from unexpired timer queue
	AcquireSRWLockExclusive( &arg->timer_queue->lock );
	diff  = arg->timer_queue->arg_list.size();
	arg->timer_queue->arg_list.remove( arg );
	diff -= arg->timer_queue->arg_list.size();
	ReleaseSRWLockExclusive( &arg->timer_queue->lock );

	// If the remove was unsuccessful bail because we were
	// stepped on by 'cancelEvent'
	if( diff == 0 ) return;

	arg->func( arg->inner_arg );

	// Add myself to the expired timer queue
	AcquireSRWLockExclusive( &arg->queue->retiredTimersLock );
	arg->queue->retiredTimers.push_front( arg );
	ReleaseSRWLockExclusive( &arg->queue->retiredTimersLock );
}

inline uint64_t scale64(uint64_t i, uint32_t m, uint32_t n)
{
	uint64_t tmp, res, rem;

	rem = i % n;
	i /= n;

	res = i * m;
	tmp = rem * m;

	tmp /= n;

	return res + tmp;
}

void WirelessTimestamperCallback( LPVOID arg )
{
	WirelessTimestamperCallbackArg *larg =
		(WirelessTimestamperCallbackArg *)arg;
	WindowsWirelessTimestamper *timestamper =
		dynamic_cast<WindowsWirelessTimestamper *> (larg->timestamper);
	WirelessDialog tmp1, tmp2;
	LinkLayerAddress *peer_addr = NULL;

	if (timestamper == NULL)
	{
		GPTP_LOG_ERROR( "Wrong timestamper type: %p",
				larg->timestamper );
		return;
	}

	switch( larg->iEvent_type )
	{
	default:
	case TIMINGMSMT_CONFIRM_EVENT:
		tmp1.action_devclk = larg->event_data.confirm.T1;
		tmp1.ack_devclk = larg->event_data.confirm.T4;
		tmp1.dialog_token = (BYTE)larg->event_data.confirm.DialogToken;
		GPTP_LOG_VERBOSE
			( "Got confirm, %hhu(%llu,%llu)", tmp1.dialog_token,
			  tmp1.action_devclk, tmp1.ack_devclk );
		peer_addr = new LinkLayerAddress
			( larg->event_data.confirm.PeerMACAddress );
		larg->timestamper->
			timingMeasurementConfirmCB( *peer_addr, &tmp1 );

		break;

	case TIMINGMSMT_EVENT:
		tmp1/*prev*/.action_devclk = larg->event_data.indication.
			indication.T1;
		tmp1/*prev*/.ack_devclk = larg->event_data.indication.
			indication.T4;
		tmp1/*prev*/.dialog_token = (BYTE)larg->event_data.indication.
			indication.FollowUpDialogToken;
		tmp2/*curr*/.action_devclk = larg->event_data.indication.
			indication.T2;
		tmp2/*curr*/.ack_devclk = larg->event_data.indication.
			indication.T3;
		tmp2/*curr*/.dialog_token = (BYTE)larg->event_data.indication.
			indication.DialogToken;
		GPTP_LOG_VERBOSE
			("Got indication, %hhu(%llu,%llu) %hhu(%llu,%llu)",
			 tmp1.dialog_token, tmp1.action_devclk,
			 tmp1.ack_devclk, tmp2.dialog_token,
			 tmp2.action_devclk, tmp2.ack_devclk);
		peer_addr = new LinkLayerAddress(larg->event_data.indication.
						 indication.PeerMACAddress);

		larg->timestamper->timeMeasurementIndicationCB
			( *peer_addr, &tmp2, &tmp1,
			  larg->event_data.indication.
			  indication.PtpSpec.fwup_data,
			  larg->event_data.indication.
			  indication.WiFiVSpecHdr.Length - (sizeof(PTP_SPEC) -
							    1));

		break;

	case TIMINGMSMT_CORRELATEDTIME_EVENT:
		timestamper->system_counter =
			scale64( larg->event_data.ptm_wa.TSC, NS_PER_SECOND,
				 (uint32_t)timestamper->tsc_hz.QuadPart );
		timestamper->system_time.set64(timestamper->system_counter);
		// Scale from TM timescale to nanoseconds
		larg->event_data.ptm_wa.LocalClk *= 10;
		timestamper->device_time.set64
			(larg->event_data.ptm_wa.LocalClk*10);

		break;
	}

	delete peer_addr;
}

net_result WindowsWirelessTimestamper::_requestTimingMeasurement
(TIMINGMSMT_REQUEST *timingmsmt_req)
{
	net_result ret = net_succeed;

	if (!adapter->initiateTimingRequest(timingmsmt_req)) {
		GPTP_LOG_ERROR("Failed to send timing measurement request\n");
		ret = net_fatal;
	}

	return ret;
}

bool WindowsWirelessTimestamper::HWTimestamper_gettime
(	Timestamp *system_time,
	Timestamp * device_time,
	uint32_t * local_clock,
	uint32_t * nominal_clock_rate ) const
{
	bool refreshed = adapter->refreshCrossTimestamp();
	if (refreshed)
	{
		// We have a fresh cross-timestamp just use it
		*system_time = this->system_time;
		*device_time = this->device_time;
	} else
	{
		// We weren't able to get a fresh timestamp,
		// extrapolate from the last
		LARGE_INTEGER tsc_now;
		QueryPerformanceCounter(&tsc_now);
		unsigned device_delta = (unsigned)
			(((long double) (tsc_now.QuadPart - system_counter)) /
			 (((long double)tsc_hz.QuadPart) / 1000000000));
		device_delta = (unsigned)(device_delta*getPort()->
					  getLocalSystemFreqOffset());
		system_time->set64((uint64_t)
				   (((long double)tsc_now.QuadPart) /
				    ((long double)tsc_hz.QuadPart /
				     1000000000)));
		device_time->set64(device_delta);
		*device_time = *device_time + this->device_time;
	}

	return true;
}

bool WindowsWirelessTimestamper::HWTimestamper_init
(InterfaceLabel *iface_label, OSNetworkInterface *iface)
{
	uint8_t mac_addr_local[ETHER_ADDR_OCTETS];

	if (!initialized) {
		if (!adapter->initialize()) return false;
		if (getPort()->getLocalAddr() == NULL)
			return false;

		getPort()->getLocalAddr()->toOctetArray(mac_addr_local);
		if (!adapter->attachAdapter(mac_addr_local)) {
			return false;
		}

		tsc_hz.QuadPart = getTSCFrequency(false);
		if (tsc_hz.QuadPart == 0) {
			return false;
		}

		if (!adapter->registerTimestamper(this))
			return false;
	}

	initialized = true;
	return true;
}

WindowsWirelessTimestamper::~WindowsWirelessTimestamper() {
	if (adapter->deregisterTimestamper(this))
		adapter->shutdown();
	else
		GPTP_LOG_INFO("Failed to shutdown time sync on adapter");
}

bool WindowsEtherTimestamper::HWTimestamper_init( InterfaceLabel *iface_label, OSNetworkInterface *net_iface ) {
	char network_card_id[64];
	LinkLayerAddress *addr = dynamic_cast<LinkLayerAddress *>(iface_label);
	if( addr == NULL ) return false;
	PIP_ADAPTER_INFO pAdapterInfo;
	IP_ADAPTER_INFO AdapterInfo[32];       // Allocate information for up to 32 NICs
	DWORD dwBufLen = sizeof(AdapterInfo);  // Save memory size of buffer

	DWORD dwStatus = GetAdaptersInfo( AdapterInfo, &dwBufLen );
	if( dwStatus != ERROR_SUCCESS ) return false;

	for( pAdapterInfo = AdapterInfo; pAdapterInfo != NULL; pAdapterInfo = pAdapterInfo->Next ) {
		if( pAdapterInfo->AddressLength == ETHER_ADDR_OCTETS && *addr == LinkLayerAddress( pAdapterInfo->Address )) {
			break;
		}
	}

	if( pAdapterInfo == NULL ) return false;

	DeviceClockRateMapping *rate_map = DeviceClockRateMap;
	while (rate_map->device_desc != NULL)
	{
		if (strstr(pAdapterInfo->Description, rate_map->device_desc) != NULL)
			break;
		++rate_map;
	}
	if (rate_map->device_desc != NULL) {
		netclock_hz.QuadPart = rate_map->clock_rate;
	}
	else {
		GPTP_LOG_ERROR("Unable to determine clock rate for interface %s", pAdapterInfo->Description);
		return false;
	}

	GPTP_LOG_INFO( "Adapter UID: %s\n", pAdapterInfo->AdapterName );
	PLAT_strncpy( network_card_id, NETWORK_CARD_ID_PREFIX, 63 );
	PLAT_strncpy( network_card_id+strlen(network_card_id), pAdapterInfo->AdapterName, 63-strlen(network_card_id) );

	miniport = CreateFile( network_card_id,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, 0, NULL );
	if( miniport == INVALID_HANDLE_VALUE ) return false;

	tsc_hz.QuadPart = getTSCFrequency( true );
	if( tsc_hz.QuadPart == 0 ) {
		return false;
	}

	return true;
}

bool WindowsNamedPipeIPC::init(OS_IPC_ARG *arg) {
	IPCListener *ipclistener;
	IPCSharedData ipcdata = { &peerList_, &lOffset_ };

	ipclistener = new IPCListener();
	// Start IPC listen thread
	if (!ipclistener->start(ipcdata)) {
		GPTP_LOG_ERROR("Starting IPC listener thread failed");
	}
	else {
		GPTP_LOG_INFO("Starting IPC listener thread succeeded");
	}

	return true;
}

bool WindowsNamedPipeIPC::update(
	int64_t ml_phoffset,
	int64_t ls_phoffset,
	FrequencyRatio ml_freqoffset,
	FrequencyRatio ls_freq_offset,
	uint64_t local_time,
	uint32_t sync_count,
	uint32_t pdelay_count,
	PortState port_state,
	bool asCapable )
{
	lOffset_.get();
	lOffset_.local_time = local_time;
	lOffset_.ml_freqoffset = ml_freqoffset;
	lOffset_.ml_phoffset = ml_phoffset;
	lOffset_.ls_freqoffset = ls_freq_offset;
	lOffset_.ls_phoffset = ls_phoffset;

	if (!lOffset_.isReady()) lOffset_.setReady(true);
	lOffset_.put();
	return true;
}

bool WindowsNamedPipeIPC::update_grandmaster(
	uint8_t gptp_grandmaster_id[],
	uint8_t gptp_domain_number )
{
	lOffset_.get();
	memcpy(lOffset_.gptp_grandmaster_id, gptp_grandmaster_id, PTP_CLOCK_IDENTITY_LENGTH);
	lOffset_.gptp_domain_number = gptp_domain_number;

	if (!lOffset_.isReady()) lOffset_.setReady(true);
	lOffset_.put();
	return true;
}

bool WindowsNamedPipeIPC::update_network_interface(
	uint8_t  clock_identity[],
	uint8_t  priority1,
	uint8_t  clock_class,
	int16_t  offset_scaled_log_variance,
	uint8_t  clock_accuracy,
	uint8_t  priority2,
	uint8_t  domain_number,
	int8_t   log_sync_interval,
	int8_t   log_announce_interval,
	int8_t   log_pdelay_interval,
	uint16_t port_number )
{
	lOffset_.get();
	memcpy(lOffset_.clock_identity, clock_identity, PTP_CLOCK_IDENTITY_LENGTH);
	lOffset_.priority1 = priority1;
	lOffset_.clock_class = clock_class;
	lOffset_.offset_scaled_log_variance = offset_scaled_log_variance;
	lOffset_.clock_accuracy = clock_accuracy;
	lOffset_.priority2 = priority2;
	lOffset_.domain_number = domain_number;
	lOffset_.log_sync_interval = log_sync_interval;
	lOffset_.log_announce_interval = log_announce_interval;
	lOffset_.log_pdelay_interval = log_pdelay_interval;
	lOffset_.port_number = port_number;

	if (!lOffset_.isReady()) lOffset_.setReady(true);
	lOffset_.put();
	return true;
}


void WindowsPCAPNetworkInterface::watchNetLink( CommonPort *pPort)
{
	/* ToDo add link up/down detection, Google MIB_IPADDR_DISCONNECTED */
}
