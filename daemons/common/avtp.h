/****************************************************************************
  Copyright (c) 2015, J.D. Koftinoff Software, Ltd.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of J.D. Koftinoff Software, Ltd. nor the names of its
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

#ifndef AVTP_H_
#define AVTP_H_

/** \addtogroup pdu AVTP and AVDECC PDU definitions */
/*@{*/

#define AVTP_ETHERTYPE (0x22f0)  /// See IEEE Std 1722-2011 Clause 5.1.2
#define AVTP_PAYLOAD_OFFSET (12) /// See IEEE Std 1722-2011 Clause 5.2

/// See IEEE Std 1722-2011 Annex B
#define MAAP_MULTICAST_MAC (0x91e0f000ff00ULL)

/** \addtogroup subtype AVTP Rev1 Subtype definitions - See IEEE P1722-rev1 */
/*@{*/

#define AVTP_SUBTYPE_61883_IIDC (0x00)
#define AVTP_SUBTYPE_MMA_STREAM (0x01)
#define AVTP_SUBTYPE_AAF (0x02)
#define AVTP_SUBTYPE_CVF (0x03)
#define AVTP_SUBTYPE_CRF (0x04)
#define AVTP_SUBTYPE_TSCF (0x05)
#define AVTP_SUBTYPE_SVF (0x06)
#define AVTP_SUBTYPE_RVF (0x07)
#define AVTP_SUBTYPE_AEF_CONTINUOUS (0x6e)
#define AVTP_SUBTYPE_VSF_STREAM (0x6f)
#define AVTP_SUBTYPE_EF_STREAM (0x7f)
#define AVTP_SUBTYPE_MMA_CONTROL (0x81)
#define AVTP_SUBTYPE_NTSCF (0x82)
#define AVTP_SUBTYPE_ESCF (0xec)
#define AVTP_SUBTYPE_EECF (0xed)
#define AVTP_SUBTYPE_AEF_DISCRETE (0xee)
#define AVTP_SUBTYPE_ADP (0xfa)
#define AVTP_SUBTYPE_AECP (0xfb)
#define AVTP_SUBTYPE_ACMP (0xfc)
#define AVTP_SUBTYPE_MAAP (0xfe)
#define AVTP_SUBTYPE_EF_CONTROL (0xff)

/*@}*/

/** \addtogroup subtype_data AVTP subtype_data field - See IEEE Std 1722revD13
 * Clause 5.4 */
/*@{*/

#define AVTP_SUBTYPE_DATA_SUBTYPE_WIDTH (8)
#define AVTP_SUBTYPE_DATA_VERSION_WIDTH (3)
#define AVTP_SUBTYPE_DATA_CONTROL_DATA_WIDTH (4)
#define AVTP_SUBTYPE_DATA_STATUS_WIDTH (5)
#define AVTP_SUBTYPE_DATA_CONTROL_DATA_LENGTH_WIDTH (11)
#define AVTP_SUBTYPE_DATA_STREAM_MR_WIDTH (1)
#define AVTP_SUBTYPE_DATA_STREAM_GV_WIDTH (1)
#define AVTP_SUBTYPE_DATA_STREAM_TV_WIDTH (1)
#define AVTP_SUBTYPE_DATA_STREAM_SEQUENCE_NUM_WIDTH (8)
#define AVTP_SUBTYPE_DATA_STREAM_TU_WIDTH (1)



/** \addtogroup avtp_common_control_header AVTP common control header - See IEEE
 * 1722-2011 Clause 5.3 */
/*@{*/

#define AVTP_COMMON_CONTROL_HEADER_OFFSET_SUBTYPE_DATA (0)
#define AVTP_COMMON_CONTROL_HEADER_OFFSET_STREAM_ID (4)
#define AVTP_COMMON_CONTROL_HEADER_LEN (12)

/*@}*/

/** \addtogroup avtp_common_stream_header AVTP common stream header - See IEEE
 * Std 1722-2011 Clause 5.4 */
/*@{*/

#define AVTP_COMMON_STREAM_HEADER_OFFSET_SUBTYPE_DATA (0)
#define AVTP_COMMON_STREAM_HEADER_OFFSET_STREAM_ID (4)
#define AVTP_COMMON_STREAM_HEADER_OFFSET_TIMESTAMP (12)
#define AVTP_COMMON_STREAM_HEADER_OFFSET_GATEWAY_INFO (16)
#define AVTP_COMMON_STREAM_HEADER_OFFSET_STREAM_DATA_LENGTH (20)
#define AVTP_COMMON_STREAM_HEADER_OFFSET_PROTOCOL_SPECIFIC_HEADER (22)
#define AVTP_COMMON_STREAM_HEADER_LEN (24)

/*@}*/

/*@}*/

#endif
