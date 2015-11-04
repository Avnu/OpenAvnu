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

#ifndef MD_WIRELESS_HPP
#define MD_WIRELESS_HPP

#include <ieee1588.hpp>
#include <mdport.hpp>

#include <avbts_ostimer.hpp>
#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <avbts_osthread.hpp>
#include <avbts_oscondition.hpp>
#include <wltimestamper.hpp>

#include <stdint.h>

#include <map>
#include <list>

class WirelessDialog;
class WirelessTimestamper;

class MediaDependentWirelessPort : public MediaDependentPort {
	// Send all signal and announce messages
	static LinkLayerAddress multicast_dest;

	LinkLayerAddress local_addr;

	int64_t one_way_delay;

	OSThread *listening_thread;

	bool do_exit;
	OSCondition *port_stop_condition;

	LinkLayerAddress tm_peer_addr;

	WirelessDialog *prev_dialog;
 public:
	bool serializeState( void *buf, off_t *count );
	bool restoreSerializedState( void *buf, off_t *count );
	
	net_result sendGeneralMessage( uint8_t *buf, size_t len );

	OSTimerFactory *getTimerFactory() const {
		return timer_factory;
	}
	
	~MediaDependentWirelessPort();
	MediaDependentWirelessPort
	( WirelessTimestamper *timestamper, InterfaceLabel *net_label,
	  OSConditionFactory *condition_factory,
	  OSThreadFactory *thread_factory, OSTimerFactory *timer_factory,
	  OSLockFactory *lock_factory );
	bool init_port();

	bool openPort( OSCondition *port_ready_condition );
	bool _openPort( OSCondition *port_ready_condition );
	unsigned getPayloadOffset() const;
	bool init_port( MediaIndependentPort *port );
	bool stop() {
		do_exit = true;
		port_stop_condition->wait_prelock();
		port_stop_condition->wait();
		return true;
	}

	bool processSync( uint16_t seq, bool grandmaster, long *elapsed_time );

	void setPeerAddr(LinkLayerAddress *addr) {
		this->tm_peer_addr = *addr;
	}
	void setPrevDialog(WirelessDialog *dialog);
	WirelessDialog *getPrevDialog();
};

#endif/*MD_ETHPORT_HPP*/
