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

#ifndef IPCDEF_HPP
#define IPCDEF_HPP

/**@file
 * This is a common header file. OS-specific implementations should use
 * this file as base. Currently we have two IPC implementations:
 * Linux: Located at linux/src/linux_ipc.hpp (among other files that include this)
 * Windows: Located at windows/daemon_cl/windows_ipc.hpp
*/

#if defined (__unix__) || defined(__linux__)
#include <sys/types.h>

/*Type for process id*/
#define PID_TYPE    pid_t

#elif defined(_WIN32) || defined(_WIN64)

/*Definition of DWORD*/
#include <IntSafe.h>

/*Type for process ID*/
#define PID_TYPE    DWORD

#else
/*Create new ifdefs for different OSs and/or add to the existing ones*/
#error "ERROR. OS not supported"
#endif /*__unix__ / _WIN*/

#include <ptptypes.hpp>

#ifndef PTP_CLOCK_IDENTITY_LENGTH
#define PTP_CLOCK_IDENTITY_LENGTH 8		/*!< Size of a clock identifier stored in the ClockIndentity class, described at IEEE 802.1AS Clause 8.5.2.4*/
#endif

/**
 * @brief Provides a data structure for gPTP time
 */
typedef struct {
    int64_t ml_phoffset;			//!< Master to local phase offset
    int64_t ls_phoffset;			//!< Local to system phase offset
    FrequencyRatio ml_freqoffset;	//!< Master to local frequency offset
    FrequencyRatio ls_freqoffset;	//!< Local to system frequency offset
    uint64_t local_time;			//!< Local time of last update
    uint8_t grandmaster_id[PTP_CLOCK_IDENTITY_LENGTH];	//!< Current grandmaster id (all 0's if no grandmaster selected)
    uint8_t domain_number;			//!< gPTP domain number
    uint32_t sync_count;			//!< Sync messages count
    uint32_t pdelay_count;			//!< pdelay messages count
    bool asCapable;                 //!< asCapable flag: true = device is AS Capable; false otherwise
    PortState port_state;			//!< gPTP port state. It can assume values defined at ::PortState
    PID_TYPE process_id;			//!< Process id number
} gPtpTimeData;

/*

   Integer64  <master-local phase offset>
   Integer64  <local-system phase offset>
   LongDouble <master-local frequency offset>
   LongDouble <local-system frequency offset>
   UInteger64 <local time of last update>

 * Meaning of IPC provided values:

 master  ~= local   - <master-local phase offset>
 local   ~= system  - <local-system phase offset>
 Dmaster ~= Dlocal  * <master-local frequency offset>
 Dlocal  ~= Dsystem * <local-system freq offset>        (where D denotes a delta)

*/

#endif/*IPCDEF_HPP*/

