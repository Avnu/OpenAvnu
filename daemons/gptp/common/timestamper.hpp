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

#ifndef TIMESTAMPER_HPP
#define TIMESTAMPER_HPP

#include <avbts_oslock.hpp>
#include <ptptypes.hpp>
#include <vector>
#include <list>
#include <stdint.h>
#include <avbts_osnet.hpp>


class MediaDependentPort;
class InterfaceLabel;
class OSNetworkInterface;
class Timestamp;
class PortIdentity;
class OSThread;
class OSThreadFactory;
class OSTimerFactory;

typedef std::vector<MediaDependentPort *> MDPortList;
typedef MDPortList::const_iterator MDPortListIterator;

typedef std::list<OSNetworkInterface *> OSNetworkInterfaceList;


/* id identifies the timestamper 0 is reserved meaning no timestamper is 
   availabled */

class Timestamper {
private:
	MDPortList port_list;
	static unsigned index;
	unsigned id;

	FrequencyRatio local_system_ratio;
protected:
	bool initialized;
	OSLock *glock;

	OSTimerFactory *timer_factory;
	OSLockFactory *lock_factory;
	OSThreadFactory *thread_factory;
	uint8_t version;
	OSThread *thread;
	virtual bool _adjclockphase( int64_t phase_adjust ) { return false; }
	OSNetworkInterfaceList iface_list;
public:
	virtual bool HWTimestamper_init
	( InterfaceLabel *iface_label, OSNetworkInterface *iface,
	  OSLockFactory *lock_factory, OSThreadFactory *thread_factory,
	  OSTimerFactory *timer_factory );

	virtual void HWTimestamper_final(void) {
	}

	virtual bool HWTimestamper_adjclockrate( float frequency_offset )
	{ return false; }
	bool HWTimestamper_adjclockphase( int64_t phase_adjust ) { return false; }

	virtual bool HWTimestamper_gettime
	( Timestamp *system_time, Timestamp *device_time ) = 0;

	virtual bool HWTimestamper_PPS_start() { return false; };
	virtual bool HWTimestamper_PPS_stop() { return true; };

	bool suspendTransmission();
	bool resumeTransmission();

	int getVersion() {
		return version;
	}

	Timestamper() {
		version = 0;
		id = ++index;
		local_system_ratio = 1.0;
		initialized = false;
	}
	virtual ~Timestamper() {
	}

	virtual OSLockResult lock() { return glock->lock(); }
	virtual OSLockResult unlock() { return glock->unlock(); }

	MDPortListIterator getPortListBegin() const {
		return port_list.cbegin();
	}
	MDPortListIterator getPortListEnd() const {
		return port_list.cend();
	}

	bool registerPort( MediaDependentPort *port ) {
		if( lock() != oslock_ok ) return false;
		port_list.push_back( port );
		if( unlock() != oslock_ok ) return false;
		return true;
	}

	unsigned getDeviceId() { return id; }
	OSTimerFactory *getTimerFactory() { return timer_factory; }
	void setLocalSystemRatio( FrequencyRatio local_system_ratio ) {
		this->local_system_ratio = local_system_ratio;
	}
	FrequencyRatio getLocalSystemRatio() {
		return local_system_ratio;
	}
};


#endif/*TIMESTAMPER_HPP*/
