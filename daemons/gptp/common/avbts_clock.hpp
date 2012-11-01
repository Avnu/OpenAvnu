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
#include <avbts_port.hpp>
#include <avbts_ostimerq.hpp>
#include <avbts_osipc.hpp>

#define EVENT_TIMER_GRANULARITY 5000000

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
	uint16_t number_ports;
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
	PortIdentity grandmaster_port_identity;
	ClockQuality grandmaster_clock_quality;
	unsigned char grandmaster_priority1;
	unsigned char grandmaster_priority2;
	bool grandmaster_is_boundary_clock;
	uint8_t time_source;

	int32_t master_local_offset_125us_offset;
	uint64_t master_local_offset_nrst125us_initial;
	bool master_local_offset_nrst125us_initialized;

  ClockIdentity LastEBestIdentity;

	IEEE1588Port *port_list[MAX_PORTS];

	static Timestamp start_time;
	Timestamp last_sync_time;

	OS_IPC *ipc;

	OSTimerQueue *timerq;

	bool forceOrdinarySlave;
 public:
	 IEEE1588Clock(bool forceOrdinarySlave,
		       OSTimerQueueFactory * timerq_factory, OS_IPC * ipc);
	~IEEE1588Clock(void);

	Timestamp getTime(void);
	Timestamp getPreciseTime(void);

	bool isBetterThan(PTPMessageAnnounce * msg);

  ClockIdentity getLastEBestIdentity( void ) {
    return LastEBestIdentity;
	}
  void setLastEBestIdentity( ClockIdentity id ) {
    LastEBestIdentity = id;
		return;
	}

	void setClockIdentity(char *id) {
		clock_identity.set((uint8_t *) id);
	}
	void setClockIdentity(LinkLayerAddress * addr) {
		clock_identity.set(addr);
	}

	unsigned char getDomain(void) {
		return domain_number;
	}

	PortIdentity getGrandmasterPortIdentity(void) {
		return grandmaster_port_identity;
	}
	void setGrandmasterPortIdentity(PortIdentity id) {
		grandmaster_port_identity = id;
	}

	ClockQuality getGrandmasterClockQuality(void) {
		return grandmaster_clock_quality;
	}

	ClockQuality getClockQuality(void) {
		return clock_quality;
	}

	unsigned char getGrandmasterPriority1(void) {
		return grandmaster_priority1;
	}

	unsigned char getGrandmasterPriority2(void) {
		return grandmaster_priority2;
	}

	uint16_t getMasterStepsRemoved(void) {
		return steps_removed;
	}

	uint16_t getCurrentUtcOffset(void) {
		return current_utc_offset;
	}

	uint8_t getTimeSource(void) {
		return time_source;
	}

	void getGrandmasterIdentity(char *id);

	unsigned char getPriority1(void) {
		return priority1;
	}

	unsigned char getPriority2(void) {
		return priority2;
	}
	uint16_t getNextPortId(void) {
		return (number_ports++ % (MAX_PORTS + 1)) + 1;
	}
	void registerPort(IEEE1588Port * port, uint16_t index) {
		if (index < MAX_PORTS) {
			port_list[index - 1] = port;
		}
		++number_ports;
	}
	void getPortList(int &count, IEEE1588Port ** &ports) {
		ports = this->port_list;
		count = number_ports;
		return;
	}

	static Timestamp getSystemTime(void);

	void addEventTimer(IEEE1588Port * target, Event e,
			   unsigned long long time_ns);
	void deleteEventTimer(IEEE1588Port * target, Event e);

#define SHM_SIZE  2*sizeof( int64_t )					\
    + 2*(sizeof( last_sync_time.seconds_ms ) + sizeof( last_sync_time.seconds_ls ) + sizeof( last_sync_time.nanoseconds )) \
    + 4*sizeof(uint32_t) + sizeof( uint16_t )

	void setMasterOffset(int64_t master_local_offset, Timestamp local_time,
			     int32_t master_local_freq_offset,
			     int64_t local_system_offset, Timestamp system_time,
			     int32_t local_system_freq_offset,
			     uint32_t nominal_clock_rate, uint32_t local_clock);

	ClockIdentity getClockIdentity() {
		return clock_identity;
	}

	friend void tick_handler(int sig);
};

void tick_handler(int sig);

#endif
