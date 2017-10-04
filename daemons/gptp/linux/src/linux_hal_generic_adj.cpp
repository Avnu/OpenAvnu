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

#include <linux/timex.h>
 // avoid indirect inclusion of time.h since this will clash with linux/timex.h
#define _TIME_H  1
#define _STRUCT_TIMEVAL 1
#include <linux_hal_generic.hpp>
#include <syscall.h>
#include <math.h>
#include <iostream>

void InitTimex(timex& tx)
{
	tx.modes = 0;
	tx.offset = 0;
	tx.freq = 0;
	tx.maxerror = 0;
	tx.esterror = 0;
	tx.status = 0;
	tx.constant = 0;
	tx.precision = 0;
	tx.tolerance = 0;
	tx.time.tv_sec = 0;
	tx.time.tv_usec = 0;
	tx.tick = 0;
}

bool LinuxTimestamperGeneric::resetFrequencyAdjustment() {
	struct timex tx;
	InitTimex(tx);
	tx.modes = ADJ_FREQUENCY;
	tx.freq = 0;

	GPTP_LOG_INFO("LinuxTimestamperGeneric::resetFrequencyAdjustment  Before adjust...");
	return Adjust(&tx);
}

bool LinuxTimestamperGeneric::HWTimestamper_adjclockphase( int64_t phase_adjust ) {
	struct timex tx;
	InitTimex(tx);
	LinuxNetworkInterfaceList::iterator iface_iter;
	bool ret = true;
	LinuxTimerFactory factory;
	OSTimer *timer = factory.createTimer();
		
	/* Walk list of interfaces disabling them all */
	iface_iter = iface_list.begin();
	for
		( iface_iter = iface_list.begin(); iface_iter != iface_list.end();
		  ++iface_iter ) {
		(*iface_iter)->disable_rx_queue();
	}
		
	rxTimestampList.clear();
	
#ifndef APTP		
	/* Wait 180 ms - This is plenty of time for any time sync frames
	   to clear the queue */
	timer->sleep(180000);
#endif
		
	++version;
		
	std::cout << "HWTimestamper_adjclockphase  phase_adjust:" << phase_adjust << std::endl;

	tx.modes = ADJ_SETOFFSET | ADJ_NANO;
	if( phase_adjust >= 0 ) {
		tx.time.tv_sec  = phase_adjust / 1000000000LL;
		tx.time.tv_usec = phase_adjust % 1000000000LL;
	} else {
		tx.time.tv_sec  = (phase_adjust / 1000000000LL)-1;
		tx.time.tv_usec = (phase_adjust % 1000000000LL)+1000000000;
	}

	GPTP_LOG_INFO("LinuxTimestamperGeneric::HWTimestamper_adjclockphase  Before adjust...  tx.time.tv_sec:%d  tx.time.tv_usec:%d", tx.time.tv_sec, tx.time.tv_usec);

	if(!Adjust(tx.time))
	{
		ret = false;
	}

	// Walk list of interfaces re-enabling them
	iface_iter = iface_list.begin();
	for( iface_iter = iface_list.begin(); iface_iter != iface_list.end();
		 ++iface_iter ) {
		auto item = *iface_iter;
	   if (item != nullptr)
	   {
	   	item->clear_reenable_rx_queue();
	   }
	}
	
	delete timer;
	return ret;
}
	
bool LinuxTimestamperGeneric::HWTimestamper_adjclockrate( float freq_offset ) const {
	struct timex tx;
	InitTimex(tx);
	//tx.modes = ADJ_FREQUENCY;
	// tx.freq  = long(freq_offset) << 16;
	// tx.freq += long(fmodf( freq_offset, 1.0 )*65536.0);

   const float kMaxPpb = 500.0;

   /* Clamp to max PPM */
   if (freq_offset > kMaxPpb)
   {
      freq_offset = kMaxPpb;
   }
   else if (freq_offset < -kMaxPpb)
   {
      freq_offset = -kMaxPpb;
   }

   tx.modes = MOD_FREQUENCY;

   double freq = freq_offset * ((1 << 16) / 1000.0);
   tx.freq = (int)round(freq);


	GPTP_LOG_INFO("LinuxTimestamperGeneric::HWTimestamper_adjclockrate  Before adjust...freq:%d", tx.freq);
	return Adjust(&tx);
}
