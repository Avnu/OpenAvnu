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

#include <string>

#include <stdint.h>

#include <string.h>

#include <stdio.h>

#include <platform.hpp>
#include <ptptypes.hpp>

#include <debugout.hpp>

#define MAX_PORTS 32

#define PTP_CLOCK_IDENTITY_LENGTH 8

class LinkLayerAddress;
struct ClockQuality;
class PortIdentity;
class PTPMessageCommon;
class PTPMessageSync;
class PTPMessageAnnounce;
class PTPMessagePathDelayReq;
class PTPMessagePathDelayResp;
class PTPMessagePathDelayRespFollowUp;
class IEEE1588Port;
class IEEE1588Clock;
class OSNetworkInterface;

/**
 * IEEE 1588 event enumeration type
 * Defined at: IEEE 1588-2008 Clause 9.2.6
 */
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

/**
 * Defines an event descriptor type
 */
typedef struct {
	IEEE1588Port *port;	//!< IEEE 1588 Port
	Event event;	//!< Event enumeration
} event_descriptor_t;

/**
 * Provides a generic InterfaceLabel class
 */
class InterfaceLabel {
 public:
	virtual ~ InterfaceLabel() {
	};
};

/**
 * Provides a ClockIdentity abstraction
 * See IEEE 802.1AS-2011 Clause 8.5.2.2
 */
class ClockIdentity {
 private:
	uint8_t id[PTP_CLOCK_IDENTITY_LENGTH];
 public:
	/**
	 * Default constructor. Sets ID to zero
	 */
	ClockIdentity() {
		memset( id, 0, PTP_CLOCK_IDENTITY_LENGTH );
	}

	/**
	 * @brief  Constructs the object and sets its ID
	 * @param  id [in] clock id as an octet array
	 */
	ClockIdentity( uint8_t *id ) {
		set(id);
	}

	/**
	 * @brief  Implements the operator '==' overloading method.
	 * @param  cmp Reference to the ClockIdentity comparing value
	 * @return TRUE if cmp equals to the object's clock identity. FALSE otherwise
	 */
	bool operator==(const ClockIdentity & cmp) const {
		return memcmp(this->id, cmp.id,
			      PTP_CLOCK_IDENTITY_LENGTH) == 0 ? true : false;
	}

	/**
	 * @brief  Implements the operator '!=' overloading method.
	 * @param  cmp Reference to the ClockIdentity comparing value
	 * @return TRUE if cmp differs from the object's clock identity. FALSE otherwise.
	 */
    bool operator!=( const ClockIdentity &cmp ) const {
        return memcmp( this->id, cmp.id, PTP_CLOCK_IDENTITY_LENGTH ) != 0 ? true : false;
	}
	
	/**
	 * @brief  Implements the operator '<' overloading method.
	 * @param  cmp Reference to the ClockIdentity comparing value
	 * @return TRUE if cmp value is lower than the object's clock identity value. FALSE otherwise.
	 */
	bool operator<(const ClockIdentity & cmp)const {
		return memcmp(this->id, cmp.id,
			      PTP_CLOCK_IDENTITY_LENGTH) < 0 ? true : false;
	}

	/**
	 * @brief  Implements the operator '>' overloading method.
	 * @param  cmp Reference to the ClockIdentity comparing value
	 * @return TRUE if cmp value is greater than the object's clock identity value. FALSE otherwise
	 */
	bool operator>(const ClockIdentity & cmp)const {
		return memcmp(this->id, cmp.id,
			      PTP_CLOCK_IDENTITY_LENGTH) > 0 ? true : false;
	}

	/**
	 * @brief  Gets the identity string from the ClockIdentity object
	 * @return String containing the clock identity
	 */
	std::string getIdentityString();

	/**
	 * @brief  Gets the identity string from the ClockIdentity object
	 * @param  id [out] Value copied from the object ClockIdentity
	 * @return void
	 */
	void getIdentityString(uint8_t *id) {
		memcpy(id, this->id, PTP_CLOCK_IDENTITY_LENGTH);
	}

	/**
	 * @brief  Set the clock id to the object
	 * @param  id [in] Value to be set
	 * @return void
	 */
	void set(uint8_t * id) {
		memcpy(this->id, id, PTP_CLOCK_IDENTITY_LENGTH);
	}
	
	/**
	 * @brief  Set clock id based on the link layer address. Clock id is 8 octets
	 * long whereas link layer address is 6 octets long. This method pads 0xFEFF after the 3
	 * first octets
	 * @param  address Link layer address
	 * @return void
	 */
	void set(LinkLayerAddress * address);
	
	/**
	 * @brief  This method is only enabled at compiling time. When enabled, it prints on the
	 * stderr output the clock identity information
	 * @param  str [in] String to be print out before the clock identity value
	 * @return void
	 */
	void print(const char *str) {
		XPTPD_INFO
			( "Clock Identity(%s): %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx\n",
			  str, id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7] );
	}
};

#define INVALID_TIMESTAMP_VERSION 0xFF
#define MAX_NANOSECONDS 1000000000
#define MAX_TIMESTAMP_STRLEN 28

/**
 * Provides a Timestamp interface
 */
class Timestamp {
private:
	char output_string[MAX_TIMESTAMP_STRLEN];
public:
	/**
	 * @brief  Creates a Timestamp instance
	 * @param  ns 32 bit nano-seconds value
	 * @param  s_l 32 bit seconds field LSB
	 * @param  s_m 32 bit seconds field MSB
	 * @param  ver 8 bit version field
	 */
	Timestamp
	(uint32_t ns, uint32_t s_l, uint16_t s_m,
	 uint8_t ver = INVALID_TIMESTAMP_VERSION) {
		nanoseconds = ns;
		seconds_ls = s_l;
		seconds_ms = s_m;
		_version = ver;
	}
	/*
	 * Default constructor. Initializes
	 * the private parameters
	 */
	Timestamp() {
		output_string[0] = '\0';
	}
	uint32_t nanoseconds;	//!< 32 bit nanoseconds value
	uint32_t seconds_ls;	//!< 32 bit seconds LSB value
	uint16_t seconds_ms;	//!< 32 bit seconds MSB value
	uint8_t _version;		//!< 8 bit version value

	/**
	 * @brief Copies the timestamp to the internal string in the following format:
	 * seconds_ms seconds_ls nanoseconds
	 * @return Formated string (as a char *)
	 */
	char *toString() {
		PLAT_snprintf
			( output_string, 28, "%hu %u %u", seconds_ms, seconds_ls
			  ,
			  nanoseconds );
		return output_string;
	}

	/**
	 * @brief Implements the operator '+' overloading method.
	 * @param o Constant reference to the timestamp to be added
	 * @return Object's timestamp + o.
	 */
	Timestamp operator+( const Timestamp& o ) {
		uint32_t nanoseconds;
		uint32_t seconds_ls;
		uint16_t seconds_ms;
		uint8_t version;
		bool carry;

		nanoseconds  = this->nanoseconds;
		nanoseconds += o.nanoseconds;
		carry =
			nanoseconds < this->nanoseconds ||
			nanoseconds >= MAX_NANOSECONDS ? true : false;
		nanoseconds -= carry ? MAX_NANOSECONDS : 0;

		seconds_ls  = this->seconds_ls;
		seconds_ls += o.seconds_ls;
		seconds_ls += carry ? 1 : 0;
		carry = seconds_ls < this->seconds_ls ? true : false;

		seconds_ms  = this->seconds_ms;
		seconds_ms += o.seconds_ms;
		seconds_ms += carry ? 1 : 0;
		carry = seconds_ms < this->seconds_ms ? true : false;

		version = this->_version == o._version ? this->_version :
			INVALID_TIMESTAMP_VERSION;
		return Timestamp( nanoseconds, seconds_ls, seconds_ms, version );
	}

	/**
	 * @brief  Implements the operator '-' overloading method.
	 * @param  o Constant reference to the timestamp to be subtracted
	 * @return Object's timestamp - o.
	 */
	Timestamp operator-( const Timestamp& o ) {
		uint32_t nanoseconds;
		uint32_t seconds_ls;
		uint16_t seconds_ms;
		uint8_t version;
		bool carry, borrow_this;
		unsigned borrow_total = 0;

		borrow_this = this->nanoseconds < o.nanoseconds;
		nanoseconds =
			((borrow_this ? MAX_NANOSECONDS : 0) + this->nanoseconds) -
			o.nanoseconds;
		carry = nanoseconds > MAX_NANOSECONDS;
		nanoseconds -= carry ? MAX_NANOSECONDS : 0;
		borrow_total += borrow_this ? 1 : 0;

		seconds_ls  = carry ? 1 : 0;
		seconds_ls += this->seconds_ls;
		borrow_this =
			borrow_total > seconds_ls ||
			seconds_ls - borrow_total < o.seconds_ls;
		seconds_ls  = 
			borrow_this ? seconds_ls - o.seconds_ls + (uint32_t)-1 :
			(seconds_ls - borrow_total) - o.seconds_ls;
		borrow_total = borrow_this ? borrow_total + 1 : 0;

		seconds_ms  = carry ? 1 : 0;
		seconds_ms += this->seconds_ms;
		borrow_this =
			borrow_total > seconds_ms ||
			seconds_ms - borrow_total < o.seconds_ms;
		seconds_ms  = 
			borrow_this ? seconds_ms - o.seconds_ms + (uint32_t)-1 :
			(seconds_ms - borrow_total) - o.seconds_ms;
		borrow_total = borrow_this ? borrow_total + 1 : 0;

		version = this->_version == o._version ? this->_version :
			INVALID_TIMESTAMP_VERSION;
		return Timestamp( nanoseconds, seconds_ls, seconds_ms, version );
	}

	/**
	 * @brief  Sets a 64bit value to the object's timestamp
	 * @param  value Value to be set
	 * @return void
	 */
	void set64( uint64_t value ) {
		nanoseconds = value % 1000000000;
		seconds_ls = (uint32_t) (value / 1000000000);
		seconds_ms = (uint16_t)((value / 1000000000) >> 32);
	}
};

#define INVALID_TIMESTAMP (Timestamp( 0xC0000000, 0, 0 ))
#define PDELAY_PENDING_TIMESTAMP (Timestamp( 0xC0000001, 0, 0 ))

#define TIMESTAMP_TO_NS(ts) (((static_cast<long long int>((ts).seconds_ms) \
			       << sizeof((ts).seconds_ls)*8) + \
			      (ts).seconds_ls)*1000000000LL + (ts).nanoseconds)

/**
 * @brief  Swaps out byte-a-byte a 64 bit value
 * @param  in Value to be swapped
 * @return Swapped value
 */
static inline uint64_t byte_swap64(uint64_t in)
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

/**
 * @brief  Subtracts a nanosecond value from the timestamp
 * @param  ts [inout] Timestamp value
 * @param  ns Nanoseconds value to subtract from ts
 * @return void
 */
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

/**
 * @brief  Adds a nanosecond value to the timestamp
 * @param  ts [inout] Timestamp value
 * @param  ns Nanoseconds value to add to ts
 * @return void
 */
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

#define HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE 4096

/**
 * Provides a generic interface for hardware timestamping
 */
class HWTimestamper {
protected:
	uint8_t version; //!< HWTimestamper version
public:
	/**
	 * @brief Initializes the hardware timestamp unit
	 * @param iface_label [in] Interface label
	 * @param iface [in] Network interface
	 * @return true
	 */
	virtual bool HWTimestamper_init
		( InterfaceLabel *iface_label, OSNetworkInterface *iface )
		{ return true; }

	/**
	 * @brief  Finalizes the hardware timestamp unit
	 * @return void
	 */
	virtual void HWTimestamper_final(void) {
	}

	/**
	 * @brief  Adjusts the clock frequency
	 * @param  frequency_offset Frequency offset
	 * @return false
	 */
	virtual bool HWTimestamper_adjclockrate( float frequency_offset )
	{ return false; }

	/**
	 * @brief  Adjusts the clock phase
	 * @param  phase_adjust Phase offset
	 * @return false
	 */
	virtual bool HWTimestamper_adjclockphase( int64_t phase_adjust )
	{ return false; }

	/**
	 * @brief  Get information about the local time
	 * @param  system_time [inout] System time
	 * @param  device_time [inout] Device time
	 * @param  local_clock [inout] Local clock
	 * @param  nominal_clock_rate [out] Nominal clock rate
	 * @return True or False
	 */
	virtual bool HWTimestamper_gettime(Timestamp * system_time,
			Timestamp * device_time,
			uint32_t * local_clock,
			uint32_t * nominal_clock_rate) = 0;

	/**
	 * @brief  Get tx timestamp
	 * @param  identity PTP port identity
	 * @param  sequenceId Sequence ID
	 * @param  timestamp [inout] Timestamp value
	 * @param  clock_value [inout] Clock value
	 * @param  last Boolean flag
	 * @return A signed integer
	 */
	virtual int HWTimestamper_txtimestamp(PortIdentity * identity,
			uint16_t sequenceId,
			Timestamp & timestamp,
			unsigned &clock_value,
			bool last) = 0;

	/**
	 * @brief  Get rx timestamp
	 * @param  identity PTP port identity
	 * @param  sequenceId Sequence ID
	 * @param  timestamp [inout] Timestamp value
	 * @param  clock_value [inout] Clock value
	 * @param  last Boolean flag
	 * @return A signed integer
	 */
	virtual int HWTimestamper_rxtimestamp(PortIdentity * identity,
			uint16_t sequenceId,
			Timestamp & timestamp,
			unsigned &clock_value,
			bool last) = 0;

	/**
	 * @brief  Get external clock offset
	 * @param  local_time [inout] Local time
	 * @param  clk_offset [inout] clock offset
	 * @param  ppt_freq_offset [inout] Frequency offset in ppts
	 * @return false
	 */
	virtual bool HWTimestamper_get_extclk_offset(Timestamp * local_time,
			int64_t * clk_offset,
			int32_t *
			ppt_freq_offset) {
		return false;
	}

	/**
	 * @brief  Gets a string with the error
	 * @param  msg [out] String error
	 * @return void
	 */
	virtual void HWTimestamper_get_extderror(char *msg) {
		*msg = '\0';
	}

	/**
	 * @brief  Starts the PPS (pulse per second) interface
	 * @return false
	 */
	virtual bool HWTimestamper_PPS_start() { return false; };

	/**
	 * @brief  Stops the PPS (pulse per second) interface
	 * @return true
	 */
	virtual bool HWTimestamper_PPS_stop() { return true; };

	/**
	 * @brief  Gets the HWTimestamper version
	 * @return version (signed integer)
	 */
	int getVersion() {
		return version;
	}

	/**
	 * Default constructor. Sets version to zero.
	 */
	HWTimestamper() { version = 0; }

	/*Deletes HWtimestamper object
	*/
	virtual ~HWTimestamper() { }
};

/**
 * @brief  Builds a PTP message
 * @param  buf [in] message buffer to send
 * @param  size message length
 * @param  remote Destination link layer address
 * @param  port [in] IEEE1588 port
 * @return PTP message instace of PTPMessageCommon
 */
PTPMessageCommon *buildPTPMessage(char *buf, int size,
		LinkLayerAddress * remote,
		IEEE1588Port * port);

#endif
