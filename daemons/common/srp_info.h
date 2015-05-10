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
#include "srp.h"

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
 * @brief srp_info_domain_init
 *
 * Initialize an srp_info_domain object
 *
 * @param self
 * @param class_id
 * @param class_priority
 * @param class_vid
 */
void srp_info_domain_init(
        struct srp_info_domain *self,
        uint8_t class_id,
        uint8_t class_priority,
        uint16_t class_vid
        );

/**
 *  SRP First Value for Talker Advertise and Talker Fail attributes.
 *  A failure code of SRP_INFO_NO_FAILURE means Talker Advertise.
 *  See IEEE Std 802.1Q-2011 Clause 35.2.2.8.1
 */
struct srp_info_talker
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
 * @brief srp_info_talker_advertise_init
 *
 * Initialize an srp_info_talker object to contain a talker advertise
 *
 * @param self
 * @param stream_id
 * @param destination_address
 * @param vlan_identifier
 * @param max_frame_size
 * @param max_interval_frames
 * @param data_frame_priority
 * @param rank
 * @param accumulated_latency
 */
void srp_info_talker_advertise_init(
        struct srp_info_talker *self,
        struct eui64 const *stream_id,
        struct eui48 const *destination_address,
        uint16_t vlan_identifier,
        uint16_t max_frame_size,
        uint16_t max_interval_frames,
        uint8_t data_frame_priority,
        uint8_t rank,
        uint32_t accumulated_latency
        );

/**
 * @brief srp_info_talker_fail_init
 *
 * Initialize an srp_info_talker object to contain a talker fail
 *
 * @param self
 * @param stream_id
 * @param destination_address
 * @param vlan_identifier
 * @param max_frame_size
 * @param max_interval_frames
 * @param data_frame_priority
 * @param rank
 * @param accumulated_latency
 * @param failure_bridge_id
 * @param failure_code
 */
void srp_info_talker_fail_init(
        struct srp_info_talker *self,
        struct eui64 const *stream_id,
        struct eui48 const *destination_address,
        uint16_t vlan_identifier,
        uint16_t max_frame_size,
        uint16_t max_interval_frames,
        uint8_t data_frame_priority,
        uint8_t rank,
        uint32_t accumulated_latency,
        struct eui64 const *failure_bridge_id,
        uint8_t failure_code
        );

/**
 *  SRP First Value for Listener attribute
 *  See IEEE Std 802.1Q-2011 Clause 35.2.2.8.1
 */
struct srp_info_listener
{
    struct eui64 stream_id;
};

/**
 * @brief srp_info_listener_init
 *
 * Initialize an srp_info_listener object to contain a stream_id
 *
 * @param self
 * @param stream_id
 */
void srp_info_listener_init(
        struct srp_info_listener *self,
        struct eui64 const *stream_id );


#endif /* SRP_INFO_H_ */
