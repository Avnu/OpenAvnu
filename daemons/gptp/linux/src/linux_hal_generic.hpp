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

#ifndef LINUX_HAL_GENERIC_HPP
#define LINUX_HAL_GENERIC_HPP

#include <linux_hal_common.hpp>

struct LinuxTimestamperGenericPrivate;
typedef struct LinuxTimestamperGenericPrivate * LinuxTimestamperGenericPrivate_t;

#ifdef WITH_IGBLIB
struct LinuxTimestamperIGBPrivate;
typedef struct LinuxTimestamperIGBPrivate * LinuxTimestamperIGBPrivate_t;
#endif

class LinuxTimestamperGeneric : public LinuxTimestamper {
private:
	int sd;
	Timestamp crstamp_system;
	Timestamp crstamp_device;
	LinuxTimestamperGenericPrivate_t _private;
	bool cross_stamp_good;
	std::list<Timestamp> rxTimestampList;
	LinuxNetworkInterfaceList iface_list;

	TicketingLock *net_lock;
	
#ifdef WITH_IGBLIB
	LinuxTimestamperIGBPrivate_t igb_private;
#endif
	
public:
	LinuxTimestamperGeneric();
	bool resetFrequencyAdjustment();
	bool Adjust( void *tmx );
	virtual bool HWTimestamper_init
	( InterfaceLabel *iface_label, OSNetworkInterface *iface );

	void updateCrossStamp( Timestamp *system_time, Timestamp *device_time );

	void pushRXTimestamp( Timestamp *tstamp ) {
		tstamp->_version = version;
		rxTimestampList.push_front(*tstamp);
	}
	bool post_init( int ifindex, int sd, TicketingLock *lock );

	virtual bool HWTimestamper_gettime
	( Timestamp *system_time, Timestamp *device_time, uint32_t *local_clock,
	  uint32_t *nominal_clock_rate );

	virtual int HWTimestamper_txtimestamp
	( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
	  unsigned &clock_value, bool last );

	virtual int HWTimestamper_rxtimestamp
	( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
	  unsigned &clock_value, bool last ) {
		/* This shouldn't happen. Ever. */
		if( rxTimestampList.empty() ) return -72;
		timestamp = rxTimestampList.back();
		rxTimestampList.pop_back();
		
		return 0;
	}

	virtual bool HWTimestamper_adjclockphase( int64_t phase_adjust );
	
	virtual bool HWTimestamper_adjclockrate( float freq_offset );

#ifdef WITH_IGBLIB
	bool HWTimestamper_PPS_start( );
	bool HWTimestamper_PPS_stop();
#endif

	virtual ~LinuxTimestamperGeneric();
};


#endif/*LINUX_HAL_GENERIC_HPP*/
