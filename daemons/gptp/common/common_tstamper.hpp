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

#ifndef COMMON_TSTAMPER_HPP
#define COMMON_TSTAMPER_HPP

#include <unordered_map>
#include <stdint.h>
#include <memory>

#include "ptptypes.hpp"

#define HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE 4096	/*!< Maximum size of HWTimestamper extended message */

class EtherPort;
class InterfaceLabel;
class Timestamp;
class OSNetworkInterface;
class PortIdentity;
class PTPMessageId;

/**
 * @brief Provides a generic interface for hardware timestamping
 */
class CommonTimestamper
{
	public:
			 /**
		 * @brief Default constructor. Sets version to zero.
		 */
		CommonTimestamper();

		 /**
		 * @brief Deletes HWtimestamper object
		 */
		virtual ~CommonTimestamper();

	public:
		/**
		 * @brief Initializes the hardware timestamp unit
		 * @param iface_label [in] Interface label
		 * @param iface [in] Network interface
		 * @return true
		 */
		virtual bool HWTimestamper_init(InterfaceLabel *iface_label,
		 OSNetworkInterface *iface, EtherPort *port = nullptr);

		/**
		 * @brief Reset the hardware timestamp unit
		 * @return void
		 */
		virtual void HWTimestamper_reset(void);

		/**
		 * @brief  This method is called before the object is de-allocated.
		 * @return void
		 */
		virtual void HWTimestamper_final(void);

		/**
		 * @brief  Adjusts the hardware clock frequency
		 * @param  frequency_offset Frequency offset
		 * @return false
		 */
		virtual bool HWTimestamper_adjclockrate(float frequency_offset) const;

		/**
		 * @brief  Adjusts the hardware clock phase
		 * @param  phase_adjust Phase offset
		 * @return false
		 */
		virtual bool HWTimestamper_adjclockphase(int64_t phase_adjust);

		/**
		 * @brief  Get the cross timestamping information.
		 * The gPTP subsystem uses these samples to calculate
		 * ratios which can be used to translate or extrapolate
		 * one clock into another clock reference. The gPTP service
		 * uses these supplied cross timestamps to perform internal
		 * rate estimation and conversion functions.
		 * @param  system_time [out] System time
		 * @param  device_time [out] Device time
		 * @param  local_clock [out] Local clock
		 * @param  nominal_clock_rate [out] Nominal clock rate
		 * @return True in case of success. FALSE in case of error
		 */
		virtual bool HWTimestamper_gettime(Timestamp * system_time,
		 Timestamp * device_time, uint32_t * local_clock,
		 uint32_t * nominal_clock_rate) const = 0;

		/**
		 * @brief  Gets a string with the error from the hardware timestamp block
		 * @param  msg [out] String error
		 * @return void
		 * @todo There is no current implementation for this method.
		 */
		virtual void HWTimestamper_get_extderror(char *msg) const;

		/**
		 * @brief  Starts the PPS (pulse per second) interface
		 * @return false
		 */
		virtual bool HWTimestamper_PPS_start();

		/**
		 * @brief  Stops the PPS (pulse per second) interface
		 * @return true
		 */
		virtual bool HWTimestamper_PPS_stop();

		virtual int HWTimestamper_txtimestamp
		(std::shared_ptr<PortIdentity> identity, PTPMessageId messageId,
		  Timestamp &timestamp, unsigned &clock_value, bool last ) = 0;

		virtual int HWTimestamper_rxtimestamp(std::shared_ptr<PortIdentity> identity,
			PTPMessageId messageId,
			Timestamp & timestamp,
			unsigned &clock_value,
			bool last) = 0;

		/**
		 * @brief  Gets the HWTimestamper version
		 * @return version (signed integer)
		 */
		int getVersion() const;

		void MasterOffset(FrequencyRatio offset);
		FrequencyRatio MasterOffset() const;
		void AdjustedTime(FrequencyRatio t);
		FrequencyRatio AdjustedTime() const;

	protected:
		uint8_t version; //!< HWTimestamper version

	private:
		FrequencyRatio fMasterOffset;
		FrequencyRatio fAdjustedTime;
};

#endif/*COMMON_TSTAMPER_HPP*/
