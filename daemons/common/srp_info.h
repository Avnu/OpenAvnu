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

#ifndef SRP_INFO_H_
#define SRP_INFO_H_

#include "pdu.h"
#include "eui64.h"
#include "eui48.h"

/** \addtogroup srp_failure_codes
 *  See IEEE Std 802.1Q-2011 Clause 35.2.2.8.7 Table 35-6
 */
/* @{ */

#define SRP_INFO_NO_FAILURE ( 0 )
#define SRP_INFO_INSUFFICIENT_BANDWIDTH ( 1 )
#define SRP_INFO_INSUFFICIENT_BRIDGE_RESOURCES ( 2 )
#define SRP_INFO_INSUFFICIENT_BANDWIDTH_FOR_TRAFFIC_CLASS ( 3 )
#define SRP_INFO_STREAM_ID_IN_USE_BY_TALKER ( 4 )
#define SRP_INFO_STREAM_DESTINATION_ADDRESS_IN_USE ( 5 )
#define SRP_INFO_STREAM_PREEMPTED_BY_HIGHER_RANK ( 6 )
#define SRP_INFO_LATENCY_CHANGED ( 7 )
#define SRP_INFO_PORT_NOT_AVB_CAPABLE ( 8 )
#define SRP_INFO_USE_DIFFERENT_DESTINATION_ADDRESS ( 9 )
#define SRP_INFO_OUT_OF_MSRP_RESOURCES ( 10 )
#define SRP_INFO_OUT_OF_MMRP_RESOURCES ( 11 )
#define SRP_INFO_CANNOT_STORE_DESTINATION_ADDRESS ( 12 )
#define SRP_INFO_NOT_SR_CLASS_PRIORITY ( 13 )
#define SRP_INFO_MAX_FRAME_SIZE_TOO_LARGE ( 14 )
#define SRP_INFO_MSRP_MAX_FAN_IN_PORTS_REACHED ( 15 )
#define SRP_INFO_CHANGES_IN_FIRSTVALUE_FOR_STREAMID ( 16 )
#define SRP_INFO_VLAN_BLOCKED_ON_PORT ( 17 )
#define SRP_INFO_VLAN_TAGGING_DISABLED_ON_PORT ( 18 )
#define SRP_INFO_PRIORITY_MISMATCH ( 19 )
/* @} */

/** \addtogroup srp_attribute_type SRP AttributeTypes
 *  See IEEE Std 802.1Q-2011 Clause 35.2.2.4 Table 35-1
 */
/* @{ */
#define SRP_INFO_TALKER_ADVERTISE ( 1 )
#define SRP_INFO_TALKER_FAILED ( 2 )
#define SRP_INFO_LISTENER ( 3 )
#define SRP_INFO_DOMAIN ( 4 )
/* @} */

/** \addtogroup srp_fourpacked_events FourPackedEvent values
 *  See IEEE Std 802.1Q-2011 Clause 35.2.2.5 Table 35-2
 */
/* @{ */
#define SRP_INFO_FOURPACK_IGNORE ( 0 )
#define SRP_INFO_FOURPACK_ASKING_FAILED ( 1 )
#define SRP_INFO_FOURPACK_READY ( 2 )
#define SRP_INFO_FOURPACK_READY_FAILED ( 3 )
/* @} */

/** \addtogroup srp_threepack_events ThreePackEvent values
 *  See IEEE STd 802.1Q-2011 Clause 10.8.1.2
 */
/* @{ */
#define SRP_INFO_THREEPACK_NEW ( 0 )
#define SRP_INFO_THREEPACK_JOIN_IN ( 1 )
#define SRP_INFO_THREEPACK_IN ( 2 )
#define SRP_INFO_THREEPACK_JOIN_MT ( 3 )
#define SRP_INFO_THREEPACK_MT ( 4 )
#define SRP_INFO_THREEPACK_LV ( 5 )
/* @} */

#define SRP_INFO_CLASS_A ( 6 )
#define SRP_INFO_CLASS_B ( 5 )

/** SRP Domain Attribute
 *  See IEEE Std 802.1Q-2011 Clause 35.2.2.9.1
 */
struct srp_info_domain
{
    uint8_t class_id;
    uint8_t class_priority;
    uint16_t class_vid;
};

/**
 *  SRP First Value for Talker Advertise and Talker Fail attributes.
 *  A failure code of SRP_INFO_NO_FAILURE means Talker Advertise.
 *  See IEEE Std 802.1Q-2011 Clause 35.2.2.8.1
 */
struct srp_info_first_value_talker
{

    /** See IEEE Std 802.1Q-2011 Clause 35.2.2.8.2 */
    struct eui64 stream_id;

    /** See IEEE Std 802.1Q-2011 Clause 35.2.2.8.3 */
    struct
    {
        struct eui48 destination_address;
        uint16_t vlan_identifier;
    } data_frame_parameters;

    /** See IEEE Std 802.1Q-2011 Clause 35.2.2.8.4 */
    struct
    {
        uint16_t max_frame_size;
        uint16_t max_interval_frames;
    } tspec;

    /** See IEEE Std 802.1Q-2011 Clause 35.2.2.8.5 */
    struct
    {
        uint8_t data_frame_priority : 3;
        uint8_t rank : 1;
        uint8_t reserved : 4;
    } priority_and_rank;

    /** See IEEE Std 802.1Q-2011 Clause 35.2.2.8.6 */
    uint32_t accumulated_latency;

    /** See IEEE Std 802.1Q-2011 Clause 35.2.2.8.7 */
    struct
    {
        struct eui64 failure_bridge_id;
        uint8_t failure_code;
    } failure_information;
};

/**
 *  SRP First Value for Listener attribute
 *  See IEEE Std 802.1Q-2011 Clause 35.2.2.8.1
 */
struct srp_info_first_value_listener
{
    struct eui64 stream_id;
};

struct srp_info_talker_vector
{
    struct srp_info_first_value_talker first_value;
    uint16_t number_of_values;
    uint8_t fourpackedevents[2048];
};

struct srp_info_listener_vector
{
    struct srp_info_first_value_listener first_value;
    uint16_t number_of_values;
    uint8_t threepackedevents[2048];
    uint8_t fourpackedevents[2048];
};

struct srp_info_domain_vector
{
    struct srp_info_domain first_value;
    uint16_t number_of_values;
};

#endif /* SRP_INFO_H_ */
