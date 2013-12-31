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

#ifndef IEEE1588_HPP
#define IEEE1588_HPP

#include <stdint.h>

#include <string.h>

#include <signal.h>

#include <stdio.h>

#include <platform.hpp>
#include <ptptypes.hpp>

#define MAX_PORTS 32

#define PTP_CLOCK_IDENTITY_LENGTH 8

class LinkLayerAddress;
struct ClockQuality;
class PortIdentity;
class IEEE1588Clock;
class PTPMessageCommon;
class PTPMessageSync;
class PTPMessageAnnounce;
class PTPMessagePathDelayReq;
class PTPMessagePathDelayResp;
class PTPMessagePathDelayRespFollowUp;
class IEEE1588Port;
class OSNetworkInterface;

typedef enum {
	NULL_EVENT = 0,
	POWERUP = 5,
	INITIALIZE,
	STATE_CHANGE_EVENT,
	SYNC_INTERVAL_TIMEOUT_EXPIRES,
	PDELAY_INTERVAL_TIMEOUT_EXPIRES,
	SYNC_RECEIPT_TIMEOUT_EXPIRES,
	QUALIFICATION_TIMEOUT_EXPIRES,
	ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
	ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES,
	FAULT_DETECTED,
	PDELAY_DEFERRED_PROCESSING,
	PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES,
} Event;

typedef struct {
	IEEE1588Port *port;
	Event event;
} event_descriptor_t;

class InterfaceLabel {
 public:
	virtual ~ InterfaceLabel() {
	};
};

class ClockIdentity {
 private:
	uint8_t id[PTP_CLOCK_IDENTITY_LENGTH];
 public:
	bool operator==(const ClockIdentity & cmp) const {
		return memcmp(this->id, cmp.id,
			      PTP_CLOCK_IDENTITY_LENGTH) == 0 ? true : false;
	}
    bool operator!=( const ClockIdentity &cmp ) const {
        return memcmp( this->id, cmp.id, PTP_CLOCK_IDENTITY_LENGTH ) != 0 ? true : false;
	}
	bool operator<(const ClockIdentity & cmp)const {
		return memcmp(this->id, cmp.id,
			      PTP_CLOCK_IDENTITY_LENGTH) < 0 ? true : false;
	}
	bool operator>(const ClockIdentity & cmp)const {
		return memcmp(this->id, cmp.id,
			      PTP_CLOCK_IDENTITY_LENGTH) < 0 ? true : false;
	}
	void getIdentityString(uint8_t *id) {
		memcpy(id, this->id, PTP_CLOCK_IDENTITY_LENGTH);
	} 
	void set(uint8_t * id) {
		memcpy(this->id, id, PTP_CLOCK_IDENTITY_LENGTH);
	}
	void set(LinkLayerAddress * address);
};

class Timestamp {
 public:
	Timestamp(uint32_t ns, uint32_t s_l, uint16_t s_m) {
		nanoseconds = ns;
		seconds_ls = s_l;
		seconds_ms = s_m;
	} 
	Timestamp() {
	};
	uint32_t nanoseconds;
	uint32_t seconds_ls;
	uint16_t seconds_ms;
        uint8_t _version;
};

#define INVALID_TIMESTAMP (Timestamp( 0xC0000000, 0, 0 ))
#define PDELAY_PENDING_TIMESTAMP (Timestamp( 0xC0000001, 0, 0 ))

#define TIMESTAMP_TO_NS(ts) (((static_cast<long long int>((ts).seconds_ms) \
			       << sizeof((ts).seconds_ls)*8) + \
			      (ts).seconds_ls)*1000000000LL + (ts).nanoseconds)

static inline uint64_t bswap_64(uint64_t in)
{
	uint8_t *s = (uint8_t *) & in;
	uint8_t *e = s + 7;
	while (e > s) {
		uint8_t t;
		t = *s;
		*s = *e;
		*e = t;
		++s;
		--e;
	}
	return in;
}

#define NS_PER_SECOND 1000000000
#define LS_SEC_MAX 0xFFFFFFFFull

static inline void TIMESTAMP_SUB_NS( Timestamp &ts, uint64_t ns ) {
       uint64_t secs = (uint64_t)ts.seconds_ls | ((uint64_t)ts.seconds_ms) << 32;
	   uint64_t nanos = (uint64_t)ts.nanoseconds;

       secs -= ns / NS_PER_SECOND;
	   ns = ns % NS_PER_SECOND;
	   
	   if(ns > nanos)
	   {  //borrow
          nanos += NS_PER_SECOND;
		  --secs;
	   }
	   
	   nanos -= ns;
	   
	   ts.seconds_ms = (uint16_t)(secs >> 32);
	   ts.seconds_ls = (uint32_t)(secs & LS_SEC_MAX);
	   ts.nanoseconds = (uint32_t)nanos;
}

static inline void TIMESTAMP_ADD_NS( Timestamp &ts, uint64_t ns ) {
       uint64_t secs = (uint64_t)ts.seconds_ls | ((uint64_t)ts.seconds_ms) << 32;
	   uint64_t nanos = (uint64_t)ts.nanoseconds;

       secs += ns / NS_PER_SECOND;
	   nanos += ns % NS_PER_SECOND;
	   
	   if(nanos > NS_PER_SECOND)
	   {  //carry
          nanos -= NS_PER_SECOND;
		  ++secs;
	   }
	   
	   ts.seconds_ms = (uint16_t)(secs >> 32);
	   ts.seconds_ls = (uint32_t)(secs & LS_SEC_MAX);
	   ts.nanoseconds = (uint32_t)nanos;
}

#define XPTPD_ERROR(fmt,...) fprintf( stderr, "ERROR at %u in %s: " fmt "\n", __LINE__, __FILE__ ,## __VA_ARGS__)
#ifdef PTP_DEBUG
#define XPTPD_INFO(fmt,...) fprintf( stderr, "DEBUG at %u in %s: " fmt "\n", __LINE__, __FILE__ ,## __VA_ARGS__)
#else
#define XPTPD_INFO(fmt,...)
#endif

#define HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE 4096

class HWTimestamper {
protected:
  uint8_t version;
public:
  virtual bool HWTimestamper_init
  ( InterfaceLabel *iface_label, OSNetworkInterface *iface )
  { return true; }
	virtual void HWTimestamper_final(void) {
	}

  virtual bool HWTimestamper_adjclockrate( float frequency_offset )
  { return false; }
  virtual bool HWTimestamper_adjclockphase( int64_t phase_adjust )
  { return false; }

	virtual bool HWTimestamper_gettime(Timestamp * system_time,
					   Timestamp * device_time,
					   uint32_t * local_clock,
					   uint32_t * nominal_clock_rate) = 0;

	virtual int HWTimestamper_txtimestamp(PortIdentity * identity,
					      uint16_t sequenceId,
					      Timestamp & timestamp,
					      unsigned &clock_value,
					      bool last) = 0;

	virtual int HWTimestamper_rxtimestamp(PortIdentity * identity,
					      uint16_t sequenceId,
					      Timestamp & timestamp,
					      unsigned &clock_value,
					      bool last) = 0;

	virtual bool HWTimestamper_get_extclk_offset(Timestamp * local_time,
						     int64_t * clk_offset,
						     int32_t *
						     ppt_freq_offset) {
		return false;
	}

	virtual void HWTimestamper_get_extderror(char *msg) {
		*msg = '\0';
	}

  virtual bool HWTimestamper_PPS_start() { return false; };
  virtual bool HWTimestamper_PPS_stop() { return true; };

  int getVersion() {
    return version;
  }
  HWTimestamper() { version = 0; }
  virtual ~ HWTimestamper() {
  }
};

PTPMessageCommon *buildPTPMessage(char *buf, int size,
				  LinkLayerAddress * remote,
				  IEEE1588Port * port);

#endif
