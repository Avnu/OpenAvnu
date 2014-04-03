/******************************************************************************

  Copyright (c) 2012, Intel Corporation 
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

#include <linux_hal_intelce.hpp>
#include <avbts_message.hpp>
extern "C" {
#include <ismd_sysclk.h>
#include <ismd_core.h>
}
#include <netpacket/packet.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>

#define NOMINAL_NET_CLOCK_RATE (25)/* MHz */
#define NET_CLOCK_ADJUST (1000/NOMINAL_NET_CLOCK_RATE)

#define TX_PHY_TIME 8000
#define RX_PHY_TIME 8000

uint64_t scale_clock( uint64_t count ) {
	return count*NET_CLOCK_ADJUST;
}

int LinuxTimestamperIntelCE::ce_timestamp_common
( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
  unsigned &clock_value, bool tx ) {
	uint64_t timestamp_s;
	uint16_t captured_sequence;
	uint16_t port_no;
	PortIdentity captured_id;
	ismd_sysclk_msg_t message;
	ismd_result_t result;
	int ret = -72;
	
	if( tx ) {
		result = ismd_sysclk_get_tx_time( &timestamp_s, &message );
		if( timestamp_s == last_tx_time ) {
			result = ISMD_ERROR_NO_DATA_AVAILABLE;
		} else {
			last_tx_time = timestamp_s;
		}
	} else {
		struct timespec ts;
		clock_gettime( CLOCK_REALTIME, &ts );
		result = ismd_sysclk_get_rx_time( &timestamp_s, &message );
		if( timestamp_s == last_rx_time ) {
			result = ISMD_ERROR_NO_DATA_AVAILABLE;
		} else {
			last_rx_time = timestamp_s;
		}
	}
		
	if( result == ISMD_ERROR_NO_DATA_AVAILABLE ) {
		goto fail;
	} else if( result != ISMD_SUCCESS ) {
		ret = -1;
		goto fail;
	}

	timestamp_s = scale_clock( timestamp_s );

	captured_id.setClockIdentity( ClockIdentity(message.msgid) );
	port_no =
		PLAT_ntohs(*((uint16_t *)(message.msgid+PTP_CLOCK_IDENTITY_LENGTH)));
	captured_id.setPortNumber( &port_no );
	captured_sequence = PLAT_ntohs( *((uint16_t *)message.msgseq ));

	if( captured_sequence != sequenceId || captured_id != *identity ) {
		uint16_t cap_port_no;
		uint16_t id_port_no;
		captured_id.getPortNumber(&cap_port_no);
		identity->getPortNumber(&id_port_no);
		goto fail;
	}

	ret = 0;

	if( tx ) {
		timestamp_s += TX_PHY_TIME;
	} else {
		timestamp_s -= RX_PHY_TIME;
	}

	timestamp.set64( timestamp_s );
	timestamp._version = version;
	clock_value = (unsigned) timestamp_s;

 fail:
	return ret;
}

int LinuxTimestamperIntelCE::HWTimestamper_txtimestamp
( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
  unsigned &clock_value, bool last ) {
	return ce_timestamp_common
		( identity, sequenceId, timestamp, clock_value, true );
}

int LinuxTimestamperIntelCE::HWTimestamper_rxtimestamp
( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
  unsigned &clock_value, bool last ) {
	return ce_timestamp_common
		( identity, sequenceId, timestamp, clock_value, false );
}

bool LinuxTimestamperIntelCE::post_init( int ifindex, int sd, TicketingLock *lock ) {
	ismd_sysclk_ptp_filter_config_t filter_config;
	filter_config.tx_filter_enable = true;
	filter_config.rx_filter_enable = true;
	filter_config.compare_ptp_ver = true;
	filter_config.ip_l2 = false;
	filter_config.ptp_version = GPTP_VERSION;
	filter_config.ether_type = PTP_ETHERTYPE;
	filter_config.da_hash_inclusion = false;
	ismd_result_t result;

	result = ismd_sysclk_set_ptp_filter( filter_config );
	if( result != ISMD_SUCCESS ) return false; 

	return true;
}
bool LinuxTimestamperIntelCE::HWTimestamper_init
( InterfaceLabel *iface_label, OSNetworkInterface *iface ) {
	ismd_result_t result;

	// Allocate clock
	result = ismd_clock_alloc_typed
		( ISMD_CLOCK_CLASS_SLAVE, ISMD_CLOCK_DOMAIN_TYPE_AV, &iclock );
	if( result != ISMD_SUCCESS ) return false; 

	return true;
}

bool LinuxTimestamperIntelCE::HWTimestamper_gettime
( Timestamp *system_time, Timestamp *device_time, uint32_t *local_clock,
  uint32_t *nominal_clock_rate ) {
	uint64_t system_time_s;
	uint64_t device_time_s;

	if( ismd_clock_trigger_software_event( iclock ) != ISMD_SUCCESS )
		return false;

	if( ismd_clock_get_last_trigger_correlated_time
		( iclock, &system_time_s, &device_time_s ) != ISMD_SUCCESS )
		return false;

	system_time->set64( system_time_s );

	device_time_s = scale_clock( device_time_s );
	device_time->set64( device_time_s );

	return true;
}
	


net_result LinuxNetworkInterface::nrecv
( LinkLayerAddress *addr, uint8_t *payload, size_t &length ) {
	fd_set readfds;
	int err;
	struct sockaddr_ll remote;        
	net_result ret = net_succeed;
	bool got_net_lock;

	struct timeval timeout = { 0, 16000 }; // 16 ms

	if( !net_lock.lock( &got_net_lock ) || !got_net_lock ) {
		XPTPD_ERROR( "A Failed to lock mutex" );
		return net_fatal;
	}

	FD_ZERO( &readfds );
	FD_SET( sd_event, &readfds );
  
	err = select( sd_event+1, &readfds, NULL, NULL, &timeout );		
	if( err == 0 ) {
		ret = net_trfail;
		goto done;
	} else if( err == -1 ) {
		if( err == EINTR ) {
			// Caught signal
			XPTPD_ERROR( "select() recv signal" );
			ret = net_trfail;
			goto done;
		} else {
			XPTPD_ERROR( "select() failed" );
			ret = net_fatal;
			goto done;
		}
	} else if( !FD_ISSET( sd_event, &readfds )) {
		ret = net_trfail;
		goto done;
	}
  
	err = recv( sd_event, payload, length, 0 );
	if( err < 0 ) {
		XPTPD_ERROR( "recvmsg() failed: %s", strerror(errno) );
		ret = net_fatal;
		goto done;
	}
	*addr = LinkLayerAddress( remote.sll_addr );
  
	length = err;

 done:
	if( !net_lock.unlock()) {
		XPTPD_ERROR( "A Failed to unlock, %d\n", err );
		return net_fatal;
	}

	return ret;		
}
