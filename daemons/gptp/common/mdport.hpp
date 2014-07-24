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

#ifndef MDPORT_HPP
#define MDPORT_HPP

#include <min_port.hpp>
#include <avbts_oscondition.hpp>
#include <avbts_osnet.hpp>
#include <timestamper.hpp>
#include <avbts_clock.hpp>
#include <stdint.h>

class MediaDependentPort {
private:
	/* Signed value allows this to be negative result because of inaccurate
	timestamp */
	int64_t one_way_delay;
protected:
	MediaIndependentPort *port;
	OSNetworkInterface *net_iface;

	InterfaceLabel *net_label;
	OSTimerFactory *timer_factory;
	OSThreadFactory *thread_factory;
	OSConditionFactory *condition_factory;
	OSLockFactory *lock_factory;

	Timestamper *_hw_timestamper;

	MediaDependentPort
	( OSTimerFactory *timer_factory, OSThreadFactory *thread_factory,
	  InterfaceLabel *net_label, OSConditionFactory *condition_factory,
	  OSLockFactory *lock_factory )
	{
		this->timer_factory = timer_factory;
		this->thread_factory = thread_factory;
		
		this->net_label = net_label;
	
		this->condition_factory = condition_factory;
		this->lock_factory = lock_factory;
		one_way_delay = 3600000000000;
	}
public:
	virtual ~MediaDependentPort() = 0;

	virtual bool init_port( MediaIndependentPort *port ) = 0;
	virtual bool stop() {
		return false;
	}
	virtual bool openPort( OSCondition *port_ready_condition ) = 0;
	virtual bool recoverPort() { return false; }

	bool serializeState( void *buf, off_t *count ) { return true; }
	bool restoreSerializedState( void *buf, off_t *count ) { return true; }
	
	void getLinkLayerAddress( LinkLayerAddress *addr ) {
		net_iface->getLinkLayerAddress( addr );
	}

	// Send announce/signal message
	virtual net_result sendGeneralMessage( uint8_t *buf, size_t len ) = 0;	
	unsigned getPayloadOffset() const
	{
		return net_iface->getPayloadOffset();
	}
	
	// PDelay is optional
	virtual bool processSync( uint16_t seq, bool grandmaster, long *elapsed_time ) = 0;
	virtual bool processPDelay( uint16_t seq, long *elapsed_time ) { return true; }
	bool getDeviceTime
	(Timestamp & system_time, Timestamp & device_time ) const
	{
		if (_hw_timestamper) {
			return _hw_timestamper->HWTimestamper_gettime
				( &system_time, &device_time );
		} 
		
		device_time = system_time = getClock()->getSystemTime();
		return true;
	}

	virtual bool adjustClockRate( double freq_offset ) { return false; }
	virtual bool adjustClockPhase( int64_t phase_adjust ) { return false; }

	/* Suspend transmission on all ports associated with this port's
	   timestamp clock called from MediaIndependentPort */
	bool suspendTransmissionAll() {
		return _hw_timestamper != NULL ?
			_hw_timestamper->suspendTransmission() : false;
	}
	bool resumeTransmissionAll() {
		return _hw_timestamper != NULL ?
			_hw_timestamper->suspendTransmission() : false;
	}

	/* Callback (typically from timestamp module) to suspend tranmission on
	   this port */
	virtual bool suspendTransmission() { return false; }
	virtual bool resumeTransmission() { return false; }

	IEEE1588Clock *getClock() const {
		return port->getClock();
	}
	
	MediaIndependentPort *getPort() const {
		return port;
	}

	OSTimerFactory *getTimerFactory() {
		return timer_factory;
	}

	virtual FrequencyRatio getLocalSystemRateOffset() {
		return
			_hw_timestamper ? _hw_timestamper->getLocalSystemRatio() : 1.0;
	}

	virtual InterfaceLabel *getNetLabel() const {
		return net_label;
	}

	unsigned getTimestampDeviceId() {
		return _hw_timestamper != NULL ?
			_hw_timestamper->getDeviceId() : 0;
	}
	int getTimestampVersion() const {
		if( _hw_timestamper != NULL ) return _hw_timestamper->getVersion();
		return 0;
	}
	virtual bool processEvent(Event e) { return false; }

	virtual OSLockResult lock() {
		return oslock_ok;
	}
	virtual OSLockResult unlock() {
		return oslock_ok;
	}

	uint64_t getLinkDelay(void) const {
		return one_way_delay > 0LL ? one_way_delay : 0LL;
	}
	void setLinkDelay(int64_t delay) {
		one_way_delay = delay;
	}


};

inline MediaDependentPort::~MediaDependentPort() { }

inline bool MediaDependentPort::init_port( MediaIndependentPort *port ) {
	this->port = port;
	port->setPort( this );
	return true;
}

#endif/*MDPORT_HPP*/
