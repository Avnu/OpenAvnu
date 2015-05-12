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

#ifndef SRP_H_
#define SRP_H_

/** \addtogroup srp_attribute_type SRP AttributeTypes
 *  See IEEE Std 802.1Q-2011 Clause 35.2.2.4 Table 35-1
 */
/* @{ */
#define SRP_TALKER_ADVERTISE (1)
#define SRP_TALKER_FAILED (2)
#define SRP_LISTENER (3)
#define SRP_DOMAIN (4)
/* @} */

/** \addtogroup srp_fourpacked_events FourPackedEvent values
 *  See IEEE Std 802.1Q-2011 Clause 35.2.2.5 Table 35-2
 */
/* @{ */
#define SRP_FOURPACK_IGNORE (0)
#define SRP_FOURPACK_ASKING_FAILED (1)
#define SRP_FOURPACK_READY (2)
#define SRP_FOURPACK_READY_FAILED (3)
/* @} */

/** \addtogroup srp_threepack_events ThreePackEvent values
 *  See IEEE Std 802.1Q-2011 Clause 10.8.1.2
 */
/* @{ */
#define SRP_THREEPACK_NEW (0)
#define SRP_THREEPACK_JOIN_IN (1)
#define SRP_THREEPACK_IN (2)
#define SRP_THREEPACK_JOIN_MT (3)
#define SRP_THREEPACK_MT (4)
#define SRP_THREEPACK_LV (5)
/* @} */

/** \addtogroup srp_failure_codes
 *  See IEEE Std 802.1Q-2011 Clause 35.2.2.8.7 Table 35-6
 */
/* @{ */
#define SRP_NO_FAILURE (0)
#define SRP_INSUFFICIENT_BANDWIDTH (1)
#define SRP_INSUFFICIENT_BRIDGE_RESOURCES (2)
#define SRP_INSUFFICIENT_BANDWIDTH_FOR_TRAFFIC_CLASS (3)
#define SRP_STREAM_ID_IN_USE_BY_TALKER (4)
#define SRP_STREAM_DESTINATION_ADDRESS_IN_USE (5)
#define SRP_STREAM_PREEMPTED_BY_HIGHER_RANK (6)
#define SRP_LATENCY_CHANGED (7)
#define SRP_PORT_NOT_AVB_CAPABLE (8)
#define SRP_USE_DIFFERENT_DESTINATION_ADDRESS (9)
#define SRP_OUT_OF_MSRP_RESOURCES (10)
#define SRP_OUT_OF_MMRP_RESOURCES (11)
#define SRP_CANNOT_STORE_DESTINATION_ADDRESS (12)
#define SRP_NOT_SR_CLASS_PRIORITY (13)
#define SRP_MAX_FRAME_SIZE_TOO_LARGE (14)
#define SRP_MSRP_MAX_FAN_IN_PORTS_REACHED (15)
#define SRP_CHANGES_IN_FIRSTVALUE_FOR_STREAMID (16)
#define SRP_VLAN_BLOCKED_ON_PORT (17)
#define SRP_VLAN_TAGGING_DISABLED_ON_PORT (18)
#define SRP_PRIORITY_MISMATCH (19)
/* @} */

/** \addtogroup srp_class_id SR Class ID values
 *  See IEEE Std 802.1Q-2011 Clause 35.2.2.9.2
 */
/* @{ */
#define SRP_CLASS_A (6)
#define SRP_CLASS_B (5)
/* @} */

#endif /* SRP_H_ */
