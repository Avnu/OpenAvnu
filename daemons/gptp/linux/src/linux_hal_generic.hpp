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

/**@file*/

struct LinuxTimestamperGenericPrivate;
/**
 * @brief Provides LinuxTimestamperGeneric a private type
 */
typedef struct LinuxTimestamperGenericPrivate * LinuxTimestamperGenericPrivate_t;

#ifdef WITH_IGBLIB
struct LinuxTimestamperIGBPrivate;
typedef struct LinuxTimestamperIGBPrivate * LinuxTimestamperIGBPrivate_t;
#endif

/**
 * @brief Linux timestamper generic interface
 */
class LinuxTimestamperGeneric : public LinuxTimestamper {
private:
	int sd;
	int phc_fd;
	Timestamp crstamp_system;
	Timestamp crstamp_device;
	LinuxTimestamperGenericPrivate_t _private;
	bool cross_stamp_good;
	std::list<Timestamp> rxTimestampList;
	LinuxNetworkInterfaceList iface_list;
#ifdef PTP_HW_CROSSTSTAMP
	bool precise_timestamp_enabled;
#endif

	TicketingLock *net_lock;

#ifdef WITH_IGBLIB
	LinuxTimestamperIGBPrivate_t igb_private;
#endif

public:
	/**
	 * @brief Default constructor. Initializes internal variables
	 */
	LinuxTimestamperGeneric();

	/**
	 * @brief Resets frequency adjustment value to zero and calls
	 * linux system calls for frequency adjustment.
	 * @return TRUE if success, FALSE if error.
	 */
	bool resetFrequencyAdjustment();

	/**
	 * @brief  Calls linux system call for adjusting frequency or phase.
	 * @param  tmx [in] Void pointer that must be cast (and filled in correctly) to
	 * the struct timex
	 * @return TRUE if ok, FALSE if error.
	 */
	bool Adjust( void *tmx );

	/**
	 * @brief  Initializes the Hardware timestamp interface
	 * @param  iface_label [in] Network interface label (used to find the phc index)
	 * @param  iface [in] Network interface
	 * @return FALSE in case of error, TRUE if success.
	 */
	virtual bool HWTimestamper_init
	( InterfaceLabel *iface_label, OSNetworkInterface *iface );

	/**
	 * @brief  Reset the Hardware timestamp interface
	 * @return void
	 */
	virtual void HWTimestamper_reset();

	/**
	 * @brief  Inserts a new timestamp to the beginning of the
	 * RX timestamp list.
	 * @param tstamp [in] RX timestamp
	 * @return void
	 */
	void pushRXTimestamp( Timestamp *tstamp ) {
		tstamp->_version = version;
		rxTimestampList.push_front(*tstamp);
	}

	/**
	 * @brief  Post initialization procedure.
	 * @param  ifindex struct ifreq.ifr_ifindex value
	 * @param  sd Socket file descriptor
	 * @param  lock [in] Instance of TicketingLock object
	 * @return TRUE if ok. FALSE if error.
	 */
	bool post_init( int ifindex, int sd, TicketingLock *lock );

	/**
	 * @brief  Gets the ptp clock time information
	 * @param  system_time [out] System time
	 * @param  device_time [out] Device time
	 * @param  local_clock Not Used
	 * @param  nominal_clock_rate Not Used
	 * @return TRUE if got the time successfully, FALSE otherwise
	 */
	virtual bool HWTimestamper_gettime
	( Timestamp *system_time, Timestamp *device_time,
	  uint32_t *local_clock, uint32_t *nominal_clock_rate ) const;

	/**
	 * @brief  Gets the TX timestamp from hardware interface
	 * @param  identity PTP port identity
	 * @param  PTPMessageId Message ID
	 * @param  timestamp [out] Timestamp value
	 * @param  clock_value [out] Clock value
	 * @param  last Signalizes that it is the last timestamp to get. When TRUE, releases the lock when its done.
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	virtual int HWTimestamper_txtimestamp
	( PortIdentity *identity, PTPMessageId messageId, Timestamp &timestamp,
	  unsigned &clock_value, bool last );

	/**
	 * @brief  Gets the RX timestamp from the hardware interface. This
	 * Currently the RX timestamp is retrieved at LinuxNetworkInterface::nrecv method.
	 * @param  identity PTP port identity
	 * @param  PTPMessageId Message ID
	 * @param  timestamp [out] Timestamp value
	 * @param  clock_value [out] Clock value
	 * @param  last Signalizes that it is the last timestamp to get. When TRUE, releases the lock when its done.
     * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	virtual int HWTimestamper_rxtimestamp
	( PortIdentity *identity, PTPMessageId messageId, Timestamp &timestamp,
	  unsigned &clock_value, bool last ) {
		/* This shouldn't happen. Ever. */
		if( rxTimestampList.empty() ) return GPTP_EC_EAGAIN;
		timestamp = rxTimestampList.back();
		rxTimestampList.pop_back();

		return GPTP_EC_SUCCESS;
	}

	/**
	 * @brief  Adjusts the clock phase
	 * @param  phase_adjust Phase adjustment
	 * @return TRUE if success, FALSE if error.
	 */
	virtual bool HWTimestamper_adjclockphase( int64_t phase_adjust );

	/**
	 * @brief  Adjusts the frequency
	 * @param  freq_offset Frequency adjustment
	 * @return TRUE in case of sucess, FALSE if error.
	 */
	virtual bool HWTimestamper_adjclockrate( float freq_offset );

#ifdef WITH_IGBLIB
	bool HWTimestamper_PPS_start( );
	bool HWTimestamper_PPS_stop();
#endif

	/**
	 * @brief deletes LinuxTimestamperGeneric object
	 */
	virtual ~LinuxTimestamperGeneric();
};


#endif/*LINUX_HAL_GENERIC_HPP*/
