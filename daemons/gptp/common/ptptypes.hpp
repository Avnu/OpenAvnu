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

#ifndef PTP_TYPES_HPP
#define PTP_TYPES_HPP

/**@file*/

#if defined(__clang__) &&  defined(__x86_64__)
// Clang/llvm has incompatible long double (fp128) for x86_64.
typedef double FrequencyRatio; /*!< Frequency Ratio */
#else
typedef long double FrequencyRatio; /*!< Frequency Ratio */
#endif

#define ETHER_ADDR_OCTETS	6		/*!< Number of octets in a link layer address*/
#define IP_ADDR_OCTETS		4		/*!< Number of octets in a ip address*/
#define PTP_ETHERTYPE 0x88F7		/*!< PTP ethertype */
#define AVTP_ETHERTYPE 0x22F0   /*!< AVTP ethertype used for Test Status Message */

/**
 * @brief PortState enumeration
 */
typedef enum {
	PTP_MASTER = 7,		//!< Port is PTP Master
	PTP_PRE_MASTER,		//!< Port is not PTP Master yet.
	PTP_SLAVE,			//!< Port is PTP Slave
	PTP_UNCALIBRATED,	//!< Port is uncalibrated.
	PTP_DISABLED,		//!< Port is not PTP enabled. All messages are ignored when in this state.
	PTP_FAULTY,			//!< Port is in a faulty state. Recovery is implementation specific.
	PTP_INITIALIZING,	//!< Port's initial state.
	PTP_LISTENING		//!< Port is in a PTP listening state. Currently not in use.
} PortState;

#endif/*PTP_TYPES_HPP*/
