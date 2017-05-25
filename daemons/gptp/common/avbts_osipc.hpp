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

#ifndef AVBTS_OSIPC_HPP
#define AVBTS_OSIPC_HPP

#include <stdint.h>
#include <ptptypes.hpp>
#include <ether_port.hpp>

/**@file*/

/**
 * @brief Generic interface for Inter Process Communication arguments
 */
class OS_IPC_ARG {
public:
	virtual ~OS_IPC_ARG() = 0;
};

inline OS_IPC_ARG::~OS_IPC_ARG () { }

/**
 * @brief Generic interface for Inter Process Communication
 */
class OS_IPC {
public:
	/**
	 * @brief  Initializes the IPC
	 * @return Implementation dependent
	 */
    virtual bool init( OS_IPC_ARG *arg = NULL ) = 0;

	/**
	 * @brief  Updates IPC values
	 *
	 * @param ml_phoffset Master to local phase offset
	 * @param ls_phoffset Local to system phase offset
	 * @param ml_freqoffset Master to local frequency offset
	 * @param ls_freq_offset Local to system frequency offset
	 * @param local_time Local time
	 * @param sync_count Count of syncs
	 * @param pdelay_count Count of pdelays
	 * @param port_state Port's state
	 * @param asCapable asCapable flag
	 *
	 * @return Implementation dependent.
	 */
	virtual bool update(
		int64_t  ml_phoffset,
		int64_t ls_phoffset,
		FrequencyRatio  ml_freqoffset,
		FrequencyRatio ls_freq_offset,
		uint64_t local_time,
		uint32_t sync_count,
		uint32_t pdelay_count,
		PortState port_state,
		bool asCapable ) = 0;

	/**
	 * @brief  Updates grandmaster IPC values
	 *
	 * @param gptp_grandmaster_id Current grandmaster id (all 0's if no grandmaster selected)
	 * @param gptp_domain_number gPTP domain number
	 *
	 * @return Implementation dependent.
	 */
	virtual bool update_grandmaster(
		uint8_t gptp_grandmaster_id[],
		uint8_t gptp_domain_number ) = 0;

	/**
	 * @brief  Updates network interface IPC values
	 *
	 * @param  clock_identity  The clock identity of the interface
	 * @param  priority1  The priority1 field of the grandmaster functionality of the interface, or 0xFF if not supported
	 * @param  clock_class  The clockClass field of the grandmaster functionality of the interface, or 0xFF if not supported
	 * @param  offset_scaled_log_variance  The offsetScaledLogVariance field of the grandmaster functionality of the interface, or 0x0000 if not supported
	 * @param  clock_accuracy  The clockAccuracy field of the grandmaster functionality of the interface, or 0xFF if not supported
	 * @param  priority2  The priority2 field of the grandmaster functionality of the interface, or 0xFF if not supported
	 * @param  domain_number  The domainNumber field of the grandmaster functionality of the interface, or 0 if not supported
	 * @param  log_sync_interval  The currentLogSyncInterval field of the grandmaster functionality of the interface, or 0 if not supported
	 * @param  log_announce_interval  The currentLogAnnounceInterval field of the grandmaster functionality of the interface, or 0 if not supported
	 * @param  log_pdelay_interval  The currentLogPDelayReqInterval field of the grandmaster functionality of the interface, or 0 if not supported
	 * @param  port_number  The portNumber field of the interface, or 0x0000 if not supported
	 *
	 * @return Implementation dependent.
	 */
	virtual bool update_network_interface(
		uint8_t  clock_identity[],
		uint8_t  priority1,
		uint8_t  clock_class,
		int16_t  offset_scaled_log_variance,
		uint8_t  clock_accuracy,
		uint8_t  priority2,
		uint8_t  domain_number,
		int8_t   log_sync_interval,
		int8_t   log_announce_interval,
		int8_t   log_pdelay_interval,
		uint16_t port_number ) = 0;

	/*
	 * Destroys IPC
	 */
	virtual ~OS_IPC() = 0;
};

inline OS_IPC::~OS_IPC() {}

#endif

