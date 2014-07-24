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

#include <timestamper.hpp>
#include <mdport.hpp>
#include <avbts_osthread.hpp>
#include <ethtimestamper.hpp>

#define DEFAULT_PHASEADJ_SLEEP (100) /*ms*/ 

bool EthernetTimestamper::HWTimestamper_init
( InterfaceLabel *iface_label, OSNetworkInterface *iface,
  OSLockFactory *lock_factory, OSThreadFactory *thread_factory,
  OSTimerFactory *timer_factory ) {
	Timestamper::HWTimestamper_init
		( iface_label, iface, lock_factory, thread_factory,
		  timer_factory );
	
	return true;
}

bool EthernetTimestamper::HWTimestamper_adjclockphase
( int64_t phase_adjust ) {
	OSNetworkInterfaceList::iterator iface_iter;
	OSTimer *timer;
	net_result err;
	bool ret = true;
		
	/* Walk list of interfaces disabling them all */
	iface_iter = iface_list.begin();
	for
		( iface_iter = iface_list.begin(); iface_iter != iface_list.end();
		  ++iface_iter ) {
		err = (*iface_iter)->disable_clear_rx_queue();
		ret = false;
		switch(err) {
		default:
			ret = true;
			break;
		case net_fatal:
			goto bail;
		case net_trfail:
			goto bail_reenable;
		}
	}
		
	// Wait to ensure that any inflight RX data is processed
	timer = getTimerFactory()->createTimer();
	timer->sleep( DEFAULT_PHASEADJ_SLEEP*1000 );
		
	if( !clear_rx_timestamp_list() ) {
		ret = false;
		goto bail_reenable;
	}
		
	if( !_adjclockphase( phase_adjust )) {
		ret = false;
		goto bail_reenable;
	}
	++version;

 bail_reenable:
	// Walk list of interfaces re-enabling them
	iface_iter = iface_list.begin();
	for( iface_iter = iface_list.begin(); iface_iter != iface_list.end();
		 ++iface_iter ) {
		err = (*iface_iter)->reenable_rx_queue();
		if( err != net_succeed ) {
			ret = false;
			goto bail;
		}
	}
 bail:
	
	return ret;
}
