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

#ifndef MD_ETHPORT_HPP
#define MD_ETHPORT_HPP

#include <ieee1588.hpp>
#include <avbts_message.hpp>
#include <mdport.hpp>

#include <avbts_ostimer.hpp>
#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <avbts_osthread.hpp>
#include <avbts_oscondition.hpp>
#include <ethtimestamper.hpp>

#include <stdint.h>

#include <map>
#include <list>

#define TIMEOUT_BASE 1000 /* microseconds */
#define TIMEOUT_ITER 6

#define TX_TIMEOUT_BASE TIMEOUT_BASE
#define TX_TIMEOUT_ITER TIMEOUT_ITER

#define RX_TIMEOUT_BASE TIMEOUT_BASE
#define RX_TIMEOUT_ITER TIMEOUT_ITER

#define PDELAY_MULTICAST GPTP_MULTICAST

enum MulticastType {
	MCAST_NONE,
	MCAST_PDELAY,
	MCAST_OTHER
};

class MediaDependentEtherPort : public MediaDependentPort {
	static LinkLayerAddress other_multicast;
	static LinkLayerAddress pdelay_multicast;

	LinkLayerAddress local_addr;

	int _accelerated_sync_count;

	PTPMessagePathDelayReq *last_pdelay_req;
	PTPMessagePathDelayResp *last_pdelay_resp;
	PTPMessagePathDelayRespFollowUp *last_pdelay_resp_fwup;

	PTPMessageSync *last_sync;

	OSThread *listening_thread;

	OSLock *pdelay_rx_lock;
	OSLock *port_tx_lock;

	net_result sendGeneralMessage( uint8_t *buf, size_t len );

	net_result port_send
	( uint8_t * buf, size_t size, MulticastType mcast_type, bool timestamp );

	bool pdelay_started;

	OSLock *glock;
 public:
	bool serializeState( void *buf, off_t *count );
	bool restoreSerializedState( void *buf, off_t *count );
	
	OSTimerFactory *getTimerFactory() const {
		return timer_factory;
	}
	
	~MediaDependentEtherPort();
	MediaDependentEtherPort
	( EthernetTimestamper *timestamper, InterfaceLabel *net_label,
	  OSConditionFactory *condition_factory,
	  OSThreadFactory *thread_factory, OSTimerFactory *timer_factory,
	  OSLockFactory *lock_factory );
	bool init_port();

	bool openPort( OSCondition *port_ready_condition );
	bool _openPort( OSCondition *port_ready_condition );
	bool init_port( MediaIndependentPort *port );

	net_result sendEventPort
	(uint8_t * buf, size_t len, MulticastType mcast_type );
	net_result sendGeneralPort
	(uint8_t * buf, size_t len, MulticastType mcast_type );

	bool processEvent(Event e);
	bool processSync( uint16_t seq, bool grandmaster, long *elapsed_time );
	bool processPDelay( uint16_t seq, long *elapsed_time );

	virtual bool adjustClockRate( double freq_offset );
	virtual bool adjustClockPhase( int64_t phase_adjust );

	uint16_t getParentLastSyncSequenceNumber(void) const ;
	bool setParentLastSyncSequenceNumber(uint16_t num);

	void setLastSync(PTPMessageSync * msg) {
		last_sync = msg;
	}
	PTPMessageSync *getLastSync(void) const {
		return last_sync;
	}


	unsigned getTimestampDeviceId() {
		return _hw_timestamper != NULL ?
			_hw_timestamper->getDeviceId() : 0;
	}

	bool suspendTransmission() {
		return getTxLock();
	}
	bool resumeTransmission() {
		return putTxLock();
	}

	bool getPDelayRxLock() {
		return pdelay_rx_lock->lock() == oslock_ok ? true : false;
	}

	bool tryPDelayRxLock() {
		return pdelay_rx_lock->trylock() == oslock_ok ? true : false;
	}

	bool putPDelayRxLock() {
		return pdelay_rx_lock->unlock() == oslock_ok ? true : false;
	}

	bool getTxLock() {
		return port_tx_lock->lock() == oslock_ok ? true : false;
	}
	bool putTxLock() {
		return port_tx_lock->unlock() == oslock_ok ? true : false;
	}

	void setLastPDelayReq(PTPMessagePathDelayReq * msg) {
		last_pdelay_req = msg;
	}
	PTPMessagePathDelayReq *getLastPDelayReq(void) const {
		return last_pdelay_req;
	}

	void setLastPDelayResp(PTPMessagePathDelayResp * msg) {
		last_pdelay_resp = msg;
	}
	PTPMessagePathDelayResp *getLastPDelayResp(void) const {
		return last_pdelay_resp;
	}

	void setLastPDelayRespFollowUp(PTPMessagePathDelayRespFollowUp * msg) {
		last_pdelay_resp_fwup = msg;
	}
	PTPMessagePathDelayRespFollowUp *getLastPDelayRespFollowUp(void) const {
		return last_pdelay_resp_fwup;
	}

	int getRxTimestamp
	(PortIdentity * sourcePortIdentity, uint16_t sequenceId,
	 Timestamp & timestamp, bool last);
	int getTxTimestamp
	(PortIdentity * sourcePortIdentity, uint16_t sequenceId,
	 Timestamp & timestamp, bool last);

	int getTxTimestamp
	(PTPMessageCommon * msg, Timestamp & timestamp, bool last);
	int getRxTimestamp
	(PTPMessageCommon * msg, Timestamp & timestamp, bool last);

	bool getTxTimestampRetry
	(PTPMessageCommon *msg, Timestamp & timestamp, int count, long *timeout );
	bool getRxTimestampRetry
	(PTPMessageCommon *msg, Timestamp & timestamp, int count, long timeout );


	OSLockResult lock() {
		return glock->lock();
	}
	OSLockResult unlock() {
		return glock->unlock();
	}
	OSLockResult timestamper_lock() {
		if( _hw_timestamper ) return _hw_timestamper->lock();
		return oslock_ok;
	}
	OSLockResult timestamper_unlock() {
		if( _hw_timestamper ) return _hw_timestamper->unlock();
		return oslock_ok;
	}
};

#endif/*MD_ETHPORT_HPP*/
