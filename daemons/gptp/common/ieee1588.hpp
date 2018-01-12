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

/** @file */

#include <string>

#include <stdint.h>

#include <string.h>

#include <stdio.h>

#include <platform.hpp>
#include <ptptypes.hpp>

#include <gptp_log.hpp>
#include <limits.h>

#define MAX_PORTS 32	/*!< Maximum number of EtherPort instances */


/**
 * @brief Return codes for gPTP
*/
#define GPTP_EC_SUCCESS     0       /*!< No errors.*/
#define GPTP_EC_FAILURE     -1      /*!< Generic error */
#define GPTP_EC_EAGAIN      -72     /*!< Error: Try again */


class LinkLayerAddress;
struct ClockQuality;
class PortIdentity;
class PTPMessageId;
class PTPMessageCommon;
class PTPMessageSync;
class PTPMessageAnnounce;
class PTPMessagePathDelayReq;
class PTPMessagePathDelayResp;
class PTPMessagePathDelayRespFollowUp;
class EtherPort;
class CommonPort;
class IEEE1588Clock;
class OSNetworkInterface;

/**
 * @enum Event
 * IEEE 1588 event enumeration type
 * Defined at: IEEE 1588-2008 Clause 9.2.6
 */
typedef enum {
	NULL_EVENT = 0,						//!< Null Event. Used to initialize events.
	POWERUP = 5,						//!< Power Up. Initialize state machines.
	INITIALIZE,							//!< Same as POWERUP.
	LINKUP,								//!< Triggered when link comes up.
	LINKDOWN,							//!< Triggered when link goes down.
	STATE_CHANGE_EVENT,					//!< Signalizes that something has changed. Recalculates best master.
	SYNC_INTERVAL_TIMEOUT_EXPIRES,		//!< Sync interval expired. Its time to send a sync message.
	PDELAY_INTERVAL_TIMEOUT_EXPIRES,	//!< PDELAY interval expired. Its time to send pdelay_req message
	SYNC_RECEIPT_TIMEOUT_EXPIRES,		//!< Sync receipt timeout. Restart timers and take actions based on port's state.
	QUALIFICATION_TIMEOUT_EXPIRES,		//!< Qualification timeout. Event not currently used
	ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,	//!< Announce receipt timeout. Same as SYNC_RECEIPT_TIMEOUT_EXPIRES
	ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES,	//!< Announce interval timout. Its time to send an announce message if asCapable is true
	FAULT_DETECTED,						//!< A fault was detected.
	PDELAY_DEFERRED_PROCESSING,			//!< Defers pdelay processing
	PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES,	//!< Pdelay response message timeout
	PDELAY_RESP_PEER_MISBEHAVING_TIMEOUT_EXPIRES,	//!< Timeout for peer misbehaving. This even will re-enable the PDelay Requests
	SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED,  //!< Sync rate signal timeout for the Automotive Profile
} Event;

/**
 * @brief Defines an event descriptor type
 */
typedef struct {
	CommonPort *port;	//!< Media Dependent Ether Port
	Event event;	//!< Event enumeration
} event_descriptor_t;

/**
 * @brief Provides a generic InterfaceLabel class
 */
class InterfaceLabel {
 public:
	virtual ~ InterfaceLabel() {
	};
};

/**
 * @brief Provides a ClockIdentity abstraction
 * See IEEE 802.1AS-2011 Clause 8.5.2.2
 */
class ClockIdentity {
 private:
	uint8_t id[PTP_CLOCK_IDENTITY_LENGTH];
 public:
	/**
	 * @brief Default constructor. Sets ID to zero
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
	 * @param  id [out] Value copied from the object ClockIdentity. Needs to be at least ::PTP_CLOCK_IDENTITY_LENGTH long.
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
	 * long whereas link layer address is 6 octets long and it is turned into a
	 * clock identity as per the 802.1AS standard described in clause 8.5.2.2
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
		GPTP_LOG_VERBOSE
			( "Clock Identity(%s): %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx",
			  str, id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7] );
	}
};

#define INVALID_TIMESTAMP_VERSION 0xFF		/*!< Value defining invalid timestamp version*/
#define MAX_NANOSECONDS 1000000000			/*!< Maximum value of nanoseconds (1 second)*/
#define MAX_TSTAMP_STRLEN 25				/*!< Maximum size of timestamp strlen*/

/**
 * @brief Provides a Timestamp interface
 */
class Timestamp {
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
		Timestamp( 0, 0, 0 );
	}
	uint32_t nanoseconds;	//!< 32 bit nanoseconds value
	uint32_t seconds_ls;	//!< 32 bit seconds LSB value
	uint16_t seconds_ms;	//!< 32 bit seconds MSB value
	uint8_t _version;		//!< 8 bit version value

	/**
	 * @brief Copies the timestamp to the internal string in the following format:
	 * seconds_ms seconds_ls nanoseconds
	 * @return STL string containing timestamp
	 */
	std::string toString() const
	{
		char output_string[MAX_TSTAMP_STRLEN+1];

		PLAT_snprintf
			( output_string, MAX_TSTAMP_STRLEN+1, "%hu %u.%09u",
			  seconds_ms, seconds_ls, nanoseconds );

		return std::string( output_string );
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

#define INVALID_TIMESTAMP (Timestamp( 0xC0000000, 0, 0 ))	/*!< Defines an invalid timestamp using a Timestamp instance and a fixed value*/
#define PDELAY_PENDING_TIMESTAMP (Timestamp( 0xC0000001, 0, 0 ))	/*!< PDelay is pending timestamp */

static inline uint64_t TIMESTAMP_TO_NS(Timestamp &ts)
{
	return (((static_cast<long long int>(ts.seconds_ms) << sizeof(ts.seconds_ls)*8) +
			      ts.seconds_ls)*1000000000LL + ts.nanoseconds)	;	/*!< Converts timestamp value into nanoseconds value*/
}

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

#define NS_PER_SECOND 1000000000		/*!< Amount of nanoseconds in a second*/
#define LS_SEC_MAX 0xFFFFFFFFull		/*!< Maximum value of seconds LSB field */

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

#endif
