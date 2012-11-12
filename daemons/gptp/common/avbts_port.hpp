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

#ifndef AVBTS_PORT_HPP
#define AVBTS_PORT_HPP

#include <ieee1588.hpp>
#include <avbts_message.hpp>

#include <avbts_ostimer.hpp>
#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <avbts_osthread.hpp>
#include <avbts_oscondition.hpp>

#include <stdint.h>

#include <map>
#include <list>

#define GPTP_MULTICAST 0x0180C200000EULL
#define PDELAY_MULTICAST GPTP_MULTICAST
#define OTHER_MULTICAST GPTP_MULTICAST

#define PDELAY_RESP_RECEIPT_TIMEOUT_MULTIPLIER 3
#define SYNC_RECEIPT_TIMEOUT_MULTIPLIER 10
#define ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER 10

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
	 PortIdentity() {
	};
	PortIdentity(uint8_t * clock_id, uint16_t * portNumber) {
		this->portNumber = *portNumber;
		this->portNumber = PLAT_ntohs(this->portNumber);
		this->clock_id.set(clock_id);
	}
	bool operator!=(const PortIdentity & cmp) const {
		return !(this->clock_id == cmp.clock_id)
		    || this->portNumber != cmp.portNumber ? true : false;
	}
	bool operator==(const PortIdentity & cmp)const {
		return this->clock_id == cmp.clock_id
		    && this->portNumber == cmp.portNumber ? true : false;
	}
	bool operator<(const PortIdentity & cmp)const {
		return this->clock_id < cmp.clock_id ? true :
		    this->clock_id == cmp.clock_id
		    && this->portNumber < cmp.portNumber ? true : false;
	}
	bool operator>(const PortIdentity & cmp)const {
		return this->clock_id > cmp.clock_id ? true :
		    this->clock_id == cmp.clock_id
		    && this->portNumber > cmp.portNumber ? true : false;
	}
	void getClockIdentityString(char *id) {
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

typedef std::list < PTPMessageAnnounce * >AnnounceList_t;

class IEEE1588Port {
	static LinkLayerAddress other_multicast;
	static LinkLayerAddress pdelay_multicast;

	PortIdentity port_identity;
	// directly connected node
	PortIdentity peer_identity;

	OSNetworkInterface *net_iface;
	LinkLayerAddress local_addr;

	// Port Configuration
	unsigned char delay_mechanism;
	PortState port_state;
	char log_mean_unicast_sync_interval;
	char log_mean_sync_interval;
	char log_mean_announce_interval;
	char log_min_mean_delay_req_interval;
	char log_min_mean_pdelay_req_interval;
	bool burst_enabled;
	int64_t one_way_delay;	// Allow this to be negative result of a large clock skew
	// Implementation Specific data/methods
	IEEE1588Clock *clock;	// Pointer to clock object with which the port is associated

	bool _syntonize;

	bool asCapable;

	int32_t *rate_offset_array;
	uint32_t rate_offset_array_size;
	uint32_t rate_offset_count;
	uint32_t rate_offset_index;

	int32_t _peer_rate_offset;
	Timestamp _peer_offset_ts_theirs;
	Timestamp _peer_offset_ts_mine;
	bool _peer_offset_init;

	int32_t _master_rate_offset;

	int32_t _initial_clock_offset;
	int32_t _current_clock_offset;

	bool _master_local_freq_offset_init;
	int64_t _prev_master_local_offset;
	Timestamp _prev_sync_time;

	bool _local_system_freq_offset_init;
	int64_t _prev_local_system_offset;
	Timestamp _prev_system_time;

	AnnounceList_t qualified_announce;

	uint16_t announce_sequence_id;
	uint16_t sync_sequence_id;

	uint16_t pdelay_sequence_id;
	PTPMessagePathDelayReq *last_pdelay_req;
	PTPMessagePathDelayResp *last_pdelay_resp;
	PTPMessagePathDelayRespFollowUp *last_pdelay_resp_fwup;

	// Network socket description
	uint16_t ifindex;	// physical interface number that object represents

	IdentityMap_t identity_map;

	PTPMessageSync *last_sync;

	OSThread *listening_thread;

	OSCondition *port_ready_condition;

	OSLock *pdelay_rx_lock;
	OSLock *port_tx_lock;

	OSThreadFactory *thread_factory;
	OSTimerFactory *timer_factory;

	HWTimestamper *_hw_timestamper;

	net_result port_send(uint8_t * buf, int size, MulticastType mcast_type,
			     PortIdentity * destIdentity, bool timestamp);

	InterfaceLabel *net_label;

	OSLockFactory *lock_factory;
	OSConditionFactory *condition_factory;
 public:
	// Added for testing
	bool forceSlave;

	OSTimerFactory *getTimerFactory() {
		return timer_factory;
	}
	void setAsCapable(bool ascap) {
		if (ascap != asCapable) {
			fprintf(stderr, "AsCapable: %s\n",
				ascap == true ? "Enabled" : "Disabled");
		}
		asCapable = ascap;
	}

	~IEEE1588Port();
	IEEE1588Port(IEEE1588Clock * clock, uint16_t index, bool forceSlave,
		     HWTimestamper * timestamper, bool syntonize,
		     int32_t offset, InterfaceLabel * net_label,
		     OSConditionFactory * condition_factory,
		     OSThreadFactory * thread_factory,
		     OSTimerFactory * timer_factory,
		     OSLockFactory * lock_factory);
	bool init_port();

	void recoverPort(void);
	void *openPort(void);
	unsigned getPayloadOffset();
	void sendEventPort(uint8_t * buf, int len, MulticastType mcast_type,
			   PortIdentity * destIdentity);
	void sendGeneralPort(uint8_t * buf, int len, MulticastType mcast_type,
			     PortIdentity * destIdentity);
	void processEvent(Event e);

	PTPMessageAnnounce *calculateERBest(void);

	void addForeignMaster(PTPMessageAnnounce * msg);
	void removeForeignMaster(PTPMessageAnnounce * msg);
	void removeForeignMasterAll(void);

	void addQualifiedAnnounce(PTPMessageAnnounce * msg) {
		qualified_announce.push_front(msg);
	}

	void removeQualifiedAnnounce(PTPMessageAnnounce * msg) {
		qualified_announce.remove(msg);
	}

	char getSyncInterval(void) {
		return log_mean_sync_interval;
	}
	char getAnnounceInterval(void) {
		return log_mean_announce_interval;
	}
	char getPDelayInterval(void) {
		return log_min_mean_pdelay_req_interval;
	}
	PortState getPortState(void) {
		return port_state;
	}
	void getPortIdentity(PortIdentity & identity) {
		identity = this->port_identity;
	}
	bool burstEnabled(void) {
		return burst_enabled;
	}
	uint16_t getNextAnnounceSequenceId(void) {
		return announce_sequence_id++;
	}
	uint16_t getNextSyncSequenceId(void) {
		return sync_sequence_id++;
	}
	uint16_t getNextPDelaySequenceId(void) {
		return pdelay_sequence_id++;
	}

	uint16_t getParentLastSyncSequenceNumber(void);
	void setParentLastSyncSequenceNumber(uint16_t num);

	IEEE1588Clock *getClock(void);

	void setLastSync(PTPMessageSync * msg) {
		last_sync = msg;
	}
	PTPMessageSync *getLastSync(void) {
		return last_sync;
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

	int getTxLock() {
		return port_tx_lock->lock() == oslock_ok ? true : false;
	}
	int putTxLock() {
		return port_tx_lock->unlock() == oslock_ok ? true : false;
	}

	void setLastPDelayReq(PTPMessagePathDelayReq * msg) {
		last_pdelay_req = msg;
	}
	PTPMessagePathDelayReq *getLastPDelayReq(void) {
		return last_pdelay_req;
	}

	void setLastPDelayResp(PTPMessagePathDelayResp * msg) {
		last_pdelay_resp = msg;
	}
	PTPMessagePathDelayResp *getLastPDelayResp(void) {
		return last_pdelay_resp;
	}

	void setLastPDelayRespFollowUp(PTPMessagePathDelayRespFollowUp * msg) {
		last_pdelay_resp_fwup = msg;
	}
	PTPMessagePathDelayRespFollowUp *getLastPDelayRespFollowUp(void) {
		return last_pdelay_resp_fwup;
	}

	int calcMasterLocalClockRateDifference(signed long long offset,
					       Timestamp sync_time);
	int calcLocalSystemClockRateDifference(signed long long offset,
					       Timestamp sync_time);

	int32_t getMasterRateOffset(void) {
		return _master_rate_offset;
	}
	void setMasterRateOffset(int32_t offset
				 /* parts-per-trillion frequency offset */ ) {
		_master_rate_offset = offset;
	}
	int32_t getPeerRateOffset(void) {
		return _peer_rate_offset;
	}
	void setPeerRateOffset(int32_t offset
			       /* parts-per-trillion frequency offset */ ) {
		_peer_rate_offset = offset;
	}
	void setPeerOffset(Timestamp mine, Timestamp theirs) {
		_peer_offset_ts_mine = mine;
		_peer_offset_ts_theirs = theirs;
		_peer_offset_init = true;
	}
	bool getPeerOffset(Timestamp & mine, Timestamp & theirs) {
		mine = _peer_offset_ts_mine;
		theirs = _peer_offset_ts_theirs;
		return _peer_offset_init;
	}

	bool _adjustClockRate(int32_t freq_offset, unsigned counter_value,
			      Timestamp master_timestamp, int64_t offset,
			      bool change_master) {
		if (_hw_timestamper) {
			return
			    _hw_timestamper->HWTimestamper_adjclockrate
			    (freq_offset, counter_value, master_timestamp,
			     offset, change_master);
		}
		return false;
	}
	bool adjustClockRate(int32_t freq_offset, unsigned counter_value,
			     Timestamp master_timestamp, int64_t offset,
			     bool change_master) {
		return _adjustClockRate(freq_offset, counter_value,
					master_timestamp, offset,
					change_master);
	}

	bool doSyntonization(void) {
		return _syntonize;
	}

	void getExtendedError(char *msg) {
		if (_hw_timestamper) {
			_hw_timestamper->HWTimestamper_get_extderror(msg);
		} else {
			*msg = '\0';
		}
	}

	bool getExternalClockRate(Timestamp & local_time,
				  int64_t & external_local_offset,
				  int32_t & external_local_freq_offset) {
		bool s = false;
		if (_hw_timestamper) {
			s = _hw_timestamper->HWTimestamper_get_extclk_offset
			    (&local_time, &external_local_offset,
			     &external_local_freq_offset);
		}
		return s;
	}

	int getRxTimestamp(PortIdentity * sourcePortIdentity,
			   uint16_t sequenceId, Timestamp & timestamp,
			   unsigned &counter_value, bool last);
	int getTxTimestamp(PortIdentity * sourcePortIdentity,
			   uint16_t sequenceId, Timestamp & timestamp,
			   unsigned &counter_value, bool last);

	int getTxTimestamp(PTPMessageCommon * msg, Timestamp & timestamp,
			   unsigned &counter_value, bool last);
	int getRxTimestamp(PTPMessageCommon * msg, Timestamp & timestamp,
			   unsigned &counter_value, bool last);

	void getDeviceTime(Timestamp & system_time, Timestamp & device_time,
			   uint32_t & local_clock,
			   uint32_t & nominal_clock_rate);

	int64_t getLinkDelay(void) {
		return one_way_delay;
	}
	void setLinkDelay(int64_t delay) {
		one_way_delay = delay;
	}

	void recommendState(PortState state, bool changed_master);

	void mapSocketAddr(PortIdentity * destIdentity,
			   LinkLayerAddress * remote);
	void addSockAddrMap(PortIdentity * destIdentity,
			    LinkLayerAddress * remote);
};

#endif
