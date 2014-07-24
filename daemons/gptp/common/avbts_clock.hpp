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

#ifndef AVBTS_CLOCK_HPP
#define AVBTS_CLOCK_HPP

#include <stdint.h>
#include <ieee1588.hpp>
#include <min_port.hpp>
#include <avbts_ostimerq.hpp>
#include <offset.hpp>

#include <vector>

class OS_IPC;

#define EVENT_TIMER_GRANULARITY 16000000

#define INTEGRAL 0.0024
#define PROPORTIONAL 1.0
#define UPPER_FREQ_LIMIT  250.0
#define LOWER_FREQ_LIMIT -250.0

typedef std::vector<MediaIndependentPort *> MinPortList;
typedef MinPortList::const_iterator MinPortListIterator;

struct ClockQuality {
	unsigned char cq_class;
	unsigned char clockAccuracy;
	int16_t offsetScaledLogVariance;
};

class IEEE1588Clock {
private:
	ClockIdentity clock_identity;
	ClockQuality clock_quality;
	unsigned char priority1;
	unsigned char priority2;
	bool initializable;
	bool is_boundary_clock;
	bool two_step_clock;
	unsigned char domain_number;
	uint16_t number_foreign_records;
	bool slave_only;
	int16_t current_utc_offset;
	bool leap_59;
	bool leap_61;
	uint16_t epoch_number;
	uint16_t steps_removed;
	int64_t offset_from_master;
	Timestamp one_way_delay;
	PortIdentity parent_identity;
	ClockIdentity grandmaster_clock_identity;
	ClockQuality grandmaster_clock_quality;
	unsigned char grandmaster_priority1;
	unsigned char grandmaster_priority2;
	bool grandmaster_is_boundary_clock;
	uint8_t time_source;

	bool _grandmaster;
	
	ClockIdentity LastEBestIdentity;

	MinPortList port_list;
	
	Timestamp _last_sync_orig;
	Timestamp _last_sync_rcpt_dev;
	Timestamp _last_sync_rcpt_sys;
	uint64_t _last_sync_correction;
	FrequencyRatio _last_sync_cumoffset;
	unsigned _last_sync_devid;
	bool _last_sync_valid;
	
	OS_IPC *ipc;

	OSTimerQueue *timerq;
	
	FrequencyRatio _master_system_freq_offset;

	OSLock *glock;
public:
	IEEE1588Clock
	(bool forceOrdinarySlave, bool syntonize, uint8_t priority1,
	 Timestamper *timestamper, OSTimerQueueFactory * timerq_factory,
	 OSLockFactory *lock_factory, OS_IPC * ipc);
	~IEEE1588Clock(void);

	bool serializeState( void *buf, long *count );
	bool restoreSerializedState( void *buf, long *count );

	bool stop_port(MediaIndependentPort *port) {
		return timerq->stop_port(port);
	}

	Timestamp getTime( void ) const;
	Timestamp getPreciseTime( void ) const;
	static Timestamp getSystemTime(void);
	
	bool isBetterThan( PTPMessageAnnounce * msg ) const;
	
	ClockIdentity getLastEBestIdentity() const {
		return LastEBestIdentity;
	}
	void setLastEBestIdentity( ClockIdentity id ) {
		LastEBestIdentity = id;
	}
	
	void setLastSyncOriginTime( Timestamp ts ) {
		_last_sync_orig = ts;
	}
	void setLastSyncReceiveDeviceTime( Timestamp ts ) {
		_last_sync_rcpt_dev = ts;
	}
	void setLastSyncReceiveSystemTime( Timestamp ts ) {
		_last_sync_rcpt_sys = ts;
	}
	void setLastSyncCorrection( uint64_t correction ) {
		_last_sync_correction = correction;
	}
	void setLastSyncCumulativeOffset( FrequencyRatio ratio ) {
		_last_sync_cumoffset = ratio;
	}
	void setLastSyncReceiveDeviceId( unsigned id ) {
		_last_sync_devid = id;
	}
	void setLastSyncValid() {
		_last_sync_valid = true;
	}
	Timestamp getLastSyncOriginTime() {
		return _last_sync_orig;
	}
	Timestamp getLastSyncReceiveDeviceTime() {
		return _last_sync_rcpt_dev;
	}
	Timestamp getLastSyncReceiveSystemTime() {
		return _last_sync_rcpt_sys;
	}
	uint64_t getLastSyncCorrection() {
		return _last_sync_correction;
	}
	FrequencyRatio getLastSyncCumulativeOffset() {
		return _last_sync_cumoffset;
	}
    unsigned getLastSyncReceiveDeviceId() {
		return _last_sync_devid;
	}
	bool getLastSyncValid() {
		return _last_sync_valid;
	}


	void setClockIdentity(char *id) {
		clock_identity.set((uint8_t *) id);
	}
	void setClockIdentity(LinkLayerAddress * addr) {
		clock_identity.set(addr);
	}
	
	unsigned char getDomain(void) const {
		return domain_number;
	}

	ClockIdentity getGrandmasterClockIdentity(void) const {
		return grandmaster_clock_identity;
	}
	void setGrandmasterClockIdentity(ClockIdentity id) {
		grandmaster_clock_identity = id;
	}
	
	ClockQuality getGrandmasterClockQuality(void) const {
		return grandmaster_clock_quality;
	}

	void setGrandmasterClockQuality( ClockQuality clock_quality ) {
		grandmaster_clock_quality = clock_quality;
	}
	
	ClockQuality getClockQuality(void) const {
		return clock_quality;
	}
	
	unsigned char getGrandmasterPriority1(void) const {
		return grandmaster_priority1;
	}
	
	unsigned char getGrandmasterPriority2(void) const {
		return grandmaster_priority2;
	}
	void setGrandmasterPriority1( unsigned char priority1 ) {
		grandmaster_priority1 = priority1;
	}
	
	void setGrandmasterPriority2( unsigned char priority2 ) {
		grandmaster_priority2 = priority2;
	}
	
	uint16_t getMasterStepsRemoved(void) const {
		return steps_removed;
	}
	
	uint16_t getCurrentUtcOffset(void) const {
		return current_utc_offset;
	}
	
	uint8_t getTimeSource(void) const {
		return time_source;
	}
	
	unsigned char getPriority1(void) const {
		return priority1;
	}

	unsigned char getPriority2(void) const {
		return priority2;
	}
	bool registerPort( MediaIndependentPort *port ) {
		if( lock() != oslock_ok ) return false;
		port_list.push_back( port );
		if( unlock() != oslock_ok ) return false;
		return true;
	}

	MinPortListIterator getPortListBegin() const {
		return port_list.cbegin();
	}
	MinPortListIterator getPortListEnd() const {
		return port_list.cend();
	}
	
	bool addEventTimer
	( MediaIndependentPort *target, Event e, unsigned long long time_ns);
	bool deleteEventTimer( MediaIndependentPort *target, Event e );

	void setMasterOffset( clock_offset_t *offset );

	FrequencyRatio getMasterSystemFrequencyOffset() {
		return _master_system_freq_offset;
	}
	
	ClockIdentity getClockIdentity() const {
		return clock_identity;
	}

	bool getGrandmaster() {
		return _grandmaster;
	}
	void setGrandmaster() {
		_grandmaster = true;
	}
	void clearGrandmaster() {
		_grandmaster = false;
	}

	OSLockResult lock() {
		return glock->lock();
	}
	OSLockResult unlock() {
		return glock->unlock();
	}

	OSLockResult timerq_lock() {
		return timerq->lock();
	}
	OSLockResult timerq_unlock() {
		return timerq->unlock();
	}

	friend void tick_handler(int sig);
};

void tick_handler(int sig);

#endif
