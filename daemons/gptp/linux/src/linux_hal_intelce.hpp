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

#ifndef LINUX_HAL_INTELCE_HPP
#define LINUX_HAL_INTELCE_HPP

#include <linux_hal_common.hpp>
#include <ismd_core.h>

class LinuxTimestamperIntelCE : public LinuxTimestamper {
private:
	ismd_clock_t iclock;
	uint64_t last_tx_time;
	uint64_t last_rx_time;
	int ce_timestamp_common
	( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
	  unsigned &clock_value, bool tx );
public:
	virtual bool HWTimestamper_init
	( InterfaceLabel *iface_label, OSNetworkInterface *iface );

	virtual int HWTimestamper_txtimestamp
	( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
	  unsigned &clock_value, bool last );

	virtual int HWTimestamper_rxtimestamp
	( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
	  unsigned &clock_value, bool last );

	bool post_init( int ifindex, int sd, TicketingLock *lock );

	virtual ~LinuxTimestamperIntelCE() {
	}

	LinuxTimestamperIntelCE() {
		last_tx_time = 0;
		last_rx_time = 0;
	}

	virtual bool HWTimestamper_gettime
	( Timestamp *system_time, Timestamp *device_time, uint32_t *local_clock,
	  uint32_t *nominal_clock_rate );
};

#endif/*LINUX_HAL_INTELCE_HPP*/
