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
#include <HHMusicSyncApis.h>
#include <dummy.hpp>

void WirelessTimestamperCallback(EVENT_ID event, void *data, void *context) {
	WindowsWirelessTimestamper *timestamper = (WindowsWirelessTimestamper *)context;
	P_TmOnConfirm confirm;
	P_TmOnIndication indication;
	WirelessDialog tmp1, tmp2;
	LinkLayerAddress *peer_addr;

	switch (event) {
	default:
		// We're not interested
		break;
	case TIMINGMSMT_CONFIRM_EVENT:
		confirm = (P_TmOnConfirm)data;
		tmp1.action_devclk = (uint32_t)confirm->current_dialog.request;
		tmp1.ack_devclk = (uint32_t) confirm->current_dialog.ack;
		tmp1.dialog_token = (uint8_t) confirm->current_dialog.dialog_token;
		peer_addr = new LinkLayerAddress(confirm->peer_address);
		timestamper->timingMeasurementConfirmCB(*peer_addr, &tmp1);
		delete peer_addr;
		break;
	case TIMINGMSMT_INDICATION_EVENT:
		indication = (P_TmOnIndication)data;
		tmp1/*prev*/.action_devclk = (uint32_t) indication->prev_dialog.request;
		tmp1/*prev*/.ack_devclk = (uint32_t)indication->prev_dialog.ack;
		tmp1/*prev*/.dialog_token = (BYTE)indication->prev_dialog.dialog_token;
		tmp2/*curr*/.action_devclk = (uint32_t) indication->current_dialog.request;
		tmp2/*curr*/.ack_devclk = (uint32_t) indication->current_dialog.ack;
		tmp2/*curr*/.dialog_token = (BYTE)indication->current_dialog.dialog_token;
		printf("Got indication, %hhu(%u,%u)\n", tmp1.dialog_token, tmp2.action_devclk, tmp2.ack_devclk);
		peer_addr = new LinkLayerAddress(indication->peer_address);
		timestamper->timeMeasurementIndicationCB(*peer_addr, &tmp2, &tmp1, indication->ptp_context.data, indication->ptp_context.length);
		delete peer_addr;
		break;
	case CORRELATED_TIME_EVENT:
		P_CorrelatedTime ct = (P_CorrelatedTime)data;
		timestamper->system_time = NS_TO_TIMESTAMP(ct->system_time);
		timestamper->device_time = NS_TO_TIMESTAMP(_32to64(&timestamper->rollover, &timestamper->last_count, ct->device_time * 10));
		SetEvent(timestamper->CtsEvent);
		break;
	}
}

bool WindowsWirelessTimestamper::HWTimestamper_init
(InterfaceLabel *iface_label, OSNetworkInterface *iface,
OSLockFactory *lock_factory, OSThreadFactory *thread_factory,
OSTimerFactory *timer_factory) {
	bool ret = true;
	int i;
	P_AdapterList adapter_list;
	LinkLayerAddress *local_mac = dynamic_cast<LinkLayerAddress *>(iface_label);

	if (!initialized) {
		if (local_mac == NULL) return false;
		uint8_t mac_addr_local[ETHER_ADDR_OCTETS];
		local_mac->toOctetArray(mac_addr_local);
		GetAdapterList(adapter_list);
		for (i = 0; i < adapter_list->count; ++i) {
			if (memcmp(adapter_list->adapter[i].local_address, mac_addr_local, ETHER_ADDR_OCTETS) == 0) break;
		}
		if (i == adapter_list->count) {
			XPTPD_ERROR("Failed to find the requested adapter\n");
			ret = false;
			goto done;
		}
		hAdapter = adapter_list->adapter[i].id;

		CtsEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		// Register callback
		TimingMsmtCallback tempCallback;
		tempCallback.fn = WirelessTimestamperCallback;
		tempCallback.context = this;
		RegisterTimingMsmtCallback(&tempCallback);
	}

	ret = WirelessTimestamper::HWTimestamper_init(iface_label, iface, lock_factory, thread_factory, timer_factory);

done:
	return ret;
}

net_result WindowsWirelessTimestamper::_requestTimingMeasurement
(LinkLayerAddress dest, uint8_t dialog_token, WirelessDialog *prev_dialog,
uint8_t *follow_up, int followup_length) {
	net_result ret = net_succeed;
	P_PTPContext tmr_context;
	Dialog dialog;
	uint8_t dest_addr[ETHER_ADDR_OCTETS];
	uint8_t *msgbuf = new uint8_t[sizeof(PTPContext) + followup_length];
	tmr_context = (P_PTPContext)msgbuf;
	memset(tmr_context, 0, sizeof(*tmr_context));
	dialog.dialog_token = prev_dialog->dialog_token;
	if (dialog.dialog_token != 0) {
		dialog.request = prev_dialog->action_devclk;
		dialog.ack = prev_dialog->ack_devclk;
		tmr_context->length = followup_length;
		tmr_context->element_id = 0xDD;
		memcpy(tmr_context->data , follow_up, followup_length);
	}
	dest.toOctetArray(dest_addr);
	WifiTimingMeasurementRequest(hAdapter, dest_addr, dialog_token, &dialog, tmr_context);
	delete msgbuf;
	return ret;
}

bool WindowsWirelessTimestamper::HWTimestamper_gettime(Timestamp *system_time, Timestamp *device_time) {
	bool ret = true;
	DWORD wait_return;
	//ResetEvent(ptmWaEvent);
	lock();
	WifiCorrelatedTimestampRequest(hAdapter);
	wait_return = WaitForSingleObject(CtsEvent, 100);
	if (wait_return == WAIT_OBJECT_0) {
		*system_time = this->system_time;
		*device_time = this->device_time;
	}
	else {
		ret = false;
	}
	unlock();
	return ret;
}

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

DWORD WINAPI OSThreadCallback( LPVOID input ) {
	OSThreadArg *arg = (OSThreadArg*) input;
	arg->ret = arg->func( arg->arg );
	return 0;
}

VOID CALLBACK WindowsTimerQueueHandler( PVOID arg_in, BOOLEAN ignore ) {
	WindowsTimerQueueHandlerArg *arg = (WindowsTimerQueueHandlerArg *) arg_in;
	size_t diff = 0;

	// Remove myself from unexpired timer queue
	arg->queue->lock();
	if (!arg->queue->stopped) {
		diff = arg->timer_queue->arg_list.size();
		if (diff != 0) {
			arg->timer_queue->arg_list.remove(arg);
			diff -= arg->timer_queue->arg_list.size();
		}


		// If diff == 0 the event was already cancelled
		if( diff > 0 ) arg->func( arg->inner_arg );
	}
	arg->queue->unlock();

	// Add myself to the expired timer queue
	AcquireSRWLockExclusive( &arg->queue->retiredTimersLock );
	arg->queue->retiredTimers.push_front( arg );
	ReleaseSRWLockExclusive( &arg->queue->retiredTimersLock );
}


bool WindowsTimestamper::HWTimestamper_init(InterfaceLabel *iface_label, OSNetworkInterface *net_iface, OSLockFactory *lock_factory, OSThreadFactory *thread_factory, OSTimerFactory *timer_factory) {
	char network_card_id[64];

	EthernetTimestamper::HWTimestamper_init(iface_label, net_iface, lock_factory, thread_factory, timer_factory);

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

	if( strstr( pAdapterInfo->Description, NANOSECOND_CLOCK_PART_DESCRIPTION ) != NULL ) {
		netclock_hz.QuadPart = NETCLOCK_HZ_NANO;
	} else {
		netclock_hz.QuadPart = NETCLOCK_HZ_OTHER;
	}
	fprintf( stderr, "Adapter UID: %s\n", pAdapterInfo->AdapterName );
	PLAT_strncpy( network_card_id, NETWORK_CARD_ID_PREFIX, 63 );
	PLAT_strncpy( network_card_id+strlen(network_card_id), pAdapterInfo->AdapterName, 63-strlen(network_card_id) );

	miniport = CreateFile( network_card_id,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, 0, NULL );
	if( miniport == INVALID_HANDLE_VALUE ) return false;
	printf("Got OK miniport value\n");

	tsc_hz.QuadPart = getTSCFrequency( 1000 );
	if( tsc_hz.QuadPart == 0 ) {
		return false;
	}

	return true;
}

bool WindowsNamedPipeIPC::init( OS_IPC_ARG *arg ) {
	IPCListener *ipclistener;
	IPCSharedData ipcdata = { &peerlist, &loffset };

	ipclistener = new IPCListener();
	// Start IPC listen thread
	if( !ipclistener->start( ipcdata ) ) {
		XPTPD_ERROR( "Starting IPC listener thread failed" );
	} else {
		XPTPD_INFO( "Starting IPC listener thread succeeded" );
	}

	return true;
}

bool WindowsNamedPipeIPC::update(clock_offset_t *update) {


	loffset.get();
	loffset.local_time = update->master_time + update->ml_phoffset;
	loffset.ml_freqoffset = update->ml_freqoffset;
	loffset.ml_phoffset = update->ml_phoffset;
	loffset.ls_freqoffset = update->ls_freqoffset;
	loffset.ls_phoffset = update->ls_phoffset;
	
	if( !loffset.isReady() ) loffset.setReady( true );
	loffset.put();
	return true;
}
