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
	int phc_fd;
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
	/**
	 * Default constructor. Initializes internal variables
	 */
	LinuxTimestamperGeneric();

	/**
	 * @brief Resets frequency adjustment value to zero and call
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
	( Timestamp *system_time, Timestamp *device_time, uint32_t *local_clock,
	  uint32_t *nominal_clock_rate );

	/**
	 * @brief  Gets the TX timestamp from hardware interface
	 * @param  identity  Not Used
	 * @param sequenceId Not Used
	 * @param timestamp [out] Reference to the HW timestamp object
	 * @param clock_value Not Used
	 * @param last Flag that unlocks the net_lock lock when its done if is set to TRUE
	 * @return -1 Error, -72 to try again (EAGAIN), 0 success.
	 */
	virtual int HWTimestamper_txtimestamp
	( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
	  unsigned &clock_value, bool last );

	/**
	 * @brief  Gets the RX timestamp from the hardware interface. This
	 * method is not currently in use by the LinuxTimestamperGeneric.
	 * @param identity Not Used
	 * @param sequenceId Not Used
	 * @param timestamp Not Used
	 * @param clock_value Not Used
	 * @param last Not Used
	 * @return -1 error, -72 to try again. 0 Success.
	 */
	virtual int HWTimestamper_rxtimestamp
	( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
	  unsigned &clock_value, bool last ) {
		/* This shouldn't happen. Ever. */
		if( rxTimestampList.empty() ) return -72;
		timestamp = rxTimestampList.back();
		rxTimestampList.pop_back();
		
		return 0;
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

	/*
	 * deletes LinuxTimestamperGeneric object
	 */
	virtual ~LinuxTimestamperGeneric();
};


#endif/*LINUX_HAL_GENERIC_HPP*/
