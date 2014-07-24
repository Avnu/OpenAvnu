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

#ifndef MIN_PORT_HPP
#define MIN_PORT_HPP

#include <ieee1588.hpp>

#include <avbts_ostimer.hpp>
#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <avbts_osthread.hpp>
#include <avbts_oscondition.hpp>

#include <stdint.h>

#include <map>
#include <list>

#define GPTP_MULTICAST 0x0180C200000EULL
#define OTHER_MULTICAST GPTP_MULTICAST

#define PDELAY_RESP_RECEIPT_TIMEOUT_MULTIPLIER 3
#define SYNC_RECEIPT_TIMEOUT_MULTIPLIER 3
#define ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER 3

class MediaDependentPort;
class PTPMessageAnnounce;

typedef enum {
	PTP_MASTER,
	PTP_PRE_MASTER,
	PTP_SLAVE,
	PTP_UNCALIBRATED,
	PTP_DISABLED,
	PTP_FAULTY,
	PTP_INITIALIZING,
	PTP_LISTENING
} PortState;

typedef enum {
	V1,
	V2_E2E,
	V2_P2P
} PortType;

class PortIdentity {
private:
	ClockIdentity clock_id;
	uint16_t portNumber;
public:
	PortIdentity() { };
	PortIdentity(uint8_t * clock_id, uint16_t * portNumber) {
		this->portNumber = *portNumber;
		this->portNumber = PLAT_ntohs(this->portNumber);
		this->clock_id.set(clock_id);
	}
	bool operator!=(const PortIdentity & cmp) const {
		return
			!(this->clock_id == cmp.clock_id) ||
			this->portNumber != cmp.portNumber ? true : false;
	}
	bool operator==(const PortIdentity & cmp)const {
		return
			this->clock_id == cmp.clock_id &&
			this->portNumber == cmp.portNumber ? true : false;
	}
	bool operator<(const PortIdentity & cmp)const {
		return
			this->clock_id < cmp.clock_id ?
			true : this->clock_id == cmp.clock_id &&
			this->portNumber < cmp.portNumber ? true : false;
	}
	bool operator>(const PortIdentity & cmp)const {
		return
			this->clock_id > cmp.clock_id ?
			true : this->clock_id == cmp.clock_id &&
			this->portNumber > cmp.portNumber ? true : false;
	}
	void getClockIdentityString(uint8_t *id) {
		clock_id.getIdentityString(id);
	}
	void setClockIdentity(ClockIdentity clock_id) {
		this->clock_id = clock_id;
	}
    ClockIdentity getClockIdentity( void ) {
        return this->clock_id;
    }
	void getPortNumberNO(uint16_t * id) {	// Network byte order
		uint16_t portNumberNO = PLAT_htons(portNumber);
		*id = portNumberNO;
	}
	void getPortNumber(uint16_t * id) {	// Host byte order
		*id = portNumber;
	}
	void setPortNumber(uint16_t * id) {
		portNumber = *id;
	}
};

typedef std::map < PortIdentity, LinkLayerAddress > IdentityMap_t;

class MediaIndependentPort {
	static LinkLayerAddress other_multicast;
	static LinkLayerAddress pdelay_multicast;
	static uint16_t port_index;

	bool _new_syntonization_set_point;
	bool _syntonize;
	float _ppm;

	PortIdentity port_identity;

	OSNetworkInterface *net_iface;
	LinkLayerAddress local_addr;

	/* Port Status */
	unsigned sync_count;  // 0 for master, ++ for each sync receive as slave
	// set to 0 when asCapable is false, increment for each pdelay recvd
	unsigned pdelay_count;

	/* Port Configuration */
	unsigned char delay_mechanism;
	PortState port_state;

	uint8_t log_mean_sync_interval;
	uint8_t log_mean_announce_interval;
    uint8_t log_mean_pdelay_req_interval;

	int _accelerated_sync_count;
	/* Signed value allows this to be negative result because of inaccurate
	   timestamp */
	/* Implementation Specific data/methods */
	IEEE1588Clock *clock;

	bool asCapable;

	FrequencyRatio _peer_rate_offset;
	Timestamp _peer_offset_ts_theirs;
	Timestamp _peer_offset_ts_mine;
	bool _peer_offset_init;

	bool _local_system_freq_offset_init;
	Timestamp _prev_local_time;
	Timestamp _prev_system_time;

	PTPMessageAnnounce *qualified_announce;

	uint16_t announce_sequence_id;
	uint16_t sync_sequence_id;
	uint16_t pdelay_sequence_id;

	PTPMessagePathDelayReq *last_pdelay_req;
	PTPMessagePathDelayResp *last_pdelay_resp;
	PTPMessagePathDelayRespFollowUp *last_pdelay_resp_fwup;

	IdentityMap_t identity_map;

	PTPMessageSync *last_sync;

	OSThread *listening_thread;

	OSCondition *port_ready_condition;

	OSThreadFactory *thread_factory;
	OSTimerFactory *timer_factory;

	OSLockFactory *lock_factory;
	OSConditionFactory *condition_factory;
	
	bool pdelay_started;

	OSLock *glock;

	MediaDependentPort *inferior;
public:
	~MediaIndependentPort();
	MediaIndependentPort
	(
	 IEEE1588Clock *clock, int accelerated_sync_count, bool syntonize,
	 OSConditionFactory *condition_factory,
	 OSThreadFactory *thread_factory, OSTimerFactory *timer_factory,
	 OSLockFactory *lock_factory );

	bool serializeState( void *buf, long *count );
	bool restoreSerializedState( void *buf, long *count );

	bool init_port();
	bool openPort();
	bool recoverPort();
	bool stop();

	void becomeMaster( bool annc );
	void becomeSlave( bool );
	
	bool startPDelay();
	bool startAnnounce();
	
	void syncDone() {
		if( !pdelay_started ) {
			startPDelay();
		}
	}

	void stopSyncReceiptTimeout();
	void startSyncReceiptTimeout();

	OSTimerFactory *getTimerFactory() const {
		return timer_factory;
	}
	void setAsCapable();
	void unsetAsCapable() {
		if (asCapable == true) {
			XPTPD_INFO("AsCapable: Disabled" );
		}
		asCapable = false;
		_peer_offset_init = false;
	}
	bool getAsCapable() {
		return asCapable;
	}

	bool processEvent(Event e);

	PTPMessageAnnounce *calculateERBest(void);

	char getSyncInterval(void) const {
		return log_mean_sync_interval;
	}
	char getAnnounceInterval(void) const {
		return log_mean_announce_interval;
	}
	char getPDelayInterval(void) const {
		return log_mean_pdelay_req_interval;
	}

	PortState getPortState(void) const {
		return port_state;
	}

	void setPortState( PortState state ) {
		port_state = state;
	}

	void getPortIdentity(PortIdentity & identity) const {
		identity = this->port_identity;
	}
	uint16_t getPortNumber() {
		uint16_t ret;
		port_identity.getPortNumber( &ret );
		return ret;
	}

	uint16_t getNextAnnounceSequenceId() {
		return announce_sequence_id++;
	}
	uint16_t getNextSyncSequenceId() {
		return sync_sequence_id++;
	}
	uint16_t getNextPDelaySequenceId() {
		return pdelay_sequence_id++;
	}

	uint16_t getParentLastSyncSequenceNumber(void) const ;
	bool setParentLastSyncSequenceNumber(uint16_t num);

	IEEE1588Clock *getClock(void) const {
		return clock;
	}

	FrequencyRatio getPeerRateOffset(void) const {
		return _peer_rate_offset;
	}

	void setPeerRateOffset( FrequencyRatio offset ) {
		_peer_rate_offset = offset;
	}
	void setPeerOffset(Timestamp mine, Timestamp theirs) {
		_peer_offset_ts_mine = mine;
		_peer_offset_ts_theirs = theirs;
		_peer_offset_init = true;
	}
	bool getPeerOffset(Timestamp & mine, Timestamp & theirs) const {
		if( _peer_offset_init ) {
			mine = _peer_offset_ts_mine;
			theirs = _peer_offset_ts_theirs;
		}
		return _peer_offset_init;
	}

	bool adjustPhaseError
	( int64_t master_local_offset, FrequencyRatio master_local_freq_offset ); 
	bool doSyntonize() {
		return _syntonize;
	}
	void newSyntonizationSetPoint() {
		_new_syntonization_set_point = true;
	}

	void recommendState(PortState state, bool changed_external_master);

	void addQualifiedAnnounce(PTPMessageAnnounce *msg);

	MediaDependentPort *getPort() {
		return inferior;
	}
	void setPort( MediaDependentPort *port ) {
		this->inferior = port;
	}

	OSLockResult lock() {
		return glock->lock();
	}
	OSLockResult unlock() {
		return glock->unlock();
	}

	void incPdelayCount() {
		++pdelay_count;
	}
	unsigned getPdelayCount() {
		return pdelay_count;
	}
	void incSyncCount() {
		++sync_count;
	}
	unsigned getSyncCount() {
		return sync_count;
	}
};

#endif/*MIN_PORT_HPP*/
