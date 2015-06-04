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

#include "srp_info.h"

void srp_info_domain_init(struct srp_info_domain *self, uint8_t class_id,
			  uint8_t class_priority, uint16_t class_vid)
{
	self->class_id = class_id;
	self->class_priority = class_priority;
	self->class_vid = class_vid;
}

void srp_info_talker_advertise_init(
    struct srp_info_talker *self, const struct jdksavdecc_eui64 *stream_id,
    const struct jdksavdecc_eui48 *destination_address,
    uint16_t vlan_identifier, uint16_t max_frame_size,
    uint16_t max_interval_frames, uint8_t data_frame_priority, uint8_t rank,
    uint32_t accumulated_latency)
{
	self->stream_id = *stream_id;
	self->data_frame_parameters.destination_address = *destination_address;
	self->data_frame_parameters.vlan_identifier = vlan_identifier;
	self->tspec.max_frame_size = max_frame_size;
	self->tspec.max_interval_frames = max_interval_frames;
	self->priority_and_rank.data_frame_priority = data_frame_priority;
	self->priority_and_rank.rank = rank;
	self->accumulated_latency = accumulated_latency;
	self->failure_information.failure_code = SRP_NO_FAILURE;
	jdksavdecc_eui64_init(&self->failure_information.failure_bridge_id);
}

void srp_info_talker_fail_init(
    struct srp_info_talker *self, const struct jdksavdecc_eui64 *stream_id,
    const struct jdksavdecc_eui48 *destination_address,
    uint16_t vlan_identifier, uint16_t max_frame_size,
    uint16_t max_interval_frames, uint8_t data_frame_priority, uint8_t rank,
    uint32_t accumulated_latency,
    const struct jdksavdecc_eui64 *failure_bridge_id, uint8_t failure_code)
{
	self->stream_id = *stream_id;
	self->data_frame_parameters.destination_address = *destination_address;
	self->data_frame_parameters.vlan_identifier = vlan_identifier;
	self->tspec.max_frame_size = max_frame_size;
	self->tspec.max_interval_frames = max_interval_frames;
	self->priority_and_rank.data_frame_priority = data_frame_priority;
	self->priority_and_rank.rank = rank;
	self->accumulated_latency = accumulated_latency;
	self->failure_information.failure_bridge_id = *failure_bridge_id;
	self->failure_information.failure_code = failure_code;
}

void srp_info_listener_init(struct srp_info_listener *self,
			    const struct jdksavdecc_eui64 *stream_id)
{
	self->stream_id = *stream_id;
}
