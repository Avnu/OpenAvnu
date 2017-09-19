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

#ifndef ETHER_TSTAMPER_HPP
#define ETHER_TSTAMPER_HPP

#include <common_tstamper.hpp>

class EtherTimestamper : public CommonTimestamper
{
public:
	/**
	 * @brief  Gets the tx timestamp from hardware
	 * @param  identity PTP port identity
	 * @param  PTPMessageId Message ID
	 * @param  timestamp [out] Timestamp value
	 * @param  clock_value [out] Clock value
	 * @param  last Signalizes that it is the last timestamp to get. When TRUE, releases the lock when its done.
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	virtual int HWTimestamper_txtimestamp
	( PortIdentity * identity, PTPMessageId messageId,
	  Timestamp &timestamp, unsigned &clock_value, bool last ) = 0;

	/**
	 * @brief  Get rx timestamp
	 * @param  identity PTP port identity
	 * @param  messageId Message ID
	 * @param  timestamp [out] Timestamp value
	 * @param  clock_value [out] Clock value
	 * @param  last Signalizes that it is the last timestamp to get. When TRUE, releases the lock when its done.
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	virtual int HWTimestamper_rxtimestamp(PortIdentity * identity,
			PTPMessageId messageId,
			Timestamp & timestamp,
			unsigned &clock_value,
			bool last) = 0;

	virtual ~EtherTimestamper() {}
};

#endif/*ETHER_TSTAMPER_HPP*/
