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

#include <stdlib.h>
#include "adpdu.h"

int adpdu_read(struct adpdu *p, const void *base, int pos, int len)
{
	int r = pdu_validate_range(pos, len, ADPDU_LEN);
	if (r >= 0) {
		p->subtype = avtp_common_control_header_get_subtype(base, pos);
		p->sv = avtp_common_control_header_get_sv(base, pos);
		p->version = avtp_common_control_header_get_version(base, pos);
		p->message_type =
		    avtp_common_control_header_get_control_data(base, pos);
		p->valid_time =
		    avtp_common_control_header_get_status(base, pos);
		p->control_data_length =
		    avtp_common_control_header_get_control_data_length(base,
								       pos);
		eui64_init_from_uint64(
		    &p->entity_id,
		    avtp_common_control_header_get_stream_id(base, pos));
		p->entity_model_id = adpdu_get_entity_model_id(base, pos);
		p->entity_capabilities =
		    adpdu_get_entity_capabilities(base, pos);
		p->talker_stream_sources =
		    adpdu_get_talker_stream_sources(base, pos);
		p->talker_capabilities =
		    adpdu_get_talker_capabilities(base, pos);
		p->listener_stream_sinks =
		    adpdu_get_listener_stream_sinks(base, pos);
		p->listener_capabilities =
		    adpdu_get_listener_capabilities(base, pos);
		p->controller_capabilities =
		    adpdu_get_controller_capabilities(base, pos);
		p->available_index = adpdu_get_available_index(base, pos);
		p->gptp_grandmaster_id =
		    adpdu_get_gptp_grandmaster_id(base, pos);
		p->gptp_domain_number = adpdu_get_gptp_domain_number(base, pos);
		p->identify_control_index =
		    adpdu_get_identify_control_index(base, pos);
		p->interface_index = adpdu_get_interface_index(base, pos);
		p->reserved0 = adpdu_get_reserved0(base, pos);
		p->association_id = adpdu_get_association_id(base, pos);
		p->reserved1 = adpdu_get_reserved1(base, pos);
	}
	return r;
}

int adpdu_write(const struct adpdu *p, void *base, int pos, int len)
{
	int r = pdu_validate_range(pos, len, ADPDU_LEN);
	if (r >= 0) {
		avtp_common_control_header_set_subtype(p->subtype, base, pos);
		avtp_common_control_header_set_sv(p->sv, base, pos);
		avtp_common_control_header_set_version(p->version, base, pos);
		avtp_common_control_header_set_control_data(p->message_type,
							    base, pos);
		avtp_common_control_header_set_status(p->valid_time, base, pos);
		avtp_common_control_header_set_control_data_length(
		    p->control_data_length, base, pos);
		avtp_common_control_header_set_stream_id(
		    eui64_convert_to_uint64(&p->entity_id), base, pos);
		adpdu_set_entity_model_id(p->entity_model_id, base, pos);
		adpdu_set_entity_capabilities(p->entity_capabilities, base,
					      pos);
		adpdu_set_talker_stream_sources(p->talker_stream_sources, base,
						pos);
		adpdu_set_talker_capabilities(p->talker_capabilities, base,
					      pos);
		adpdu_set_listener_stream_sinks(p->listener_stream_sinks, base,
						pos);
		adpdu_set_listener_capabilities(p->listener_capabilities, base,
						pos);
		adpdu_set_controller_capabilities(p->controller_capabilities,
						  base, pos);
		adpdu_set_available_index(p->available_index, base, pos);
		adpdu_set_gptp_grandmaster_id(p->gptp_grandmaster_id, base,
					      pos);
		adpdu_set_gptp_domain_number(p->gptp_domain_number, base, pos);
		adpdu_set_reserved0(p->reserved0, base, pos);
		adpdu_set_identify_control_index(p->identify_control_index,
						 base, pos);
		adpdu_set_interface_index(p->interface_index, base, pos);
		adpdu_set_association_id(p->association_id, base, pos);
		adpdu_set_reserved1(p->reserved1, base, pos);
	}
	return r;
}
